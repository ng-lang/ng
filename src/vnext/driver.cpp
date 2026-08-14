// AI-generated code; reviewed for this repository's vNext rewrite.
#include "vnext/driver.hpp"
#include "vnext/bytecode.hpp"
#include "vnext/module_loader.hpp"
#include "vnext/flowir.hpp"
#include "vnext/hir.hpp"
#include "vnext/syntax/module_parser.hpp"
#include "vnext/typecheck.hpp"
#include "vnext/vm.hpp"
#include "vnext/native.hpp"
#include "vnext/syntax/parser.hpp"
#include <algorithm>
#include <charconv>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <ostream>
#include <sstream>
#include <string>

namespace NG::vnext
{
  namespace
  {
    void printUsage(std::ostream &output)
    {
      output << "Usage: ngi --expr <expression>\n"
             << "       ngi --source <source-unit>\n"
             << "       ngi <source-file>\n"
             << "\n"
             << "The vNext frontend currently accepts expression fragments and source units with function declarations.\n";
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

    [[nodiscard]] auto compileSourceUnitAndReport(const syntax::SourceUnit &unit,
                                                   const std::vector<std::string_view> &runtimeArguments,
                                                   std::ostream &output, std::ostream &errors) -> int
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
          const auto result = vm::VM{}.run(artifact, main->id, values, 1'000'000, &natives);
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
    [[nodiscard]] auto parseSourceAndReport(std::string_view source, const std::vector<std::string_view> &runtimeArguments,
                                            std::ostream &output, std::ostream &errors) -> int
    {
      try
      {
        const auto unit = modules::ModuleLoader{}.loadSource(source, std::filesystem::current_path());
        return compileSourceUnitAndReport(unit, runtimeArguments, output, errors);
      }
      catch (const modules::LoadError &error)
      {
        errors << "module error: " << error.what() << '\n';
        return 1;
      }
    }
  } // namespace

  auto runDriver(const std::vector<std::string_view> &arguments, std::ostream &output, std::ostream &errors) -> int
  {
    if (arguments.empty() || arguments[0] == "--help" || arguments[0] == "-h")
    {
      printUsage(output);
      return arguments.empty() ? 1 : 0;
    }

    if (arguments[0] == "--expr")
    {
      if (arguments.size() != 2)
      {
        errors << "--expr requires exactly one expression argument\n";
        return 1;
      }
      return parseExpressionAndReport(arguments[1], output, errors);
    }

    if (arguments[0] == "--source")
    {
      if (arguments.size() < 2)
      {
        errors << "--source requires one source-unit argument\n";
        return 1;
      }
      std::vector<std::string_view> runtimeArguments;
      if (arguments.size() > 2)
      {
        if (arguments[2] != "--")
        {
          errors << "runtime arguments must follow `--`\n";
          return 1;
        }
        runtimeArguments.assign(arguments.begin() + 3, arguments.end());
      }
      return parseSourceAndReport(arguments[1], runtimeArguments, output, errors);
    }

    if (arguments[0].starts_with('-') || arguments.size() != 1)
    {
      errors << "unknown or incomplete command-line arguments\n";
      printUsage(errors);
      return 1;
    }

    // File mode loads the transitive import graph before compiling.
    try
    {
      const auto unit = modules::ModuleLoader{}.loadFile(std::filesystem::path{std::string{arguments[0]}});
      return compileSourceUnitAndReport(unit, {}, output, errors);
    }
    catch (const modules::LoadError &error)
    {
      errors << "module error: " << error.what() << '\n';
      return 1;
    }
  }
} // namespace NG::vnext
