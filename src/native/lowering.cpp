// AI-generated code; reviewed for this repository's vNext rewrite.
#include "native/lowering.hpp"

#include <algorithm>
#include <bit>
#include <cctype>
#include <format>
#include <functional>
#include <optional>
#include <set>
#include <sstream>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace NG::native
{
  namespace
  {
    using flowir::Function;
    using flowir::Instruction;
    using flowir::InstructionKind;
    using flowir::Terminator;
    using flowir::TerminatorKind;
    using flowir::ValueId;
    using hir::ExpressionKind;
    using typecheck::TypeId;
    using typecheck::TypeKind;

    // Tier 0 scalar mapping: every NG integer (incl. bool — stored as i64
    // 0/1) is a QBE `l`, and every float (f32/f64; Value stores doubles) is a
    // QBE `d`. Structs map to QBE aggregate types (values are pointers into
    // the aggregate's memory). `None` marks unit-typed results.
    enum class QType
    {
      Long,
      Double,
      None,
      Aggregate,
    };

    [[nodiscard]] auto suffix(QType type) -> const char *
    {
      switch (type)
      {
      case QType::Long: return "l";
      case QType::Double: return "d";
      case QType::None: return "";
      case QType::Aggregate: return "l"; // call sites pass pointers at IL level
      }
      return "";
    }

    [[nodiscard]] auto isFloat(TypeId type) -> bool { return typecheck::isFloatBuiltin(type); }

    /// QBE double literal: `d_0.5`. A bare integer formatting is given a `.0`
    /// so the parser reads it as a float constant.
    [[nodiscard]] auto doubleConstant(double value) -> std::string
    {
      std::string text = std::format("{}", value);
      if (text.find_first_of(".en") == std::string::npos) text += ".0";
      return "d_" + text;
    }

    /// Escapes a raw byte string for a QBE `b "..."` data item. QBE's string
    /// lexer copies characters verbatim; only `\` and `"` carry escape
    /// meaning.
    [[nodiscard]] auto escapeQbeString(std::string_view text) -> std::string
    {
      std::string escaped;
      escaped.reserve(text.size());
      for (const char character : text)
      {
        if (character == '\\' || character == '"') escaped += '\\';
        escaped += character;
      }
      return escaped;
    }

    /// Per-module ngrt bookkeeping: runtime helper functions are emitted once
    /// per module, on first use, as QBE IL text appended after all functions.
    struct NgrtContext
    {
      std::set<std::string> usedHelpers;
      bool needsTraitDispatch{};
      /// DefIds of `native fun` placeholders: calling one from native code is
      /// rejected until the declared-descriptor shims land (M5).
      std::unordered_set<uint32_t> nativeDefIds;
      /// R9 first slice: DefId -> declared parameter types of `native fun`
      /// declarations (arity validation and shim selection).
      std::unordered_map<uint32_t, std::vector<TypeId>> nativeSignatures;
      /// B3 first slice: DefIds of `extern "C"` declarations. Call sites emit
      /// direct QBE calls to the C symbol instead of an NG function symbol.
      std::unordered_set<uint32_t> externDefIds;
      /// B3 first slice: DefId -> declared parameter types and result of
      /// `extern "C"` declarations (authoritative for the C ABI; NG call
      /// sites widen sub-word values to i64).
      std::unordered_map<uint32_t, std::vector<TypeId>> externParameterTypes;
      std::unordered_map<uint32_t, TypeId> externResultTypes;
    };

    /// Builds a QBE symbol for a named function. `main` keeps its C-runtime
    /// name; everything else is `<sanitized-name>_<defid>` so that overloads,
    /// generic instances (`foo#3`), and same-named functions from different
    /// modules cannot collide.
    [[nodiscard]] auto qbeSymbol(const std::string &name, uint32_t defId) -> std::string
    {
      if (name == "main") return "$main";
      std::string cleaned = name;
      for (auto &character : cleaned)
        if (!std::isalnum(static_cast<unsigned char>(character)) && character != '_' && character != '.')
          character = '_';
      return std::format("${}_{}", cleaned, defId);
    }

    // Tier 0 runtime helpers. Aggregate values are 8-byte pointers into
    // malloc'd objects (or QBE data items for string literals):
    //   string: { l len, b data[len], b 0 }
    //   array/tuple: { l len, l cap, l-or-d elements[cap] }
    //   range: { l start, l end }
    // All aggregate slots hold `l`; double elements are bit-cast at the
    // boundary. Helpers call libc (malloc/memcpy/memcmp) through QBE's C ABI.
    [[nodiscard]] auto ngrtHelpers() -> const std::unordered_map<std::string, std::string> &
    {
      static const std::unordered_map<std::string, std::string> helpers = {
        {"panic",
         // Halting trap for paths the VM rejects (e.g. unresolved trait-slot
         // calls in dead generic originals).
         "function l $ngrt_panic() {\n"
         "@start\n"
         "\thlt\n"
         "}\n"},
        {"checked_add",
         "function l $ngrt_checked_add(l %a, l %b) {\n"
         "@start\n"
         "\t%r =l add %a, %b\n"
         "\t%ap =w csgtl %a, 0\n"
         "\t%bp =w csgtl %b, 0\n"
         "\t%rn =w cslel %r, 0\n"
         "\t%an =w csltl %a, 0\n"
         "\t%bn =w csltl %b, 0\n"
         "\t%rz =w csgel %r, 0\n"
         "\t%o1 =w and %ap, %bp\n"
         "\t%o1 =w and %o1, %rn\n"
         "\t%o2 =w and %an, %bn\n"
         "\t%o2 =w and %o2, %rz\n"
         "\t%ov =w or %o1, %o2\n"
         "\tjnz %ov, @halt, @ok\n"
         "@halt\n"
         "\thlt\n"
         "@ok\n"
         "\tret %r\n"
         "}\n"},
        {"checked_sub",
         "function l $ngrt_checked_sub(l %a, l %b) {\n"
         "@start\n"
         "\t%r =l sub %a, %b\n"
         "\t%ap =w csgel %a, 0\n"
         "\t%bn =w csltl %b, 0\n"
         "\t%rn =w csltl %r, 0\n"
         "\t%an =w csltl %a, 0\n"
         "\t%bp =w csgtl %b, 0\n"
         "\t%rz =w csgel %r, 0\n"
         "\t%o1 =w and %ap, %bn\n"
         "\t%o1 =w and %o1, %rn\n"
         "\t%o2 =w and %an, %bp\n"
         "\t%o2 =w and %o2, %rz\n"
         "\t%ov =w or %o1, %o2\n"
         "\tjnz %ov, @halt, @ok\n"
         "@halt\n"
         "\thlt\n"
         "@ok\n"
         "\tret %r\n"
         "}\n"},
        {"checked_mul",
         "function l $ngrt_checked_mul(l %a, l %b) {\n"
         "@start\n"
         "\t%r =l mul %a, %b\n"
         "\t%bz =w ceql %b, 0\n"
         "\t%bm1 =w ceql %b, -1\n"
         "\t%amin =w ceql %a, -9223372036854775808\n"
         "\t%edge =w and %bm1, %amin\n"
         "\t%skip =w or %bz, %edge\n"
         "\t%neg =w ceqw %skip, 0\n"
         "\tjnz %neg, @check, @ok\n"
         "@check\n"
         "\t%q =l div %r, %b\n"
         "\t%same =w ceql %q, %a\n"
         "\tjnz %same, @ok, @halt\n"
         "@halt\n"
         "\thlt\n"
         "@ok\n"
         "\tret %r\n"
         "}\n"},
        {"checked_div",
         "function l $ngrt_checked_div(l %a, l %b) {\n"
         "@start\n"
         "\t%bz =w ceql %b, 0\n"
         "\t%bm1 =w ceql %b, -1\n"
         "\t%amin =w ceql %a, -9223372036854775808\n"
         "\t%edge =w and %bm1, %amin\n"
         "\t%bad =w or %bz, %edge\n"
         "\tjnz %bad, @halt, @ok\n"
         "@halt\n"
         "\thlt\n"
         "@ok\n"
         "\t%r =l div %a, %b\n"
         "\tret %r\n"
         "}\n"},
        {"checked_rem",
         "function l $ngrt_checked_rem(l %a, l %b) {\n"
         "@start\n"
         "\t%bz =w ceql %b, 0\n"
         "\t%bm1 =w ceql %b, -1\n"
         "\t%amin =w ceql %a, -9223372036854775808\n"
         "\t%edge =w and %bm1, %amin\n"
         "\t%bad =w or %bz, %edge\n"
         "\tjnz %bad, @halt, @ok\n"
         "@halt\n"
         "\thlt\n"
         "@ok\n"
         "\t%r =l rem %a, %b\n"
         "\tret %r\n"
         "}\n"},
        {"checked_neg",
         "function l $ngrt_checked_neg(l %a) {\n"
         "@start\n"
         "\t%amin =w ceql %a, -9223372036854775808\n"
         "\tjnz %amin, @halt, @ok\n"
         "@halt\n"
         "\thlt\n"
         "@ok\n"
         "\t%r =l neg %a\n"
         "\tret %r\n"
         "}\n"},
        {"checked_shl",
         "function l $ngrt_checked_shl(l %a, l %b) {\n"
         "@start\n"
         "\t%neg =w csltl %b, 0\n"
         "\t%high =w csgel %b, 64\n"
         "\t%bad =w or %neg, %high\n"
         "\tjnz %bad, @halt, @ok\n"
         "@halt\n"
         "\thlt\n"
         "@ok\n"
         "\t%r =l shl %a, %b\n"
         "\tret %r\n"
         "}\n"},
        {"checked_shr",
         "function l $ngrt_checked_shr(l %a, l %b) {\n"
         "@start\n"
         "\t%neg =w csltl %b, 0\n"
         "\t%high =w csgel %b, 64\n"
         "\t%bad =w or %neg, %high\n"
         "\tjnz %bad, @halt, @ok\n"
         "@halt\n"
         "\thlt\n"
         "@ok\n"
         "\t%r =l shr %a, %b\n"
         "\tret %r\n"
         "}\n"},
        {"check_w_i8",
         "function l $ngrt_check_w_i8(l %v) {\n"
         "@start\n"
         "\t%lo =w csltl %v, -128\n"
         "\t%hi =w csgtl %v, 127\n"
         "\t%bad =w or %lo, %hi\n"
         "\tjnz %bad, @halt, @ok\n"
         "@halt\n"
         "\thlt\n"
         "@ok\n"
         "\tret %v\n"
         "}\n"},
        {"check_w_i16",
         "function l $ngrt_check_w_i16(l %v) {\n"
         "@start\n"
         "\t%lo =w csltl %v, -32768\n"
         "\t%hi =w csgtl %v, 32767\n"
         "\t%bad =w or %lo, %hi\n"
         "\tjnz %bad, @halt, @ok\n"
         "@halt\n"
         "\thlt\n"
         "@ok\n"
         "\tret %v\n"
         "}\n"},
        {"check_w_i32",
         "function l $ngrt_check_w_i32(l %v) {\n"
         "@start\n"
         "\t%lo =w csltl %v, -2147483648\n"
         "\t%hi =w csgtl %v, 2147483647\n"
         "\t%bad =w or %lo, %hi\n"
         "\tjnz %bad, @halt, @ok\n"
         "@halt\n"
         "\thlt\n"
         "@ok\n"
         "\tret %v\n"
         "}\n"},
        {"check_w_u8",
         "function l $ngrt_check_w_u8(l %v) {\n"
         "@start\n"
         "\t%lo =w csltl %v, 0\n"
         "\t%hi =w csgtl %v, 255\n"
         "\t%bad =w or %lo, %hi\n"
         "\tjnz %bad, @halt, @ok\n"
         "@halt\n"
         "\thlt\n"
         "@ok\n"
         "\tret %v\n"
         "}\n"},
        {"check_w_u16",
         "function l $ngrt_check_w_u16(l %v) {\n"
         "@start\n"
         "\t%lo =w csltl %v, 0\n"
         "\t%hi =w csgtl %v, 65535\n"
         "\t%bad =w or %lo, %hi\n"
         "\tjnz %bad, @halt, @ok\n"
         "@halt\n"
         "\thlt\n"
         "@ok\n"
         "\tret %v\n"
         "}\n"},
        {"check_w_u32",
         "function l $ngrt_check_w_u32(l %v) {\n"
         "@start\n"
         "\t%lo =w csltl %v, 0\n"
         "\t%hi =w csgtl %v, 4294967295\n"
         "\t%bad =w or %lo, %hi\n"
         "\tjnz %bad, @halt, @ok\n"
         "@halt\n"
         "\thlt\n"
         "@ok\n"
         "\tret %v\n"
         "}\n"},
        {"str_clone",
         "function l $ngrt_str_clone(l %s) {\n"
         "@start\n"
         "\t%len =l loadl %s\n"
         "\t%total =l add %len, 8\n"
         "\t%p =l call $malloc(l %total)\n"
         "\tstorel %len, %p\n"
         "\t%dst =l add %p, 8\n"
         "\t%src =l add %s, 8\n"
         "\t%m =l call $memcpy(l %dst, l %src, l %len)\n"
         "\tret %p\n"
         "}\n"},
        {"arr_clone_words",
         "function l $ngrt_arr_clone_words(l %src) {\n"
         "@start\n"
         "\t%len =l loadl %src\n"
         "\t%size =l mul %len, 8\n"
         "\t%total =l add %size, 16\n"
         "\t%p =l call $malloc(l %total)\n"
         "\tstorel %len, %p\n"
         "\t%cap =l add %p, 8\n"
         "\tstorel %len, %cap\n"
         "\t%dst =l add %p, 16\n"
         "\t%srcp =l add %src, 16\n"
         "\t%m =l call $memcpy(l %dst, l %srcp, l %size)\n"
         "\tret %p\n"
         "}\n"},
        {"arr_clone_strings",
         "function l $ngrt_arr_clone_strings(l %src) {\n"
         "@start\n"
         "\t%len =l loadl %src\n"
         "\t%size =l mul %len, 8\n"
         "\t%total =l add %size, 16\n"
         "\t%p =l call $malloc(l %total)\n"
         "\tstorel %len, %p\n"
         "\t%cap =l add %p, 8\n"
         "\tstorel %len, %cap\n"
         "\t%i =l copy 0\n"
         "@loop\n"
         "\t%done =w csgel %i, %len\n"
         "\tjnz %done, @end, @copy\n"
         "@copy\n"
         "\t%v =l call $ngrt_arr_get(l %src, l %i)\n"
         "\t%c =l call $ngrt_str_clone(l %v)\n"
         "\t%off =l mul %i, 8\n"
         "\t%base =l add %p, 16\n"
         "\t%addr =l add %base, %off\n"
         "\tstorel %c, %addr\n"
         "\t%i =l add %i, 1\n"
         "\tjmp @loop\n"
         "@end\n"
         "\tret %p\n"
         "}\n"},
        {"str_concat",
         "function l $ngrt_str_concat(l %a, l %b) {\n"
         "@start\n"
         "\t%la =l loadl %a\n"
         "\t%lb =l loadl %b\n"
         "\t%n =l add %la, %lb\n"
         "\t%size =l add %n, 8\n"
         "\t%p =l call $malloc(l %size)\n"
         "\tstorel %n, %p\n"
         "\t%pa =l add %p, 8\n"
         "\t%sa =l add %a, 8\n"
         "\t%m =l call $memcpy(l %pa, l %sa, l %la)\n"
         "\t%pd =l add %pa, %la\n"
         "\t%sb =l add %b, 8\n"
         "\t%m2 =l call $memcpy(l %pd, l %sb, l %lb)\n"
         "\tret %p\n"
         "}\n"},
        {"str_eq",
         "function w $ngrt_str_eq(l %a, l %b) {\n"
         "@start\n"
         "\t%la =l loadl %a\n"
         "\t%lb =l loadl %b\n"
         "\t%eq =w ceql %la, %lb\n"
         "\tjnz %eq, @cmp, @no\n"
         "@cmp\n"
         "\t%pa =l add %a, 8\n"
         "\t%pb =l add %b, 8\n"
         "\t%c =w call $memcmp(l %pa, l %pb, l %la)\n"
         "\t%z =w ceqw %c, 0\n"
         "\tret %z\n"
         "@no\n"
         "\tret 0\n"
         "}\n"},
        {"arr_addr",
         "function l $ngrt_arr_addr(l %a, l %i) {\n"
         "@start\n"
         "\t%len =l loadl %a\n"
         "\t%neg =w csltl %i, 0\n"
         "\t%high =w csgel %i, %len\n"
         "\t%bad =w or %neg, %high\n"
         "\tjnz %bad, @halt, @ok\n"
         "@halt\n"
         "\thlt\n"
         "@ok\n"
         "\t%off =l mul %i, 8\n"
         "\t%base =l add %a, 16\n"
         "\t%addr =l add %base, %off\n"
         "\tret %addr\n"
         "}\n"},
        {"arr_get",
         "function l $ngrt_arr_get(l %a, l %i) {\n"
         "@start\n"
         "\t%addr =l call $ngrt_arr_addr(l %a, l %i)\n"
         "\t%v =l loadl %addr\n"
         "\tret %v\n"
         "}\n"},
        {"arr_append",
         "function l $ngrt_arr_append(l %a, l %e) {\n"
         "@start\n"
         "\t%len =l loadl %a\n"
         "\t%n =l add %len, 1\n"
         "\t%total =l mul %n, 8\n"
         "\t%total =l add %total, 16\n"
         "\t%p =l call $malloc(l %total)\n"
         "\tstorel %n, %p\n"
         "\t%cap =l add %p, 8\n"
         "\tstorel %n, %cap\n"
         "\t%dst =l add %p, 16\n"
         "\t%src =l add %a, 16\n"
         "\t%oldsize =l mul %len, 8\n"
         "\t%m =l call $memcpy(l %dst, l %src, l %oldsize)\n"
         "\t%last =l add %dst, %oldsize\n"
         "\tstorel %e, %last\n"
         "\tret %p\n"
         "}\n"},
        {"arr_slice",
         "function l $ngrt_arr_slice(l %a, l %s, l %e) {\n"
         "@start\n"
         "\t%len =l loadl %a\n"
         "\t%sneg =w csltl %s, 0\n"
         "\t%shigh =w csgel %s, %e\n"
         "\t%ehigh =w csgtl %e, %len\n"
         "\t%bad =w or %sneg, %shigh\n"
         "\t%bad =w or %bad, %ehigh\n"
         "\tjnz %bad, @halt, @ok\n"
         "@halt\n"
         "\thlt\n"
         "@ok\n"
         "\t%n =l sub %e, %s\n"
         "\t%size =l mul %n, 8\n"
         "\t%total =l add %size, 16\n"
         "\t%p =l call $malloc(l %total)\n"
         "\tstorel %n, %p\n"
         "\t%cap =l add %p, 8\n"
         "\tstorel %n, %cap\n"
         "\t%dst =l add %p, 16\n"
         "\t%off =l mul %s, 8\n"
         "\t%src =l add %a, 16\n"
         "\t%src =l add %src, %off\n"
         "\t%m =l call $memcpy(l %dst, l %src, l %size)\n"
         "\tret %p\n"
         "}\n"},
         {"ref_load",
          // A reference value is { l root-cell, l count, { l kind, l payload }[] }.
          // Steps: 0 = struct field (offset 8p), 1 = runtime index
          // (bounds-checked element), 2 = constant index into an array/tuple
          // (offset 16 + 8p). Loading walks from the root cell's current
          // value, mirroring the VM's (cell + path) view semantics.
          "function l $ngrt_ref_load(l %ref) {\n"
          "@start\n"
          "\t%root =l loadl %ref\n"
          "\t%cntp =l add %ref, 8\n"
          "\t%cnt =l loadl %cntp\n"
          "\t%cur =l loadl %root\n"
          "\t%sp =l add %ref, 16\n"
          "\t%i =l copy 0\n"
          "@loop\n"
          "\t%done =w csgel %i, %cnt\n"
          "\tjnz %done, @end, @step\n"
          "@step\n"
          "\t%off =l mul %i, 16\n"
          "\t%kindp =l add %sp, %off\n"
          "\t%kind =l loadl %kindp\n"
          "\t%isIndex =w ceql %kind, 1\n"
          "\t%payloadp =l add %kindp, 8\n"
          "\t%payload =l loadl %payloadp\n"
          "\tjnz %isIndex, @index, @memberlike\n"
          "@memberlike\n"
          "\t%isHeader =w ceql %kind, 2\n"
          "\tjnz %isHeader, @header, @plain\n"
          "@header\n"
          "\t%moff =l mul %payload, 8\n"
          "\t%moff =l add %moff, 16\n"
          "\tjmp @apply\n"
          "@plain\n"
          "\t%moff =l copy %payload\n"
          "@apply\n"
          "\t%addr =l add %cur, %moff\n"
          "\t%cur =l loadl %addr\n"
          "\tjmp @next\n"
          "@index\n"
          "\t%cur =l call $ngrt_arr_get(l %cur, l %payload)\n"
          "@next\n"
          "\t%i =l add %i, 1\n"
          "\tjmp @loop\n"
          "@end\n"
          "\tret %cur\n"
          "}\n"},
         {"ref_addr",
          // Walks to the address of the referenced place (same step kinds as
          // ref_load); a plain local reference addresses the cell itself.
          "function l $ngrt_ref_addr(l %ref) {\n"
          "@start\n"
          "\t%root =l loadl %ref\n"
          "\t%cntp =l add %ref, 8\n"
          "\t%cnt =l loadl %cntp\n"
          "\t%none =w ceql %cnt, 0\n"
          "\t%cur =l loadl %root\n"
          "\tjnz %none, @end0, @pre\n"
          "@end0\n"
          "\tret %root\n"
          "@pre\n"
          "\t%sp =l add %ref, 16\n"
          "\t%i =l copy 0\n"
          "@loop\n"
          "\t%done =w csgel %i, %cnt\n"
          "\tjnz %done, @end, @step\n"
          "@step\n"
          "\t%off =l mul %i, 16\n"
          "\t%kindp =l add %sp, %off\n"
          "\t%kind =l loadl %kindp\n"
          "\t%isIndex =w ceql %kind, 1\n"
          "\t%payloadp =l add %kindp, 8\n"
          "\t%payload =l loadl %payloadp\n"
          "\tjnz %isIndex, @index, @memberlike\n"
          "@memberlike\n"
          "\t%isHeader =w ceql %kind, 2\n"
          "\tjnz %isHeader, @header, @plain\n"
          "@header\n"
          "\t%moff =l mul %payload, 8\n"
          "\t%moff =l add %moff, 16\n"
          "\tjmp @apply\n"
          "@plain\n"
          "\t%moff =l copy %payload\n"
          "@apply\n"
          "\t%cur =l add %cur, %moff\n"
          "\tjmp @next\n"
          "@index\n"
          "\t%cur =l call $ngrt_arr_addr(l %cur, l %payload)\n"
          "@next\n"
          "\t%i =l add %i, 1\n"
          "\tjmp @loop\n"
          "@end\n"
          "\tret %cur\n"
          "}\n"},
        {"copy_elements",
         // Copies `count` 8-byte elements from src's payload into dst
         // (tuple-splice building block).
         "function l $ngrt_copy_elements(l %dst, l %src, l %count) {\n"
         "@start\n"
         "\t%i =l copy 0\n"
         "@loop\n"
         "\t%done =w csgel %i, %count\n"
         "\tjnz %done, @end, @copy\n"
         "@copy\n"
         "\t%v =l call $ngrt_arr_get(l %src, l %i)\n"
         "\t%off =l mul %i, 8\n"
         "\t%addr =l add %dst, %off\n"
         "\tstorel %v, %addr\n"
         "\t%i =l add %i, 1\n"
         "\tjmp @loop\n"
         "@end\n"
         "\tret %dst\n"
         "}\n"},
        {"enum_list_len",
         // Recursive List<T> length: Nil has a null payload word; a Cons cell
         // is { tag, tuple { head, ref tail } } where the ref's root slot
         // holds the next node.
         "function l $ngrt_enum_list_len(l %node) {\n"
         "@start\n"
         "\t%count =l copy 0\n"
         "@loop\n"
         "\t%null =w ceql %node, 0\n"
         "\tjnz %null, @end, @walk\n"
         "@walk\n"
         "\t%po =l add %node, 8\n"
         "\t%payload =l loadl %po\n"
         "\t%pnul =w ceql %payload, 0\n"
         "\tjnz %pnul, @end, @next\n"
         "@next\n"
         "\t%refp =l add %payload, 24\n"
         "\t%ref =l loadl %refp\n"
         "\t%root =l loadl %ref\n"
         "\t%node =l loadl %root\n"
         "\t%count =l add %count, 1\n"
         "\tjmp @loop\n"
         "@end\n"
         "\tret %count\n"
         "}\n"},
        {"enum_list_get",
         // Recursive List<T> indexed head access; halts on out-of-range walks.
         "function l $ngrt_enum_list_get(l %node, l %index) {\n"
         "@start\n"
         "@loop\n"
         "\t%zero =w cslel %index, 0\n"
         "\tjnz %zero, @head, @advance\n"
         "@advance\n"
         "\t%po =l add %node, 8\n"
         "\t%payload =l loadl %po\n"
         "\t%pnul =w ceql %payload, 0\n"
         "\tjnz %pnul, @halt, @ok\n"
         "@ok\n"
         "\t%refp =l add %payload, 24\n"
         "\t%ref =l loadl %refp\n"
         "\t%root =l loadl %ref\n"
         "\t%node =l loadl %root\n"
         "\t%index =l sub %index, 1\n"
         "\tjmp @loop\n"
         "@head\n"
         "\t%po2 =l add %node, 8\n"
         "\t%payload2 =l loadl %po2\n"
         "\t%pnul2 =w ceql %payload2, 0\n"
         "\tjnz %pnul2, @halt, @read\n"
         "@read\n"
         "\t%headp =l add %payload2, 16\n"
         "\t%head =l loadl %headp\n"
         "\tret %head\n"
         "@halt\n"
         "\thlt\n"
         "}\n"},
      };
      return helpers;
    }

    class FunctionLowerer final
    {
    public:
      FunctionLowerer(const Function &function, const FunctionNames &names, NgrtContext &ngrt)
          : function_(function), names_(names), ngrt_(ngrt)
      {
      }

      [[nodiscard]] auto run() -> std::string
      {
        if (function_.nativeFunction)
          throw LoweringError(std::format("native lowering (M1): native placeholder `{}` has no body", function_.name));
        collectBlocks();
        collectLocals();
        preassignValueTemps();
        return emit();
      }

    private:
      const Function &function_;
      const FunctionNames &names_;
      NgrtContext &ngrt_;
      std::ostringstream out_;
      size_t tempCounter_{};
      std::vector<std::string> dataItems_;
      std::vector<std::string> labels_;
      bool hasTailRecur_{};
      /// Per block: (predecessor index, block-parameter argument values).
      std::vector<std::vector<std::pair<size_t, std::vector<ValueId>>>> predecessors_;
      struct SlotSpec
      {
        uint32_t local;
        bool heapCell;
        size_t words;
      };
      std::vector<SlotSpec> slotOrder_;
      /// Locals that are ever borrowed (`ref` roots, incl. trait views): their
      /// storage must be a heap cell so escaping references (recursive enum
      /// payloads) stay valid after the frame returns — the VM's locals are
      /// shared cells for the same reason.
      std::unordered_set<uint32_t> cellLocals_;
      /// Tier 1 slice: BindLocal-only locals (and unassigned parameters)
      /// live as QBE temps; QBE's non-SSA fixup handles rebinding and
      /// cross-block uses.
      std::unordered_set<uint32_t> ssaCandidates_;
      std::unordered_set<uint32_t> ssaExcluded_;
      std::unordered_set<uint32_t> ssaLocals_;
      /// Current temp for an SSA local (updated on every BindLocal).
      std::unordered_map<uint32_t, std::string> currentTemps_;
      std::unordered_map<uint32_t, std::string> localSlots_;
      std::unordered_map<uint32_t, QType> localQTypes_;
      std::unordered_map<uint32_t, std::string> valueTemps_;
      /// ValueIds that carry a tagged union box -> the union type. The
      /// checker records union operands as their member type at comparison
      /// sites, so union-ness must be tracked by data flow, not by type
      /// tables.
      std::unordered_map<uint32_t, TypeId> unionBoxedValues_;
      std::vector<std::string> paramTemps_;
      std::optional<QType> returnType_;
      std::optional<TypeId> returnTypeId_;

      [[nodiscard]] auto fresh() -> std::string { return std::format("%t{}", tempCounter_++); }

      void useHelper(std::string_view name) { ngrt_.usedHelpers.emplace(name); }

      /// Tier 0 QBE mapping: integers/bools are `l`, floats are `d`, unit is
      /// `None`, and every aggregate value (string/array/tuple/range) is an
      /// 8-byte pointer (`l`).
      [[nodiscard]] auto qtypeOf(TypeId type) -> QType
      {
        if (type.value >= function_.typeDescriptors.size())
          throw LoweringError(
              std::format("native lowering (M2): type id {} is out of range in `{}`", type.value, function_.name));
        const auto &descriptor = function_.typeDescriptors[type.value];
        switch (descriptor.kind)
        {
        case TypeKind::Builtin:
          if (type == typecheck::builtin::String || type == typecheck::builtin::Bool || typecheck::isIntegerBuiltin(type))
            return QType::Long;
          if (typecheck::isFloatBuiltin(type)) return QType::Double;
          if (type == typecheck::builtin::Unit) return QType::None;
          break;
        case TypeKind::DynamicArray:
        case TypeKind::FixedArray:
        case TypeKind::Tuple:
        case TypeKind::Range:
        case TypeKind::Reference:
        case TypeKind::TraitReference:
        case TypeKind::Opaque:
        case TypeKind::Union:
          return QType::Long;
        case TypeKind::Struct:
        case TypeKind::Enum:
          return QType::Aggregate;
        case TypeKind::TypeParameter:
        case TypeKind::TypeConstructor:
        case TypeKind::TypeApplication:
          // Monomorphized instance bodies can carry the generic type
          // parameter id in value/local type tables (e.g. `*a` types as the
          // reference's element, which stays `T` until specialize substitutes
          // it). Tier 0 falls back to `l`; QBE rejects float-context misuse
          // loudly, so this cannot silently miscompile. The proper fix is
          // substitution-aware typing in the typechecker (tracked for M4).
          return QType::Long;
        default: break;
        }
        throw LoweringError(
            std::format("native lowering (M2): type `{}` has no QBE mapping yet", descriptor.name));
      }

      /// Aggregate slots hold `l`; doubles are bit-cast when stored/loaded.
      [[nodiscard]] auto castForSlot(const std::string &temp, QType type) -> std::string
      {
        if (type != QType::Double) return temp;
        const auto cast = fresh();
        line(std::format("{} =l cast {}", cast, temp));
        return cast;
      }

      [[nodiscard]] auto resultTypeId(const Instruction &instruction) -> TypeId
      {
        return function_.valueTypes.at(instruction.result.value);
      }

      [[nodiscard]] auto isUnionType(TypeId type) -> bool
      {
        return type.value < function_.typeDescriptors.size() &&
               function_.typeDescriptors[type.value].kind == TypeKind::Union;
      }

      /// Index of the union member matching a value's natural type (first
      /// integer/bool member for integers, first float member for floats,
      /// the string member for strings).
      [[nodiscard]] auto unionMemberFor(TypeId unionType, TypeId memberType) -> int64_t
      {
        const auto &descriptor = function_.typeDescriptors[unionType.value];
        const auto matches = [&](TypeId candidate) {
          if (memberType == typecheck::builtin::String) return candidate == typecheck::builtin::String;
          if (typecheck::isFloatBuiltin(memberType)) return typecheck::isFloatBuiltin(candidate);
          if (memberType == typecheck::builtin::Bool) return candidate == typecheck::builtin::Bool;
          return typecheck::isIntegerBuiltin(candidate);
        };
        for (size_t index = 0; index < descriptor.elements.size(); ++index)
          if (matches(descriptor.elements[index])) return static_cast<int64_t>(index);
        throw LoweringError(std::format("native lowering (M2): value does not match any member of union `{}`",
                                        descriptor.name));
      }

      /// Wraps a produced payload temp into a tagged union box
      /// { l memberIndex, l payloadWord } and binds the instruction result.
      void bindUnionBox(const Instruction &instruction, TypeId unionType, int64_t member, const std::string &payloadTemp,
                        QType payloadType)
      {
        const auto pointer = fresh();
        line(std::format("{} =l call $malloc(l 16)", pointer));
        line(std::format("storel {}, {}", member, pointer));
        const auto address = fresh();
        line(std::format("{} =l add {}, 8", address, pointer));
        line(std::format("store{} {}, {}", suffix(payloadType), payloadTemp, address));
        bindResult(instruction, QType::Long, std::format("copy {}", pointer));
        unionBoxedValues_.emplace(instruction.result.value, unionType);
      }

      /// Reverses castForSlot after a load from an aggregate slot.
      [[nodiscard]] auto castFromSlot(const std::string &temp, QType type) -> std::string
      {
        if (type != QType::Double) return temp;
        const auto cast = fresh();
        line(std::format("{} =d cast {}", cast, temp));
        return cast;
      }

      [[nodiscard]] auto symbolFor(hir::DefId id) -> std::string
      {
        std::string name;
        if (id == function_.source) name = function_.name;
        else if (const auto found = names_.find(id.value); found != names_.end()) name = found->second;
        else name = std::format("fn{}", id.value);
        return qbeSymbol(name, id.value);
      }

      void line(std::string_view text) { out_ << '\t' << text << '\n'; }

      void collectBlocks()
      {
        const size_t count = function_.blocks.size();
        labels_.resize(count);
        predecessors_.assign(count, {});
        hasTailRecur_ = false;
        for (size_t index = 0; index < count; ++index)
        {
          const auto &terminator = *function_.blocks[index].terminator;
          if (terminator.kind == TerminatorKind::TailRecur) hasTailRecur_ = true;
          if (terminator.kind == TerminatorKind::Jump || terminator.kind == TerminatorKind::LoopBackedge)
            predecessors_.at(terminator.targets[0].value).push_back({index, terminator.arguments});
          else if (terminator.kind == TerminatorKind::Branch)
          {
            // Branch targets never carry block arguments in FlowIR.
            predecessors_.at(terminator.targets[0].value).push_back({index, {}});
            predecessors_.at(terminator.targets[1].value).push_back({index, {}});
          }
          else if (terminator.kind == TerminatorKind::TailRecur)
          {
            // Tail recursion rebinds the function parameters, not block
            // parameters; the entry block takes no jump arguments.
            predecessors_.front().push_back({index, {}});
          }
        }
        // QBE forbids jumping to `@start`. A function with tail recursion
        // keeps the one-shot entry logic (allocs + parameter stores) under
        // `@start`, which falls through a jump into `@body0`; the tail
        // recursion loop jumps back to `@body0`, skipping the entry logic.
        for (size_t index = 0; index < count; ++index)
          labels_[index] = index == 0 ? (hasTailRecur_ ? "@body0" : "@start") : std::format("@b{}", index);
      }

      void ensureSlot(uint32_t local)
      {
        if (localSlots_.contains(local)) return;
        QType type = QType::Long;
        if (const auto found = function_.localTypes.find(local); found != function_.localTypes.end())
          type = qtypeOf(found->second);
        localSlots_.emplace(local, fresh());
        localQTypes_.emplace(local, type);
        size_t words = 1;
        if (type == QType::Aggregate)
          words = (aggregateSize(function_.localTypes.at(local)) + 7) / 8;
        slotOrder_.push_back(SlotSpec{.local = local, .heapCell = cellLocals_.contains(local), .words = words});
      }

      [[nodiscard]] auto isCellLocal(uint32_t local) -> bool { return cellLocals_.contains(local); }

      /// QBE aggregate type name for a struct: `:ngs_<typeId>`.
      [[nodiscard]] auto aggregateName(TypeId type) -> std::string
      {
        return std::format(":ng{}_{}", function_.typeDescriptors[type.value].kind == TypeKind::Enum ? 'e' : 's',
                           type.value);
      }

      /// Byte size of an aggregate's Tier 1 storage (all members are 8-byte
      /// words in this slice; enums are { tag, payload-word }).
      /// Real member layout: f32 occupies a QBE `s` (4 bytes, 4-aligned),
      /// f64 a `d`, and everything else an 8-byte `l` word.
      [[nodiscard]] auto memberSuffix(TypeId type) -> std::string
      {
        if (type == typecheck::builtin::I8 || type == typecheck::builtin::U8) return "b";
        if (type == typecheck::builtin::I16 || type == typecheck::builtin::U16) return "h";
        if (type == typecheck::builtin::I32 || type == typecheck::builtin::U32) return "w";
        if (type == typecheck::builtin::F32) return "s";
        if (type == typecheck::builtin::F64) return "d";
        return "l";
      }

      [[nodiscard]] auto memberLayout(TypeId type) -> std::pair<size_t, size_t>
      {
        if (type == typecheck::builtin::I8 || type == typecheck::builtin::U8) return {1, 1};
        if (type == typecheck::builtin::I16 || type == typecheck::builtin::U16) return {2, 2};
        if (type == typecheck::builtin::I32 || type == typecheck::builtin::U32 || type == typecheck::builtin::F32)
          return {4, 4};
        return {8, 8};
      }

      /// Aligned byte offset of a struct field.
      [[nodiscard]] auto fieldOffset(TypeId structType, size_t field) -> size_t
      {
        const auto &descriptor = function_.typeDescriptors[structType.value];
        size_t offset = 0;
        for (size_t index = 0; index < field; ++index)
        {
          const auto [memberSize, memberAlign] = memberLayout(descriptor.elements[index]);
          offset = (offset + memberAlign - 1) / memberAlign * memberAlign + memberSize;
        }
        const auto [memberSize, memberAlign] = memberLayout(descriptor.elements[field]);
        return (offset + memberAlign - 1) / memberAlign * memberAlign;
      }

      [[nodiscard]] auto aggregateSize(TypeId type) -> size_t
      {
        const auto &descriptor = function_.typeDescriptors[type.value];
        if (descriptor.kind == TypeKind::Enum) return 16;
        size_t size = 0;
        size_t align = 1;
        for (const auto field : descriptor.elements)
        {
          const auto [memberSize, memberAlign] = memberLayout(field);
          size = (size + memberAlign - 1) / memberAlign * memberAlign + memberSize;
          align = std::max(align, memberAlign);
        }
        return (size + align - 1) / align * align;
      }

      /// Stores a field value (a QBE temp in the value's QBE type) into an
      /// aggregate at the given address; f32 values truncate to `s`.
      void storeField(const std::string &address, const std::string &value, TypeId fieldType)
      {
        if (fieldType == typecheck::builtin::F32)
        {
          const auto single = fresh();
          line(std::format("{} =s truncd {}", single, value));
          line(std::format("stores {}, {}", single, address));
        }
        else if (typecheck::isFloatBuiltin(fieldType))
        {
          line(std::format("stored {}, {}", value, address));
        }
        else if (fieldType == typecheck::builtin::I8 || fieldType == typecheck::builtin::U8)
        {
          line(std::format("storeb {}, {}", value, address));
        }
        else if (fieldType == typecheck::builtin::I16 || fieldType == typecheck::builtin::U16)
        {
          line(std::format("storeh {}, {}", value, address));
        }
        else if (fieldType == typecheck::builtin::I32 || fieldType == typecheck::builtin::U32)
        {
          line(std::format("storew {}, {}", value, address));
        }
        else
        {
          line(std::format("storel {}, {}", castForSlot(value, qtypeOf(fieldType)), address));
        }
      }

      /// Loads a field value from an aggregate address, returning a QBE temp
      /// in the value's QBE type (f32 loads extend back to `d`).
      [[nodiscard]] auto loadField(const std::string &address, TypeId fieldType) -> std::string
      {
        if (fieldType == typecheck::builtin::F32)
        {
          const auto single = fresh();
          line(std::format("{} =s loads {}", single, address));
          const auto extended = fresh();
          line(std::format("{} =d exts {}", extended, single));
          return extended;
        }
        const auto loaded = fresh();
        const char *loadOp = "loadl";
        if (fieldType == typecheck::builtin::I8) loadOp = "loadsb";
        else if (fieldType == typecheck::builtin::U8) loadOp = "loadub";
        else if (fieldType == typecheck::builtin::I16) loadOp = "loadsh";
        else if (fieldType == typecheck::builtin::U16) loadOp = "loaduh";
        else if (fieldType == typecheck::builtin::I32) loadOp = "loadsw";
        else if (fieldType == typecheck::builtin::U32) loadOp = "loaduw";
        line(std::format("{} =l {} {}", loaded, loadOp, address));
        if (typecheck::isFloatBuiltin(fieldType))
        {
          const auto casted = fresh();
          line(std::format("{} =d cast {}", casted, loaded));
          return casted;
        }
        return loaded;
      }

      /// Field-wise aggregate copy with the real layout: structs copy every
      /// field at its aligned offset; enums copy their two words (tag +
      /// payload — payload words are never mutated in place, so sharing a
      /// tuple payload is unobservable). (QBE's `blit` is not usable here:
      /// its optimizer does not model blit's memory definitions, folding the
      /// copied data to an uninitialized sentinel.)
      void emitAggregateCopy(const std::string &destination, const std::string &source, TypeId type)
      {
        const auto &descriptor = function_.typeDescriptors[type.value];
        if (descriptor.kind == TypeKind::Enum)
        {
          for (size_t index = 0; index < 2; ++index)
          {
            const auto sourceAddress = fresh();
            line(std::format("{} =l add {}, {}", sourceAddress, source, index * 8));
            const auto value = fresh();
            line(std::format("{} =l loadl {}", value, sourceAddress));
            const auto target = fresh();
            line(std::format("{} =l add {}, {}", target, destination, index * 8));
            line(std::format("storel {}, {}", value, target));
          }
          return;
        }
        for (size_t index = 0; index < descriptor.fieldNames.size(); ++index)
        {
          const auto offset = fieldOffset(type, index);
          const auto sourceAddress = fresh();
          line(std::format("{} =l add {}, {}", sourceAddress, source, offset));
          const auto value = loadField(sourceAddress, descriptor.elements[index]);
          const auto target = fresh();
          line(std::format("{} =l add {}, {}", target, destination, offset));
          storeField(target, value, descriptor.elements[index]);
        }
      }

      /// The QBE type spelling for a value type in signature/call positions:
      /// aggregates name their QBE type (passed by pointer at IL level and
      /// classified by the backend ABI).
      [[nodiscard]] auto qbeSuffixFor(TypeId type) -> std::string
      {
        const auto qtype = qtypeOf(type);
        if (qtype == QType::Aggregate) return aggregateName(type);
        return suffix(qtype);
      }

      /// The QBE type spelling for an `extern "C"` scalar boundary, derived
      /// from the DECLARED C signature type (NG carries every integer in an
      /// `l` temp, so the value-type mapping cannot be reused): sub-word
      /// integers keep their sign/zero-extension class (sb/ub/sh/uh),
      /// i32/u32/bool are C `int` (`w`), f32 is `s`, f64 `d`, and i64/u64
      /// are `l`.
      [[nodiscard]] auto externSuffixFor(TypeId type) -> std::string
      {
        if (type == typecheck::builtin::I8) return "sb";
        if (type == typecheck::builtin::U8) return "ub";
        if (type == typecheck::builtin::I16) return "sh";
        if (type == typecheck::builtin::U16) return "uh";
        if (type == typecheck::builtin::I32 || type == typecheck::builtin::U32 || type == typecheck::builtin::Bool)
          return "w";
        if (type == typecheck::builtin::F32) return "s";
        if (type == typecheck::builtin::F64) return "d";
        return "l";
      }

      /// The QBE type of a local; SSA locals skip slot creation, so the type
      /// falls back to the declared local type.
      [[nodiscard]] auto localQType(uint32_t local) -> QType
      {
        if (const auto found = localQTypes_.find(local); found != localQTypes_.end()) return found->second;
        if (const auto typed = function_.localTypes.find(local); typed != function_.localTypes.end())
          return qtypeOf(typed->second);
        return QType::Long;
      }

      /// Loads a local's value; cell locals use the slot -> cell indirection
      /// (two loads).
      [[nodiscard]] auto loadLocal(uint32_t local) -> std::string
      {
        const auto &slot = localSlots_.at(local);
        if (!isCellLocal(local))
        {
          const auto value = fresh();
          line(std::format("{} =l loadl {}", value, slot));
          return value;
        }
        const auto cell = fresh();
        line(std::format("{} =l loadl {}", cell, slot));
        const auto value = fresh();
        line(std::format("{} =l loadl {}", value, cell));
        return value;
      }

      /// Allocates a fresh cell for a cell local and stores a value in it;
      /// the slot is repointed to the new cell (BindLocal semantics: the VM
      /// creates a fresh shared cell on every bind, so outstanding refs keep
      /// observing the old cell — a snapshot, not a mutation).
      void bindFreshCell(uint32_t local, const std::string &value)
      {
        const auto &slot = localSlots_.at(local);
        const auto cell = fresh();
        line(std::format("{} =l call $malloc(l 8)", cell));
        line(std::format("storel {}, {}", cell, slot));
        line(std::format("storel {}, {}", value, cell));
      }

      void collectLocals()
      {
        // Pass 1a: locals rooted by MakeRef/MakeTraitView need heap cells.
        for (const auto &block : function_.blocks)
          for (const auto &instruction : block.instructions)
            if ((instruction.kind == InstructionKind::MakeRef || instruction.kind == InstructionKind::MakeTraitView) &&
                instruction.placeRootLocal)
              cellLocals_.insert(instruction.placeRootLocal->value);
        // Pass 1b: SSA locals — never borrowed, never assigned, never a
        // block parameter — live as QBE temps (no slot traffic; QBE's
        // non-SSA fixup handles rebinding and cross-block uses). Parameters
        // qualify unless the function rebinds them through TailRecur.
        for (const auto &block : function_.blocks)
        {
          for (const auto &instruction : block.instructions)
          {
            if (instruction.local) ssaCandidates_.insert(instruction.local->value);
            if (instruction.kind == InstructionKind::AssignPlace && instruction.placeRootLocal)
              ssaExcluded_.insert(instruction.placeRootLocal->value);
          }
        }
        for (const auto local : function_.parameterLocals)
        {
          ssaCandidates_.insert(local.value);
          if (hasTailRecur_) ssaExcluded_.insert(local.value);
        }
        for (const auto local : ssaCandidates_)
          if (!ssaExcluded_.contains(local) && !cellLocals_.contains(local) &&
              !(function_.localTypes.contains(local) &&
                qtypeOf(function_.localTypes.at(local)) == QType::Aggregate))
            ssaLocals_.insert(local);
        paramTemps_.reserve(function_.parameterLocals.size());
        for (size_t index = 0; index < function_.parameterLocals.size(); ++index)
        {
          const auto local = function_.parameterLocals[index].value;
          paramTemps_.push_back(std::format("%p{}", index));
          if (ssaLocals_.contains(local)) currentTemps_.emplace(local, paramTemps_.back());
          else ensureSlot(local);
        }
        for (const auto &block : function_.blocks)
          for (const auto local : block.parameterLocals) ensureSlot(local.value);
        for (const auto &block : function_.blocks)
        {
          for (const auto &instruction : block.instructions)
          {
            if (instruction.local && !ssaLocals_.contains(instruction.local->value)) ensureSlot(instruction.local->value);
            if (instruction.kind == InstructionKind::Evaluate &&
                instruction.expressionKind == ExpressionKind::ResolvedName)
            {
              const auto local = static_cast<uint32_t>(instruction.payload);
              if (!ssaLocals_.contains(local)) ensureSlot(local);
            }
            if (instruction.kind == InstructionKind::AssignPlace && instruction.placeRootLocal)
              ensureSlot(instruction.placeRootLocal->value);
          }
        }
      }

      /// Assigns a stable QBE temp name to every FlowIR ValueId up front.
      /// QBE accepts phi arguments that are defined textually later (its own
      /// loop tests rely on this), so backedges can reference temps whose
      /// defining instruction is emitted in a later block.
      void preassignValueTemps()
      {
        const auto touch = [&](ValueId value) {
          if (!valueTemps_.contains(value.value)) valueTemps_.emplace(value.value, fresh());
        };
        for (const auto &block : function_.blocks)
        {
          for (const auto &instruction : block.instructions)
          {
            touch(instruction.result);
            if (instruction.source) touch(*instruction.source);
            for (const auto operand : instruction.operands) touch(operand);
          }
          if (block.terminator)
            for (const auto argument : block.terminator->arguments) touch(argument);
        }
      }

      [[nodiscard]] auto findReturnType() -> std::optional<QType>
      {
        for (const auto &block : function_.blocks)
        {
          const auto &terminator = *block.terminator;
          if (terminator.kind == TerminatorKind::Return && !terminator.arguments.empty())
          {
            returnTypeId_ = function_.valueTypes.at(terminator.arguments[0].value);
            return qtypeOf(*returnTypeId_);
          }
        }
        return std::nullopt;
      }

      [[nodiscard]] auto emit() -> std::string
      {
        returnType_ = findReturnType();
        // `main` is the C entry point: a unit return becomes an i64 exit code.
        if (function_.name == "main" && (!returnType_ || *returnType_ == QType::None))
        {
          returnType_ = QType::Long;
          returnTypeId_ = typecheck::builtin::I64;
        }
        out_ << (function_.name == "main" ? "export function" : "function");
        if (returnType_ && *returnType_ != QType::None)
          // Aggregate returns are heap-cloned pointers (`l`): callee stack
          // slots must not outlive the frame, and QBE's aggregate return
          // areas interact poorly with recursive aggregate traffic.
          out_ << ' ' << (*returnType_ == QType::Aggregate ? "l" : suffix(*returnType_));
        out_ << ' ' << symbolFor(function_.source) << '(';
        for (size_t index = 0; index < paramTemps_.size(); ++index)
        {
          if (index != 0) out_ << ", ";
          out_ << qbeSuffixFor(function_.localTypes.at(function_.parameterLocals[index].value)) << ' ' << paramTemps_[index];
        }
        out_ << ") {\n";
        // One-shot entry logic: frame slots and parameter stores live under
        // `@start` so tail recursion never re-runs them.
        out_ << "@start\n";
        for (const auto &slot : slotOrder_)
        {
          if (slot.heapCell)
          {
            // Cell locals: the slot stores a pointer to the current cell.
            line(std::format("{} =l call $malloc(l 8)", localSlots_.at(slot.local)));
            const auto cell = fresh();
            line(std::format("{} =l call $malloc(l 8)", cell));
            line(std::format("storel {}, {}", cell, localSlots_.at(slot.local)));
            line(std::format("storel 0, {}", cell));
          }
          else
          {
            line(std::format("{} =l alloc8 {}", localSlots_.at(slot.local), slot.words));
          }
        }
        for (size_t param = 0; param < paramTemps_.size(); ++param)
        {
          const auto local = function_.parameterLocals[param].value;
          if (ssaLocals_.contains(local)) continue; // parameter temps are used directly
          const auto localTypeId = function_.localTypes.at(local);
          if (isCellLocal(local))
          {
            const auto cell = fresh();
            line(std::format("{} =l loadl {}", cell, localSlots_.at(local)));
            line(std::format("storel {}, {}", paramTemps_[param], cell));
          }
          else if (qtypeOf(localTypeId) == QType::Aggregate)
          {
            // Copy-first semantics: aggregates are copied into the frame.
            emitAggregateCopy(localSlots_.at(local), paramTemps_[param], localTypeId);
          }
          else
          {
            line(std::format("store{} {}, {}", suffix(localQTypes_.at(local)), paramTemps_[param],
                             localSlots_.at(local)));
          }
        }
        if (hasTailRecur_) line(std::format("jmp {}", labels_.front()));
        for (size_t index = 0; index < function_.blocks.size(); ++index) emitBlock(index);
        out_ << "}\n";
        // String-literal data items are collected while emitting blocks, so
        // they are written after the body — data declarations are top-level
        // and order-independent.
        for (const auto &item : dataItems_) out_ << item << '\n';
        return out_.str();
      }

      void emitBlock(size_t index)
      {
        const auto &block = function_.blocks[index];
        // Block 0 without tail recursion continues `@start` directly; every
        // other block (and block 0 behind the tail-recursion trampoline)
        // carries its own label.
        if (index != 0 || hasTailRecur_) out_ << labels_[index] << '\n';
        // Block parameters become phi temporaries stored into their slots.
        // QBE requires every phi of a block to be consecutive at the top, so
        // all phis are emitted before any store.
        const auto &predecessors = predecessors_[index];
        std::vector<std::string> phiTemps;
        phiTemps.reserve(block.parameterLocals.size());
        for (size_t param = 0; param < block.parameterLocals.size(); ++param)
        {
          const auto local = block.parameterLocals[param].value;
          const auto type = localQTypes_.at(local);
          std::string arguments;
          for (size_t edge = 0; edge < predecessors.size(); ++edge)
          {
            const auto &[predIndex, values] = predecessors[edge];
            if (values.size() != block.parameterLocals.size())
              throw LoweringError(std::format("native lowering (M1): malformed block-parameter edge in `{}`",
                                              function_.name));
            if (edge != 0) arguments += ", ";
            arguments += std::format("{} {}", labels_[predIndex], valueTemps_.at(values[param].value));
          }
          const auto phi = fresh();
          line(std::format("{} ={} phi {}", phi, suffix(type), arguments));
          phiTemps.push_back(phi);
        }
        for (size_t param = 0; param < block.parameterLocals.size(); ++param)
        {
          const auto local = block.parameterLocals[param].value;
          if (isCellLocal(local))
          {
            // The VM rebinds block parameters into fresh cells on every
            // entry (jumpToBlock), so loop accumulators observed through
            // refs keep their per-iteration snapshot.
            bindFreshCell(local, phiTemps[param]);
          }
          else
          {
            line(std::format("store{} {}, {}", suffix(localQTypes_.at(local)), phiTemps[param],
                             localSlots_.at(local)));
          }
        }
        for (const auto &instruction : block.instructions) lowerInstruction(instruction);
        lowerTerminator(*block.terminator, index);
      }

      void bindResult(const Instruction &instruction, QType type, std::string_view text)
      {
        line(std::format("{} ={} {}", valueTemps_.at(instruction.result.value), suffix(type), text));
      }

      [[nodiscard]] auto operandTemp(ValueId value) -> const std::string &
      {
        return valueTemps_.at(value.value);
      }

      /// Integer compare returning an NG bool (i64 0/1).
      void integerCompare(const Instruction &instruction, std::string_view qbeOp, const std::string &left,
                          const std::string &right)
      {
        const auto compared = fresh();
        line(std::format("{} =w {} {}, {}", compared, qbeOp, left, right));
        bindResult(instruction, QType::Long, std::format("extsw {}", compared));
      }

      /// Emits a checked arithmetic helper call into the instruction's result
      /// temp (overflow/division/shift violations halt, matching the VM's
      /// BytecodeError), then a narrow-width check when the result type
      /// requires one (D-008).
      void emitCheckedOp(const Instruction &instruction, std::string_view helper, std::string_view arguments)
      {
        useHelper(helper);
        line(std::format("{} =l call $ngrt_{}({})", valueTemps_.at(instruction.result.value), helper, arguments));
        applyWidthCheck(instruction);
      }

      /// D-008 per-width range check on an integer result (i8-i32, u8-u32).
      void applyWidthCheck(const Instruction &instruction)
      {
        const auto type = function_.valueTypes.at(instruction.result.value);
        const char *name = nullptr;
        switch (type.value)
        {
        case typecheck::builtin::I8.value: name = "check_w_i8"; break;
        case typecheck::builtin::I16.value: name = "check_w_i16"; break;
        case typecheck::builtin::I32.value: name = "check_w_i32"; break;
        case typecheck::builtin::U8.value: name = "check_w_u8"; break;
        case typecheck::builtin::U16.value: name = "check_w_u16"; break;
        case typecheck::builtin::U32.value: name = "check_w_u32"; break;
        default: return;
        }
        useHelper(name);
        const auto temp = valueTemps_.at(instruction.result.value);
        line(std::format("{} =l call $ngrt_{}(l {})", temp, name, temp));
      }

      void lowerInstruction(const Instruction &instruction)
      {
        switch (instruction.kind)
        {
        case InstructionKind::Evaluate: lowerEvaluate(instruction); return;
        case InstructionKind::ExtractTuple:
        {
          // Tuple representation == array representation.
          useHelper("arr_get");
          const auto source = operandTemp(*instruction.source);
          const auto loaded = fresh();
          line(std::format("{} =l call $ngrt_arr_get(l {}, l {})", loaded, source, instruction.payload));
          const auto resultType = qtypeOf(function_.valueTypes.at(instruction.result.value));
          bindResult(instruction, resultType, std::format("copy {}", castFromSlot(loaded, resultType)));
          return;
        }
        case InstructionKind::AppendArray:
        {
          useHelper("arr_append");
          const auto array = operandTemp(instruction.operands[0]);
          const auto elementType = qtypeOf(function_.valueTypes.at(instruction.operands[1].value));
          const auto element = castForSlot(operandTemp(instruction.operands[1]), elementType);
          bindResult(instruction, QType::Long, std::format("call $ngrt_arr_append(l {}, l {})", array, element));
          return;
        }
        case InstructionKind::ArrayLength:
        {
          const auto source = operandTemp(*instruction.source);
          const auto sourceType = function_.valueTypes.at(instruction.source->value);
          const auto &descriptor = function_.typeDescriptors[sourceType.value];
          if (descriptor.kind == TypeKind::Range)
          {
            const auto start = fresh();
            line(std::format("{} =l loadl {}", start, source));
            const auto endAddress = fresh();
            line(std::format("{} =l add {}, 8", endAddress, source));
            const auto end = fresh();
            line(std::format("{} =l loadl {}", end, endAddress));
            bindResult(instruction, QType::Long, std::format("sub {}, {}", end, start));
          }
          else
          {
            bindResult(instruction, QType::Long, std::format("loadl {}", source));
          }
          return;
        }
        case InstructionKind::RangeStart:
          bindResult(instruction, QType::Long, std::format("loadl {}", operandTemp(*instruction.source)));
          return;
        case InstructionKind::Slice:
        {
          useHelper("arr_slice");
          const auto array = operandTemp(instruction.operands[0]);
          const auto range = operandTemp(instruction.operands[1]);
          const auto start = fresh();
          line(std::format("{} =l loadl {}", start, range));
          const auto endAddress = fresh();
          line(std::format("{} =l add {}, 8", endAddress, range));
          const auto end = fresh();
          line(std::format("{} =l loadl {}", end, endAddress));
          bindResult(instruction, QType::Long, std::format("call $ngrt_arr_slice(l {}, l {}, l {})", array, start, end));
          return;
        }
        case InstructionKind::BindLocal:
        {
          const auto source = operandTemp(*instruction.source);
          const auto sourceType = function_.valueTypes.at(instruction.source->value);
          const auto localType = function_.localTypes.at(instruction.local->value);
          const auto type = localQType(instruction.local->value);
          std::string boundValue = source;
          // Copy-first semantics: aggregates are deep-copied on bind so
          // later in-place mutation cannot alias the source.
          if (needsClone(sourceType))
          {
            const bool aggregateSlot = qtypeOf(sourceType) == QType::Aggregate && !isCellLocal(instruction.local->value) &&
                                       !ssaLocals_.contains(instruction.local->value);
            if (aggregateSlot)
            {
              // Copy the value straight into the local's stack slot.
              emitAggregateCopy(localSlots_.at(instruction.local->value), source, sourceType);
              bindResult(instruction, QType::Aggregate, std::format("copy {}", localSlots_.at(instruction.local->value)));
              return;
            }
            boundValue = emitClone(source, sourceType);
          }
          if (isUnionType(localType) && !isUnionType(sourceType))
          {
            // Values flowing into a union slot are wrapped in a tagged box.
            bindUnionBox(instruction, localType, unionMemberFor(localType, sourceType), boundValue,
                         qtypeOf(sourceType));
          }
          else
          {
            bindResult(instruction, type, std::format("copy {}", boundValue));
          }
          if (ssaLocals_.contains(instruction.local->value))
          {
            // SSA locals are pure temps: record the current temp (QBE's
            // non-SSA fixup handles later rebinding) and skip slot traffic.
            currentTemps_[instruction.local->value] = valueTemps_.at(instruction.result.value);
            return;
          }
          if (isCellLocal(instruction.local->value))
            bindFreshCell(instruction.local->value, valueTemps_.at(instruction.result.value));
          else
            line(std::format("store{} {}, {}", suffix(type), valueTemps_.at(instruction.result.value),
                             localSlots_.at(instruction.local->value)));
          return;
        }
        case InstructionKind::EnumVariantIndex:
          bindResult(instruction, QType::Long, std::format("loadl {}", operandTemp(*instruction.source)));
          return;
        case InstructionKind::ExtractEnumPayload:
        {
          const auto pointer = operandTemp(*instruction.source);
          const auto address = fresh();
          line(std::format("{} =l add {}, 8", address, pointer));
          const auto loaded = fresh();
          line(std::format("{} =l loadl {}", loaded, address));
          const auto resultType = qtypeOf(function_.valueTypes.at(instruction.result.value));
          if (resultType == QType::None)
            return; // Payloadless variant: unit, no temp is consumed.
          bindResult(instruction, resultType, std::format("copy {}", castFromSlot(loaded, resultType)));
          return;
        }
        case InstructionKind::EnumListLength:
        {
          useHelper("enum_list_len");
          bindResult(instruction, QType::Long,
                     std::format("call $ngrt_enum_list_len(l {})", operandTemp(*instruction.source)));
          return;
        }
        case InstructionKind::EnumListGet:
        {
          useHelper("enum_list_get");
          const auto source = operandTemp(instruction.operands[0]);
          const auto index = operandTemp(instruction.operands[1]);
          const auto loaded = fresh();
          line(std::format("{} =l call $ngrt_enum_list_get(l {}, l {})", loaded, source, index));
          const auto resultType = qtypeOf(function_.valueTypes.at(instruction.result.value));
          bindResult(instruction, resultType, std::format("copy {}", castFromSlot(loaded, resultType)));
          return;
        }
        case InstructionKind::TupleSplice:
          lowerTupleSplice(instruction);
          return;
        case InstructionKind::MakeRef:
        {
          // Ref-rooted chaining (placeRootRef) is deferred.
          if (!instruction.placeRootLocal || instruction.placeRootRef)
            throw LoweringError(std::format("native lowering (M2): only local-rooted references are supported in `{}`",
                                            function_.name));
          bindResult(instruction, QType::Long,
                     std::format("copy {}", emitRefObject(instruction.placeRootLocal->value, instruction.placeSteps)));
          return;
        }
        case InstructionKind::MakeTraitView:
        {
          // A trait view is { l ref-object, l trait-id, l concrete-id };
          // the ref object carries the (cell + path) receiver so method
          // bodies observe root rebinding exactly like the VM. Ref-rooted
          // views are deferred.
          if (!instruction.placeRootLocal || instruction.placeRootRef)
            throw LoweringError(std::format("native lowering (M2): only local-rooted trait views are supported in `{}`",
                                            function_.name));
          const auto reference = emitRefObject(instruction.placeRootLocal->value, instruction.placeSteps);
          const auto view = fresh();
          line(std::format("{} =l call $malloc(l 24)", view));
          line(std::format("storel {}, {}", reference, view));
          const auto traitAddress = fresh();
          line(std::format("{} =l add {}, 8", traitAddress, view));
          line(std::format("storel {}, {}", instruction.traitType, traitAddress));
          const auto concreteAddress = fresh();
          line(std::format("{} =l add {}, 16", concreteAddress, view));
          line(std::format("storel {}, {}", instruction.payload, concreteAddress));
          bindResult(instruction, QType::Long, std::format("copy {}", view));
          return;
        }
        case InstructionKind::CallTrait:
        {
          ngrt_.needsTraitDispatch = true;
          const auto view = operandTemp(instruction.operands[0]);
          const auto traitAddress = fresh();
          line(std::format("{} =l add {}, 8", traitAddress, view));
          const auto traitId = fresh();
          line(std::format("{} =l loadl {}", traitId, traitAddress));
          const auto concreteAddress = fresh();
          line(std::format("{} =l add {}, 16", concreteAddress, view));
          const auto concreteId = fresh();
          line(std::format("{} =l loadl {}", concreteId, concreteAddress));
          const auto shifted = fresh();
          line(std::format("{} =l shl {}, 32", shifted, traitId));
          const auto key = fresh();
          line(std::format("{} =l or {}, {}", key, shifted, concreteId));
          const auto table = fresh();
          line(std::format("{} =l call $ngrt_trait_vtable(l {})", table, key));
          const auto functionAddress = fresh();
          line(std::format("{} =l add {}, {}", functionAddress, table, instruction.payload * 8));
          const auto function = fresh();
          line(std::format("{} =l loadl {}", function, functionAddress));
          // The receiver is the view's ref object, passed as `Self ref`.
          const auto receiver = fresh();
          line(std::format("{} =l loadl {}", receiver, view));
          std::string arguments = std::format("l {}", receiver);
          for (size_t index = 1; index < instruction.operands.size(); ++index)
          {
            const auto typeId = function_.valueTypes.at(instruction.operands[index].value);
            arguments += std::format(", {} {}", qbeSuffixFor(typeId), operandTemp(instruction.operands[index]));
          }
          const auto resultTypeId = function_.valueTypes.at(instruction.result.value);
          const auto resultType = qtypeOf(resultTypeId);
          if (resultType == QType::None) line(std::format("call {}({})", function, arguments));
          else
            line(std::format("{} ={} call {}({})", valueTemps_.at(instruction.result.value),
                             resultType == QType::Aggregate ? "l" : qbeSuffixFor(resultTypeId), function, arguments));
          return;
        }
        case InstructionKind::LoadRef:
        {
          useHelper("ref_load");
          const auto reference = operandTemp(instruction.operands[0]);
          const auto loaded = fresh();
          line(std::format("{} =l call $ngrt_ref_load(l {})", loaded, reference));
          const auto resultType = qtypeOf(function_.valueTypes.at(instruction.result.value));
          const auto resultTypeId = function_.valueTypes.at(instruction.result.value);
          std::string value = loaded;
          // Copy-first semantics: `*r` yields a deep copy of aggregates
          // (the VM deep-copies on every reference load).
          if (needsClone(resultTypeId)) value = emitClone(loaded, resultTypeId);
          bindResult(instruction, resultType, std::format("copy {}", castFromSlot(value, resultType)));
          return;
        }
        case InstructionKind::AssignPlace:
        {
          const auto value = operandTemp(instruction.operands.back());
          const auto valueType = qtypeOf(function_.valueTypes.at(instruction.operands.back().value));
          if (instruction.placeRootRef)
          {
            const auto reference = operandTemp(*instruction.placeRootRef);
            if (instruction.placeSteps.empty())
            {
              useHelper("ref_addr");
              const auto address = fresh();
              line(std::format("{} =l call $ngrt_ref_addr(l {})", address, reference));
              if (valueType == QType::Aggregate)
              {
                const auto clone = emitClone(value, function_.valueTypes.at(instruction.operands.back().value));
                line(std::format("storel {}, {}", clone, address));
              }
              else
              {
                line(std::format("store{} {}, {}", suffix(valueType), value, address));
              }
              return;
            }
            // Ref-rooted place with a path (`(*self).field := ...`): the
            // ref's prefix steps walk to the containing aggregate value,
            // then the static path steps walk to the target address.
            useHelper("ref_load");
            const auto current = fresh();
            line(std::format("{} =l call $ngrt_ref_load(l {})", current, reference));
            std::string address = current;
            const auto referent = function_.typeDescriptors[function_.valueTypes.at(instruction.placeRootRef->value).value].element;
            for (const auto &step : normalizeSteps(referent, instruction.placeSteps))
            {
              const auto next = fresh();
              if (step.kind == 1)
              {
                useHelper("arr_addr");
                line(std::format("{} =l call $ngrt_arr_addr(l {}, l {})", next, address, step.payload));
              }
              else if (step.kind == 2)
              {
                line(std::format("{} =l add {}, {}", next, address,
                                 std::format("{}", 16 + 8 * std::stoll(step.payload))));
              }
              else
              {
                line(std::format("{} =l add {}, {}", next, address, step.payload));
              }
              address = next;
            }
            line(std::format("store{} {}, {}", suffix(valueType), value, address));
            return;
          }
          if (!instruction.placeRootLocal)
            throw LoweringError(std::format("native lowering (M2): malformed place in `{}`", function_.name));
          // Local-rooted place: walk the static steps from the slot value to
          // the target address.
          const auto rootLocal = instruction.placeRootLocal->value;
          const auto rootSlot = localSlots_.at(rootLocal);
          if (instruction.placeSteps.empty())
          {
            const auto localType = function_.localTypes.at(rootLocal);
            const auto valueTypeId = function_.valueTypes.at(instruction.operands.back().value);
            std::string wrappedBox;
            std::string storedValue = value;
            if (isUnionType(localType) && !isUnionType(valueTypeId))
            {
              wrappedBox = fresh();
              line(std::format("{} =l call $malloc(l 16)", wrappedBox));
              line(std::format("storel {}, {}", unionMemberFor(localType, valueTypeId), wrappedBox));
              const auto address = fresh();
              line(std::format("{} =l add {}, 8", address, wrappedBox));
              line(std::format("store{} {}, {}", suffix(valueType), value, address));
              storedValue = wrappedBox;
            }
            if (isCellLocal(rootLocal))
            {
              // `:=` mutates the current cell (outstanding refs observe it).
              const auto cell = fresh();
              line(std::format("{} =l loadl {}", cell, rootSlot));
              if (valueType == QType::Aggregate)
              {
                // Copy-first semantics: aggregates are cloned onto the heap
                // so loop-rebuilt stack literals cannot alias into cells.
                const auto clone = emitClone(storedValue, valueTypeId);
                line(std::format("storel {}, {}", clone, cell));
              }
              else
              {
                line(std::format("storel {}, {}", storedValue, cell));
              }
            }
            else if (qtypeOf(localType) == QType::Aggregate)
            {
              // Copy-first semantics: the assigned aggregate is copied into
              // the local's stack slot.
              emitAggregateCopy(rootSlot, storedValue, localType);
            }
            else
            {
              line(std::format("store{} {}, {}", suffix(valueType), storedValue, rootSlot));
            }
            return;
          }
          std::string current;
          if (isCellLocal(rootLocal)) current = loadLocal(rootLocal);
          else if (qtypeOf(function_.localTypes.at(rootLocal)) == QType::Aggregate)
            current = rootSlot; // the slot IS the aggregate object
          else
          {
            current = fresh();
            line(std::format("{} =l loadl {}", current, rootSlot));
          }
          std::string address = current;
          for (const auto &step : normalizeSteps(function_.localTypes.at(rootLocal), instruction.placeSteps))
          {
            const auto next = fresh();
            if (step.kind == 1)
            {
              useHelper("arr_addr");
              line(std::format("{} =l call $ngrt_arr_addr(l {}, l {})", next, address, step.payload));
            }
            else if (step.kind == 2)
            {
              // Constant index into an array/tuple: header + element offset.
              line(std::format("{} =l add {}, {}", next, address, std::format("{}", 16 + 8 * std::stoll(step.payload))));
            }
            else
            {
              line(std::format("{} =l add {}, {}", next, address, step.payload));
            }
            address = next;
          }
          line(std::format("store{} {}, {}", suffix(valueType), value, address));
          return;
        }
        default:
          throw LoweringError(std::format("native lowering (M1): instruction kind {} is not supported yet in `{}`",
                                          static_cast<int>(instruction.kind), function_.name));
        }
      }

      void lowerEvaluate(const Instruction &instruction)
      {
        if (instruction.callTarget)
        {
          lowerCall(instruction);
          return;
        }
        const auto kind = instruction.expressionKind;
        const auto payload = instruction.payload;
        switch (kind)
        {
        case ExpressionKind::IntegerLiteral:
        case ExpressionKind::BooleanLiteral:
        {
          if (isUnionType(resultTypeId(instruction)))
          {
            const auto temp = fresh();
            line(std::format("{} =l copy {}", temp, payload));
            bindUnionBox(instruction, resultTypeId(instruction),
                         unionMemberFor(resultTypeId(instruction), typecheck::builtin::I64), temp, QType::Long);
          }
          else
          {
            bindResult(instruction, QType::Long, std::format("copy {}", payload));
          }
          return;
        }
        case ExpressionKind::FloatLiteral:
        {
          if (isUnionType(resultTypeId(instruction)))
          {
            const auto temp = fresh();
            line(std::format("{} =d copy {}", temp, doubleConstant(std::bit_cast<double>(payload))));
            bindUnionBox(instruction, resultTypeId(instruction),
                         unionMemberFor(resultTypeId(instruction), typecheck::builtin::F64), temp, QType::Double);
          }
          else
          {
            bindResult(instruction, QType::Double, std::format("copy {}", doubleConstant(std::bit_cast<double>(payload))));
          }
          return;
        }
        case ExpressionKind::StringLiteral:
        {
          const auto name = std::format("$ngstr_{}_{}", function_.source.value, dataItems_.size());
          dataItems_.push_back(std::format("data {} = {{ l {}, b \"{}\", b 0 }}", name, instruction.text.size(),
                                            escapeQbeString(instruction.text)));
          const auto text = fresh();
          line(std::format("{} =l copy {}", text, name));
          if (isUnionType(resultTypeId(instruction)))
            bindUnionBox(instruction, resultTypeId(instruction),
                         unionMemberFor(resultTypeId(instruction), typecheck::builtin::String), text, QType::Long);
          else
            bindResult(instruction, QType::Long, std::format("copy {}", text));
          return;
        }
        case ExpressionKind::ArrayLiteral:
        case ExpressionKind::TupleLiteral:
          lowerAggregateLiteral(instruction);
          return;
        case ExpressionKind::EnumLiteral:
        {
          // Enum object: { l tag, l payload-word } — multi-field variants
          // carry a tuple pointer in the payload word.
          const auto variant = static_cast<uint32_t>(static_cast<uint64_t>(payload) >> 32);
          const auto pointer = fresh();
          line(std::format("{} =l alloc8 2", pointer));
          line(std::format("storel {}, {}", variant, pointer));
          const auto wordAddress = fresh();
          line(std::format("{} =l add {}, 8", wordAddress, pointer));
          if (instruction.operands.empty())
          {
            line(std::format("storel 0, {}", wordAddress));
          }
          else
          {
            const auto elementType = qtypeOf(function_.valueTypes.at(instruction.operands[0].value));
            line(std::format("storel {}, {}",
                             castForSlot(operandTemp(instruction.operands[0]), elementType), wordAddress));
          }
          bindResult(instruction, QType::Aggregate, std::format("copy {}", pointer));
          return;
        }
        case ExpressionKind::StructLiteral:
          lowerStructLiteral(instruction);
          return;
        case ExpressionKind::Member:
        {
          const auto receiver = operandTemp(instruction.operands[0]);
          const auto field = static_cast<size_t>(payload);
          const auto receiverType = function_.valueTypes.at(instruction.operands[0].value);
          const auto address = fresh();
          line(std::format("{} =l add {}, {}", address, receiver, fieldOffset(receiverType, field)));
          const auto fieldType = function_.typeDescriptors[receiverType.value].elements[field];
          const auto loaded = loadField(address, fieldType);
          bindResult(instruction, qtypeOf(fieldType), std::format("copy {}", loaded));
          return;
        }
        case ExpressionKind::Index:
        {
          useHelper("arr_get");
          const auto receiver = operandTemp(instruction.operands[0]);
          const auto index = operandTemp(instruction.operands[1]);
          const auto loaded = fresh();
          line(std::format("{} =l call $ngrt_arr_get(l {}, l {})", loaded, receiver, index));
          const auto resultType = qtypeOf(function_.valueTypes.at(instruction.result.value));
          bindResult(instruction, resultType, std::format("copy {}", castFromSlot(loaded, resultType)));
          return;
        }
        case ExpressionKind::ResolvedName:
        {
          const uint32_t local = static_cast<uint32_t>(payload);
          const auto type = localQType(local);
          if (ssaLocals_.contains(local))
          {
            const auto found = currentTemps_.find(local);
            if (found == currentTemps_.end())
              throw LoweringError(std::format("native lowering (M4): SSA local {} is read before it is bound in `{}`",
                                              local, function_.name));
            bindResult(instruction, type, std::format("copy {}", found->second));
          }
          else if (isCellLocal(local))
            bindResult(instruction, type, std::format("copy {}", castFromSlot(loadLocal(local), type)));
          else if (type == QType::Aggregate)
            bindResult(instruction, type, std::format("copy {}", localSlots_.at(local)));
          else bindResult(instruction, type, std::format("load{} {}", suffix(type), localSlots_.at(local)));
          if (const auto found = function_.localTypes.find(local); found != function_.localTypes.end() &&
                                                               isUnionType(found->second))
            unionBoxedValues_.emplace(instruction.result.value, found->second);
          return;
        }
        case ExpressionKind::Grouped:
        {
          const auto source = operandTemp(instruction.operands[0]);
          bindResult(instruction, qtypeOf(function_.valueTypes.at(instruction.result.value)),
                     std::format("copy {}", source));
          if (const auto found = unionBoxedValues_.find(instruction.operands[0].value); found != unionBoxedValues_.end())
            unionBoxedValues_.emplace(instruction.result.value, found->second);
          return;
        }
        case ExpressionKind::Prefix: lowerPrefix(instruction); return;
        case ExpressionKind::Binary: lowerBinary(instruction); return;
        case ExpressionKind::Call:
          // An unresolved trait-slot call inside a generic original body:
          // the VM throws on the same path (evaluateInstruction rejects
          // ExpressionKind::Call), so this code is dead for every reachable
          // program. Emit a defined-but-halting trap.
          bindResult(instruction, qtypeOf(function_.valueTypes.at(instruction.result.value)), "copy 0");
          useHelper("panic");
          {
            const auto unused = fresh();
            line(std::format("{} =l call $ngrt_panic()", unused));
          }
          return;
        default:
          throw LoweringError(std::format("native lowering (M1): expression kind {} is not supported yet in `{}`",
                                          static_cast<int>(kind), function_.name));
        }
      }

      /// Direct call: `%r =T call $sym(T %a, ...)`. A unit result emits a
      /// bare `call`. Native callees and variadic (tuple-packed) calls are
      /// deferred to the descriptor/shim work of M5.
      void lowerCall(const Instruction &instruction)
      {
        if (ngrt_.externDefIds.contains(instruction.callTarget->value))
        {
          lowerExternCall(instruction);
          return;
        }
        if (ngrt_.nativeDefIds.contains(instruction.callTarget->value))
        {
          lowerNativeCall(instruction);
          return;
        }
        std::string arguments;
        for (size_t index = 0; index < instruction.operands.size(); ++index)
        {
          const auto typeId = function_.valueTypes.at(instruction.operands[index].value);
          const auto type = qtypeOf(typeId);
          if (type == QType::None)
            throw LoweringError(std::format("native lowering (M2): unit call argument in `{}`", function_.name));
          if (index != 0) arguments += ", ";
          arguments += std::format("{} {}", qbeSuffixFor(typeId), operandTemp(instruction.operands[index]));
        }
        const auto resultId = function_.valueTypes.at(instruction.result.value);
        const auto resultType = qtypeOf(resultId);
        const auto symbol = symbolFor(*instruction.callTarget);
        if (resultType == QType::None) line(std::format("call {}({})", symbol, arguments));
        else
          line(std::format("{} ={} call {}({})", valueTemps_.at(instruction.result.value),
                           resultType == QType::Aggregate ? "l" : qbeSuffixFor(resultId), symbol, arguments));
        if (isUnionType(resultId))
          unionBoxedValues_.emplace(instruction.result.value, resultId);
      }

      /// B3 first slice: `extern "C"` call site — a direct QBE call to the C
      /// symbol. Argument and result types come from the DECLARED C signature
      /// (NG call sites widen sub-word values to i64, which would corrupt the
      /// ABI): sub-word integers keep sb/ub/sh/uh and are width-checked so C
      /// never sees silently truncated values, f32 values truncate to `s`,
      /// and repr(C) aggregates are passed by pointer at IL level and
      /// classified by the backend. The declared result is then widened into
      /// the NG call-site temp, and aggregate results are copied from QBE's
      /// return area onto the heap (NG aggregate values are heap pointers).
      void lowerExternCall(const Instruction &instruction)
      {
        const auto defId = instruction.callTarget->value;
        const auto symbol = std::format("${}", names_.at(defId));
        const auto declared = [&](size_t index) -> TypeId
        {
          if (const auto found = ngrt_.externParameterTypes.find(defId);
              found != ngrt_.externParameterTypes.end() && index < found->second.size())
            return found->second[index];
          return function_.valueTypes.at(instruction.operands[index].value);
        };
        /// The ngrt width-check helper for a narrow declared C integer type
        /// (D-008 parity: C must never see a silently truncated NG value).
        const auto widthCheck = [](TypeId type) -> const char * {
          switch (type.value)
          {
          case typecheck::builtin::I8.value: return "check_w_i8";
          case typecheck::builtin::I16.value: return "check_w_i16";
          case typecheck::builtin::I32.value: return "check_w_i32";
          case typecheck::builtin::U8.value: return "check_w_u8";
          case typecheck::builtin::U16.value: return "check_w_u16";
          case typecheck::builtin::U32.value: return "check_w_u32";
          default: return nullptr;
          }
        };
        std::string arguments;
        for (size_t index = 0; index < instruction.operands.size(); ++index)
        {
          const auto typeId = declared(index);
          std::string value = operandTemp(instruction.operands[index]);
          if (const char *check = widthCheck(typeId); check != nullptr)
          {
            useHelper(check);
            const auto checked = fresh();
            line(std::format("{} =l call $ngrt_{}(l {})", checked, check, value));
            value = checked;
          }
          else if (typeId == typecheck::builtin::F32)
          {
            const auto single = fresh();
            line(std::format("{} =s truncd {}", single, value));
            value = single;
          }
          if (index != 0) arguments += ", ";
          if (qtypeOf(typeId) == QType::Aggregate)
            arguments += std::format("{} {}", aggregateName(typeId), value);
          else
            arguments += std::format("{} {}", externSuffixFor(typeId), value);
        }
        const auto siteResult = function_.valueTypes.at(instruction.result.value);
        const auto declaredResult = [&]() -> TypeId
        {
          if (const auto found = ngrt_.externResultTypes.find(defId); found != ngrt_.externResultTypes.end())
            return found->second;
          return siteResult;
        }();
        const auto siteType = qtypeOf(siteResult);
        if (siteType == QType::None)
        {
          line(std::format("call {}({})", symbol, arguments));
          return;
        }
        const auto declaredType = qtypeOf(declaredResult);
        if (siteType == QType::Aggregate || declaredType == QType::Aggregate)
        {
          if (siteType != QType::Aggregate || declaredType != QType::Aggregate)
            throw LoweringError(std::format("native lowering (B3): aggregate/{} extern result mismatch in `{}`",
                                            siteType == QType::Aggregate ? "scalar" : "aggregate", function_.name));
          const auto returnArea = fresh();
          line(std::format("{} ={} call {}({})", returnArea, aggregateName(declaredResult), symbol, arguments));
          bindResult(instruction, QType::Aggregate, std::format("copy {}", returnArea));
          return;
        }
        const auto declaredSuffix = externSuffixFor(declaredResult);
        const auto siteSuffix = qbeSuffixFor(siteResult);
        if (declaredSuffix == siteSuffix)
        {
          line(std::format("{} ={} call {}({})", valueTemps_.at(instruction.result.value), declaredSuffix, symbol,
                           arguments));
          return;
        }
        // Widening conversion: the C signature returns a 32-bit (or single-
        // precision) value and the NG call site expects the widened i64/f64
        // (the checker widens sub-word results at call sites); extend per the
        // declared signedness.
        const auto raw = fresh();
        line(std::format("{} ={} call {}({})", raw, declaredSuffix, symbol, arguments));
        if (siteSuffix == "l" && declaredSuffix == "w")
        {
          const auto widen = typecheck::isUnsignedIntegerBuiltin(declaredResult) ? "extuw" : "extsw";
          bindResult(instruction, QType::Long, std::format("{} {}", widen, raw));
          return;
        }
        if (siteSuffix == "d" && declaredSuffix == "s")
        {
          bindResult(instruction, QType::Double, std::format("exts {}", raw));
          return;
        }
        throw LoweringError(std::format("native lowering (B3): unsupported extern result conversion from {} to {} "
                                        "in `{}`",
                                        declaredSuffix, siteSuffix, function_.name));
      }

      /// Struct literal: a stack slot (alloc8) with 8-byte word fields
      /// (Tier 1 layout: offset = 8 * field ordinal).
      void lowerStructLiteral(const Instruction &instruction)
      {
        const auto typeId = function_.valueTypes.at(instruction.result.value);
        const auto &descriptor = function_.typeDescriptors[typeId.value];
        const size_t count = descriptor.fieldNames.size();
        if (count != instruction.operands.size())
          throw LoweringError(std::format("native lowering (M2): struct literal arity mismatch in `{}`", function_.name));
        const auto pointer = fresh();
        line(std::format("{} =l alloc8 {}", pointer, (aggregateSize(typeId) + 7) / 8));
        for (size_t index = 0; index < count; ++index)
        {
          const auto address = fresh();
          line(std::format("{} =l add {}, {}", address, pointer, fieldOffset(typeId, index)));
          storeField(address, operandTemp(instruction.operands[index]), descriptor.elements[index]);
        }
        bindResult(instruction, QType::Aggregate, std::format("copy {}", pointer));
      }

      /// Tuple splice: flattens tuple operands into one tuple object. Tuple
      /// vs plain is decided statically from the operand type descriptors,
      /// and the runtime element count drives the allocation.
      void lowerTupleSplice(const Instruction &instruction)
      {
        useHelper("copy_elements");
        size_t plainCount = 0;
        for (const auto operand : instruction.operands)
        {
          const auto type = function_.valueTypes.at(operand.value);
          if (type.value >= function_.typeDescriptors.size() ||
              function_.typeDescriptors[type.value].kind != TypeKind::Tuple)
            ++plainCount;
        }
        const auto total = fresh();
        line(std::format("{} =l copy {}", total, plainCount));
        for (const auto operand : instruction.operands)
        {
          const auto type = function_.valueTypes.at(operand.value);
          if (type.value < function_.typeDescriptors.size() &&
              function_.typeDescriptors[type.value].kind == TypeKind::Tuple)
          {
            const auto length = fresh();
            line(std::format("{} =l loadl {}", length, operandTemp(operand)));
            line(std::format("{} =l add {}, {}", total, total, length));
          }
        }
        const auto bytes = fresh();
        line(std::format("{} =l mul {}, 8", bytes, total));
        const auto allocation = fresh();
        line(std::format("{} =l add {}, 16", allocation, bytes));
        const auto pointer = fresh();
        line(std::format("{} =l call $malloc(l {})", pointer, allocation));
        line(std::format("storel {}, {}", total, pointer));
        const auto capAddress = fresh();
        line(std::format("{} =l add {}, 8", capAddress, pointer));
        line(std::format("storel {}, {}", total, capAddress));
        auto destination = fresh();
        line(std::format("{} =l add {}, 16", destination, pointer));
        for (const auto operand : instruction.operands)
        {
          const auto type = function_.valueTypes.at(operand.value);
          if (type.value < function_.typeDescriptors.size() &&
              function_.typeDescriptors[type.value].kind == TypeKind::Tuple)
          {
            const auto length = fresh();
            line(std::format("{} =l loadl {}", length, operandTemp(operand)));
            const auto unused = fresh();
            line(std::format("{} =l call $ngrt_copy_elements(l {}, l {}, l {})", unused, destination,
                             operandTemp(operand), length));
            const auto lengthBytes = fresh();
            line(std::format("{} =l mul {}, 8", lengthBytes, length));
            const auto next = fresh();
            line(std::format("{} =l add {}, {}", next, destination, lengthBytes));
            destination = next;
          }
          else
          {
            const auto elementType = qtypeOf(type);
            line(std::format("storel {}, {}", castForSlot(operandTemp(operand), elementType), destination));
            const auto next = fresh();
            line(std::format("{} =l add {}, 8", next, destination));
            destination = next;
          }
        }
        bindResult(instruction, QType::Long, std::format("copy {}", pointer));
      }

      /// Builds a ref object { l root-slot, l count, { l kind, l payload }[] }
      /// on the heap; shared by MakeRef and MakeTraitView.
      [[nodiscard]] auto emitRefObject(uint32_t rootLocal, const std::vector<flowir::PlaceStep> &steps) -> std::string
      {
        // The ref's root is the local's current cell (captured now); cell
        // locals keep the snapshot semantics of the VM's shared cells.
        std::string root;
        if (isCellLocal(rootLocal))
        {
          const auto cell = fresh();
          line(std::format("{} =l loadl {}", cell, localSlots_.at(rootLocal)));
          root = cell;
        }
        else
        {
          root = localSlots_.at(rootLocal);
        }
        const auto rootType = function_.localTypes.at(rootLocal);
        const auto normalized = normalizeSteps(rootType, steps);
        const size_t count = normalized.size();
        const auto pointer = fresh();
        line(std::format("{} =l call $malloc(l {})", pointer, 16 + count * 16));
        line(std::format("storel {}, {}", root, pointer));
        const auto countAddress = fresh();
        line(std::format("{} =l add {}, 8", countAddress, pointer));
        line(std::format("storel {}, {}", count, countAddress));
        for (size_t index = 0; index < count; ++index)
        {
          const auto stepAddress = fresh();
          line(std::format("{} =l add {}, {}", stepAddress, pointer, 16 + index * 16));
          const auto payloadAddress = fresh();
          line(std::format("{} =l add {}, 8", payloadAddress, stepAddress));
          line(std::format("storel {}, {}", normalized[index].kind, stepAddress));
          line(std::format("storel {}, {}", normalized[index].payload, payloadAddress));
        }
        return pointer;
      }

      /// True for aggregate kinds whose copy-first semantics require a deep
      /// copy at bind/load sites (the VM deep-copies all of these).
      [[nodiscard]] auto needsClone(TypeId type) -> bool
      {
        if (type.value >= function_.typeDescriptors.size()) return false;
        const auto kind = function_.typeDescriptors[type.value].kind;
        return kind == TypeKind::Struct || kind == TypeKind::Enum || kind == TypeKind::Tuple ||
               kind == TypeKind::DynamicArray || kind == TypeKind::FixedArray ||
               (kind == TypeKind::Builtin && type == typecheck::builtin::String);
      }

      /// Emits a deep copy of a Tier 0 aggregate value (static type-driven:
      /// structs clone field-by-field recursively, tuples unroll statically,
      /// arrays pick a per-element-kind helper, strings use $ngrt_str_clone).
      [[nodiscard]] auto emitClone(const std::string &source, TypeId type) -> std::string
      {
        const auto &descriptor = function_.typeDescriptors[type.value];
        if (descriptor.kind == TypeKind::Builtin)
        {
          if (type != typecheck::builtin::String) return source;
          useHelper("str_clone");
          const auto clone = fresh();
          line(std::format("{} =l call $ngrt_str_clone(l {})", clone, source));
          return clone;
        }
        if (descriptor.kind == TypeKind::Enum)
        {
          // Heap copy of the two words (tag + payload; payload words are
          // never mutated in place, so sharing a tuple payload is safe).
          const auto clone = fresh();
          line(std::format("{} =l call $malloc(l 16)", clone));
          emitAggregateCopy(clone, source, type);
          return clone;
        }
        if (descriptor.kind == TypeKind::Struct || descriptor.kind == TypeKind::Tuple)
        {
          const size_t count = descriptor.kind == TypeKind::Struct ? descriptor.fieldNames.size()
                                                                   : descriptor.elements.size();
          const size_t header = descriptor.kind == TypeKind::Struct ? 0 : 16;
          const auto clone = fresh();
          // Struct clones live on the heap: they are values that can be
          // borrowed and escape their frame (cell locals store the pointer).
          if (descriptor.kind == TypeKind::Struct) line(std::format("{} =l call $malloc(l {})", clone, count * 8));
          else line(std::format("{} =l call $malloc(l {})", clone, count * 8 + header));
          if (descriptor.kind == TypeKind::Tuple)
          {
            line(std::format("storel {}, {}", count, clone));
            const auto capAddress = fresh();
            line(std::format("{} =l add {}, 8", capAddress, clone));
            line(std::format("storel {}, {}", count, capAddress));
          }
          for (size_t index = 0; index < count; ++index)
          {
            const auto fieldAddress = fresh();
            line(std::format("{} =l add {}, {}", fieldAddress, source, header + index * 8));
            const auto fieldValue = fresh();
            line(std::format("{} =l loadl {}", fieldValue, fieldAddress));
            const auto fieldClone = emitClone(fieldValue, descriptor.elements[index]);
            const auto target = fresh();
            line(std::format("{} =l add {}, {}", target, clone, header + index * 8));
            line(std::format("storel {}, {}", fieldClone, target));
          }
          return clone;
        }
        if (descriptor.kind == TypeKind::DynamicArray || descriptor.kind == TypeKind::FixedArray)
        {
          const auto elementKind = function_.typeDescriptors[descriptor.element.value].kind;
          if (elementKind == TypeKind::Builtin && descriptor.element == typecheck::builtin::String)
          {
            useHelper("arr_clone_strings");
            const auto clone = fresh();
            line(std::format("{} =l call $ngrt_arr_clone_strings(l {})", clone, source));
            return clone;
          }
          if (elementKind == TypeKind::Struct || elementKind == TypeKind::Tuple || elementKind == TypeKind::DynamicArray ||
              elementKind == TypeKind::FixedArray)
            throw LoweringError(std::format("native lowering (M2): deep copy of nested aggregate arrays is not "
                                            "supported yet in `{}`",
                                            function_.name));
          useHelper("arr_clone_words");
          const auto clone = fresh();
          line(std::format("{} =l call $ngrt_arr_clone_words(l {})", clone, source));
          return clone;
        }
        return source;
      }

      /// Normalized place-step for emission: 0 = struct field (offset 8f),
      /// 1 = runtime index (bounds-checked element address), 2 = constant
      /// index into an array/tuple (offset 16 + 8f — FlowIR encodes constant
      /// indexes as Member steps, matching the VM's walkStep).
      struct NormalizedStep
      {
        int64_t kind{};
        std::string payload{};
      };

      /// Walks a static place path with type tracking, normalizing constant
      /// indexes over arrays/tuples to header-aware offsets.
      [[nodiscard]] auto normalizeSteps(TypeId rootType, const std::vector<flowir::PlaceStep> &steps)
          -> std::vector<NormalizedStep>
      {
        std::vector<NormalizedStep> result;
        result.reserve(steps.size());
        TypeId current = rootType;
        for (const auto &step : steps)
        {
          if (step.kind == flowir::PlaceStep::Kind::Member)
          {
            const auto &descriptor = function_.typeDescriptors[current.value];
            const bool elementLike = descriptor.kind == TypeKind::DynamicArray ||
                                     descriptor.kind == TypeKind::FixedArray || descriptor.kind == TypeKind::Tuple;
            const int64_t payload = elementLike ? static_cast<int64_t>(step.field)
                                                : static_cast<int64_t>(fieldOffset(current, step.field));
            result.push_back(NormalizedStep{.kind = elementLike ? 2 : 0, .payload = std::format("{}", payload)});
            current = elementLike ? (descriptor.kind == TypeKind::Tuple ? descriptor.elements[step.field]
                                                                        : descriptor.element)
                                  : descriptor.elements[step.field];
          }
          else
          {
            result.push_back(NormalizedStep{.kind = 1, .payload = operandTemp(step.indexValue)});
            current = function_.typeDescriptors[current.value].element;
          }
        }
        return result;
      }

      /// Array/tuple literal: allocates a { len, cap, elements } object and
      /// stores the elements (doubles are bit-cast into `l` slots).
      void lowerAggregateLiteral(const Instruction &instruction)
      {
        const size_t count = instruction.operands.size();
        const auto pointer = fresh();
        line(std::format("{} =l call $malloc(l {})", pointer, count * 8 + 16));
        line(std::format("storel {}, {}", count, pointer));
        const auto cap = fresh();
        line(std::format("{} =l add {}, 8", cap, pointer));
        line(std::format("storel {}, {}", count, cap));
        for (size_t index = 0; index < count; ++index)
        {
          const auto elementType = qtypeOf(function_.valueTypes.at(instruction.operands[index].value));
          const auto element = castForSlot(operandTemp(instruction.operands[index]), elementType);
          const auto address = fresh();
          line(std::format("{} =l add {}, {}", address, pointer, 16 + index * 8));
          line(std::format("storel {}, {}", element, address));
        }
        bindResult(instruction, QType::Long, std::format("copy {}", pointer));
      }

      /// AOT shims for the standard natives: emitted per call site from the
      /// native's name and the call site's static signature. The C
      /// implementations live in src/native/ngrt_shims.c (linked as
      /// `libngrt.a`); the declared-descriptor registry (M5) will supersede
      /// this name-keyed table.
      void lowerNativeCall(const Instruction &instruction)
      {
        const auto name = names_.at(instruction.callTarget->value);
        // R9 declared signature: validate arity at lowering time.
        if (const auto found = ngrt_.nativeSignatures.find(instruction.callTarget->value);
            found != ngrt_.nativeSignatures.end() && found->second.size() != instruction.operands.size())
          throw LoweringError(std::format("native `{}` expects {} argument(s), got {} in `{}`", name,
                                          found->second.size(), instruction.operands.size(), function_.name));
        const auto arg = [&](size_t index) -> std::string { return operandTemp(instruction.operands[index]); };
        const auto argType = [&](size_t index) -> TypeId
        {
          return function_.valueTypes.at(instruction.operands[index].value);
        };
        // The declared parameter type is authoritative for shim selection
        // (e.g. print's unsigned/boolean variants).
        const auto declaredType = [&](size_t index) -> TypeId
        {
          if (const auto found = ngrt_.nativeSignatures.find(instruction.callTarget->value);
              found != ngrt_.nativeSignatures.end() && index < found->second.size())
            return found->second[index];
          return argType(index);
        };
        // Emits a call to a C shim; long-returning shims always receive a
        // temp (QBE requires one), unit results get a throwaway temp.
        const auto emitShim = [&](std::string_view symbol, bool returnsLong, std::string_view arguments)
        {
          if (returnsLong)
          {
            const auto target = qtypeOf(function_.valueTypes.at(instruction.result.value)) == QType::None
                                    ? fresh()
                                    : valueTemps_.at(instruction.result.value);
            line(std::format("{} =l call {}({})", target, symbol, arguments));
          }
          else
          {
            line(std::format("call {}({})", symbol, arguments));
          }
        };
        if (name == "print")
        {
          const auto type = declaredType(0);
          if (type == typecheck::builtin::String) emitShim("$ngshim_print_str", true, "l " + arg(0));
          else if (type == typecheck::builtin::Bool) emitShim("$ngshim_print_bool", true, "l " + arg(0));
          else if (typecheck::isUnsignedIntegerBuiltin(type)) emitShim("$ngshim_print_u64", true, "l " + arg(0));
          else if (isFloat(type)) emitShim("$ngshim_print_f64", true, "d " + arg(0));
          else emitShim("$ngshim_print_i64", true, "l " + arg(0));
          return;
        }
        if (name == "assert")
        {
          emitShim("$ngshim_assert", false, "l " + arg(0));
          return;
        }
        if (name == "length")
        {
          emitShim("$ngshim_str_len", true, "l " + arg(0));
          return;
        }
        if (name == "len")
        {
          // Array length is a direct header load.
          bindResult(instruction, QType::Long, std::format("loadl {}", arg(0)));
          return;
        }
        if (name == "charAt") return emitShim("$ngshim_str_char_at", true, std::format("l {}, l {}", arg(0), arg(1)));
        if (name == "substring")
          return emitShim("$ngshim_str_substring", true, std::format("l {}, l {}, l {}", arg(0), arg(1), arg(2)));
        if (name == "trim") return emitShim("$ngshim_str_trim", true, "l " + arg(0));
        if (name == "toUpper") return emitShim("$ngshim_str_to_upper", true, "l " + arg(0));
        if (name == "toLower") return emitShim("$ngshim_str_to_lower", true, "l " + arg(0));
        if (name == "contains") return emitShim("$ngshim_str_contains", true, std::format("l {}, l {}", arg(0), arg(1)));
        if (name == "startsWith")
          return emitShim("$ngshim_str_starts_with", true, std::format("l {}, l {}", arg(0), arg(1)));
        if (name == "endsWith") return emitShim("$ngshim_str_ends_with", true, std::format("l {}, l {}", arg(0), arg(1)));
        if (name == "replace")
          return emitShim("$ngshim_str_replace", true, std::format("l {}, l {}, l {}", arg(0), arg(1), arg(2)));
        if (name == "split") return emitShim("$ngshim_str_split", true, std::format("l {}, l {}", arg(0), arg(1)));
        if (name == "join") return emitShim("$ngshim_str_join", true, std::format("l {}, l {}", arg(0), arg(1)));
        if (name == "regexMatch")
          return emitShim("$ngshim_regex_match", true, std::format("l {}, l {}", arg(0), arg(1)));
        if (name == "sum") return emitShim("$ngshim_arr_sum", true, "l " + arg(0));
        if (name == "arrayContains")
          return emitShim("$ngshim_arr_contains", true, std::format("l {}, l {}", arg(0), arg(1)));
        if (name == "reverse") return emitShim("$ngshim_arr_reverse", true, "l " + arg(0));
        if (name == "allocate") return emitShim("$ngshim_allocate", true, "l " + arg(0));
        if (name == "load") return emitShim("$ngshim_load", true, "l " + arg(0));
        if (name == "store") return emitShim("$ngshim_store", false, std::format("l {}, l {}", arg(0), arg(1)));
        if (name == "release") return emitShim("$ngshim_release", false, "l " + arg(0));
        if (name == "outstanding") return emitShim("$ngshim_outstanding", true, "");
        if (name == "currentExecutablePath") return emitShim("$ngshim_current_executable_path", true, "");
        if (name == "readLine") return emitShim("$ngshim_read_line", true, "");
        if (name == "readFile") return emitShim("$ngshim_read_file", true, "l " + arg(0));
        if (name == "writeFile")
          return emitShim("$ngshim_write_file", false, std::format("l {}, l {}", arg(0), arg(1)));
        throw LoweringError(std::format("native `{}` has no AOT shim yet", name));
      }

      /// Union equality/inequality against a member value: the tag must match
      /// the member's index and the payload must equal the operand. Ordering
      /// compares the integer payload directly (the VM throws for non-integer
      /// payloads; the tagless native form silently compares raw words — a
      /// documented Tier 0 deviation).
      void lowerUnionCompare(const Instruction &instruction, TypeId unionType, bool leftIsUnion)
      {
        const auto payload = instruction.payload;
        const auto memberType = function_.valueTypes.at(
            (leftIsUnion ? instruction.operands[1] : instruction.operands[0]).value);
        const auto unionTemp = leftIsUnion ? operandTemp(instruction.operands[0]) : operandTemp(instruction.operands[1]);
        const auto memberTemp = leftIsUnion ? operandTemp(instruction.operands[1]) : operandTemp(instruction.operands[0]);
        const auto member = unionMemberFor(unionType, memberType);
        const auto payloadAddress = fresh();
        line(std::format("{} =l add {}, 8", payloadAddress, unionTemp));
        const auto payloadValue = fresh();
        line(std::format("{} =l loadl {}", payloadValue, payloadAddress));
        if (payload == 8 || payload == 9 || payload == 10 || payload == 11)
        {
          const char *qbeOp = payload == 8 ? "csltl" : payload == 9 ? "cslel" : payload == 10 ? "csgtl" : "csgel";
          const auto compared = fresh();
          line(std::format("{} =w {} {}, {}", compared, qbeOp, payloadValue, memberTemp));
          const auto extended = fresh();
          line(std::format("{} =l extsw {}", extended, compared));
          bindResult(instruction, QType::Long, std::format("copy {}", extended));
          return;
        }
        if (payload != 6 && payload != 7)
          throw LoweringError(std::format("native lowering (M2): unsupported union binary payload {} in `{}`",
                                          payload, function_.name));
        const auto tag = fresh();
        line(std::format("{} =l loadl {}", tag, unionTemp));
        const auto tagOk = fresh();
        line(std::format("{} =w ceql {}, {}", tagOk, tag, member));
        const auto valueOk = fresh();
        if (memberType == typecheck::builtin::String)
        {
          useHelper("str_eq");
          line(std::format("{} =w call $ngrt_str_eq(l {}, l {})", valueOk, payloadValue, memberTemp));
        }
        else if (typecheck::isFloatBuiltin(memberType))
        {
          const auto payloadDouble = fresh();
          line(std::format("{} =d cast {}", payloadDouble, payloadValue));
          line(std::format("{} =w ceqd {}, {}", valueOk, payloadDouble, memberTemp));
        }
        else
        {
          line(std::format("{} =w ceql {}, {}", valueOk, payloadValue, memberTemp));
        }
        const auto combined = fresh();
        line(std::format("{} =w and {}, {}", combined, tagOk, valueOk));
        const auto extended = fresh();
        line(std::format("{} =l extsw {}", extended, combined));
        if (payload == 6) bindResult(instruction, QType::Long, std::format("copy {}", extended));
        else bindResult(instruction, QType::Long, std::format("xor {}, 1", extended));
      }

      void lowerPrefix(const Instruction &instruction)
      {
        const auto payload = instruction.payload;
        const auto operand = operandTemp(instruction.operands[0]);
        const auto operandType = function_.valueTypes.at(instruction.operands[0].value);
        const auto resultType = qtypeOf(function_.valueTypes.at(instruction.result.value));
        // Unary +, move, and clone are all plain copies for scalars.
        if (payload == 3 || payload == 4 || payload == 5)
        {
          bindResult(instruction, resultType, std::format("copy {}", operand));
          if (payload == 3 && typecheck::isIntegerBuiltin(operandType)) applyWidthCheck(instruction);
          if (const auto found = unionBoxedValues_.find(instruction.operands[0].value); found != unionBoxedValues_.end())
            unionBoxedValues_.emplace(instruction.result.value, found->second);
          return;
        }
        if (payload == 2)
        {
          if (isFloat(operandType)) bindResult(instruction, resultType, std::format("neg {}", operand));
          else emitCheckedOp(instruction, "checked_neg", std::format("l {}", operand));
          return;
        }
        if (payload == 1)
        {
          if (isFloat(operandType))
            throw LoweringError(std::format("native lowering (M1): logical negation of a float in `{}`", function_.name));
          integerCompare(instruction, "ceql", operand, "0");
          return;
        }
        throw LoweringError(std::format("native lowering (M1): unsupported prefix payload {} in `{}`", payload,
                                        function_.name));
      }

      /// Converts an integer temp to double for mixed numeric promotion
      /// (VM `asNumber` semantics: any float operand promotes the whole op).
      [[nodiscard]] auto promoteToDouble(const std::string &temp, TypeId type) -> std::string
      {
        if (isFloat(type)) return temp;
        const auto converted = fresh();
        line(std::format("{} =d sltof {}", converted, temp));
        return converted;
      }

      void lowerBinary(const Instruction &instruction)
      {
        const auto payload = instruction.payload;
        const auto left = operandTemp(instruction.operands[0]);
        const auto right = operandTemp(instruction.operands[1]);
        const auto leftType = function_.valueTypes.at(instruction.operands[0].value);
        const auto rightType = function_.valueTypes.at(instruction.operands[1].value);

        // Unions compare against member values: (tag == member) && payload.
        // Union-ness is tracked by data flow (unionBoxedValues_), because the
        // checker records union operands as their member type at comparison
        // sites.
        const auto unionTypeOf = [&](ValueId value) -> std::optional<TypeId>
        {
          if (const auto found = unionBoxedValues_.find(value.value); found != unionBoxedValues_.end())
            return found->second;
          const auto type = function_.valueTypes.at(value.value);
          if (isUnionType(type)) return type;
          return std::nullopt;
        };
        const auto leftUnion = unionTypeOf(instruction.operands[0]);
        const auto rightUnion = unionTypeOf(instruction.operands[1]);
        if (leftUnion || rightUnion)
        {
          lowerUnionCompare(instruction, leftUnion ? *leftUnion : *rightUnion, leftUnion.has_value());
          return;
        }

        // Strings: concatenation and content equality/inequality.
        if (leftType == typecheck::builtin::String)
        {
          if (payload == 1)
          {
            useHelper("str_concat");
            bindResult(instruction, QType::Long, std::format("call $ngrt_str_concat(l {}, l {})", left, right));
            return;
          }
          if (payload == 6 || payload == 7)
          {
            useHelper("str_eq");
            const auto compared = fresh();
            line(std::format("{} =w call $ngrt_str_eq(l {}, l {})", compared, left, right));
            const auto extended = fresh();
            line(std::format("{} =l extsw {}", extended, compared));
            if (payload == 6) bindResult(instruction, QType::Long, std::format("copy {}", extended));
            else bindResult(instruction, QType::Long, std::format("xor {}, 1", extended));
            return;
          }
          throw LoweringError(std::format("native lowering (M2): unsupported string binary payload {} in `{}`",
                                          payload, function_.name));
        }
        // Range construction: { l start, l end }.
        if (payload == 19)
        {
          const auto pointer = fresh();
          line(std::format("{} =l call $malloc(l 16)", pointer));
          line(std::format("storel {}, {}", left, pointer));
          const auto endAddress = fresh();
          line(std::format("{} =l add {}, 8", endAddress, pointer));
          line(std::format("storel {}, {}", right, endAddress));
          bindResult(instruction, QType::Long, std::format("copy {}", pointer));
          return;
        }

        if (isFloat(leftType) || isFloat(rightType))
        {
          const auto leftDouble = promoteToDouble(left, leftType);
          const auto rightDouble = promoteToDouble(right, rightType);
          switch (payload)
          {
          case 1: bindResult(instruction, QType::Double, std::format("add {}, {}", leftDouble, rightDouble)); return;
          case 2: bindResult(instruction, QType::Double, std::format("sub {}, {}", leftDouble, rightDouble)); return;
          case 3: bindResult(instruction, QType::Double, std::format("mul {}, {}", leftDouble, rightDouble)); return;
          case 4: bindResult(instruction, QType::Double, std::format("div {}, {}", leftDouble, rightDouble)); return;
          case 6:
          {
            const auto compared = fresh();
            line(std::format("{} =w ceqd {}, {}", compared, leftDouble, rightDouble));
            bindResult(instruction, QType::Long, std::format("extsw {}", compared));
            return;
          }
          case 7:
          {
            const auto compared = fresh();
            line(std::format("{} =w cned {}, {}", compared, leftDouble, rightDouble));
            bindResult(instruction, QType::Long, std::format("extsw {}", compared));
            return;
          }
          case 8:
          {
            const auto compared = fresh();
            line(std::format("{} =w cltd {}, {}", compared, leftDouble, rightDouble));
            bindResult(instruction, QType::Long, std::format("extsw {}", compared));
            return;
          }
          case 9:
          {
            const auto compared = fresh();
            line(std::format("{} =w cled {}, {}", compared, leftDouble, rightDouble));
            bindResult(instruction, QType::Long, std::format("extsw {}", compared));
            return;
          }
          case 10:
          {
            const auto compared = fresh();
            line(std::format("{} =w cgtd {}, {}", compared, leftDouble, rightDouble));
            bindResult(instruction, QType::Long, std::format("extsw {}", compared));
            return;
          }
          case 11:
          {
            const auto compared = fresh();
            line(std::format("{} =w cged {}, {}", compared, leftDouble, rightDouble));
            bindResult(instruction, QType::Long, std::format("extsw {}", compared));
            return;
          }
          default:
            throw LoweringError(std::format("native lowering (M1): unsupported float binary payload {} in `{}`",
                                            payload, function_.name));
          }
        }

        switch (payload)
        {
        case 1: emitCheckedOp(instruction, "checked_add", std::format("l {}, l {}", left, right)); return;
        case 2: emitCheckedOp(instruction, "checked_sub", std::format("l {}, l {}", left, right)); return;
        case 3: emitCheckedOp(instruction, "checked_mul", std::format("l {}, l {}", left, right)); return;
        case 4: emitCheckedOp(instruction, "checked_div", std::format("l {}, l {}", left, right)); return;
        case 5: emitCheckedOp(instruction, "checked_rem", std::format("l {}, l {}", left, right)); return;
        case 6: integerCompare(instruction, "ceql", left, right); return;
        case 7: integerCompare(instruction, "cnel", left, right); return;
        case 8: integerCompare(instruction, "csltl", left, right); return;
        case 9: integerCompare(instruction, "cslel", left, right); return;
        case 10: integerCompare(instruction, "csgtl", left, right); return;
        case 11: integerCompare(instruction, "csgel", left, right); return;
        case 12:
        case 13:
        {
          // Short-circuit-free boolean semantics: both sides are compared to
          // zero and combined (VM evaluates both operands eagerly).
          const auto leftBool = fresh();
          const auto rightBool = fresh();
          const auto combined = fresh();
          line(std::format("{} =w cnel {}, 0", leftBool, left));
          line(std::format("{} =w cnel {}, 0", rightBool, right));
          line(std::format("{} =w {} {}, {}", combined, payload == 12 ? "and" : "or", leftBool, rightBool));
          bindResult(instruction, QType::Long, std::format("extsw {}", combined));
          return;
        }
        case 14: bindResult(instruction, QType::Long, std::format("and {}, {}", left, right)); return;
        case 15: bindResult(instruction, QType::Long, std::format("or {}, {}", left, right)); return;
        case 16: bindResult(instruction, QType::Long, std::format("xor {}, {}", left, right)); return;
        case 17: emitCheckedOp(instruction, "checked_shl", std::format("l {}, l {}", left, right)); return;
        case 18: emitCheckedOp(instruction, "checked_shr", std::format("l {}, l {}", left, right)); return;
        default:
          throw LoweringError(std::format("native lowering (M1): unsupported integer binary payload {} in `{}`",
                                          payload, function_.name));
        }
      }

      void lowerTerminator(const Terminator &terminator, size_t blockIndex)
      {
        switch (terminator.kind)
        {
        case TerminatorKind::Return:
          if (terminator.arguments.empty())
          {
            line(function_.name == "main" ? "ret 0" : "ret");
          }
          else if (qtypeOf(function_.valueTypes.at(terminator.arguments[0].value)) == QType::Aggregate &&
                   function_.name != "main")
          {
            const auto clone = emitClone(operandTemp(terminator.arguments[0]),
                                         function_.valueTypes.at(terminator.arguments[0].value));
            line(std::format("ret {}", clone));
          }
          else if (function_.name == "main")
          {
            // String/float mains have no meaningful exit code: print the
            // value (matching the VM driver's "with value ...") and exit 0.
            const auto value = operandTemp(terminator.arguments[0]);
            const auto type = function_.valueTypes.at(terminator.arguments[0].value);
            if (type == typecheck::builtin::String)
            {
              const auto unused = fresh();
              line(std::format("{} =l call $ngshim_print_str(l {})", unused, value));
              line("ret 0");
            }
            else if (typecheck::isFloatBuiltin(type))
            {
              const auto unused = fresh();
              line(std::format("{} =l call $ngshim_print_f64(d {})", unused, value));
              line("ret 0");
            }
            else
            {
              line(std::format("ret {}", value));
            }
          }
          else
          {
            line(std::format("ret {}", operandTemp(terminator.arguments[0])));
          }
          return;
        case TerminatorKind::Jump:
        case TerminatorKind::LoopBackedge:
          line(std::format("jmp {}", labels_.at(terminator.targets[0].value)));
          return;
        case TerminatorKind::Branch:
          line(std::format("jnz {}, {}, {}", operandTemp(terminator.arguments[0]),
                           labels_.at(terminator.targets[0].value), labels_.at(terminator.targets[1].value)));
          return;
        case TerminatorKind::TailRecur:
          for (size_t index = 0; index < function_.parameterLocals.size(); ++index)
          {
            const auto local = function_.parameterLocals[index].value;
            if (isCellLocal(local)) bindFreshCell(local, operandTemp(terminator.arguments[index]));
            else
              line(std::format("store{} {}, {}", suffix(localQTypes_.at(local)), operandTemp(terminator.arguments[index]),
                               localSlots_.at(local)));
          }
          line(std::format("jmp {}", labels_.front()));
          return;
        }
      }
    };
  } // namespace

  auto lower(const Function &function, const FunctionNames &names) -> std::string
  {
    NgrtContext ngrt;
    return FunctionLowerer{function, names, ngrt}.run();
  }

  auto lowerModule(const std::vector<Function> &functions, const VtableMap &vtables) -> std::string
  {
    FunctionNames names;
    for (const auto &function : functions) names.emplace(function.source.value, function.name);
    NgrtContext ngrt;
    for (const auto &function : functions)
    {
      if (function.nativeFunction)
      {
        ngrt.nativeDefIds.insert(function.source.value);
        std::vector<TypeId> declared;
        for (const auto local : function.parameterLocals)
          if (const auto found = function.localTypes.find(local.value); found != function.localTypes.end())
            declared.push_back(found->second);
        ngrt.nativeSignatures.emplace(function.source.value, std::move(declared));
      }
      else if (function.externC)
      {
        // B3 first slice: extern declarations have no NG body; call sites
        // emit direct QBE calls to the C symbol with the declared C types.
        ngrt.externDefIds.insert(function.source.value);
        std::vector<TypeId> declared;
        for (const auto local : function.parameterLocals)
          if (const auto found = function.localTypes.find(local.value); found != function.localTypes.end())
            declared.push_back(found->second);
        ngrt.externParameterTypes.emplace(function.source.value, std::move(declared));
        if (function.declaredResultType.has_value())
          ngrt.externResultTypes.emplace(function.source.value, *function.declaredResultType);
      }
    }
    // Tier 1 aggregate type declarations: every struct type used by any
    // function gets a QBE aggregate type (all members are 8-byte words in
    // this slice). Ascending type id order satisfies define-before-use.
    std::set<uint32_t> aggregateTypeIds;
    for (const auto &function : functions)
      for (size_t index = 0; index < function.typeDescriptors.size(); ++index)
        if (function.typeDescriptors[index].kind == TypeKind::Struct ||
            function.typeDescriptors[index].kind == TypeKind::Enum)
          aggregateTypeIds.insert(static_cast<uint32_t>(index));
    std::string result;
    for (const auto typeId : aggregateTypeIds)
    {
      size_t count = 0;
      bool isEnum = false;
      for (const auto &function : functions)
        if (typeId < function.typeDescriptors.size())
        {
          const auto kind = function.typeDescriptors[typeId].kind;
          if (kind == TypeKind::Struct || kind == TypeKind::Enum)
          {
            count = kind == TypeKind::Enum ? 2 : function.typeDescriptors[typeId].fieldNames.size();
            isEnum = kind == TypeKind::Enum;
            break;
          }
        }
      result += std::format("type :ng{}_{} = {{", isEnum ? 'e' : 's', typeId);
      if (isEnum)
      {
        result += " l, l";
      }
      else
      {
        for (const auto &function : functions)
          if (typeId < function.typeDescriptors.size() && function.typeDescriptors[typeId].kind == TypeKind::Struct)
          {
            for (size_t member = 0; member < function.typeDescriptors[typeId].elements.size(); ++member)
            {
              const auto memberType = function.typeDescriptors[typeId].elements[member];
              const auto suffixFor = [](TypeId type) -> const char * {
                if (type == typecheck::builtin::I8 || type == typecheck::builtin::U8) return "b";
                if (type == typecheck::builtin::I16 || type == typecheck::builtin::U16) return "h";
                if (type == typecheck::builtin::I32 || type == typecheck::builtin::U32) return "w";
                if (type == typecheck::builtin::F32) return "s";
                if (type == typecheck::builtin::F64) return "d";
                return "l";
              };
              result += std::format("{}{}", member == 0 ? " " : ", ", suffixFor(memberType));
            }
            break;
          }
      }
      result += " }\n";
    }
    for (const auto &function : functions)
    {
      if (function.nativeFunction || function.externC) continue;
      try
      {
        result += FunctionLowerer{function, names, ngrt}.run();
      }
      catch (const LoweringError &)
      {
        throw;
      }
      catch (const std::exception &error)
      {
        throw LoweringError(std::format("native lowering: in `{}`: {}", function.name, error.what()));
      }
      result += '\n';
    }
    // Emit each used ngrt helper (with its dependencies) exactly once, after
    // all functions.
    const std::unordered_map<std::string, std::vector<std::string>> helperDependencies = {
      {"arr_get", {"arr_addr"}},       {"ref_load", {"arr_get", "arr_addr"}},
      {"ref_addr", {"arr_addr"}},      {"copy_elements", {"arr_get", "arr_addr"}},
      {"arr_clone_strings", {"arr_get", "arr_addr", "str_clone"}}};
    std::set<std::string> helpersToEmit;
    std::function<void(const std::string &)> addHelper = [&](const std::string &name) {
      if (!helpersToEmit.insert(name).second) return;
      if (const auto found = helperDependencies.find(name); found != helperDependencies.end())
        for (const auto &dependency : found->second) addHelper(dependency);
    };
    for (const auto &name : ngrt.usedHelpers) addHelper(name);
    for (const auto &name : helpersToEmit) result += ngrtHelpers().at(name) + '\n';
    // Trait dispatch: per-(trait, concrete) vtable data plus a linear
    // key -> vtable lookup (keys are sorted for deterministic output).
    if (ngrt.needsTraitDispatch)
    {
      std::vector<uint64_t> keys;
      for (const auto &[key, methods] : vtables) keys.push_back(key);
      std::sort(keys.begin(), keys.end());
      for (const auto key : keys)
      {
        result += std::format("data $ngvtab_{} = {{ ", key);
        const auto &methods = vtables.at(key);
        for (size_t index = 0; index < methods.size(); ++index)
        {
          if (index != 0) result += ", ";
          result += std::format("l {}", qbeSymbol(names.at(methods[index]), methods[index]));
        }
        result += " }\n";
      }
      result += "function l $ngrt_trait_vtable(l %key) {\n@start\n";
      for (size_t index = 0; index < keys.size(); ++index)
      {
        const auto key = keys[index];
        result += std::format("\t%k{} =w ceql %key, {}\n", index, key);
        result += std::format("\tjnz %k{}, @hit{}, @next{}\n", index, index, index);
        result += std::format("@hit{}\n\tret $ngvtab_{}\n", index, key);
        result += std::format("@next{}\n", index);
      }
      result += "\thlt\n}\n";
    }
    return result;
  }
} // namespace NG::native
