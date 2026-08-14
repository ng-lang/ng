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
#include <filesystem>
#include <fstream>
#include <ostream>
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
          flows.push_back(flowir::Lowerer{}.lower(function, typed));
          flowir::Verifier{}.verify(flows.back());
        }
        for (const auto &instance : typed.instances)
        {
          flows.push_back(flowir::Lowerer{}.lower(instance, typed));
          flowir::Verifier{}.verify(flows.back());
        }
        const auto artifact = bytecode::ModuleCompiler{}.compile(flows);
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
            else if (argument.isString()) output << argument.asString();
            else throw bytecode::BytecodeError("print does not support this value type");
            output << '\n';
            return Value{};
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
