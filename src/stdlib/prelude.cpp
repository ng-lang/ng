#include <intp/runtime.hpp>
#include <intp/runtime_numerals.hpp>
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


  static Map<Str, NGCallable> handlers{
    // ---- Core ----
    {"print",
     [](const NGSelf &self, const NGCtx &context, const NGArgs &args) -> RuntimeRef<NGObject>
     {
        auto nativeArgs = native_args_view(context, args);
        for (size_t i = 0; i < nativeArgs.size(); ++i)
        {
          auto value = nativeArgs.value_at(i);
          std::cout << runtime_value_show(value);
          if (i + 1 != nativeArgs.size())
          {
            std::cout << ", ";
          }
        }
        std::cout << '\n';
        return makert<NGUnit>();
      }},
    {"assert",
     [](const NGSelf &self, const NGCtx &context, const NGArgs &args) -> RuntimeRef<NGObject>
     {
       auto flags = require_all_args_as<NGBoolean>("assert", native_args_view(context, args), "a boolean");
       for (const auto &flag : flags)
         {
          if (!flag->value)
          {
             std::cerr << runtime_value_show(flag);
             throw AssertionException();
           }
         }
         return makert<NGUnit>();
       }},
    {"len",
     [](const NGSelf &self, const NGCtx &context, const NGArgs &args) -> RuntimeRef<NGObject>
     {
        auto value =
            require_arg_as_one_of<NGArray, NGString>("len", native_args_view(context, args), 0, "an array or string");
        return std::visit(
            [](const auto &typed) -> RuntimeRef<NGObject> {
              using TValue = std::decay_t<decltype(typed)>;
              if constexpr (std::is_same_v<TValue, RuntimeRef<NGArray>>)
              {
                return makert<NGIntegral<uint32_t>>(static_cast<uint32_t>(typed->header_length()));
              }
              else
              {
                return makert<NGIntegral<uint32_t>>(static_cast<uint32_t>(string_length(*typed)));
              }
            },
            value);
       }},

    // C1: Basic I/O
    {"readLine",
     [](const NGSelf &self, const NGCtx &context, const NGArgs &args) -> RuntimeRef<NGObject>
     {
       Str line;
       std::getline(std::cin, line);
       return makert<NGString>(line);
     }},
    {"readFile",
     [](const NGSelf &self, const NGCtx &context, const NGArgs &args) -> RuntimeRef<NGObject>
      {
          auto pathStr = require_arg_as<NGString>("readFile", native_args_view(context, args), 0, "a string path");
         auto path = pathStr->payload_value();
         std::ifstream file(path);
        if (!file.is_open())
        {
          throw RuntimeException("readFile() failed to open: " + path);
        }
       std::string content((std::istreambuf_iterator<char>(file)),
                           std::istreambuf_iterator<char>());
       return makert<NGString>(content);
     }},
    {"writeFile",
     [](const NGSelf &self, const NGCtx &context, const NGArgs &args) -> RuntimeRef<NGObject>
      {
          auto nativeArgs = native_args_view(context, args);
          auto pathStr = require_arg_as<NGString>("writeFile", nativeArgs, 0, "a string path");
          auto contentStr = require_arg_as<NGString>("writeFile", nativeArgs, 1, "string content");
         auto path = pathStr->payload_value();
         auto content = contentStr->payload_value();
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
        return makert<NGUnit>();
       }},

    // C2: String operations
    {"split",
     [](const NGSelf &self, const NGCtx &context, const NGArgs &args) -> RuntimeRef<NGObject>
      {
          auto nativeArgs = native_args_view(context, args);
          auto strObj = require_arg_as<NGString>("split", nativeArgs, 0, "a source string");
          auto delimObj = require_arg_as<NGString>("split", nativeArgs, 1, "a string delimiter");
        auto items = makert<Vec<RuntimeRef<NGObject>>>();
        auto s = strObj->payload_value();
        auto d = delimObj->payload_value();
        if (d.empty())
       {
         for (char c : s)
         {
           items->push_back(makert<NGString>(Str(1, c)));
         }
       }
       else
       {
         size_t start = 0;
         size_t pos;
         while ((pos = s.find(d, start)) != Str::npos)
         {
           items->push_back(makert<NGString>(s.substr(start, pos - start)));
           start = pos + d.size();
         }
         items->push_back(makert<NGString>(s.substr(start)));
       }
       return makert<NGArray>(*items);
     }},
    {"join",
     [](const NGSelf &self, const NGCtx &context, const NGArgs &args) -> RuntimeRef<NGObject>
      {
          auto nativeArgs = native_args_view(context, args);
          auto arrObj = require_arg_as<NGArray>("join", nativeArgs, 0, "an array");
          auto sepObj = require_arg_as<NGString>("join", nativeArgs, 1, "a string separator");
        Str result;
        auto items = arrObj->payload_items();
        for (size_t i = 0; i < items.size(); ++i)
        {
          if (i > 0)
            result += sepObj->payload_value();
          result += runtime_value_show(items[i]);
       }
       return makert<NGString>(result);
     }},
    {"trim",
     [](const NGSelf &self, const NGCtx &context, const NGArgs &args) -> RuntimeRef<NGObject>
      {
          auto strObj = require_arg_as<NGString>("trim", native_args_view(context, args), 0, "a string");
         auto s = strObj->payload_value();
        size_t start = s.find_first_not_of(" \t\n\r");
       size_t end = s.find_last_not_of(" \t\n\r");
       if (start == Str::npos)
       {
         return makert<NGString>("");
       }
       else
       {
         return makert<NGString>(s.substr(start, end - start + 1));
       }
     }},
    {"contains",
     [](const NGSelf &self, const NGCtx &context, const NGArgs &args) -> RuntimeRef<NGObject>
      {
          auto nativeArgs = native_args_view(context, args);
          require_arg_count("contains", nativeArgs, 2, 2);
          auto strObj = require_arg_as<NGString>("contains", nativeArgs, 0, "a source string");
          auto subObj = require_arg_as<NGString>("contains", nativeArgs, 1, "a string substring");
         return makert<NGBoolean>(strObj->payload_value().find(subObj->payload_value()) != Str::npos);
        }},
    {"replace",
     [](const NGSelf &self, const NGCtx &context, const NGArgs &args) -> RuntimeRef<NGObject>
      {
          auto nativeArgs = native_args_view(context, args);
          auto strObj = require_arg_as<NGString>("replace", nativeArgs, 0, "a source string");
          auto oldObj = require_arg_as<NGString>("replace", nativeArgs, 1, "an old string");
          auto newObj = require_arg_as<NGString>("replace", nativeArgs, 2, "a replacement string");
         Str result = strObj->payload_value();
         auto oldValue = oldObj->payload_value();
         auto newValue = newObj->payload_value();
        if (!oldValue.empty())
        {
          size_t pos = 0;
          while ((pos = result.find(oldValue, pos)) != Str::npos)
          {
            result.replace(pos, oldValue.size(), newValue);
            pos += newValue.size();
          }
        }
       return makert<NGString>(result);
     }},
    {"startsWith",
     [](const NGSelf &self, const NGCtx &context, const NGArgs &args) -> RuntimeRef<NGObject>
      {
          auto nativeArgs = native_args_view(context, args);
          auto strObj = require_arg_as<NGString>("startsWith", nativeArgs, 0, "a source string");
          auto prefixObj = require_arg_as<NGString>("startsWith", nativeArgs, 1, "a prefix string");
         return makert<NGBoolean>(strObj->payload_value().starts_with(prefixObj->payload_value()));
       }},
    {"endsWith",
     [](const NGSelf &self, const NGCtx &context, const NGArgs &args) -> RuntimeRef<NGObject>
      {
          auto nativeArgs = native_args_view(context, args);
          auto strObj = require_arg_as<NGString>("endsWith", nativeArgs, 0, "a source string");
          auto suffixObj = require_arg_as<NGString>("endsWith", nativeArgs, 1, "a suffix string");
         return makert<NGBoolean>(strObj->payload_value().ends_with(suffixObj->payload_value()));
       }},
    {"toUpper",
     [](const NGSelf &self, const NGCtx &context, const NGArgs &args) -> RuntimeRef<NGObject>
      {
          auto strObj = require_arg_as<NGString>("toUpper", native_args_view(context, args), 0, "a string");
          Str result = strObj->payload_value();
        std::transform(result.begin(), result.end(), result.begin(), [](unsigned char ch) {
          return static_cast<char>(std::toupper(ch));
        });
        return makert<NGString>(result);
      }},
    {"toLower",
     [](const NGSelf &self, const NGCtx &context, const NGArgs &args) -> RuntimeRef<NGObject>
      {
          auto strObj = require_arg_as<NGString>("toLower", native_args_view(context, args), 0, "a string");
          Str result = strObj->payload_value();
        std::transform(result.begin(), result.end(), result.begin(), [](unsigned char ch) {
          return static_cast<char>(std::tolower(ch));
        });
        return makert<NGString>(result);
      }},

    // C3: Collection operations
    {"reverse",
      [](const NGSelf &self, const NGCtx &context, const NGArgs &args) -> RuntimeRef<NGObject>
       {
          auto nativeArgs = native_args_view(context, args);
          require_arg_count("reverse", nativeArgs, 1, 1);
        auto arrObj = require_arg_as<NGArray>("reverse", nativeArgs, 0, "an array");
        auto items = makert<Vec<RuntimeRef<NGObject>>>();
        auto src = arrObj->payload_items();
        for (auto it = src.rbegin(); it != src.rend(); ++it)
        {
          items->push_back(*it);
        }
        return makert<NGArray>(*items);
       }},
    {"range",
      [](const NGSelf &self, const NGCtx &context, const NGArgs &args) -> RuntimeRef<NGObject>
       {
        auto nativeArgs = native_args_view(context, args);
        auto startNum = require_arg_as<NGIntegral<int32_t>>("range", nativeArgs, 0, "a start integer");
        auto endNum = require_arg_as<NGIntegral<int32_t>>("range", nativeArgs, 1, "an end integer");
        auto items = makert<Vec<RuntimeRef<NGObject>>>();
       int32_t step = startNum->value <= endNum->value ? 1 : -1;
       for (int32_t i = startNum->value; step > 0 ? i < endNum->value : i > endNum->value; i += step)
       {
         items->push_back(makert<NGIntegral<int32_t>>(i));
       }
       return makert<NGArray>(*items);
     }},
    {"slice",
      [](const NGSelf &self, const NGCtx &context, const NGArgs &args) -> RuntimeRef<NGObject>
       {
        auto nativeArgs = native_args_view(context, args);
        auto arrObj = require_arg_as<NGArray>("slice", nativeArgs, 0, "an array");
        auto startNum = require_arg_as<NGIntegral<int32_t>>("slice", nativeArgs, 1, "a start index");
        auto endNum = require_arg_as<NGIntegral<int32_t>>("slice", nativeArgs, 2, "an end index");
        auto src = arrObj->payload_items();
        auto items = makert<Vec<RuntimeRef<NGObject>>>();
        int32_t s = std::max(0, startNum->value);
        int32_t e = std::min(static_cast<int32_t>(src.size()), endNum->value);
        for (int32_t i = s; i < e; ++i)
        {
          items->push_back(src[i]);
        }
        return makert<NGArray>(*items);
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
      vm.register_native_raw(name, [handler](const Vec<RuntimeRef<NGObject>> &args) -> RuntimeRef<NGObject> {
        auto context = makert<NGContext>();
        bind_native_arg_slots(context, args);
        return handler(makert<NGUnit>(), context, args);
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
