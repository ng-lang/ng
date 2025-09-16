#include "../test.hpp"
#include <catch2/matchers/catch_matchers_string.hpp>
#include <typecheck/typecheck.hpp>

using namespace Catch::Matchers;
using namespace NG::typecheck;

inline void typecheck_failure(const Str &source, const Str &expected_error = "")
{
    auto ast = parse(source);

    REQUIRE(ast != nullptr);

    bool typecheckingExceptionFound = false;

    try
    {
        auto typeIndex = type_check(ast);
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
    destroyast(ast);
    REQUIRE(typecheckingExceptionFound);
}

inline void check_type_tag(TypeInfo &typeInfo, typeinfo_tag typeinfo_tag)
{
    REQUIRE(typeInfo.tag() == typeinfo_tag);
}