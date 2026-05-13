#include <intp/runtime.hpp>
#include <intp/runtime_numerals.hpp>
#include <orgasm/native_bridge.hpp>
#include <orgasm/vm.hpp>
#include <runtime/native_marshaling.hpp>
#include <runtime/value_access.hpp>
#include <runtime/string_layout_access.hpp>
#include <algorithm>
#include <cctype>
#include <fstream>
#include <iostream>

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
    {"range",
     [](const NGSelf &, const NGEnv &context, const NGArgs &args) -> RuntimeRef<StorageCell> {
       auto nativeArgs = native_args_view(context, args);
       auto startNum = require_numeric_arg<int32_t>("range", nativeArgs, 0, "a start integer");
       auto endNum = require_numeric_arg<int32_t>("range", nativeArgs, 1, "an end integer");
       auto items = makert<Vec<RuntimeRef<StorageCell>>>();
       int32_t step = startNum <= endNum ? 1 : -1;
       for (int32_t i = startNum; step > 0 ? i < endNum : i > endNum; i += step)
       {
         items->push_back(numeral_cell_from_value<int32_t>(i));
       }
       return make_runtime_array_cell(*items);
     }},
    {"slice",
     [](const NGSelf &, const NGEnv &context, const NGArgs &args) -> RuntimeRef<StorageCell> {
       auto nativeArgs = native_args_view(context, args);
       auto arrObj = require_array_arg_slot("slice", nativeArgs, 0, "an array");
       auto startNum = require_numeric_arg<int32_t>("slice", nativeArgs, 1, "a start index");
       auto endNum = require_numeric_arg<int32_t>("slice", nativeArgs, 2, "an end index");
       auto src = runtime_array_slots(arrObj);
       auto items = makert<Vec<RuntimeRef<StorageCell>>>();
       int32_t s = std::max(0, startNum);
       int32_t e = std::min(static_cast<int32_t>(src.size()), endNum);
       for (int32_t i = s; i < e; ++i)
       {
         items->push_back(clone_runtime_storage_cell(src[i], StorageClass::TEMPORARY));
       }
       return make_runtime_array_cell(*items);
     }},
  };

  void do_register()
  {
    register_native_library("std.prelude", handlers);
  };

  void register_vm_natives(NG::orgasm::VM &vm)
  {
    for (auto &[name, handler] : handlers)
    {
      vm.register_native_raw(name, [handler](const Vec<RuntimeRef<StorageCell>> &args) -> RuntimeRef<StorageCell> {
        auto env = make_runtime_env();
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
