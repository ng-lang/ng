// AI-generated code; reviewed for this repository's vNext rewrite.
// ARCH-2: internal header for the native lowering split. FunctionLowerer and
// its shared context live here; method definitions are distributed across
// lowering_scaffold.cpp / lowering_expr.cpp / lowering_aggregate.cpp /
// lowering_call.cpp. Not part of the public API.
#pragma once

#include "flowir.hpp"
#include "native/lowering.hpp"
#include "typecheck.hpp"

#include <algorithm>
#include <format>
#include <optional>
#include <set>
#include <sstream>
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

namespace NG::native
{
  namespace detail
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

    enum class QType
    {
      Long,
      Double,
      None,
      Aggregate,
    };

    [[nodiscard]] inline auto suffix(QType type) -> const char *
    {
      switch (type)
      {
      case QType::Long:
        return "l";
      case QType::Double:
        return "d";
      case QType::None:
        return "";
      case QType::Aggregate:
        return "l"; // call sites pass pointers at IL level
      }
      return "";
    }

    [[nodiscard]] inline auto isFloat(TypeId type) -> bool
    {
      return typecheck::isFloatBuiltin(type);
    }

    /// QBE double literal: `d_0.5`. A bare integer formatting is given a `.0`
    /// so the parser reads it as a float constant.
    [[nodiscard]] inline auto doubleConstant(double value) -> std::string
    {
      std::string text = std::format("{}", value);
      if (text.find_first_of(".en") == std::string::npos)
        text += ".0";
      return "d_" + text;
    }

    /// Escapes a raw byte string for a QBE `b "..."` data item. QBE's string
    /// lexer copies characters verbatim; only `\` and `"` carry escape
    /// meaning.
    [[nodiscard]] inline auto escapeQbeString(std::string_view text) -> std::string
    {
      std::string escaped;
      escaped.reserve(text.size());
      for (const char character : text)
      {
        if (character == '\\' || character == '"')
          escaped += '\\';
        escaped += character;
      }
      return escaped;
    }

    /// Per-module native/extern bookkeeping for shim selection and ABI data.
    struct NgrtContext
    {
      bool needsTraitDispatch{};
      /// DefIds of `native fun` placeholders: calling one from native code is
      /// rejected until the declared-descriptor shims land (M5).
      std::unordered_set<uint32_t> nativeDefIds;
      /// R9 first slice: DefId -> declared parameter types of `native fun`
      /// declarations (arity validation and shim selection).
      std::unordered_map<uint32_t, std::vector<TypeId>> nativeSignatures;
      /// R9 first slice: DefId -> declared result type of `native fun`
      /// declarations (QBE result classification for libngrt calls).
      std::unordered_map<uint32_t, TypeId> nativeResultTypes;
      /// M5: DefId -> per-parameter ownership descriptors for native shim
      /// lowering (absent entries default to `Copy`).
      std::unordered_map<uint32_t, std::vector<Ownership>> nativeOwnerships;
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
    [[nodiscard]] inline auto qbeSymbol(const std::string &name, uint32_t defId) -> std::string
    {
      if (name == "main")
        return "$main";
      std::string cleaned = name;
      for (auto &character : cleaned)
        if (!std::isalnum(static_cast<unsigned char>(character)) && character != '_' && character != '.')
          character = '_';
      return std::format("${}_{}", cleaned, defId);
    }

    // Value representations (Tier 0): aggregate values are 8-byte pointers
    // into malloc'd objects (or QBE data items for string literals):
    //   string: { l len, b data[len], b 0 }
    //   array/tuple: { l len, l cap, l-or-d elements[cap] }
    //   range: { l start, l end }
    // All aggregate slots hold `l`; double elements are bit-cast at the
    // boundary. Runtime helpers live in src/native/helpers/*.ssa and are
    // assembled into libngrt_helpers.a at build time (ARCH-1); they call
    // libc (malloc/memcpy/memcmp) through QBE's C ABI.

    class FunctionLowerer final
    {
    public:
      FunctionLowerer(const Function &function, const FunctionNames &names, NgrtContext &ngrt)
          : function_(function), names_(names), ngrt_(ngrt)
      {
      }

