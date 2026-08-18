// AI-generated code; reviewed for this repository's vNext rewrite.
//
// NG driver: single execution path through the QBE native backend. There is
// no bytecode/VM tier; every program is lowered to QBE IL, assembled, linked
// against libngrt, and run as a native executable.
#include "driver.hpp"
#include "flowir.hpp"
#include "hir.hpp"
#include "module_loader.hpp"
#include "native/lowering.hpp"
#include "ngrt.hpp"
#include "syntax/module_parser.hpp"
#include "syntax/parser.hpp"
#include "typecheck.hpp"

#include <algorithm>
#include <cerrno>
#include <charconv>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <ostream>
#include <sstream>
#include <string>
#include <vector>

#if defined(NG_QBE_PATH) && !defined(_WIN32)
#include <sys/wait.h>
#include <unistd.h>
#endif

namespace NG
{
  namespace
  {
    void printUsage(std::ostream &output)
    {
      output << "Usage: ngi --expr <expression>\n"
             << "       ngi --source <source-unit>\n"
             << "       ngi <source-file> [--emit=ssa] [--output <path>]\n"
             << "\n"
             << "`--emit=ssa` lowers the module to QBE IL text instead of running it.\n"
             << "`--output <path>` compiles to `<path>` without running it.\n";
    }

    [[nodiscard]] auto parseExpressionAndReport(std::string_view source, std::ostream &output, std::ostream &errors)
        -> int
    {
      try
      {
        const auto expression = syntax::parseExpression(source);
        output << "parsed vNext expression at bytes [" << expression->span.begin << ", " << expression->span.end
               << ")\n";
        return 0;
      }
      catch (const syntax::ParseError &error)
      {
        errors << "syntax error at bytes [" << error.span().begin << ", " << error.span().end << "): " << error.what()
               << '\n';
        return 1;
      }
    }

    struct CommandResult
    {
      int status{};
      std::string output;
    };

