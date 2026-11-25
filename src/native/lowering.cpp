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

#include "function_lowerer.hpp"
namespace NG::native
{
  namespace
  {
    using namespace detail;
    using flowir::Function;
    using flowir::Instruction;
    using flowir::InstructionKind;
    using flowir::Terminator;
    using flowir::TerminatorKind;
    using flowir::ValueId;
    using hir::ExpressionKind;
    using typecheck::TypeId;
    using typecheck::TypeKind;
  } // namespace

  auto lower(const Function &function, const FunctionNames &names) -> std::string
  {
    NgrtContext ngrt;
    return FunctionLowerer{function, names, ngrt}.run();
  }

  namespace
  {
    /// INC-29: emits a C-visible wrapper for `export extern "C"` definitions.
    /// The NG body keeps its normal i64/f64 slots and mangled symbol; the
    /// wrapper translates the C ABI boundary (sub-word extension, f32
    /// widening/narrowing) and forwards to it.
    [[nodiscard]] auto emitCAbiThunk(const Function &function, const std::string &symbol, bool exported) -> std::string
    {
      const auto &declared = function.declaredResultType.value_or(typecheck::builtin::Unit);
      const bool unitResult = declared == typecheck::builtin::Unit;
      const std::string resultSuffix = unitResult ? "" : cAbiSuffix(declared);
      std::string out = std::format("{}function{} ${}(", exported ? "export " : "",
                                    resultSuffix.empty() ? "" : " " + resultSuffix, symbol);
      std::vector<std::string> forwarded;
      std::vector<std::string> conversions;
      size_t temporary = 0;
      for (size_t index = 0; index < function.parameterLocals.size(); ++index)
      {
        const auto type = function.localTypes.at(function.parameterLocals[index].value);
        const std::string cSuffix = cAbiSuffix(type);
        const std::string param = std::format("%p{}", index);
        if (index != 0)
          out += ", ";
        out += std::format("{} {}", cSuffix, param);
        std::string value = param;
        if (cSuffix == "sb" || cSuffix == "ub" || cSuffix == "sh" || cSuffix == "uh" || cSuffix == "w")
        {
          const bool isUnsigned = typecheck::isUnsignedIntegerBuiltin(type);
          const std::string op = cSuffix == "sb"   ? "extsb"
                                 : cSuffix == "ub" ? "extub"
                                 : cSuffix == "sh" ? (isUnsigned ? "extuh" : "extsh")
                                 : cSuffix == "uh" ? "extuh"
                                 : (isUnsigned ? "extuw" : "extsw");
          const std::string wide = std::format("%c{}", temporary++);
          conversions.push_back(std::format("\t{} =l {} {}", wide, op, param));
          value = wide;
        }
        else if (cSuffix == "s")
        {
          const std::string wide = std::format("%c{}", temporary++);
          conversions.push_back(std::format("\t{} =d exts {}", wide, param));
          value = wide;
        }
        forwarded.push_back(std::format("l {}", value));
      }
      out += ") {\n@start\n";
      for (const auto &conversion : conversions)
        out += conversion + "\n";
      const std::string ngSymbol = qbeSymbol(function.name, function.source.value);
      const std::string call = unitResult ? std::format("call {}({})", ngSymbol, [&]
      {
        std::string args;
        for (size_t i = 0; i < forwarded.size(); ++i)
          args += (i == 0 ? "" : ", ") + forwarded[i];
        return args;
      }())
                                          : std::format("%r ={} call {}({})",
                                                        typecheck::isFloatBuiltin(declared) ? "d" : "l", ngSymbol, [&]
      {
        std::string args;
        for (size_t i = 0; i < forwarded.size(); ++i)
          args += (i == 0 ? "" : ", ") + forwarded[i];
        return args;
      }());
      out += "\t" + call + "\n";
      if (!unitResult)
      {
        if (resultSuffix == "s")
          out += "\t%rc =s truncd %r\n\tret %rc\n";
        else if (resultSuffix == "d")
          out += "\t%rd =d copy %r\n\tret %rd\n";
        else if (resultSuffix != "l")
          out += std::format("\t%rc ={} copy %r\n\tret %rc\n", resultSuffix);
        else
          out += "\tret %r\n";
      }
      else
        out += "\tret 0\n";
      out += "}\n";
      return out;
    }
  } // namespace

