// AI-generated code; reviewed for this repository's vNext rewrite.
#include "test.hpp"
#include "driver.hpp"

#include <sstream>

namespace
{
  auto run(const std::vector<std::string_view> &arguments, std::string &output, std::string &errors) -> int
  {
    std::ostringstream outputStream;
    std::ostringstream errorStream;
    const int status = NG::runDriver(arguments, outputStream, errorStream);
    output = std::move(outputStream).str();
    errors = std::move(errorStream).str();
    return status;
  }
} // namespace

TEST_CASE("vNext array literals splice range values", "[vNext][ArraySpread]")
{
  std::string output;
  std::string errors;
  REQUIRE(run({"--source", "import prelude; fun main() -> unit {"
                           " let nums = [...(1..5)]; assert(nums[0] == 1); assert(nums[3] == 4); }"},
              output, errors) == 0);
  REQUIRE(errors.empty());
}

TEST_CASE("vNext array literals splice slice values", "[vNext][ArraySpread]")
{
  std::string output;
  std::string errors;
  REQUIRE(run({"--source", "import prelude; fun main() -> unit {"
                           " let xs = [10, 20, 30, 40, 50]; let window = [...xs[1..4]];"
                           " assert(window[0] == 20); assert(window[2] == 40); }"},
              output, errors) == 0);
  REQUIRE(errors.empty());
}

TEST_CASE("vNext array literals mix literal elements with one value spread", "[vNext][ArraySpread]")
{
  std::string output;
  std::string errors;
  REQUIRE(run({"--source", "import prelude; fun main() -> unit {"
                           " let xs = [1, 2, 3]; let mixed = [0, ...xs, 9];"
                           " assert(mixed[0] == 0); assert(mixed[2] == 2); assert(mixed[4] == 9); }"},
              output, errors) == 0);
  REQUIRE(errors.empty());
}

TEST_CASE("vNext array value spreads and map spreads coexist in the language", "[vNext][ArraySpread]")
{
  std::string output;
  std::string errors;
  REQUIRE(run({"--source", "import prelude; fun main() -> unit {"
                           " let plain = [...(1..4)]; assert(plain[2] == 3);"
                           " let doubled = [f(1..4)...]; assert(doubled[0] == 2); assert(doubled[2] == 6); }"
                           " fun f(value: i64) -> i64 { return value * 2; }"},
              output, errors) == 0);
  REQUIRE(errors.empty());
}

TEST_CASE("vNext array value spreads reject non-array sources", "[vNext][ArraySpread]")
{
  std::string output;
  std::string errors;
  REQUIRE(run({"--source", "import prelude; fun main() -> unit { let bad = [...\"nope\"]; }"}, output, errors) == 1);
  REQUIRE_THAT(errors, ContainsSubstring("cannot spread value of type string"));
}

TEST_CASE("vNext array literals reject more than one spread", "[vNext][ArraySpread]")
{
  std::string output;
  std::string errors;
  REQUIRE(run({"--source", "import prelude; fun main() -> unit { let a = [1]; let b = [...a, ...a]; }"}, output, errors) ==
              1);
  REQUIRE_THAT(errors, ContainsSubstring("at most one spread"));
}

TEST_CASE("vNext array value spreads keep element types unified", "[vNext][ArraySpread]")
{
  std::string output;
  std::string errors;
  REQUIRE(run({"--source", "import prelude; fun main() -> unit {"
                           " let xs = [\"a\", \"b\"]; let ys = [...xs, \"c\"]; assert(ys[2] == \"c\"); }"},
              output, errors) == 0);
  REQUIRE(errors.empty());
}
