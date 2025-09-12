#include "../test.hpp"
#include <intp/runtime_numerals.hpp>

using namespace NG::runtime;

TEST_CASE("test NGIntegral<T>", "[Numeral][Runtime]")
{
    auto a = makert<NGIntegral<int>>(1);
    auto b = makert<NGIntegral<unsigned int>>(2);
    auto c = makert<NGFloatingPoint<float>>(3.0);

    auto divided_by = a->opDividedBy(b);

    REQUIRE(divided_by->opEquals(makert<NGIntegral<int>>(0)));
}

TEST_CASE("test NGIntegral<T> dbz", "[Numeral][Runtime][Failure]")
{
    auto a = makert<NGIntegral<int>>(1);
    auto zero = makert<NGIntegral<int>>(0);

    REQUIRE_THROWS_MATCHES(a->opDividedBy(zero), NG::RuntimeException,
                           MessageMatches(ContainsSubstring("Division by zero")));
    REQUIRE_THROWS_MATCHES(a->opModulus(zero), NG::RuntimeException,
                           MessageMatches(ContainsSubstring("Modulus by zero")));
}

TEST_CASE("test NGFloatingPoint<T>", "[Numeral][Runtime][Failure]")
{
    auto a = makert<NGFloatingPoint<float>>(1.0);
    auto b = makert<NGFloatingPoint<double>>(2.0);

    REQUIRE(a->signedness());

    REQUIRE(a->opLessEqual(b));
    REQUIRE(b->opGreaterEqual(a));
}

TEST_CASE("test NGInteger<T> negates", "[Numeral][Runtime][Failure]")
{
    auto a = makert<NGIntegral<int>>(std::numeric_limits<int>::min());

    REQUIRE(a->signedness());

    REQUIRE_THROWS_MATCHES(a->opNegate(), RuntimeException,
                           MessageMatches(ContainsSubstring("Overflow on negation")));
}

TEST_CASE("NGIntegral<unsigned> negates: disallowed", "[Numeral][Runtime][Failure]")
{
    auto u = makert<NGIntegral<unsigned int>>(42u);
    REQUIRE_THROWS_MATCHES(u->opNegate(), RuntimeException,
                           MessageMatches(ContainsSubstring("Cannot negate unsigned integers")));
}

TEST_CASE("NGFloatingPoint<T> negates", "[Numeral][Runtime]")
{
    auto f = makert<NGFloatingPoint<float>>(-3.5f);
    auto n = f->opNegate();
    REQUIRE(n->opEquals(makert<NGFloatingPoint<float>>(3.5f)));
}