    /// Locates a libngrt archive. Search order:
    ///   1. NG_LIBRARY_PATH (colon-separated directories)
    ///   2. LIBRARY_PATH     (colon-separated directories)
    ///   3. The compile-time default path (embedded by CMake)
    [[nodiscard]] auto findNgrtLibrary(std::string_view name, std::string_view fallback) -> std::optional<std::string>
    {
      const auto searchPath = [&](const char *variable) -> std::optional<std::string>
      {
        if (variable == nullptr)
          return std::nullopt;
        std::istringstream stream{variable};
        std::string directory;
        while (std::getline(stream, directory, ':'))
        {
          const auto candidate = std::filesystem::path{directory} / std::string{name};
          if (std::filesystem::is_regular_file(candidate))
            return candidate.string();
        }
        return std::nullopt;
      };
      if (auto found = searchPath(std::getenv("NG_LIBRARY_PATH")); found.has_value())
        return found;
      if (auto found = searchPath(std::getenv("LIBRARY_PATH")); found.has_value())
        return found;
      if (!fallback.empty() && std::filesystem::is_regular_file(std::filesystem::path{fallback}))
        return std::string{fallback};
      return std::nullopt;
    }

#if defined(NG_QBE_PATH) && !defined(_WIN32)
    /// Executes argv directly (no shell) and captures merged output. The
    /// returned status is the raw wait(2) status, so WIFSIGNALED/WEXITSTATUS
    /// reflect the real child (a shell would collapse signals into
    /// 128+signal exit codes).
    [[nodiscard]] auto runCommand(const std::vector<std::string> &argv) -> CommandResult
    {
      CommandResult result;
      int pipeDescriptors[2];
      if (pipe(pipeDescriptors) != 0)
      {
        result.status = -1;
        result.output = "pipe failed";
        return result;
      }
      const pid_t child = fork();
      if (child == -1)
      {
        close(pipeDescriptors[0]);
        close(pipeDescriptors[1]);
        result.status = -1;
        result.output = "fork failed";
        return result;
      }
      if (child == 0)
      {
        dup2(pipeDescriptors[1], STDOUT_FILENO);
        dup2(pipeDescriptors[1], STDERR_FILENO);
        close(pipeDescriptors[0]);
        close(pipeDescriptors[1]);
        std::vector<char *> pointers;
        pointers.reserve(argv.size() + 1);
        for (const auto &argument : argv) pointers.push_back(const_cast<char *>(argument.c_str()));
        pointers.push_back(nullptr);
        execvp(pointers[0], pointers.data());
        _exit(127);
      }
      close(pipeDescriptors[1]);
      char buffer[256];
      ssize_t count = 0;
      while ((count = read(pipeDescriptors[0], buffer, sizeof buffer)) > 0) result.output.append(buffer, count);
      close(pipeDescriptors[0]);
      int status = 0;
      pid_t waited = 0;
      // waitpid can be interrupted by a signal; retry on EINTR and treat any
      // other error as a failure before trusting `status`.
      do
      {
        waited = waitpid(child, &status, 0);
      } while (waited == -1 && errno == EINTR);
      if (waited == -1)
      {
        result.status = -1;
        result.output = "waitpid failed";
        return result;
      }
      result.status = status;
      return result;
    }
#endif

#if defined(NG_QBE_PATH) && !defined(_WIN32)
    /// Emits QBE IL, assembles it with the vendored qbe, links it with the
    /// system cc and libngrt, and either runs the executable (default) or
    /// writes it to `outputPath` when provided.
    [[nodiscard]] auto compileNativeAndRun(const std::vector<flowir::Function> &flows,
                                           const std::unordered_map<uint64_t, std::vector<uint32_t>> &vtables,
                                           const native::NativeOwnerships &nativeOwnerships,
                                           const std::optional<std::string> &outputPath, bool needsImgui,
                                           std::ostream &output, std::ostream &errors) -> int
    {
      const auto il = native::lowerModule(flows, vtables, nativeOwnerships);
      // Unique, exclusively owned per-compilation directory (mkdtemp); the
      // guard removes it on every exit path, including failures.
      std::string directoryTemplate = (std::filesystem::temp_directory_path() / "ng_native_XXXXXX").string();
      char *created = mkdtemp(directoryTemplate.data());
      if (created == nullptr)
      {
        errors << "native: cannot create a temporary build directory\n";
        return 1;
      }
      const auto directory = std::filesystem::path{created};
      struct TempDirectoryGuard
      {
        std::filesystem::path path;
        ~TempDirectoryGuard() { std::error_code ignored; std::filesystem::remove_all(path, ignored); }
      } guard{directory};
      const auto ilFile = directory / "module.ssa";
      const auto asmFile = directory / "module.s";
      const auto executable = outputPath.has_value() ? std::filesystem::path{*outputPath} : directory / "ng_out";
      {
        std::ofstream stream(ilFile);
        stream << il;
      }
      const auto assemble = runCommand({NG_QBE_PATH, "-o", asmFile.string(), ilFile.string()});
      if (!WIFEXITED(assemble.status) || WEXITSTATUS(assemble.status) != 0)
      {
        errors << "native: qbe failed:\n" << assemble.output;
        return 1;
      }
      const auto ngrt = findNgrtLibrary("libngrt.a", NG_NGRT_PATH);
      if (!ngrt.has_value())
      {
        errors << "native: cannot find libngrt.a (set NG_LIBRARY_PATH or LIBRARY_PATH)\n";
        return 1;
      }
      std::vector<std::string> linkArgs = {"c++", asmFile.string(), *ngrt};
      if (needsImgui)
      {
#ifdef NG_NGRT_IMGUI_PATH
        const auto imgui = findNgrtLibrary("libngrt_imgui.a", NG_NGRT_IMGUI_PATH);
        if (!imgui.has_value())
        {
          errors << "native: cannot find libngrt_imgui.a (set NG_LIBRARY_PATH or LIBRARY_PATH)\n";
          return 1;
        }
        linkArgs.push_back(*imgui);
#endif
#ifdef NG_NGRT_IMGUI_EXTRA
        {
          std::istringstream stream(NG_NGRT_IMGUI_EXTRA);
          std::string item;
          while (stream >> item)
          {
            // Archive entries are resolved at runtime from NG_LIBRARY_PATH /
            // LIBRARY_PATH, falling back to the build-tree paths embedded by
            // CMake; everything else is passed to the linker verbatim.
            if (item.ends_with(".a"))
            {
              const char *fallback = nullptr;
#ifdef NG_IMGUI_PATH
              if (item == "libimgui.a") fallback = NG_IMGUI_PATH;
#endif
#ifdef NG_SDL_PATH
              if (item == "libSDL3.a") fallback = NG_SDL_PATH;
#endif
              const auto resolved = findNgrtLibrary(item, fallback != nullptr ? fallback : "");
              if (!resolved.has_value())
              {
                errors << "native: cannot find " << item << " (set NG_LIBRARY_PATH or LIBRARY_PATH)\n";
                return 1;
              }
              linkArgs.push_back(*resolved);
            }
            else
              linkArgs.push_back(item);
          }
        }
#endif
      }
      linkArgs.push_back("-o");
      linkArgs.push_back(executable.string());
#ifdef __linux__
      linkArgs.push_back("-lm");
#endif
      const auto link = runCommand(linkArgs);
      if (!WIFEXITED(link.status) || WEXITSTATUS(link.status) != 0)
      {
        errors << "native: cc failed:\n" << link.output;
        return 1;
      }
      if (outputPath.has_value())
      {
        output << "native executable written to " << executable.string() << '\n';
        return 0;
      }
      const auto run = runCommand({executable.string()});
      if (!WIFEXITED(run.status))
      {
        errors << "native: the program was killed by a signal\n" << run.output;
        return 1;
      }
      output << run.output;
      output << "native main exited with code " << WEXITSTATUS(run.status) << '\n';
      return 0;
    }
#else
    [[nodiscard]] auto compileNativeAndRun(const std::vector<flowir::Function> &,
                                           const std::unordered_map<uint64_t, std::vector<uint32_t>> &,
                                           const native::NativeOwnerships &,
                                           const std::optional<std::string> &, bool,
                                           std::ostream &, std::ostream &errors) -> int
    {
      errors << "native: the native tier requires the vendored QBE tool and a POSIX host\n";
      return 1;
    }
#endif