      [[nodiscard]] auto run() -> std::string;

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
          if (type == typecheck::builtin::String || type == typecheck::builtin::Bool ||
              typecheck::isIntegerBuiltin(type))
            return QType::Long;
          if (typecheck::isFloatBuiltin(type))
            return QType::Double;
          if (type == typecheck::builtin::Unit)
            return QType::None;
          break;
        case TypeKind::DynamicArray:
        case TypeKind::FixedArray:
        case TypeKind::Tuple:
        case TypeKind::Range:
        case TypeKind::Reference:
        case TypeKind::TraitReference:
        case TypeKind::Opaque:
        case TypeKind::Union:
        /// INC-14 S1: closure literals are inert handles; their synthesized
        /// code functions receive call parameters directly.
        case TypeKind::Closure:
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
        default:
          break;
        }
        throw LoweringError(std::format("native lowering (M2): type `{}` has no QBE mapping yet", descriptor.name));
      }

      /// Aggregate slots hold `l`; doubles are bit-cast when stored/loaded.
      [[nodiscard]] auto castForSlot(const std::string &temp, QType type) -> std::string
      {
        if (type != QType::Double)
          return temp;
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
      [[nodiscard]] auto unionMemberFor(TypeId unionType, TypeId memberType) -> int64_t;

      /// Wraps a produced payload temp into a tagged union box
      /// { l memberIndex, l payloadWord } and binds the instruction result.
      void bindUnionBox(const Instruction &instruction, TypeId unionType, int64_t member,
                        const std::string &payloadTemp, QType payloadType);

      /// Reverses castForSlot after a load from an aggregate slot.
      [[nodiscard]] auto castFromSlot(const std::string &temp, QType type) -> std::string
      {
        if (type != QType::Double)
          return temp;
        const auto cast = fresh();
        line(std::format("{} =d cast {}", cast, temp));
        return cast;
      }

      [[nodiscard]] auto symbolFor(hir::DefId id) -> std::string;

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
          if (terminator.kind == TerminatorKind::TailRecur)
            hasTailRecur_ = true;
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

      void ensureSlot(uint32_t local);

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
      [[nodiscard]] auto memberSuffix(TypeId type) -> std::string;

      [[nodiscard]] auto memberLayout(TypeId type) -> std::pair<size_t, size_t>;

      /// Aligned byte offset of a struct field.
      [[nodiscard]] auto fieldOffset(TypeId structType, size_t field) -> size_t;

      [[nodiscard]] auto aggregateSize(TypeId type) -> size_t;

      /// Stores a field value (a QBE temp in the value's QBE type) into an
      /// aggregate at the given address; f32 values truncate to `s`.
      void storeField(const std::string &address, const std::string &value, TypeId fieldType);

      /// Loads a field value from an aggregate address, returning a QBE temp
      /// in the value's QBE type (f32 loads extend back to `d`).
      [[nodiscard]] auto loadField(const std::string &address, TypeId fieldType) -> std::string;

      /// Field-wise aggregate copy with the real layout: structs copy every
      /// field at its aligned offset; enums copy their two words (tag +
      /// payload — payload words are never mutated in place, so sharing a
      /// tuple payload is unobservable). (QBE's `blit` is not usable here:
      /// its optimizer does not model blit's memory definitions, folding the
      /// copied data to an uninitialized sentinel.)
      void emitAggregateCopy(const std::string &destination, const std::string &source, TypeId type);

      /// The QBE type spelling for a value type in signature/call positions:
      /// aggregates name their QBE type (passed by pointer at IL level and
      /// classified by the backend ABI).
      [[nodiscard]] auto qbeSuffixFor(TypeId type) -> std::string;

      /// The QBE type spelling for an `extern "C"` scalar boundary, derived
      /// from the DECLARED C signature type (NG carries every integer in an
      /// `l` temp, so the value-type mapping cannot be reused): sub-word
      /// integers keep their sign/zero-extension class (sb/ub/sh/uh),
      /// i32/u32/bool are C `int` (`w`), f32 is `s`, f64 `d`, and i64/u64
      /// are `l`.
      [[nodiscard]] auto externSuffixFor(TypeId type) -> std::string;

      /// The QBE type of a local; SSA locals skip slot creation, so the type
      /// falls back to the declared local type.
      [[nodiscard]] auto localQType(uint32_t local) -> QType;

      /// Loads a local's value; cell locals use the slot -> cell indirection
      /// (two loads).
      [[nodiscard]] auto loadLocal(uint32_t local) -> std::string;

      /// Allocates a fresh cell for a cell local and stores a value in it;
      /// the slot is repointed to the new cell (BindLocal semantics: the VM
      /// creates a fresh shared cell on every bind, so outstanding refs keep
      /// observing the old cell — a snapshot, not a mutation).
      void bindFreshCell(uint32_t local, const std::string &value);

      void collectLocals();

      /// Assigns a stable QBE temp name to every FlowIR ValueId up front.
      /// QBE accepts phi arguments that are defined textually later (its own
      /// loop tests rely on this), so backedges can reference temps whose
      /// defining instruction is emitted in a later block.
      void preassignValueTemps();

      [[nodiscard]] auto findReturnType() -> std::optional<QType>;

      [[nodiscard]] auto emit() -> std::string;

      void emitBlock(size_t index);

      void bindResult(const Instruction &instruction, QType type, std::string_view text)
      {
        line(std::format("{} ={} {}", valueTemps_.at(instruction.result.value), suffix(type), text));
      }

      [[nodiscard]] auto operandTemp(ValueId value) -> const std::string & { return valueTemps_.at(value.value); }

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
      void emitCheckedOp(const Instruction &instruction, std::string_view helper, std::string_view arguments);

      /// D-008 per-width range check on an integer result (i8-i32, u8-u32).
      void applyWidthCheck(const Instruction &instruction);

      void lowerInstruction(const Instruction &instruction);

      void lowerEvaluate(const Instruction &instruction);

      /// Direct call: `%r =T call $sym(T %a, ...)`. A unit result emits a
      /// bare `call`. Native callees and variadic (tuple-packed) calls are
      /// deferred to the descriptor/shim work of M5.
      void lowerCall(const Instruction &instruction);

      /// B3 first slice: `extern "C"` call site — a direct QBE call to the C
      /// symbol. Argument and result types come from the DECLARED C signature
      /// (NG call sites widen sub-word values to i64, which would corrupt the
      /// ABI): sub-word integers keep sb/ub/sh/uh and are width-checked so C
      /// never sees silently truncated values, f32 values truncate to `s`,
      /// and repr(C) aggregates are passed by pointer at IL level and
      /// classified by the backend. The declared result is then widened into
      /// the NG call-site temp, and aggregate results are copied from QBE's
      /// return area onto the heap (NG aggregate values are heap pointers).
      void lowerExternCall(const Instruction &instruction);

      /// Struct literal: a stack slot (alloc8) with 8-byte word fields
      /// (Tier 1 layout: offset = 8 * field ordinal).
      void lowerStructLiteral(const Instruction &instruction);

      /// Tuple splice: flattens tuple operands into one tuple object. Tuple
      /// vs plain is decided statically from the operand type descriptors,
      /// and the runtime element count drives the allocation.
      void lowerTupleSplice(const Instruction &instruction);

      /// Builds a ref object { l root-slot, l count, { l kind, l payload }[] }
      /// on the heap; shared by MakeRef and MakeTraitView.
      [[nodiscard]] auto emitRefObject(uint32_t rootLocal, const std::vector<flowir::PlaceStep> &steps) -> std::string;

      /// True for aggregate kinds whose copy-first semantics require a deep
      /// copy at bind/load sites (the VM deep-copies all of these).
      [[nodiscard]] auto needsClone(TypeId type) -> bool;

      /// Emits a deep copy of a Tier 0 aggregate value (static type-driven:
      /// structs clone field-by-field recursively, tuples unroll statically,
      /// arrays pick a per-element-kind helper, strings use $ngrt_str_clone).
      [[nodiscard]] auto emitClone(const std::string &source, TypeId type) -> std::string;

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
          -> std::vector<NormalizedStep>;

      /// Array/tuple literal: allocates a { len, cap, elements } object and
      /// stores the elements (doubles are bit-cast into `l` slots).
      void lowerAggregateLiteral(const Instruction &instruction);

      /// AOT calls to libngrt. The C implementation lives in libngrt and is
      /// named `ngrt_<native fun name>`; print is the only overload-sensitive
      /// native and picks a type-suffixed symbol from the declared parameter
      /// type. This removes the old name-keyed shim table.
      void lowerNativeCall(const Instruction &instruction);

      /// Union equality/inequality against a member value: the tag must match
      /// the member's index and the payload must equal the operand. Ordering
      /// compares the integer payload directly (the VM throws for non-integer
      /// payloads; the tagless native form silently compares raw words — a
      /// documented Tier 0 deviation).
      void lowerUnionCompare(const Instruction &instruction, TypeId unionType, bool leftIsUnion);

      void lowerPrefix(const Instruction &instruction);

      /// Converts an integer temp to double for mixed numeric promotion
      /// (VM `asNumber` semantics: any float operand promotes the whole op).
      [[nodiscard]] auto promoteToDouble(const std::string &temp, TypeId type) -> std::string;

      void lowerBinary(const Instruction &instruction);

      void lowerTerminator(const Terminator &terminator, size_t blockIndex);
    };

  } // namespace detail
} // namespace NG::native