  auto lowerModule(const std::vector<Function> &functions, const VtableMap &vtables,
                   const NativeOwnerships &nativeOwnerships) -> std::string
  {
    FunctionNames names;
    for (const auto &function : functions)
      names.emplace(function.source.value, function.name);
    NgrtContext ngrt;
    ngrt.nativeOwnerships = nativeOwnerships;
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
        if (function.declaredResultType.has_value())
          ngrt.nativeResultTypes.emplace(function.source.value, *function.declaredResultType);
      }
      else if (function.externC && function.cExport)
      {
        // INC-29: an exported C ABI definition has an NG body; emit it with a
        // C-visible symbol and C boundary types (see FunctionLowerer::emit).
        ngrt.cExportDefIds.insert(function.source.value);
      }
      else if (function.externC)
      {
        // B3 first slice: extern declarations have no NG body; call sites
        // emit direct QBE calls to the C symbol with the declared C types.
        ngrt.externDefIds.insert(function.source.value);
        if (function.variadic)
          ngrt.externVariadicDefIds.insert(function.source.value);
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
              const auto suffixFor = [](TypeId type) -> const char *
              {
                if (type == typecheck::builtin::I8 || type == typecheck::builtin::U8)
                  return "b";
                if (type == typecheck::builtin::I16 || type == typecheck::builtin::U16)
                  return "h";
                if (type == typecheck::builtin::I32 || type == typecheck::builtin::U32)
                  return "w";
                if (type == typecheck::builtin::F32)
                  return "s";
                if (type == typecheck::builtin::F64)
                  return "d";
                return "l";
              };
              result += std::format("{}{}", member == 0 ? " " : ", ", suffixFor(memberType));
            }
            break;
          }
      }
      result += " }\n";
    }
    // INC-29: generate C ABI thunks for functions used as callbacks. Their
    // C-visible signature uses the callee's own ABI-safe scalar types (the
    // checker requires an exact signature match with the callback type).
    std::set<uint32_t> callbackDefIds;
    for (const auto &function : functions)
      for (const auto &block : function.blocks)
        for (const auto &instruction : block.instructions)
          if (instruction.kind == InstructionKind::Evaluate && instruction.text == "callback")
            callbackDefIds.insert(static_cast<uint32_t>(instruction.payload));
    for (const auto defId : callbackDefIds)
    {
      const auto found = std::find_if(functions.begin(), functions.end(),
                                      [&](const Function &candidate) { return candidate.source.value == defId; });
      if (found != functions.end())
        result += emitCAbiThunk(*found, std::format("cb{}", defId), false);
    }
    for (const auto &function : functions)
    {
      if (function.nativeFunction || (function.externC && !function.cExport))
        continue;
      try
      {
        result += FunctionLowerer{function, names, ngrt}.run();
      if (function.cExport)
        result += emitCAbiThunk(function, function.name, true);
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
    // Trait dispatch: per-(trait, concrete) vtable data plus a linear
    // key -> vtable lookup (keys are sorted for deterministic output).
    if (ngrt.needsTraitDispatch)
    {
      std::vector<uint64_t> keys;
      for (const auto &[key, methods] : vtables)
        keys.push_back(key);
      std::sort(keys.begin(), keys.end());
      for (const auto key : keys)
      {
        result += std::format("data $ngvtab_{} = {{ ", key);
        const auto &methods = vtables.at(key);
        for (size_t index = 0; index < methods.size(); ++index)
        {
          if (index != 0)
            result += ", ";
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
