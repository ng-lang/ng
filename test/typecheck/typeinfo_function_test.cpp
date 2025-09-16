#include "typecheck_utils.hpp"
#include <algorithm>
#include <functional>

inline auto make_function(typeinfo_tag returnTypeTag, Vec<typeinfo_tag> paramTypeTags, Vec<typeinfo_tag> paramWithDefault = {}) -> FunctionType
{
    Vec<CheckingRef<TypeInfo>> parameterTypes{};

    std::transform(paramTypeTags.begin(), paramTypeTags.end(), std::back_inserter(parameterTypes),
                   [](auto tag)
                   {
                       return makecheck<PrimitiveType>(tag);
                   });

    if (!paramWithDefault.empty())
    {
        std::transform(paramWithDefault.begin(), paramWithDefault.end(), std::back_inserter(parameterTypes),
                       [](auto tag)
                       {
                           return makecheck<ParamWithDefaultValueType>(makecheck<PrimitiveType>(tag));
                       });
    }
    return {makecheck<PrimitiveType>(returnTypeTag), parameterTypes};
}

TEST_CASE("make_function works", "[TestUtil][TypeCheck][Function]")
{
    REQUIRE(make_function(typeinfo_tag::I32, {typeinfo_tag::I32}).repr() == "fun (i32) -> i32");
    REQUIRE(make_function(typeinfo_tag::I32, {}, {typeinfo_tag::I32}).repr() == "fun (i32 = default) -> i32");
    REQUIRE(make_function(typeinfo_tag::UNIT, {typeinfo_tag::I32}).repr() == "fun (i32) -> unit");
    REQUIRE(make_function(typeinfo_tag::I32, {typeinfo_tag::I32, typeinfo_tag::I32}).repr() == "fun (i32, i32) -> i32");
    REQUIRE(make_function(typeinfo_tag::UNIT, {typeinfo_tag::I32, typeinfo_tag::I32}).repr() == "fun (i32, i32) -> unit");
    REQUIRE(make_function(typeinfo_tag::UNIT, {typeinfo_tag::I32}, {typeinfo_tag::I32}).repr() == "fun (i32, i32 = default) -> unit");
    REQUIRE(make_function(typeinfo_tag::UNIT, {}, {typeinfo_tag::I32, typeinfo_tag::I32}).repr() == "fun (i32 = default, i32 = default) -> unit");
}

TEST_CASE("test FunctionType#match", "[TypeCheck][Function][Covariance]")
{
    // fun (i32, i32) -> i32
    auto basic_function = make_function(typeinfo_tag::I32, {typeinfo_tag::I32, typeinfo_tag::I32});
    // self-reflexivity
    REQUIRE(basic_function.match(basic_function));

    // wider return type fun (i32, i32) -> i64
    auto wider_return_type = make_function(typeinfo_tag::I64, {typeinfo_tag::I32, typeinfo_tag::I32});
    REQUIRE(wider_return_type.match(basic_function));
    // antisymmetry
    REQUIRE(!basic_function.match(wider_return_type));

    // narrower return type fun (i32, i32) -> i16
    auto narrower_return_type = make_function(typeinfo_tag::I16, {typeinfo_tag::I32, typeinfo_tag::I32});
    REQUIRE(!narrower_return_type.match(basic_function));
    REQUIRE(basic_function.match(narrower_return_type));

    // transitive
    REQUIRE(wider_return_type.match(narrower_return_type));

    // arity mismatch (no defaults): fun (i32) -> i32 vs fun (i32, i32) -> i32
    auto arity_one = make_function(typeinfo_tag::I32, {typeinfo_tag::I32});
    REQUIRE(!arity_one.match(basic_function));
    REQUIRE(!basic_function.match(arity_one));

    // fun (i32, i32 = default) -> i32
    auto same_signature_with_default_parameters =
        make_function(typeinfo_tag::I32, {typeinfo_tag::I32}, {typeinfo_tag::I32});

    REQUIRE(same_signature_with_default_parameters.match(basic_function));
    REQUIRE(basic_function.match(same_signature_with_default_parameters));

    // fun (i32, i32, i32 = default) -> i32;
    auto same_signature_with_extra_default_params =
        make_function(typeinfo_tag::I32, {typeinfo_tag::I32, typeinfo_tag::I32}, {typeinfo_tag::I32});

    REQUIRE(basic_function.match(same_signature_with_extra_default_params));
    REQUIRE(!same_signature_with_extra_default_params.match(basic_function));
    // fun (i32, i32, i32 = default) -> i32;

    // fun (i16, i16) -> i32;
    auto narrower_params =
        make_function(typeinfo_tag::I32, {typeinfo_tag::I16, typeinfo_tag::I16});

    REQUIRE(narrower_params.match(basic_function));
    REQUIRE(!basic_function.match(narrower_params));

    // fun (i64, i64) -> i32;
    auto wider_params =
        make_function(typeinfo_tag::I32, {typeinfo_tag::I64, typeinfo_tag::I64});

    REQUIRE(!wider_params.match(basic_function));
    REQUIRE(basic_function.match(wider_params));
}