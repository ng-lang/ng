// AI-generated code; reviewed for this repository's vNext rewrite.
#include "test.hpp"
#include "driver.hpp"

#include <filesystem>
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

  [[nodiscard]] auto runExample(std::string_view filename, std::string &output, std::string &errors) -> int
  {
    std::string path{filename};
    if (!std::filesystem::is_directory(std::filesystem::current_path() / "example")) path = std::string{"../"} + path;
    return run({path}, output, errors);
  }
} // namespace

TEST_CASE("vNext tuple rest patterns bind prefix and remaining elements", "[vNext][TupleRest]")
{
  std::string output;
  std::string errors;
  REQUIRE(run({"--source", "import prelude; fun main() { "
                           "let pair = (1, \"two\", true); let (first, ...rest) = pair; "
                           "assert(first == 1); assert(rest[0] == \"two\"); assert(rest[1] == true); }"},
              output, errors) == 0);
  REQUIRE(errors.empty());

  REQUIRE(run({"--source", "import prelude; fun main() { "
                           "let (head, ...tail) = (9, 8, 7); assert(head == 9); "
                           "assert(tail[0] == 8); assert(tail[1] == 7); }"},
              output, errors) == 0);
  REQUIRE(errors.empty());

  REQUIRE(run({"--source", "import prelude; fun main() { "
                           "let (x, ...none) = (5, 6); assert(x == 5); assert(none[0] == 6); }"},
              output, errors) == 0);
  REQUIRE(errors.empty());
}

TEST_CASE("vNext tuple rest patterns allow empty rests and diagnose over-long prefixes", "[vNext][TupleRest][Errors]")
{
  std::string output;
  std::string errors;
  // An empty rest is valid.
  REQUIRE(run({"--source", "import prelude; fun main() { let (a, b, ...rest) = (1, 2); assert(a == 1); }"},
              output, errors) == 0);
  // A prefix longer than the tuple is not.
  REQUIRE(run({"--source", "import prelude; fun main() { let (a, b, c, ...rest) = (1, 2); }"}, output, errors) == 1);
  REQUIRE_THAT(errors, ContainsSubstring("tuple destructuring length mismatch"));
}

TEST_CASE("vNext tuple_rest_patterns example runs end to end through ngi", "[vNext][TupleRest][Examples]")
{
  std::string output;
  std::string errors;
  REQUIRE(runExample("example/tuple_rest_patterns.ng", output, errors) == 0);
  REQUIRE(errors.empty());
  REQUIRE(output.find("main returned") != std::string::npos);
}
