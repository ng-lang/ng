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
    // QBE `d`. `None` marks unit-typed results, which produce no temp.
    enum class QType
    {
      Long,
      Double,
      None,
    };

    [[nodiscard]] auto suffix(QType type) -> const char *
    {
      switch (type)
      {
      case QType::Long: return "l";
      case QType::Double: return "d";
      case QType::None: return "";
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
         // A reference value is { l root-slot, l count, { l kind, l payload }[] }.
         // Loading walks from the root slot's current value, mirroring the
         // VM's (cell + path) view semantics (rebinding the root is observed).
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
         "\t%zero =w ceql %kind, 0\n"
         "\t%payloadp =l add %kindp, 8\n"
         "\t%payload =l loadl %payloadp\n"
         "\tjnz %zero, @member, @index\n"
         "@member\n"
         "\t%moff =l mul %payload, 8\n"
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
         // Walks to the address of the referenced place; a plain local
         // reference addresses the slot itself (stores must not target the
         // loaded value).
         "function l $ngrt_ref_addr(l %ref) {\n"         "@start\n"
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
         "\t%zero =w ceql %kind, 0\n"
         "\t%payloadp =l add %kindp, 8\n"
         "\t%payload =l loadl %payloadp\n"
         "\tjnz %zero, @member, @index\n"
         "@member\n"
         "\t%moff =l mul %payload, 8\n"
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
      std::vector<uint32_t> slotOrder_;
      std::unordered_map<uint32_t, std::string> localSlots_;
      std::unordered_map<uint32_t, QType> localQTypes_;
      std::unordered_map<uint32_t, std::string> valueTemps_;
      std::vector<std::string> paramTemps_;
      std::optional<QType> returnType_;

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
        case TypeKind::Struct:
        case TypeKind::Enum:
        case TypeKind::TraitReference:
          return QType::Long;
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
        slotOrder_.push_back(local);
      }

      void collectLocals()
      {
        paramTemps_.reserve(function_.parameterLocals.size());
        for (size_t index = 0; index < function_.parameterLocals.size(); ++index)
        {
          ensureSlot(function_.parameterLocals[index].value);
          paramTemps_.push_back(std::format("%p{}", index));
        }
        for (const auto &block : function_.blocks)
          for (const auto local : block.parameterLocals) ensureSlot(local.value);
        for (const auto &block : function_.blocks)
        {
          for (const auto &instruction : block.instructions)
          {
            if (instruction.local) ensureSlot(instruction.local->value);
            if (instruction.kind == InstructionKind::Evaluate &&
                instruction.expressionKind == ExpressionKind::ResolvedName)
              ensureSlot(static_cast<uint32_t>(instruction.payload));
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
            return qtypeOf(function_.valueTypes.at(terminator.arguments[0].value));
        }
        return std::nullopt;
      }

      [[nodiscard]] auto emit() -> std::string
      {
        returnType_ = findReturnType();
        // `main` is the C entry point: a unit return becomes an i64 exit code.
        if (function_.name == "main" && (!returnType_ || *returnType_ == QType::None)) returnType_ = QType::Long;
        out_ << (function_.name == "main" ? "export function" : "function");
        if (returnType_ && *returnType_ != QType::None) out_ << ' ' << suffix(*returnType_);
        out_ << ' ' << symbolFor(function_.source) << '(';
        for (size_t index = 0; index < paramTemps_.size(); ++index)
        {
          if (index != 0) out_ << ", ";
          out_ << suffix(localQTypes_.at(function_.parameterLocals[index].value)) << ' ' << paramTemps_[index];
        }
        out_ << ") {\n";
        // One-shot entry logic: frame slots and parameter stores live under
        // `@start` so tail recursion never re-runs them.
        out_ << "@start\n";
        for (const auto local : slotOrder_) line(std::format("{} =l alloc8 8", localSlots_.at(local)));
        for (size_t param = 0; param < paramTemps_.size(); ++param)
        {
          const auto local = function_.parameterLocals[param].value;
          line(std::format("store{} {}, {}", suffix(localQTypes_.at(local)), paramTemps_[param],
                           localSlots_.at(local)));
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
          line(std::format("store{} {}, {}", suffix(localQTypes_.at(local)), phiTemps[param],
                           localSlots_.at(local)));
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
          const auto type = localQTypes_.at(instruction.local->value);
          bindResult(instruction, type, std::format("copy {}", source));
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
            const auto type = qtypeOf(function_.valueTypes.at(instruction.operands[index].value));
            arguments += std::format(", {} {}", suffix(type), operandTemp(instruction.operands[index]));
          }
          const auto resultType = qtypeOf(function_.valueTypes.at(instruction.result.value));
          if (resultType == QType::None) line(std::format("call {}({})", function, arguments));
          else
            line(std::format("{} ={} call {}({})", valueTemps_.at(instruction.result.value), suffix(resultType),
                             function, arguments));
          return;
        }
        case InstructionKind::LoadRef:
        {
          useHelper("ref_load");
          const auto reference = operandTemp(instruction.operands[0]);
          const auto loaded = fresh();
          line(std::format("{} =l call $ngrt_ref_load(l {})", loaded, reference));
          const auto resultType = qtypeOf(function_.valueTypes.at(instruction.result.value));
          bindResult(instruction, resultType, std::format("copy {}", castFromSlot(loaded, resultType)));
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
              line(std::format("store{} {}, {}", suffix(valueType), value, address));
              return;
            }
            // Ref-rooted place with a path (`(*self).field := ...`): the
            // ref's prefix steps walk to the containing aggregate value,
            // then the static path steps walk to the target address.
            useHelper("ref_load");
            const auto current = fresh();
            line(std::format("{} =l call $ngrt_ref_load(l {})", current, reference));
            std::string address = current;
            for (const auto &step : instruction.placeSteps)
            {
              const auto next = fresh();
              if (step.kind == flowir::PlaceStep::Kind::Member)
              {
                line(std::format("{} =l add {}, {}", next, address, step.field * 8));
              }
              else
              {
                useHelper("arr_addr");
                line(std::format("{} =l call $ngrt_arr_addr(l {}, l {})", next, address,
                                 operandTemp(step.indexValue)));
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
          const auto rootSlot = localSlots_.at(instruction.placeRootLocal->value);
          if (instruction.placeSteps.empty())
          {
            line(std::format("store{} {}, {}", suffix(valueType), value, rootSlot));
            return;
          }
          const auto current = fresh();
          line(std::format("{} =l loadl {}", current, rootSlot));
          std::string address = current;
          for (const auto &step : instruction.placeSteps)
          {
            const auto next = fresh();
            if (step.kind == flowir::PlaceStep::Kind::Member)
            {
              line(std::format("{} =l add {}, {}", next, address, step.field * 8));
            }
            else
            {
              useHelper("arr_addr");
              line(std::format("{} =l call $ngrt_arr_addr(l {}, l {})", next, address, operandTemp(step.indexValue)));
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
          bindResult(instruction, QType::Long, std::format("copy {}", payload));
          return;
        case ExpressionKind::FloatLiteral:
          bindResult(instruction, QType::Double, std::format("copy {}", doubleConstant(std::bit_cast<double>(payload))));
          return;
        case ExpressionKind::StringLiteral:
        {
          const auto name = std::format("$ngstr_{}_{}", function_.source.value, dataItems_.size());
          dataItems_.push_back(std::format("data {} = {{ l {}, b \"{}\", b 0 }}", name, instruction.text.size(),
                                            escapeQbeString(instruction.text)));
          bindResult(instruction, QType::Long, std::format("copy {}", name));
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
          line(std::format("{} =l call $malloc(l 16)", pointer));
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
          bindResult(instruction, QType::Long, std::format("copy {}", pointer));
          return;
        }
        case ExpressionKind::StructLiteral:
          lowerStructLiteral(instruction);
          return;
        case ExpressionKind::Member:
        {
          const auto receiver = operandTemp(instruction.operands[0]);
          const auto field = static_cast<size_t>(payload);
          const auto address = fresh();
          line(std::format("{} =l add {}, {}", address, receiver, field * 8));
          const auto loaded = fresh();
          line(std::format("{} =l loadl {}", loaded, address));
          const auto resultType = qtypeOf(function_.valueTypes.at(instruction.result.value));
          bindResult(instruction, resultType, std::format("copy {}", castFromSlot(loaded, resultType)));
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
          const auto type = localQTypes_.at(local);
          bindResult(instruction, type, std::format("load{} {}", suffix(type), localSlots_.at(local)));
          return;
        }
        case ExpressionKind::Grouped:
        {
          const auto source = operandTemp(instruction.operands[0]);
          bindResult(instruction, qtypeOf(function_.valueTypes.at(instruction.result.value)),
                     std::format("copy {}", source));
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
        if (ngrt_.nativeDefIds.contains(instruction.callTarget->value))
          throw LoweringError(std::format(
              "native function call is not available in the native tier yet (`{}`; native shims arrive in M5)",
              names_.contains(instruction.callTarget->value) ? names_.at(instruction.callTarget->value) : "unknown"));
        std::string arguments;
        for (size_t index = 0; index < instruction.operands.size(); ++index)
        {
          const auto type = qtypeOf(function_.valueTypes.at(instruction.operands[index].value));
          if (type == QType::None)
            throw LoweringError(std::format("native lowering (M2): unit call argument in `{}`", function_.name));
          if (index != 0) arguments += ", ";
          arguments += std::format("{} {}", suffix(type), operandTemp(instruction.operands[index]));
        }
        const auto resultType = qtypeOf(function_.valueTypes.at(instruction.result.value));
        const auto symbol = symbolFor(*instruction.callTarget);
        if (resultType == QType::None) line(std::format("call {}({})", symbol, arguments));
        else
          line(std::format("{} ={} call {}({})", valueTemps_.at(instruction.result.value), suffix(resultType), symbol,
                           arguments));
      }

      /// Struct literal: a malloc'd object of 8-byte fields (Tier 0 layout:
      /// offset = 8 * field ordinal; no header — the field count is static).
      void lowerStructLiteral(const Instruction &instruction)
      {
        const auto typeId = function_.valueTypes.at(instruction.result.value);
        const auto &descriptor = function_.typeDescriptors[typeId.value];
        const size_t count = descriptor.fieldNames.size();
        if (count != instruction.operands.size())
          throw LoweringError(std::format("native lowering (M2): struct literal arity mismatch in `{}`", function_.name));
        const auto pointer = fresh();
        line(std::format("{} =l call $malloc(l {})", pointer, count * 8));
        for (size_t index = 0; index < count; ++index)
        {
          const auto elementType = qtypeOf(descriptor.elements[index]);
          const auto element = castForSlot(operandTemp(instruction.operands[index]), elementType);
          const auto address = fresh();
          line(std::format("{} =l add {}, {}", address, pointer, index * 8));
          line(std::format("storel {}, {}", element, address));
        }
        bindResult(instruction, QType::Long, std::format("copy {}", pointer));
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
        const auto rootSlot = localSlots_.at(rootLocal);
        const size_t count = steps.size();
        const auto pointer = fresh();
        line(std::format("{} =l call $malloc(l {})", pointer, 16 + count * 16));
        line(std::format("storel {}, {}", rootSlot, pointer));
        const auto countAddress = fresh();
        line(std::format("{} =l add {}, 8", countAddress, pointer));
        line(std::format("storel {}, {}", count, countAddress));
        for (size_t index = 0; index < count; ++index)
        {
          const auto &step = steps[index];
          const auto stepAddress = fresh();
          line(std::format("{} =l add {}, {}", stepAddress, pointer, 16 + index * 16));
          const auto payloadAddress = fresh();
          line(std::format("{} =l add {}, 8", payloadAddress, stepAddress));
          if (step.kind == flowir::PlaceStep::Kind::Member)
          {
            line(std::format("storel 0, {}", stepAddress));
            line(std::format("storel {}, {}", step.field, payloadAddress));
          }
          else
          {
            line(std::format("storel 1, {}", stepAddress));
            line(std::format("storel {}, {}", operandTemp(step.indexValue), payloadAddress));
          }
        }
        return pointer;
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
          return;
        }
        if (payload == 2)
        {
          bindResult(instruction, resultType, std::format("neg {}", operand));
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
        case 1: bindResult(instruction, QType::Long, std::format("add {}, {}", left, right)); return;
        case 2: bindResult(instruction, QType::Long, std::format("sub {}, {}", left, right)); return;
        case 3: bindResult(instruction, QType::Long, std::format("mul {}, {}", left, right)); return;
        case 4: bindResult(instruction, QType::Long, std::format("div {}, {}", left, right)); return;
        case 5: bindResult(instruction, QType::Long, std::format("rem {}, {}", left, right)); return;
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
        case 17: bindResult(instruction, QType::Long, std::format("shl {}, {}", left, right)); return;
        case 18: bindResult(instruction, QType::Long, std::format("shr {}, {}", left, right)); return;
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
          if (terminator.arguments.empty()) line(function_.name == "main" ? "ret 0" : "ret");
          else line(std::format("ret {}", operandTemp(terminator.arguments[0])));
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
      if (function.nativeFunction) ngrt.nativeDefIds.insert(function.source.value);
    std::string result;
    for (const auto &function : functions)
    {
      if (function.nativeFunction) continue;
      result += FunctionLowerer{function, names, ngrt}.run();
      result += '\n';
    }
    // Emit each used ngrt helper (with its dependencies) exactly once, after
    // all functions.
    const std::unordered_map<std::string, std::vector<std::string>> helperDependencies = {
      {"arr_get", {"arr_addr"}},          {"ref_load", {"arr_get", "arr_addr"}},
      {"ref_addr", {"arr_addr"}},         {"copy_elements", {"arr_get", "arr_addr"}}};
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