    [[nodiscard]] auto compileSourceUnitAndReport(const syntax::SourceUnit &unit,
                                                  std::ostream &output, std::ostream &errors,
                                                  bool emitSsa = false,
                                                  const std::optional<std::string> &nativeOutput = std::nullopt) -> int
    {
      try
      {
        const auto resolved = hir::Resolver{}.resolve(unit);
        // Pure hosts callable from const contexts (D-012 `= native`
        // capability): string utilities over canonical const values. These
        // all go through libngrt via the thin C++ wrappers.
        const const_eval::ConstNativeHost constHost =
            [](const std::string &name, const std::vector<const_eval::ConstValueId> &arguments,
               const_eval::ConstInterner &interner, syntax::SourceSpan span) -> const_eval::ConstValueId
        {
          const auto requireString = [&](size_t index) -> const std::string &
          {
            if (index >= arguments.size() ||
                interner.value(arguments[index]).kind != const_eval::ConstValueKind::String)
              throw const_eval::ConstEvalError(
                  std::format("const native `{}` expects a string argument {}", name, index + 1), span);
            return interner.value(arguments[index]).stringValue;
          };
          const auto requireInteger = [&](size_t index) -> int64_t
          {
            if (index >= arguments.size() ||
                interner.value(arguments[index]).kind != const_eval::ConstValueKind::Integer)
              throw const_eval::ConstEvalError(
                  std::format("const native `{}` expects an integer argument {}", name, index + 1), span);
            return interner.value(arguments[index]).integerValue;
          };
          try
          {
            if (name == "length") return interner.internInteger(ngrt::length(requireString(0)));
            if (name == "contains") return interner.internBool(ngrt::contains(requireString(0), requireString(1)));
            if (name == "startsWith") return interner.internBool(ngrt::startsWith(requireString(0), requireString(1)));
            if (name == "endsWith") return interner.internBool(ngrt::endsWith(requireString(0), requireString(1)));
            if (name == "toUpper" || name == "toLower")
              return interner.internString(name == "toUpper" ? ngrt::toUpper(requireString(0))
                                                             : ngrt::toLower(requireString(0)));
            if (name == "trim") return interner.internString(ngrt::trim(requireString(0)));
            if (name == "replace")
              return interner.internString(ngrt::replace(requireString(0), requireString(1), requireString(2)));
            if (name == "substring")
              return interner.internString(ngrt::substring(requireString(0), requireInteger(1), requireInteger(2)));
            if (name == "charAt")
              return interner.internString(ngrt::charAt(requireString(0), requireInteger(1)));
            if (name == "regexMatch")
              return interner.internBool(ngrt::regexMatch(requireString(0), requireString(1)));
          }
          catch (const std::out_of_range &error)
          {
            throw const_eval::ConstEvalError(std::format("const {}", error.what()), span);
          }
          catch (const std::runtime_error &error)
          {
            throw const_eval::ConstEvalError(std::format("const {}", error.what()), span);
          }
          throw const_eval::ConstEvalError(std::format("native `{}` is not const-capable", name), span);
        };
        const auto typed = typecheck::TypeChecker{}.check(resolved, constHost);

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
            for (const auto &method : methods)
              ids.push_back(method.value);
            vtables.emplace((static_cast<uint64_t>(traitId) << 32) | concrete, std::move(ids));
          }
        }
        // M5 ownership descriptors: `native fun` parameters currently default
        // to Copy (copy-first D-015), so AOT shim lowering deep-copies
        // aggregate arguments before the C callee sees them.
        native::NativeOwnerships nativeOwnerships;
        for (const auto &function : resolved.functions)
        {
          if (!function.nativeFunction) continue;
          const auto found = typed.functionTypeIds.find(function.id.value);
          if (found == typed.functionTypeIds.end()) continue;
          nativeOwnerships[function.id.value].assign(found->second.parameters.size(),
                                                     native::Ownership::Copy);
        }
        bool needsImgui = false;
        for (const auto &function : resolved.functions)
          if (function.nativeFunction && function.name.rfind("imgui", 0) == 0)
          {
            needsImgui = true;
            break;
          }
        if (emitSsa)
        {
          output << native::lowerModule(flows, vtables, nativeOwnerships);
          return 0;
        }
        output << "compiled " << flows.size() << " vNext function(s)\n";
        return compileNativeAndRun(flows, vtables, nativeOwnerships, nativeOutput, needsImgui, output, errors);
      }
      catch (const modules::LoadError &error)
      {
        errors << "module error: " << error.what() << '\n';
        return 1;
      }
      catch (const syntax::ParseError &error)
      {
        errors << "syntax error at bytes [" << error.span().begin << ", " << error.span().end << "): " << error.what()
               << '\n';
        return 1;
      }
      catch (const hir::ResolutionError &error)
      {
        errors << "resolution error at bytes [" << error.span.begin << ", " << error.span.end << "): " << error.what()
               << '\n';
        return 1;
      }
      catch (const typecheck::TypeError &error)
      {
        errors << "type error at bytes [" << error.span.begin << ", " << error.span.end << "): " << error.what()
               << '\n';
        return 1;
      }
      catch (const flowir::VerificationError &error)
      {
        errors << "flowir error: " << error.what() << '\n';
        return 1;
      }
      catch (const std::exception &error)
      {
        errors << "internal error: " << error.what() << '\n';
        return 1;
      }
    }

    [[nodiscard]] auto parseSourceAndReport(std::string_view source, std::ostream &output, std::ostream &errors,
                                            bool emitSsa = false,
                                            const std::optional<std::string> &nativeOutput = std::nullopt) -> int
    {
      try
      {
        const auto unit = modules::ModuleLoader{}.loadSource(source, std::filesystem::current_path());
        return compileSourceUnitAndReport(unit, output, errors, emitSsa, nativeOutput);
      }
      catch (const modules::LoadError &error)
      {
        errors << "module error: " << error.what() << '\n';
        return 1;
      }
      catch (const syntax::ParseError &error)
      {
        errors << "syntax error at bytes [" << error.span().begin << ", " << error.span().end << "): " << error.what()
               << '\n';
        return 1;
      }
    }
  } // namespace

  auto runDriver(const std::vector<std::string_view> &arguments, std::ostream &output, std::ostream &errors) -> int
  {
    bool emitSsa = false;
    bool pastDoubleDash = false;
    std::optional<std::string> nativeOutput;
    std::vector<std::string_view> cleaned;
    cleaned.reserve(arguments.size());
    for (size_t index = 0; index < arguments.size(); ++index)
    {
      if (pastDoubleDash)
      {
        // After `--`, forward everything directly without parsing flags.
        cleaned.push_back(arguments[index]);
        continue;
      }
      if (arguments[index] == "--")
      {
        pastDoubleDash = true;
        cleaned.push_back(arguments[index]);
        continue;
      }
      if (arguments[index] == "--output" || arguments[index] == "-o")
      {
        if (index + 1 >= arguments.size())
        {
          errors << "--output requires a path\n";
          return 1;
        }
        nativeOutput = std::string{arguments[++index]};
        continue;
      }
      if (arguments[index].starts_with("--output="))
      {
        auto value = std::string{arguments[index].substr(std::string_view{"--output="}.size())};
        if (value.empty())
        {
          errors << "--output requires a path\n";
          return 1;
        }
        nativeOutput = std::move(value);
        continue;
      }
      if (arguments[index] == "--fuel" && index + 1 < arguments.size())
      {
        // Fuel was a VM-only knob; accepted as a no-op for compatibility.
        ++index;
        continue;
      }
      if (arguments[index].starts_with("--emit"))
      {
        std::string_view value;
        if (arguments[index] == "--emit" && index + 1 < arguments.size())
          value = arguments[++index];
        else if (arguments[index].starts_with("--emit="))
          value = arguments[index].substr(std::string_view{"--emit="}.size());
        else
        {
          cleaned.push_back(arguments[index]);
          continue;
        }
        if (value != "ssa")
        {
          errors << "unsupported --emit value `" << value << "` (only `ssa` is supported)\n";
          return 1;
        }
        emitSsa = true;
        continue;
      }
      // `--native` is now the only execution mode; accept it for backward
      // compatibility.
      if (arguments[index] == "--native")
        continue;
      cleaned.push_back(arguments[index]);
    }
    if (nativeOutput.has_value() && emitSsa)
    {
      errors << "--output cannot be combined with --emit=ssa\n";
      return 1;
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
      return parseSourceAndReport(cleaned[1], output, errors, emitSsa, nativeOutput);
    }

    if (cleaned[0].starts_with('-') || cleaned.size() != 1)
    {
      errors << "unknown or incomplete command-line arguments\n";
      printUsage(errors);
      return 1;
    }

    try
    {
      const auto unit = modules::ModuleLoader{}.loadFile(std::filesystem::path{std::string{cleaned[0]}});
      return compileSourceUnitAndReport(unit, output, errors, emitSsa, nativeOutput);
    }
    catch (const modules::LoadError &error)
    {
      errors << "module error: " << error.what() << '\n';
      return 1;
    }
    catch (const syntax::ParseError &error)
    {
      errors << "syntax error at bytes [" << error.span().begin << ", " << error.span().end << "): " << error.what()
             << '\n';
      return 1;
    }
  }
} // namespace NG
