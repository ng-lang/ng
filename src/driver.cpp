// AI-generated code; reviewed for this repository's vNext rewrite.
#include "driver.hpp"
#include "bytecode.hpp"
#include "module_loader.hpp"
#include "flowir.hpp"
#include "hir.hpp"
#include "syntax/module_parser.hpp"
#include "typecheck.hpp"
#include "vm.hpp"
#include "native.hpp"
#include "syntax/parser.hpp"
#include <algorithm>
#include <charconv>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <ostream>
#include <regex>
#include <sstream>
#include <string>

namespace NG
{
  namespace
  {
    void printUsage(std::ostream &output)
    {
      output << "Usage: ngi --expr <expression>\n"
             << "       ngi --source <source-unit>\n"
             << "       ngi <source-file> [--fuel <instructions>]\n"
             << "\n"
             << "`--fuel 0` lifts the instruction budget (used by interactive programs such as the imgui IDE).\n";
    }

    [[nodiscard]] auto parseExpressionAndReport(std::string_view source, std::ostream &output, std::ostream &errors) -> int
    {
      try
      {
        const auto expression = syntax::parseExpression(source);
        output << "parsed vNext expression at bytes [" << expression->span.begin << ", " << expression->span.end << ")\n";
        return 0;
      }
      catch (const syntax::ParseError &error)
      {
        errors << "syntax error at bytes [" << error.span().begin << ", " << error.span().end << "): " << error.what() << '\n';
        return 1;
      }
    }

    [[nodiscard]] auto parseRuntimeArguments(const std::vector<std::string_view> &arguments,
                                             const std::vector<hir::Parameter> &parameters, std::vector<Value> &values,
                                             std::ostream &errors) -> bool
    {
      values.clear();
      values.reserve(arguments.size());
      for (size_t index = 0; index < arguments.size(); ++index)
      {
        const auto argument = arguments[index];
        const auto &parameter = parameters[index];
        if (parameter.typeName == "string")
        {
          values.push_back(Value::string(std::string{argument}));
          continue;
        }
        if (parameter.typeName != "i64")
        {
          errors << "main parameter `" << parameter.name << "` must currently be i64 or string\n";
          return false;
        }
        int64_t value{};
        const auto [end, error] = std::from_chars(argument.data(), argument.data() + argument.size(), value);
        if (error != std::errc{} || end != argument.data() + argument.size())
        {
          errors << "invalid i64 argument `" << argument << "`\n";
          return false;
        }
        values.push_back(Value::integer(value));
      }
      return true;
    }

    void registerCoreNatives(vm::NativeRegistry &natives, std::ostream &output, std::ostream &errors);

