// AI-generated code; reviewed for this repository's vNext rewrite.
// ARCH-2: Call lowering: NG calls, extern "C" calls, and native AOT shims.
#include "function_lowerer.hpp"

namespace NG::native::detail
{

  [[nodiscard]] auto FunctionLowerer::qbeSuffixFor(TypeId type) -> std::string
  {
    const auto qtype = qtypeOf(type);
    if (qtype == QType::Aggregate)
      return aggregateName(type);
    return suffix(qtype);
  }
  [[nodiscard]] auto FunctionLowerer::externSuffixFor(TypeId type) -> std::string
  {
    if (type == typecheck::builtin::I8)
      return "sb";
    if (type == typecheck::builtin::U8)
      return "ub";
    if (type == typecheck::builtin::I16)
      return "sh";
    if (type == typecheck::builtin::U16)
      return "uh";
    if (type == typecheck::builtin::I32 || type == typecheck::builtin::U32 || type == typecheck::builtin::Bool)
      return "w";
    if (type == typecheck::builtin::F32)
      return "s";
    if (type == typecheck::builtin::F64)
      return "d";
    return "l";
  }
  void FunctionLowerer::lowerCall(const Instruction &instruction)
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
      if (index != 0)
        arguments += ", ";
      arguments += std::format("{} {}", qbeSuffixFor(typeId), operandTemp(instruction.operands[index]));
    }
    const auto resultId = function_.valueTypes.at(instruction.result.value);
    const auto resultType = qtypeOf(resultId);
    const auto symbol = symbolFor(*instruction.callTarget);
    if (resultType == QType::None)
      line(std::format("call {}({})", symbol, arguments));
    else
      line(std::format("{} ={} call {}({})", valueTemps_.at(instruction.result.value),
                       resultType == QType::Aggregate ? "l" : qbeSuffixFor(resultId), symbol, arguments));
    if (isUnionType(resultId))
      unionBoxedValues_.emplace(instruction.result.value, resultId);
  }
  void FunctionLowerer::lowerExternCall(const Instruction &instruction)
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
    const auto widthCheck = [](TypeId type) -> const char *
    {
      switch (type.value)
      {
      case typecheck::builtin::I8.value:
        return "check_w_i8";
      case typecheck::builtin::I16.value:
        return "check_w_i16";
      case typecheck::builtin::I32.value:
        return "check_w_i32";
      case typecheck::builtin::U8.value:
        return "check_w_u8";
      case typecheck::builtin::U16.value:
        return "check_w_u16";
      case typecheck::builtin::U32.value:
        return "check_w_u32";
      default:
        return nullptr;
      }
    };
    std::string arguments;
    for (size_t index = 0; index < instruction.operands.size(); ++index)
    {
      const auto typeId = declared(index);
      std::string value = operandTemp(instruction.operands[index]);
      if (const char *check = widthCheck(typeId); check != nullptr)
      {
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
      if (index != 0)
        arguments += ", ";
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
  void FunctionLowerer::lowerNativeCall(const Instruction &instruction)
  {
    const auto defId = instruction.callTarget->value;
    const auto name = names_.at(defId);
    // R9 declared signature: validate arity at lowering time.
    if (const auto found = ngrt_.nativeSignatures.find(defId);
        found != ngrt_.nativeSignatures.end() && found->second.size() != instruction.operands.size())
      throw LoweringError(std::format("native `{}` expects {} argument(s), got {} in `{}`", name, found->second.size(),
                                      instruction.operands.size(), function_.name));
    const auto argType = [&](size_t index) -> TypeId
    { return function_.valueTypes.at(instruction.operands[index].value); };
    // The declared parameter type is authoritative for shim selection
    // (e.g. print's unsigned/boolean variants).
    const auto declaredType = [&](size_t index) -> TypeId
    {
      if (const auto found = ngrt_.nativeSignatures.find(defId);
          found != ngrt_.nativeSignatures.end() && index < found->second.size())
        return found->second[index];
      return argType(index);
    };
    // M5 ownership descriptors: by default a native parameter is `Copy`
    // (copy-first D-015), so aggregate arguments are deep-copied before
    // the C callee sees them.
    const auto arg = [&](size_t index) -> std::string
    {
      const auto value = operandTemp(instruction.operands[index]);
      auto ownership = Ownership::Copy;
      if (const auto found = ngrt_.nativeOwnerships.find(defId);
          found != ngrt_.nativeOwnerships.end() && index < found->second.size())
        ownership = found->second[index];
      const auto type = argType(index);
      if (ownership == Ownership::Copy && needsClone(type))
        return emitClone(value, type);
      return value;
    };
    const auto argSuffix = [&](TypeId type) -> const char *
    {
      if (typecheck::isFloatBuiltin(type))
        return "d";
      return "l";
    };
    std::string symbol;
    if (name == "print")
    {
      const auto type = declaredType(0);
      if (type == typecheck::builtin::String)
        symbol = "$ngrt_print_str";
      else if (type == typecheck::builtin::Bool)
        symbol = "$ngrt_print_bool";
      else if (typecheck::isUnsignedIntegerBuiltin(type))
        symbol = "$ngrt_print_u64";
      else if (isFloat(type))
        symbol = "$ngrt_print_f64";
      else
        symbol = "$ngrt_print_i64";
    }
        else if (name == "toFloat")
        {
          // INC-6: the C side cannot overload, so the declared argument type
          // picks the sign class of the conversion.
          symbol = typecheck::isUnsignedIntegerBuiltin(declaredType(0)) ? "$ngrt_toFloat_u64" : "$ngrt_toFloat_i64";
        }
        else if (name == "toString")
        {
          // INC-7: same dispatch pattern as print — narrow signed widths are
          // exact in the i64 slot, any float prints through the f64 path.
          const auto type = declaredType(0);
          if (typecheck::isFloatBuiltin(type))
            symbol = "$ngrt_toString_f64";
          else if (type == typecheck::builtin::Bool)
            symbol = "$ngrt_toString_bool";
          else if (typecheck::isUnsignedIntegerBuiltin(type))
            symbol = "$ngrt_toString_u64";
          else
            symbol = "$ngrt_toString_i64";
        }
        else
        {
          symbol = "$ngrt_" + name;
        }

    std::string arguments;
    for (size_t index = 0; index < instruction.operands.size(); ++index)
    {
      if (index != 0)
        arguments += ", ";
      arguments += std::format("{} {}", argSuffix(declaredType(index)), arg(index));
    }

    const auto siteResult = function_.valueTypes.at(instruction.result.value);
    const auto declaredResult = [&]() -> TypeId
    {
      if (const auto found = ngrt_.nativeResultTypes.find(defId); found != ngrt_.nativeResultTypes.end())
        return found->second;
      return siteResult;
    }();
    const auto resultQType = qtypeOf(declaredResult);
    if (resultQType == QType::Double)
    {
      const auto target = qtypeOf(siteResult) == QType::None ? fresh() : valueTemps_.at(instruction.result.value);
      line(std::format("{} =d call {}({})", target, symbol, arguments));
      return;
    }
    // All other libngrt functions return an i64-sized word (integer,
    // bool, pointer, or unit-as-ignored-word).
    const auto target = qtypeOf(siteResult) == QType::None ? fresh() : valueTemps_.at(instruction.result.value);
    line(std::format("{} =l call {}({})", target, symbol, arguments));
  }

} // namespace NG::native::detail
