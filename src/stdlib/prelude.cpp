#include <intp/runtime.hpp>
#include <intp/runtime_numerals.hpp>
#include <orgasm/vm.hpp>
#include <algorithm>
#include <cctype>
#include <fstream>
#include <iostream>

namespace NG::library::prelude
{

  using namespace NG::runtime;

  static Map<Str, NGInvocable> handlers{
    // ---- Core ----
    {"print",
     [](const NGSelf &self, const NGCtx &context, const NGInvCtx &invCtx)
     {
       Vec<RuntimeRef<NGObject>> &params = invCtx->params;
       for (size_t i = 0; i < params.size(); ++i)
       {
         std::cout << params[i]->show();
         if (i != params.size() - 1)
         {
           std::cout << ", ";
         }
       }
       std::cout << '\n';
     }},
    {"assert",
     [](const NGSelf &self, const NGCtx &context, const NGInvCtx &invCtx)
     {
       for (const auto &param : invCtx->params)
       {
         auto ngBool = std::dynamic_pointer_cast<NGBoolean>(param);
         if (ngBool == nullptr || !ngBool->value)
         {
           std::cerr << param->show();
           throw AssertionException();
         }
       }
     }},
    {"len",
     [](const NGSelf &self, const NGCtx &context, const NGInvCtx &invCtx)
     {
       if (invCtx->params.empty())
       {
         throw RuntimeException("len() requires an argument");
       }
       auto &param = invCtx->params[0];
       if (auto arr = std::dynamic_pointer_cast<NGArray>(param))
       {
         context->retVal = makert<NGIntegral<uint32_t>>(static_cast<uint32_t>(arr->items->size()));
       }
        else if (auto str = std::dynamic_pointer_cast<NGString>(param))
        {
          context->retVal = makert<NGIntegral<uint32_t>>(static_cast<uint32_t>(str->value.size()));
        }
        else
        {
          throw RuntimeException("len() requires an array or string argument");
        }
      }},

    // C1: Basic I/O
    {"readLine",
     [](const NGSelf &self, const NGCtx &context, const NGInvCtx &invCtx)
     {
       Str line;
       std::getline(std::cin, line);
       context->retVal = makert<NGString>(line);
     }},
    {"readFile",
     [](const NGSelf &self, const NGCtx &context, const NGInvCtx &invCtx)
     {
       if (invCtx->params.empty())
       {
         throw RuntimeException("readFile() requires a path argument");
       }
       auto pathStr = std::dynamic_pointer_cast<NGString>(invCtx->params[0]);
       if (!pathStr)
       {
         throw RuntimeException("readFile() requires a string path argument");
       }
       std::ifstream file(pathStr->value);
       if (!file.is_open())
       {
         throw RuntimeException("readFile() failed to open: " + pathStr->value);
       }
       std::string content((std::istreambuf_iterator<char>(file)),
                           std::istreambuf_iterator<char>());
       context->retVal = makert<NGString>(content);
     }},
    {"writeFile",
     [](const NGSelf &self, const NGCtx &context, const NGInvCtx &invCtx)
     {
       if (invCtx->params.size() < 2)
       {
         throw RuntimeException("writeFile() requires path and content arguments");
       }
       auto pathStr = std::dynamic_pointer_cast<NGString>(invCtx->params[0]);
       auto contentStr = std::dynamic_pointer_cast<NGString>(invCtx->params[1]);
       if (!pathStr || !contentStr)
       {
         throw RuntimeException("writeFile() requires string arguments");
       }
       std::ofstream file(pathStr->value);
       if (!file.is_open())
       {
         throw RuntimeException("writeFile() failed to open: " + pathStr->value);
       }
       file << contentStr->value;
     }},

    // C2: String operations
    {"split",
     [](const NGSelf &self, const NGCtx &context, const NGInvCtx &invCtx)
     {
       if (invCtx->params.size() < 2)
       {
         throw RuntimeException("split() requires string and delimiter arguments");
       }
       auto strObj = std::dynamic_pointer_cast<NGString>(invCtx->params[0]);
       auto delimObj = std::dynamic_pointer_cast<NGString>(invCtx->params[1]);
       if (!strObj || !delimObj)
       {
         throw RuntimeException("split() requires string arguments");
       }
       auto items = makert<Vec<RuntimeRef<NGObject>>>();
       const auto &s = strObj->value;
       const auto &d = delimObj->value;
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
       context->retVal = makert<NGArray>(*items);
     }},
    {"join",
     [](const NGSelf &self, const NGCtx &context, const NGInvCtx &invCtx)
     {
       if (invCtx->params.size() < 2)
       {
         throw RuntimeException("join() requires array and separator arguments");
       }
       auto arrObj = std::dynamic_pointer_cast<NGArray>(invCtx->params[0]);
       auto sepObj = std::dynamic_pointer_cast<NGString>(invCtx->params[1]);
       if (!arrObj || !sepObj)
       {
         throw RuntimeException("join() requires (array, string) arguments");
       }
       Str result;
       auto &items = *arrObj->items;
       for (size_t i = 0; i < items.size(); ++i)
       {
         if (i > 0)
           result += sepObj->value;
         result += items[i]->show();
       }
       context->retVal = makert<NGString>(result);
     }},
    {"trim",
     [](const NGSelf &self, const NGCtx &context, const NGInvCtx &invCtx)
     {
       if (invCtx->params.empty())
       {
         throw RuntimeException("trim() requires a string argument");
       }
       auto strObj = std::dynamic_pointer_cast<NGString>(invCtx->params[0]);
       if (!strObj)
       {
         throw RuntimeException("trim() requires a string argument");
       }
       auto &s = strObj->value;
       size_t start = s.find_first_not_of(" \t\n\r");
       size_t end = s.find_last_not_of(" \t\n\r");
       if (start == Str::npos)
       {
         context->retVal = makert<NGString>("");
       }
       else
       {
         context->retVal = makert<NGString>(s.substr(start, end - start + 1));
       }
     }},
    {"contains",
     [](const NGSelf &self, const NGCtx &context, const NGInvCtx &invCtx)
     {
       if (invCtx->params.size() < 2)
       {
         throw RuntimeException("contains() requires two arguments");
       }
        if (auto strObj = std::dynamic_pointer_cast<NGString>(invCtx->params[0]))
        {
          auto subObj = std::dynamic_pointer_cast<NGString>(invCtx->params[1]);
          if (!subObj)
          {
           throw RuntimeException("contains() on string requires a string substring");
          }
          context->retVal = makert<NGBoolean>(strObj->value.find(subObj->value) != Str::npos);
        }
        else
        {
          throw RuntimeException("contains() requires a string as first argument");
        }
      }},
    {"replace",
     [](const NGSelf &self, const NGCtx &context, const NGInvCtx &invCtx)
     {
       if (invCtx->params.size() < 3)
       {
         throw RuntimeException("replace() requires string, old, and new arguments");
       }
       auto strObj = std::dynamic_pointer_cast<NGString>(invCtx->params[0]);
       auto oldObj = std::dynamic_pointer_cast<NGString>(invCtx->params[1]);
       auto newObj = std::dynamic_pointer_cast<NGString>(invCtx->params[2]);
       if (!strObj || !oldObj || !newObj)
       {
         throw RuntimeException("replace() requires string arguments");
       }
       Str result = strObj->value;
       if (!oldObj->value.empty())
       {
         size_t pos = 0;
         while ((pos = result.find(oldObj->value, pos)) != Str::npos)
         {
           result.replace(pos, oldObj->value.size(), newObj->value);
           pos += newObj->value.size();
         }
       }
       context->retVal = makert<NGString>(result);
     }},
    {"startsWith",
     [](const NGSelf &self, const NGCtx &context, const NGInvCtx &invCtx)
     {
       if (invCtx->params.size() < 2)
       {
         throw RuntimeException("startsWith() requires two string arguments");
       }
       auto strObj = std::dynamic_pointer_cast<NGString>(invCtx->params[0]);
       auto prefixObj = std::dynamic_pointer_cast<NGString>(invCtx->params[1]);
       if (!strObj || !prefixObj)
       {
         throw RuntimeException("startsWith() requires string arguments");
       }
       context->retVal = makert<NGBoolean>(strObj->value.starts_with(prefixObj->value));
     }},
    {"endsWith",
     [](const NGSelf &self, const NGCtx &context, const NGInvCtx &invCtx)
     {
       if (invCtx->params.size() < 2)
       {
         throw RuntimeException("endsWith() requires two string arguments");
       }
       auto strObj = std::dynamic_pointer_cast<NGString>(invCtx->params[0]);
       auto suffixObj = std::dynamic_pointer_cast<NGString>(invCtx->params[1]);
       if (!strObj || !suffixObj)
       {
         throw RuntimeException("endsWith() requires string arguments");
       }
       context->retVal = makert<NGBoolean>(strObj->value.ends_with(suffixObj->value));
     }},
    {"toUpper",
     [](const NGSelf &self, const NGCtx &context, const NGInvCtx &invCtx)
     {
       if (invCtx->params.empty())
       {
         throw RuntimeException("toUpper() requires a string argument");
        }
        auto strObj = std::dynamic_pointer_cast<NGString>(invCtx->params[0]);
        if (!strObj)
        {
          throw RuntimeException("toUpper() requires a string argument");
        }
        Str result = strObj->value;
        std::transform(result.begin(), result.end(), result.begin(), [](unsigned char ch) {
          return static_cast<char>(std::toupper(ch));
        });
        context->retVal = makert<NGString>(result);
      }},
    {"toLower",
     [](const NGSelf &self, const NGCtx &context, const NGInvCtx &invCtx)
     {
       if (invCtx->params.empty())
       {
         throw RuntimeException("toLower() requires a string argument");
        }
        auto strObj = std::dynamic_pointer_cast<NGString>(invCtx->params[0]);
        if (!strObj)
        {
          throw RuntimeException("toLower() requires a string argument");
        }
        Str result = strObj->value;
        std::transform(result.begin(), result.end(), result.begin(), [](unsigned char ch) {
          return static_cast<char>(std::tolower(ch));
        });
        context->retVal = makert<NGString>(result);
      }},

    // C3: Collection operations
    {"reverse",
     [](const NGSelf &self, const NGCtx &context, const NGInvCtx &invCtx)
     {
       if (invCtx->params.empty())
       {
         throw RuntimeException("reverse() requires an argument");
       }
       if (auto arrObj = std::dynamic_pointer_cast<NGArray>(invCtx->params[0]))
        {
          auto items = makert<Vec<RuntimeRef<NGObject>>>();
          auto &src = *arrObj->items;
          for (auto it = src.rbegin(); it != src.rend(); ++it)
          {
            items->push_back(*it);
          }
          context->retVal = makert<NGArray>(*items);
        }
        else
        {
          throw RuntimeException("reverse() requires an array argument");
        }
      }},
    {"range",
     [](const NGSelf &self, const NGCtx &context, const NGInvCtx &invCtx)
     {
       if (invCtx->params.size() < 2)
       {
         throw RuntimeException("range() requires start and end arguments");
       }
       auto startNum = std::dynamic_pointer_cast<NGIntegral<int32_t>>(invCtx->params[0]);
       auto endNum = std::dynamic_pointer_cast<NGIntegral<int32_t>>(invCtx->params[1]);
       if (!startNum || !endNum)
       {
         throw RuntimeException("range() requires integer arguments");
       }
       auto items = makert<Vec<RuntimeRef<NGObject>>>();
       int32_t step = startNum->value <= endNum->value ? 1 : -1;
       for (int32_t i = startNum->value; step > 0 ? i < endNum->value : i > endNum->value; i += step)
       {
         items->push_back(makert<NGIntegral<int32_t>>(i));
       }
       context->retVal = makert<NGArray>(*items);
     }},
    {"slice",
     [](const NGSelf &self, const NGCtx &context, const NGInvCtx &invCtx)
     {
       if (invCtx->params.size() < 3)
       {
         throw RuntimeException("slice() requires (collection, start, end) arguments");
       }
       auto startNum = std::dynamic_pointer_cast<NGIntegral<int32_t>>(invCtx->params[1]);
       auto endNum = std::dynamic_pointer_cast<NGIntegral<int32_t>>(invCtx->params[2]);
       if (!startNum || !endNum)
       {
         throw RuntimeException("slice() requires integer start and end indices");
       }
       if (auto arrObj = std::dynamic_pointer_cast<NGArray>(invCtx->params[0]))
        {
          auto &src = *arrObj->items;
          auto items = makert<Vec<RuntimeRef<NGObject>>>();
          int32_t s = std::max(0, startNum->value);
          int32_t e = std::min(static_cast<int32_t>(src.size()), endNum->value);
         for (int32_t i = s; i < e; ++i)
         {
           items->push_back(src[i]);
          }
          context->retVal = makert<NGArray>(*items);
        }
        else
        {
          throw RuntimeException("slice() requires an array as first argument");
        }
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
      vm.register_native_raw(name, [handler](const Vec<RuntimeRef<NGObject>> &args) -> RuntimeRef<NGObject>
      {
        auto context = makert<NGContext>();
        auto invCtx = makert<NGInvocationContext>();
        invCtx->params = args;
        RuntimeRef<NGObject> self = makert<NGUnit>();
        handler(self, context, invCtx);
        return context->retVal ? context->retVal : makert<NGUnit>();
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