    [[nodiscard]] auto compileSourceUnitAndReport(const syntax::SourceUnit &unit,
                                                   const std::vector<std::string_view> &runtimeArguments,
                                                   std::ostream &output, std::ostream &errors,
                                                   const NativeRegistration *extraNatives = nullptr,
                                                   size_t fuel = 1'000'000) -> int
    {
      try
      {
        const auto resolved = hir::Resolver{}.resolve(unit);
        const auto typed = typecheck::TypeChecker{}.check(resolved);

        std::vector<flowir::Function> flows;
        flows.reserve(resolved.functions.size() + typed.instances.size());
        for (const auto &function : resolved.functions)
        {
          if (typed.placeholderFunctions.contains(function.id.value))
          {
            flowir::Function placeholder{.source = function.id, .name = function.name};
            placeholder.entry = flowir::BlockId{0};
            flowir::Block block{.id = flowir::BlockId{0}};
            block.terminator = flowir::Terminator{.kind = flowir::TerminatorKind::Return};
            placeholder.blocks.push_back(std::move(block));
            flows.push_back(std::move(placeholder));
            continue;
          }
          flows.push_back(flowir::Lowerer{}.lower(function, typed));
          flowir::Verifier{}.verify(flows.back());
        }
        for (const auto &instance : typed.instances)
        {
          flows.push_back(flowir::Lowerer{}.lower(instance, typed));
          flowir::Verifier{}.verify(flows.back());
        }
        std::unordered_map<uint64_t, std::vector<uint32_t>> vtables;
        for (const auto &[traitName, tables] : typed.traitViewTables)
        {
          uint32_t traitId = 0;
          for (size_t index = 0; index < typed.typeDescriptors.size(); ++index)
            if (typed.typeDescriptors[index].kind == typecheck::TypeKind::Trait &&
                typed.typeDescriptors[index].name == traitName)
            {
              traitId = static_cast<uint32_t>(index);
              break;
            }
          for (const auto &[concrete, methods] : tables)
          {
            std::vector<uint32_t> ids;
            ids.reserve(methods.size());
            for (const auto &method : methods) ids.push_back(method.value);
            vtables.emplace((static_cast<uint64_t>(traitId) << 32) | concrete, std::move(ids));
          }
        }
        const auto artifact = bytecode::ModuleCompiler{}.compile(flows, vtables);
        for (const auto &function : artifact.functions) bytecode::Verifier{}.verify(function);
        const size_t verifiedFunctions = artifact.functions.size();

        const auto main = std::find_if(resolved.functions.begin(), resolved.functions.end(), [](const auto &function) {
          return function.name == "main";
        });
        if (main != resolved.functions.end())
        {
          std::vector<Value> values;
          if (runtimeArguments.size() != main->parameters.size())
          {
            errors << "main argument count mismatch: expected " << main->parameters.size() << ", got " << runtimeArguments.size() << '\n';
            return 1;
          }
          if (!parseRuntimeArguments(runtimeArguments, main->parameters, values, errors)) return 1;
          vm::NativeRegistry natives;
          registerCoreNatives(natives, output, errors);
          if (extraNatives != nullptr && *extraNatives != nullptr) (*extraNatives)(natives);
          const auto result = vm::VM{}.run(artifact, main->id, values, fuel, &natives);
          output << "compiled " << verifiedFunctions << " vNext function(s); main "
                 << (result.reason == vm::HaltReason::Return ? "returned" : "exhausted fuel") << " after "
                 << result.executedInstructions << " instruction(s)";
          if (result.returnValue.has_value())
          {
            output << " with value ";
            if (result.returnValue->isInteger()) output << result.returnValue->asInteger();
            else if (result.returnValue->isDouble()) output << result.returnValue->asDouble();
            else output << result.returnValue->asString();
          }
          output << '\n';
        }
        else
        {
          output << "compiled " << verifiedFunctions << " vNext function(s)\n";
        }
        return 0;
      }
      catch (const modules::LoadError &error)
      {
        errors << "module error: " << error.what() << '\n';
        return 1;
      }
      catch (const syntax::ParseError &error)
      {
        errors << "syntax error at bytes [" << error.span().begin << ", " << error.span().end << "): " << error.what() << '\n';
        return 1;
      }
      catch (const hir::ResolutionError &error)
      {
        errors << "resolution error at bytes [" << error.span.begin << ", " << error.span.end << "): " << error.what() << '\n';
        return 1;
      }
      catch (const typecheck::TypeError &error)
      {
        errors << "type error at bytes [" << error.span.begin << ", " << error.span.end << "): " << error.what() << '\n';
        return 1;
      }
      catch (const flowir::VerificationError &error)
      {
        errors << "flowir error: " << error.what() << '\n';
        return 1;
      }
      catch (const bytecode::BytecodeError &error)
      {
        errors << "bytecode error: " << error.what() << '\n';
        return 1;
      }
      catch (const std::exception &error)
      {
        errors << "internal error: " << error.what() << '\n';
        return 1;
      }
    }
    void registerCoreNatives(vm::NativeRegistry &natives, std::ostream &output, std::ostream &errors)
    {
    natives.registerNative("print", [&output](const std::vector<Value> &arguments,
                                                 const std::vector<typecheck::TypeId> &parameterTypes) {
      if (arguments.empty() || arguments.size() > 1) throw bytecode::BytecodeError("print expects one argument");
      const auto &argument = arguments.front();
      if (!parameterTypes.empty() && parameterTypes.front() == typecheck::builtin::Bool)
        output << (argument == 1 ? "true" : "false");
      else if (argument.isInteger()) output << argument.asInteger();
      else if (argument.isDouble()) output << argument.asDouble();
      else if (argument.isString()) output << argument.asString();
      else throw bytecode::BytecodeError("print does not support this value type");
      output << '\n';
      return Value{};
    });
    const auto expectStrings = [](const std::vector<Value> &arguments, size_t count) {
      std::vector<std::string> result;
      if (arguments.size() != count) throw bytecode::BytecodeError("native argument count mismatch");
      for (const auto &argument : arguments)
      {
        if (!argument.isString()) throw bytecode::BytecodeError("native argument is not a string");
        result.push_back(argument.asString());
      }
      return result;
    };
    natives.registerNative("readLine", [](const std::vector<Value> &, const std::vector<typecheck::TypeId> &) {
      std::string line;
      std::getline(std::cin, line);
      return Value::string(std::move(line));
    });
    natives.registerNative("readFile", [&expectStrings](const std::vector<Value> &arguments, const std::vector<typecheck::TypeId> &) {
      const auto strings = expectStrings(arguments, 1);
      std::ifstream input{strings.front()};
      if (!input) throw bytecode::BytecodeError(std::format("cannot read file `{}`", strings.front()));
      std::ostringstream buffer;
      buffer << input.rdbuf();
      return Value::string(buffer.str());
    });
    natives.registerNative("writeFile", [&expectStrings](const std::vector<Value> &arguments, const std::vector<typecheck::TypeId> &) {
      const auto strings = expectStrings(arguments, 2);
      std::ofstream output{strings.front()};
      if (!output) throw bytecode::BytecodeError(std::format("cannot write file `{}`", strings.front()));
      output << strings[1];
      return Value{};
    });
    natives.registerNative("trim", [&expectStrings](const std::vector<Value> &arguments, const std::vector<typecheck::TypeId> &) {
      const auto strings = expectStrings(arguments, 1);
      const auto first = strings.front().find_first_not_of(" \t\n\r");
      const auto last = strings.front().find_last_not_of(" \t\n\r");
      if (first == std::string::npos) return Value::string("");
      return Value::string(strings.front().substr(first, last - first + 1));
    });
    natives.registerNative("split", [&expectStrings](const std::vector<Value> &arguments, const std::vector<typecheck::TypeId> &) {
      const auto strings = expectStrings(arguments, 2);
      std::vector<Value> parts;
      size_t start = 0;
      while (start <= strings[0].size())
      {
        const auto found = strings[0].find(strings[1], start);
        if (found == std::string::npos)
        {
          parts.push_back(Value::string(strings[0].substr(start)));
          break;
        }
        parts.push_back(Value::string(strings[0].substr(start, found - start)));
        start = found + strings[1].size();
      }
      return Value::array(std::move(parts));
    });
    natives.registerNative("join", [](const std::vector<Value> &arguments, const std::vector<typecheck::TypeId> &) {
      if (arguments.size() != 2 || !arguments[0].isArray() || !arguments[1].isString())
        throw bytecode::BytecodeError("join expects an array of strings and a separator");
      std::string joined;
      const auto &items = arguments[0].asArray();
      for (size_t index = 0; index < items.size(); ++index)
      {
        if (!items[index].isString()) throw bytecode::BytecodeError("join expects an array of strings");
        if (index != 0) joined += arguments[1].asString();
        joined += items[index].asString();
      }
      return Value::string(std::move(joined));
    });
    natives.registerNative("contains", [&expectStrings](const std::vector<Value> &arguments, const std::vector<typecheck::TypeId> &) {
      const auto strings = expectStrings(arguments, 2);
      return Value::integer(strings[0].find(strings[1]) != std::string::npos);
    });
    natives.registerNative("replace", [&expectStrings](const std::vector<Value> &arguments, const std::vector<typecheck::TypeId> &) {
      const auto strings = expectStrings(arguments, 3);
      std::string result = strings[0];
      size_t position = 0;
      while ((position = result.find(strings[1], position)) != std::string::npos)
      {
        result.replace(position, strings[1].size(), strings[2]);
        position += strings[2].size();
      }
      return Value::string(std::move(result));
    });
    natives.registerNative("startsWith", [&expectStrings](const std::vector<Value> &arguments, const std::vector<typecheck::TypeId> &) {
      const auto strings = expectStrings(arguments, 2);
      return Value::integer(strings[0].starts_with(strings[1]));
    });
    natives.registerNative("endsWith", [&expectStrings](const std::vector<Value> &arguments, const std::vector<typecheck::TypeId> &) {
      const auto strings = expectStrings(arguments, 2);
      return Value::integer(strings[0].ends_with(strings[1]));
    });
    natives.registerNative("toUpper", [&expectStrings](const std::vector<Value> &arguments, const std::vector<typecheck::TypeId> &) {
      const auto strings = expectStrings(arguments, 1);
      std::string result = strings[0];
      for (auto &character : result) character = static_cast<char>(std::toupper(static_cast<unsigned char>(character)));
      return Value::string(std::move(result));
    });
    natives.registerNative("length", [&expectStrings](const std::vector<Value> &arguments, const std::vector<typecheck::TypeId> &) {
      const auto strings = expectStrings(arguments, 1);
      return Value::integer(static_cast<int64_t>(strings.front().size()));
    });
    natives.registerNative("charAt", [](const std::vector<Value> &arguments, const std::vector<typecheck::TypeId> &) {
      if (arguments.size() != 2 || !arguments[0].isString() || !arguments[1].isInteger())
        throw bytecode::BytecodeError("charAt expects a string and an index");
      const auto &text = arguments[0].asString();
      const int64_t index = arguments[1].asInteger();
      if (index < 0 || static_cast<size_t>(index) >= text.size())
        throw bytecode::BytecodeError(std::format("charAt index out of bounds: index {}, length {}", index, text.size()));
      return Value::string(std::string(1, text[static_cast<size_t>(index)]));
    });
    natives.registerNative("substring", [](const std::vector<Value> &arguments, const std::vector<typecheck::TypeId> &) {
      if (arguments.size() != 3 || !arguments[0].isString() || !arguments[1].isInteger() || !arguments[2].isInteger())
        throw bytecode::BytecodeError("substring expects a string and two indexes");
      const auto &text = arguments[0].asString();
      const int64_t start = arguments[1].asInteger();
      const int64_t end = arguments[2].asInteger();
      if (start < 0 || end < start || static_cast<size_t>(end) > text.size())
        throw bytecode::BytecodeError(std::format("substring bounds out of range: [{}..{}) of length {}", start, end, text.size()));
      return Value::string(text.substr(static_cast<size_t>(start), static_cast<size_t>(end - start)));
    });
    natives.registerNative("len", [](const std::vector<Value> &arguments, const std::vector<typecheck::TypeId> &) {
      if (arguments.size() != 1 || !arguments.front().isArray())
        throw bytecode::BytecodeError("len expects an array");
      return Value::integer(static_cast<int64_t>(arguments.front().asArray().size()));
    });
    natives.registerNative("reverse", [](const std::vector<Value> &arguments, const std::vector<typecheck::TypeId> &) {
      if (arguments.size() != 1 || !arguments.front().isArray())
        throw bytecode::BytecodeError("reverse expects an array");
      std::vector<Value> reversed;
      const auto &items = arguments.front().asArray();
      reversed.reserve(items.size());
      for (auto it = items.rbegin(); it != items.rend(); ++it) reversed.push_back(it->deepCopy());
      return Value::array(std::move(reversed));
    });
    natives.registerNative("currentExecutablePath", [](const std::vector<Value> &, const std::vector<typecheck::TypeId> &) {
      std::error_code ignored;
      return Value::string(std::filesystem::absolute(std::filesystem::current_path(), ignored).string());
    });
    natives.registerNative("sum", [](const std::vector<Value> &arguments, const std::vector<typecheck::TypeId> &) {
      if (arguments.size() != 1 || !arguments.front().isArray())
        throw bytecode::BytecodeError("sum expects an array of i64");
      int64_t total = 0;
      for (const auto &element : arguments.front().asArray())
      {
        if (!element.isInteger()) throw bytecode::BytecodeError("sum expects an array of i64");
        total += element.asInteger();
      }
      return Value::integer(total);
    });
    natives.registerNative("arrayContains", [](const std::vector<Value> &arguments, const std::vector<typecheck::TypeId> &) {
      if (arguments.size() != 2 || !arguments[0].isArray() || !arguments[1].isInteger())
        throw bytecode::BytecodeError("arrayContains expects an array of i64 and a value");
      for (const auto &element : arguments[0].asArray())
        if (element.isInteger() && element.asInteger() == arguments[1].asInteger()) return Value::integer(1);
      return Value::integer(0);
    });
    auto heapSlots = std::make_shared<std::vector<std::optional<Value>>>();
    natives.registerNative("allocate", [heapSlots](const std::vector<Value> &arguments, const std::vector<typecheck::TypeId> &) {
      if (arguments.size() != 1 || !arguments.front().isInteger())
        throw bytecode::BytecodeError("allocate expects an i64");
      for (size_t index = 0; index < heapSlots->size(); ++index)
      {
        if (!heapSlots->at(index).has_value())
        {
          heapSlots->at(index) = arguments.front();
          return Value::opaque(static_cast<uint64_t>(index + 1));
        }
      }
      heapSlots->push_back(arguments.front());
      return Value::opaque(static_cast<uint64_t>(heapSlots->size()));
    });
    const auto slotOf = [heapSlots](const Value &handle) -> Value & {
      if (!handle.isOpaque() || handle.asOpaque() == 0 || handle.asOpaque() > heapSlots->size() ||
          !heapSlots->at(handle.asOpaque() - 1).has_value())
        throw bytecode::BytecodeError("invalid heap handle");
      return *heapSlots->at(handle.asOpaque() - 1);
    };
    natives.registerNative("load", [slotOf](const std::vector<Value> &arguments, const std::vector<typecheck::TypeId> &) {
      if (arguments.size() != 1) throw bytecode::BytecodeError("load expects a handle");
      return slotOf(arguments.front());
    });
    natives.registerNative("store", [slotOf](const std::vector<Value> &arguments, const std::vector<typecheck::TypeId> &) {
      if (arguments.size() != 2 || !arguments[1].isInteger()) throw bytecode::BytecodeError("store expects a handle and an i64");
      slotOf(arguments[0]) = arguments[1];
      return Value{};
    });
    natives.registerNative("release", [heapSlots, slotOf](const std::vector<Value> &arguments, const std::vector<typecheck::TypeId> &) {
      if (arguments.size() != 1) throw bytecode::BytecodeError("release expects a handle");
      static_cast<void>(slotOf(arguments.front()));
      heapSlots->at(arguments.front().asOpaque() - 1).reset();
      return Value{};
    });
    natives.registerNative("outstanding", [heapSlots](const std::vector<Value> &, const std::vector<typecheck::TypeId> &) {
      int64_t count = 0;
      for (const auto &slot : *heapSlots)
        if (slot.has_value()) ++count;
      return Value::integer(count);
    });
    natives.registerNative("regexMatch", [&expectStrings](const std::vector<Value> &arguments, const std::vector<typecheck::TypeId> &) {
      const auto strings = expectStrings(arguments, 2);
      try
      {
        return Value::integer(std::regex_search(strings[0], std::regex(strings[1])));
      }
      catch (const std::regex_error &)
      {
        throw bytecode::BytecodeError(std::format("regexMatch: invalid pattern `{}`", strings[1]));
      }
    });
    natives.registerNative("toLower", [&expectStrings](const std::vector<Value> &arguments, const std::vector<typecheck::TypeId> &) {
      const auto strings = expectStrings(arguments, 1);
      std::string result = strings[0];
      for (auto &character : result) character = static_cast<char>(std::tolower(static_cast<unsigned char>(character)));
      return Value::string(std::move(result));
    });
    natives.registerNative("assert", [](const std::vector<Value> &arguments, const std::vector<typecheck::TypeId> &) {
      if (arguments.size() != 1) throw bytecode::BytecodeError("assert expects one argument");
      if (!(arguments.front() == 1)) throw bytecode::BytecodeError("assertion failed");
      return Value{};
    });
      natives.registerNative("runNgi", [](const std::vector<Value> &arguments, const std::vector<typecheck::TypeId> &) {
        if (arguments.size() != 1 || !arguments.front().isString())
          throw bytecode::BytecodeError("runNgi expects a source string");
        std::ostringstream captured, capturedErrors;
        int status = 0;
        try
        {
          const auto unit = modules::ModuleLoader{}.loadSource(arguments.front().asString(), std::filesystem::current_path());
          status = compileSourceUnitAndReport(unit, {}, captured, capturedErrors);
        }
        catch (const modules::LoadError &error)
        {
          capturedErrors << "module error: " << error.what() << '\n';
          status = 1;
        }
        std::string combined = captured.str();
        const auto diagnostics = capturedErrors.str();
        if (!diagnostics.empty())
        {
          if (!combined.empty() && combined.back() != '\n') combined += '\n';
          combined += diagnostics;
        }
        if (status != 0) combined += std::format("[exit {}]", status);
        return Value::string(std::move(combined));
      });
    }

