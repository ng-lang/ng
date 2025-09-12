#include "../test.hpp"
#include <catch2/matchers/catch_matchers_string.hpp>
#include <typecheck/typecheck.hpp>

using namespace Catch::Matchers;
using namespace NG::typecheck;

inline void typecheck_failure(const Str &source, const Str &expected_error = "")
{
    auto astResult = parse(source);

    REQUIRE(astResult.has_value());

    bool typecheckingExceptionFound = false;

    try
    {
        auto typeIndex = type_check(*astResult);
    }
    catch (TypeCheckingException &ex)
    {
        typecheckingExceptionFound = true;
        if (!expected_error.empty())
        {
            REQUIRE_THAT(ex.what(), ContainsSubstring(expected_error));
        }
        else
        {
            debug_log(ex.what());
        }
    }
    destroyast(*astResult);
    REQUIRE(typecheckingExceptionFound);
}

inline void check_primitive_type(TypeInfo &typeInfo, primitive_tag primitive_tag)
{
    PrimitiveType &primitive = static_cast<PrimitiveType &>(typeInfo);

    REQUIRE(primitive.primitive() == primitive_tag);
}