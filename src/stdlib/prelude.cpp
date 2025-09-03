#include <intp/runtime.hpp>

namespace NG::library::prelude
{

    using namespace NG::runtime;

    static Map<Str, NGInvocable> handlers{
        {"print", [](const NGSelf &self, const NGCtx &context, const NGInvCtx &invCtx)
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
        {"assert", [](const NGSelf &self, const NGCtx &context, const NGInvCtx &invCtx)
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
    };

    void do_register()
    {
        register_native_library("std.prelude", handlers);
    };
}