    [[nodiscard]] auto parseSourceAndReport(std::string_view source, const std::vector<std::string_view> &runtimeArguments,
                                            std::ostream &output, std::ostream &errors,
                                            const NativeRegistration *extraNatives = nullptr,
                                            size_t fuel = 1'000'000) -> int
    {
      try
      {
        const auto unit = modules::ModuleLoader{}.loadSource(source, std::filesystem::current_path());
        return compileSourceUnitAndReport(unit, runtimeArguments, output, errors, extraNatives, fuel);
      }
      catch (const modules::LoadError &error)
      {
        errors << "module error: " << error.what() << '\n';
        return 1;
      }
    }
  } // namespace

  auto runDriverImpl(const std::vector<std::string_view> &arguments, std::ostream &output, std::ostream &errors,
                    const NativeRegistration *extraNatives) -> int
  {
    size_t fuel = 1'000'000;
    std::vector<std::string_view> cleaned;
    cleaned.reserve(arguments.size());
    for (size_t index = 0; index < arguments.size(); ++index)
    {
      if (arguments[index] != "--")
      {
        if (arguments[index] == "--fuel" && index + 1 < arguments.size())
        {
          uint64_t parsed = 0;
          const auto [end, error] = std::from_chars(arguments[index + 1].data(),
                                                    arguments[index + 1].data() + arguments[index + 1].size(), parsed);
          if (error != std::errc{} || end != arguments[index + 1].data() + arguments[index + 1].size())
          {
            errors << "invalid --fuel value `" << arguments[index + 1] << "`\n";
            return 1;
          }
          fuel = static_cast<size_t>(parsed);
          ++index;
          continue;
        }
      }
      cleaned.push_back(arguments[index]);
    }
    if (cleaned.empty() || cleaned[0] == "--help" || cleaned[0] == "-h")
    {
      printUsage(output);
      return arguments.empty() ? 1 : 0;
    }

    if (cleaned[0] == "--expr")
    {
      if (cleaned.size() != 2)
      {
        errors << "--expr requires exactly one expression argument\n";
        return 1;
      }
      return parseExpressionAndReport(cleaned[1], output, errors);
    }

    if (cleaned[0] == "--source")
    {
      if (cleaned.size() < 2)
      {
        errors << "--source requires one source-unit argument\n";
        return 1;
      }
      std::vector<std::string_view> runtimeArguments;
      if (cleaned.size() > 2)
      {
        if (cleaned[2] != "--")
        {
          errors << "runtime arguments must follow `--`\n";
          return 1;
        }
        runtimeArguments.assign(cleaned.begin() + 3, cleaned.end());
      }
      return parseSourceAndReport(cleaned[1], runtimeArguments, output, errors, extraNatives, fuel);
    }

    if (cleaned[0].starts_with('-') || cleaned.size() != 1)
    {
      errors << "unknown or incomplete command-line arguments\n";
      printUsage(errors);
      return 1;
    }

    // File mode loads the transitive import graph before compiling.
    try
    {
      const auto unit = modules::ModuleLoader{}.loadFile(std::filesystem::path{std::string{cleaned[0]}});
      return compileSourceUnitAndReport(unit, {}, output, errors, extraNatives, fuel);
    }
    catch (const modules::LoadError &error)
    {
      errors << "module error: " << error.what() << '\n';
      return 1;
    }
  }

  auto runDriver(const std::vector<std::string_view> &arguments, std::ostream &output, std::ostream &errors) -> int
  {
    return runDriverImpl(arguments, output, errors, nullptr);
  }

  auto runDriverWithNatives(const std::vector<std::string_view> &arguments, std::ostream &output, std::ostream &errors,
                            NativeRegistration registration) -> int
  {
    return runDriverImpl(arguments, output, errors, &registration);
  }
} // namespace NG
