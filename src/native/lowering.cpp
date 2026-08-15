// AI-generated code; reviewed for this repository's vNext rewrite.
#include "native/lowering.hpp"

#include <bit>
#include <cctype>
#include <format>
#include <optional>
#include <sstream>
#include <string_view>
#include <unordered_map>
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

    // Tier 0 scalar mapping: every NG integer (incl. bool — stored as i64
    // 0/1) is a QBE `l`, and every float (f32/f64; Value stores doubles) is a
    // QBE `d`. `None` marks unit-typed results, which produce no temp.
    enum class QType
    {
      Long,
      Double,
      None,
    };

    [[nodiscard]] auto qtype(TypeId type) -> QType
    {
      if (type == typecheck::builtin::Bool || typecheck::isIntegerBuiltin(type)) return QType::Long;
      if (typecheck::isFloatBuiltin(type)) return QType::Double;
      if (type == typecheck::builtin::Unit) return QType::None;
      throw LoweringError(std::format("native lowering (M1): type id {} has no QBE mapping yet", type.value));
    }

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

    class FunctionLowerer final
    {
    public:
      FunctionLowerer(const Function &function, const FunctionNames &names) : function_(function), names_(names) {}

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
      std::ostringstream out_;
      size_t tempCounter_{};
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

      /// Builds the QBE symbol for a function. `main` keeps its C-runtime
      /// name; everything else is `<sanitized-name>_<defid>` so that
      /// overloads, generic instances (`foo#3`), and same-named functions
      /// from different modules cannot collide.
      [[nodiscard]] auto symbolFor(hir::DefId id) -> std::string
      {
        std::string name;
        if (id == function_.source) name = function_.name;
        else if (const auto found = names_.find(id.value); found != names_.end()) name = found->second;
        else name = std::format("fn{}", id.value);
        if (name == "main") return "$main";
        for (auto &character : name)
          if (!std::isalnum(static_cast<unsigned char>(character)) && character != '_' && character != '.')
            character = '_';
        return std::format("${}_{}", name, id.value);
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
          type = qtype(found->second);
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
            return qtype(function_.valueTypes.at(terminator.arguments[0].value));
        }
        return std::nullopt;
      }

      [[nodiscard]] auto emit() -> std::string
      {
        returnType_ = findReturnType();
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
        const auto &predecessors = predecessors_[index];
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
          line(std::format("store{} {}, {}", suffix(type), phi, localSlots_.at(local)));
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
        case InstructionKind::BindLocal:
        {
          const auto source = operandTemp(*instruction.source);
          const auto type = localQTypes_.at(instruction.local->value);
          bindResult(instruction, type, std::format("copy {}", source));
          line(std::format("store{} {}, {}", suffix(type), valueTemps_.at(instruction.result.value),
                           localSlots_.at(instruction.local->value)));
          return;
        }
        case InstructionKind::AssignPlace:
        {
          // M1: only whole-local assignment through a local-rooted place.
          if (instruction.placeRootRef || !instruction.placeRootLocal || !instruction.placeSteps.empty())
            throw LoweringError(std::format("native lowering (M1): only plain local assignment is supported in `{}`",
                                            function_.name));
          const auto value = operandTemp(instruction.operands.back());
          const auto type = localQTypes_.at(instruction.placeRootLocal->value);
          line(std::format("store{} {}, {}", suffix(type), value, localSlots_.at(instruction.placeRootLocal->value)));
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
          bindResult(instruction, qtype(function_.valueTypes.at(instruction.result.value)),
                     std::format("copy {}", source));
          return;
        }
        case ExpressionKind::Prefix: lowerPrefix(instruction); return;
        case ExpressionKind::Binary: lowerBinary(instruction); return;
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
        std::string arguments;
        for (size_t index = 0; index < instruction.operands.size(); ++index)
        {
          const auto type = qtype(function_.valueTypes.at(instruction.operands[index].value));
          if (type == QType::None)
            throw LoweringError(std::format("native lowering (M2): unit call argument in `{}`", function_.name));
          if (index != 0) arguments += ", ";
          arguments += std::format("{} {}", suffix(type), operandTemp(instruction.operands[index]));
        }
        const auto resultType = qtype(function_.valueTypes.at(instruction.result.value));
        const auto symbol = symbolFor(*instruction.callTarget);
        if (resultType == QType::None) line(std::format("call {}({})", symbol, arguments));
        else
          line(std::format("{} ={} call {}({})", valueTemps_.at(instruction.result.value), suffix(resultType), symbol,
                           arguments));
      }

      void lowerPrefix(const Instruction &instruction)
      {
        const auto payload = instruction.payload;
        const auto operand = operandTemp(instruction.operands[0]);
        const auto operandType = function_.valueTypes.at(instruction.operands[0].value);
        const auto resultType = qtype(function_.valueTypes.at(instruction.result.value));
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
        if (payload == 19)
          throw LoweringError(std::format("native lowering (M1): range construction is not supported yet in `{}`",
                                          function_.name));
        const auto left = operandTemp(instruction.operands[0]);
        const auto right = operandTemp(instruction.operands[1]);
        const auto leftType = function_.valueTypes.at(instruction.operands[0].value);
        const auto rightType = function_.valueTypes.at(instruction.operands[1].value);

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
          if (terminator.arguments.empty()) line("ret");
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
    return FunctionLowerer{function, names}.run();
  }

  auto lowerModule(const std::vector<Function> &functions) -> std::string
  {
    FunctionNames names;
    for (const auto &function : functions) names.emplace(function.source.value, function.name);
    std::string result;
    for (const auto &function : functions)
    {
      if (function.nativeFunction) continue;
      result += lower(function, names);
      result += '\n';
    }
    return result;
  }
} // namespace NG::native
