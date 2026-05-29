#include <intp/runtime.hpp>
#include <intp/runtime_numerals.hpp>
#include <module.hpp>
#include <orgasm/native_bridge.hpp>
#include <orgasm/vm.hpp>
#include <runtime/native_marshaling.hpp>
#include <runtime/value_access.hpp>
#include <runtime/string_layout_access.hpp>
#include <sysdep/process.hpp>
#include <algorithm>
#include <array>
#include <bit>
#include <cerrno>
#include <cctype>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <initializer_list>
#include <iostream>
#include <regex>
#include <cstdlib>
#ifndef _WIN32
#include <sys/wait.h>
#include <unistd.h>
#endif

namespace NG::library::prelude
{

  using namespace NG::runtime;
  using namespace NG::runtime::native;

  static auto read_line_native() -> Str
  {
    Str line;
    std::getline(std::cin, line);
    return line;
  }

  static auto read_file_native(Str path) -> Str
  {
    std::ifstream file(path);
    if (!file.is_open())
    {
      throw RuntimeException("readFile() failed to open: " + path);
    }
    return Str((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
  }

  static void write_file_native(Str path, Str content)
  {
    std::ofstream file(path);
    if (!file.is_open())
    {
      throw RuntimeException("writeFile() failed to open: " + path);
    }
    file << content;
    if (!file.good())
    {
      throw RuntimeException("writeFile() failed to write: " + path);
    }
  }

  static auto trim_native(Str s) -> Str
  {
    size_t start = s.find_first_not_of(" \t\n\r");
    size_t end = s.find_last_not_of(" \t\n\r");
    return start == Str::npos ? Str{} : s.substr(start, end - start + 1);
  }

  static auto contains_native(Str str, Str sub) -> bool
  {
    return str.find(sub) != Str::npos;
  }

  static auto replace_native(Str result, Str oldValue, Str newValue) -> Str
  {
    if (!oldValue.empty())
    {
      size_t pos = 0;
      while ((pos = result.find(oldValue, pos)) != Str::npos)
      {
        result.replace(pos, oldValue.size(), newValue);
        pos += newValue.size();
      }
    }
    return result;
  }

  static auto starts_with_native(Str str, Str prefix) -> bool
  {
    return str.starts_with(prefix);
  }

  static auto ends_with_native(Str str, Str suffix) -> bool
  {
    return str.ends_with(suffix);
  }

  static auto to_upper_native(Str result) -> Str
  {
    std::transform(result.begin(), result.end(), result.begin(),
                   [](unsigned char ch) { return static_cast<char>(std::toupper(ch)); });
    return result;
  }

  static auto to_lower_native(Str result) -> Str
  {
    std::transform(result.begin(), result.end(), result.begin(),
                   [](unsigned char ch) { return static_cast<char>(std::tolower(ch)); });
    return result;
  }

  static auto current_executable_path_native() -> Str
  {
    return NG::System::Process::current_executable_path();
  }

  static auto run_child_process(const Str &executable, const Str &path) -> Str
  {
#ifdef _WIN32
    throw RuntimeException("runNgi() is not implemented on Windows without shell execution");
#else
    int pipefd[2]{};
    if (pipe(pipefd) != 0)
    {
      throw RuntimeException("runNgi() failed to create output pipe: " + Str(std::strerror(errno)));
    }

    auto pid = fork();
    if (pid < 0)
    {
      auto error = errno;
      close(pipefd[0]);
      close(pipefd[1]);
      throw RuntimeException("runNgi() failed to fork ngi: " + Str(std::strerror(error)));
    }

    if (pid == 0)
    {
      close(pipefd[0]);
      if (dup2(pipefd[1], STDOUT_FILENO) < 0 || dup2(pipefd[1], STDERR_FILENO) < 0)
      {
        _exit(126);
      }
      close(pipefd[1]);
      execl(executable.c_str(), executable.c_str(), path.c_str(), static_cast<char *>(nullptr));
      _exit(127);
    }

    close(pipefd[1]);
    Str output;
    std::array<char, 4096> buffer{};
    while (true)
    {
      auto bytesRead = read(pipefd[0], buffer.data(), buffer.size());
      if (bytesRead > 0)
      {
        output.append(buffer.data(), static_cast<size_t>(bytesRead));
        continue;
      }
      if (bytesRead == 0)
      {
        break;
      }
      if (errno == EINTR)
      {
        continue;
      }
      auto error = errno;
      close(pipefd[0]);
      throw RuntimeException("runNgi() failed to read ngi output: " + Str(std::strerror(error)));
    }

    close(pipefd[0]);
    int status = 0;
    while (waitpid(pid, &status, 0) < 0)
    {
      if (errno == EINTR)
      {
        continue;
      }
      throw RuntimeException("runNgi() failed waiting for ngi: " + Str(std::strerror(errno)));
    }
    if (!WIFEXITED(status) || WEXITSTATUS(status) != 0)
    {
      output += "\n[process exited with status " + std::to_string(status) + "]";
    }
    return output;
#endif
  }

  static auto validate_ngi_script_path(const Str &path) -> bool
  {
    if (path.empty() || !path.ends_with(".ng"))
    {
      return false;
    }
    return std::ranges::all_of(path, [](unsigned char ch) {
      if (std::isalnum(ch) != 0 || ch == '/' || ch == '.' || ch == '_' || ch == '-')
      {
        return true;
      }
#ifdef _WIN32
      return ch == '\\' || ch == ':';
#else
      return false;
#endif
    });
  }

  static auto run_ngi_native(Str path) -> Str
  {
    if (!validate_ngi_script_path(path))
    {
      throw RuntimeException("runNgi() requires a .ng path containing only alnum, '/', '.', '_', or '-'");
    }
    auto executable = current_executable_path_native();
    if (executable.empty())
    {
      throw RuntimeException("runNgi() could not resolve current executable path");
    }
    return run_child_process(executable, path);
  }

  static auto regex_match_native(Str value, Str pattern) -> bool
  {
    try
    {
      return std::regex_search(value, std::regex(pattern));
    }
    catch (const std::regex_error &ex)
    {
      throw RuntimeException("regexMatch() invalid regex: " + Str(ex.what()));
    }
  }

  static auto native_allocation_count() -> int32_t &
  {
    static int32_t count = 0;
    return count;
  }

  static void release_unique_ptr_cell(const RuntimeRef<StorageCell> &slot)
  {
    auto handle = runtime_native_handle_value(slot);
    if (handle.address == 0)
    {
      if (slot)
      {
        slot->dropArmed = false;
        slot->lifecycleDropped = true;
      }
      return;
    }
    std::free(reinterpret_cast<void *>(handle.address));
    --native_allocation_count();
    slot->nativeHandles.insert_or_assign(0, NativeHandle{
                                                .typeName = handle.typeName,
                                                .address = 0,
                                                .owning = false,
                                            });
    slot->dropArmed = false;
    slot->lifecycleDropped = true;
  }

  static Map<Str, NGCallable> handlers{
    {"print",
     [](const NGSelf &, const NGEnv &context, const NGArgs &args) -> RuntimeRef<StorageCell> {
       auto nativeArgs = native_args_view(context, args);
       for (size_t i = 0; i < nativeArgs.size(); ++i)
       {
         std::cout << runtime_value_show(nativeArgs.slot_at(i));
         if (i + 1 != nativeArgs.size())
         {
           std::cout << ", ";
         }
       }
       std::cout << '\n';
       return unit_cell();
     }},
    {"assert",
     [](const NGSelf &, const NGEnv &context, const NGArgs &args) -> RuntimeRef<StorageCell> {
       auto nativeArgs = native_args_view(context, args);
       for (size_t i = 0; i < nativeArgs.size(); ++i)
       {
         auto flag = runtime_boolean_value(nativeArgs.slot_at(i));
         if (!flag.has_value())
         {
           throw RuntimeException("assert() requires a boolean at argument " + std::to_string(i + 1));
         }
         if (!*flag)
         {
           std::cerr << runtime_value_show(nativeArgs.slot_at(i));
           throw AssertionException();
         }
       }
       return unit_cell();
     }},
    {"len",
     [](const NGSelf &, const NGEnv &context, const NGArgs &args) -> RuntimeRef<StorageCell> {
       auto slot = require_arg_slot("len", native_args_view(context, args), 0, "an array or string");
       if (runtime_is_array_value(slot))
       {
         return numeral_cell_from_value<uint32_t>(static_cast<uint32_t>(runtime_array_length(slot)));
       }
       if (runtime_is_string_value(slot))
       {
         return numeral_cell_from_value<uint32_t>(static_cast<uint32_t>(runtime_string_value(slot).size()));
       }
       throw RuntimeException("len() requires an array or string at argument 1");
     }},
    {"readLine", NG::orgasm::wrap_native_callable(read_line_native)},
    {"readFile", NG::orgasm::wrap_native_callable(read_file_native)},
    {"writeFile", NG::orgasm::wrap_native_callable(write_file_native)},
    {"split",
     [](const NGSelf &, const NGEnv &context, const NGArgs &args) -> RuntimeRef<StorageCell> {
       auto nativeArgs = native_args_view(context, args);
       auto s = require_string_arg("split", nativeArgs, 0, "a source string");
       auto d = require_string_arg("split", nativeArgs, 1, "a string delimiter");
       auto items = makert<Vec<RuntimeRef<StorageCell>>>();
       if (d.empty())
       {
         for (char c : s)
         {
           items->push_back(make_runtime_string(Str(1, c)));
         }
       }
       else
       {
         size_t start = 0;
         size_t pos;
         while ((pos = s.find(d, start)) != Str::npos)
         {
           items->push_back(make_runtime_string(s.substr(start, pos - start)));
           start = pos + d.size();
         }
         items->push_back(make_runtime_string(s.substr(start)));
       }
       return make_runtime_array_cell(*items);
     }},
    {"join",
     [](const NGSelf &, const NGEnv &context, const NGArgs &args) -> RuntimeRef<StorageCell> {
       auto nativeArgs = native_args_view(context, args);
       auto arrObj = require_array_arg_slot("join", nativeArgs, 0, "an array");
       auto sep = require_string_arg("join", nativeArgs, 1, "a string separator");
       Str result;
       auto items = runtime_array_slots(arrObj);
       for (size_t i = 0; i < items.size(); ++i)
       {
         if (i > 0)
         {
           result += sep;
         }
         result += runtime_value_show(items[i]);
       }
       return make_runtime_string(result);
     }},
    {"trim", NG::orgasm::wrap_native_callable(trim_native)},
    {"contains", NG::orgasm::wrap_native_callable(contains_native)},
    {"replace", NG::orgasm::wrap_native_callable(replace_native)},
    {"startsWith", NG::orgasm::wrap_native_callable(starts_with_native)},
    {"endsWith", NG::orgasm::wrap_native_callable(ends_with_native)},
    {"toUpper", NG::orgasm::wrap_native_callable(to_upper_native)},
    {"toLower", NG::orgasm::wrap_native_callable(to_lower_native)},
    {"currentExecutablePath", NG::orgasm::wrap_native_callable(current_executable_path_native)},
    {"runNgi", NG::orgasm::wrap_native_callable(run_ngi_native)},
    {"regexMatch", NG::orgasm::wrap_native_callable(regex_match_native)},
    {"reverse",
     [](const NGSelf &, const NGEnv &context, const NGArgs &args) -> RuntimeRef<StorageCell> {
       auto nativeArgs = native_args_view(context, args);
       require_arg_count("reverse", nativeArgs, 1, 1);
       auto arrObj = require_array_arg_slot("reverse", nativeArgs, 0, "an array");
       auto items = makert<Vec<RuntimeRef<StorageCell>>>();
       auto src = runtime_array_slots(arrObj);
       for (auto it = src.rbegin(); it != src.rend(); ++it)
       {
         items->push_back(clone_runtime_storage_cell(*it, StorageClass::TEMPORARY));
       }
       return make_runtime_array_cell(*items);
     }},
    {"__ng_from_end",
     [](const NGSelf &, const NGEnv &context, const NGArgs &args) -> RuntimeRef<StorageCell> {
       auto value = require_numeric_arg<int32_t>("__ng_from_end", native_args_view(context, args), 0,
                                                 "a from-end index");
       return make_runtime_from_end_index(value);
     }},
    {"nativeMalloc",
     [](const NGSelf &, const NGEnv &context, const NGArgs &args) -> RuntimeRef<StorageCell> {
       auto size = require_numeric_arg<int32_t>("nativeMalloc", native_args_view(context, args), 0, "an allocation size");
       if (size < 0)
       {
         throw RuntimeException("nativeMalloc() requires a non-negative allocation size");
       }
       auto *ptr = std::malloc(static_cast<size_t>(size));
       if (!ptr && size != 0)
       {
         throw RuntimeException("nativeMalloc() failed");
       }
       ++native_allocation_count();
       auto cell = make_runtime_native_handle_cell("UniquePtr", reinterpret_cast<uintptr_t>(ptr), true);
       cell->runtimeType->dropCellHandler = release_unique_ptr_cell;
       if (context && context->symbols)
       {
         if (auto it = context->symbols->types.find("UniquePtr"); it != context->symbols->types.end())
         {
           cell->runtimeType = it->second;
           cell->layout = it->second->layout;
           cell->runtimeType->dropCellHandler = release_unique_ptr_cell;
         }
       }
       return cell;
     }},
    {"nativeFree",
     [](const NGSelf &, const NGEnv &context, const NGArgs &args) -> RuntimeRef<StorageCell> {
       auto slot = require_arg_slot("nativeFree", native_args_view(context, args), 0, "a native pointer");
       release_unique_ptr_cell(slot);
       return unit_cell();
     }},
    {"nativeOutstandingAllocations",
     [](const NGSelf &, const NGEnv &, const NGArgs &) -> RuntimeRef<StorageCell> {
       return numeral_cell_from_value<int32_t>(native_allocation_count());
     }},
    {"gcFree",
     [](const NGSelf &, const NGEnv &, const NGArgs &) -> RuntimeRef<StorageCell> {
       collect_managed_heap();
       return unit_cell();
     }},
  };

  static auto handlers_for(std::initializer_list<Str> names) -> Map<Str, NGCallable>
  {
    Map<Str, NGCallable> selected;
    for (const auto &name : names)
    {
      if (auto it = handlers.find(name); it != handlers.end())
      {
        selected.insert_or_assign(name, it->second);
      }
    }
    return selected;
  }

  void do_register()
  {
    auto preludeHandlers = handlers_for({
        "print",
        "assert",
        "len",
    });
    register_native_library("std.prelude", preludeHandlers);
    register_native_library("std.io", handlers_for({
                                      "readLine",
                                      "readFile",
                                      "writeFile",
                                      "currentExecutablePath",
                                      "runNgi",
                                  }));
    register_native_library("std.string", handlers_for({
                                          "split",
                                          "join",
                                          "trim",
                                          "contains",
                                          "replace",
                                          "startsWith",
                                          "endsWith",
                                          "toUpper",
                                          "toLower",
                                          "regexMatch",
                                      }));
    register_native_library("std.array", handlers_for({
                                         "reverse",
                                     }));
    register_native_library("std.memory", handlers_for({
                                          "nativeMalloc",
                                          "nativeFree",
                                          "nativeOutstandingAllocations",
                                          "gcFree",
                                      }));
    auto descriptor = makert<NG::module::NativeModuleDescriptor>();
    descriptor->moduleId = "std.prelude";
    descriptor->functions = preludeHandlers;
    descriptor->typeIndex = NG::typecheck::build_prelude_type_index();
    descriptor->exports.declared.insert("*");
    NG::module::get_module_registry().registerNativeModuleDescriptor(descriptor);
  };

  void register_vm_natives(NG::orgasm::VM &vm)
  {
    for (auto &[name, handler] : handlers)
    {
      vm.register_native_raw(name, [&vm, handler](const Vec<RuntimeRef<StorageCell>> &args) -> RuntimeRef<StorageCell> {
        auto env = make_runtime_env(vm.symbols());
        bind_native_arg_slots(env, args);
        return handler(unit_cell(), env, args);
      });
    }
  };

  Vec<Str> native_function_names()
  {
    Vec<Str> names;
    names.reserve(handlers.size());
    for (auto &[name, _] : handlers)
    {
      names.push_back(name);
    }
    return names;
  }

} // namespace NG::library::prelude
