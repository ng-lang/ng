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

TEST_CASE("runNgi native compiles and runs a nested source unit and captures output", "[vNext][Driver][runNgi]")
{
  std::string output;
  std::string errors;
  const std::string source = std::string{"import prelude;\n"} + R"(
fun main() -> unit {
    print(runNgi("fun add(a: i64, b: i64) -> i64 { return a + b; } fun main() -> i64 { return add(20, 22); }"));
}
)";
  REQUIRE(run({"--source", source}, output, errors) == 0);
  REQUIRE_THAT(output, ContainsSubstring("compiled 2 vNext function(s); main returned after"));
  REQUIRE_THAT(output, ContainsSubstring("with value 42"));
  REQUIRE(errors.empty());
}

TEST_CASE("runNgi native reports nested diagnostics and the failing exit status", "[vNext][Driver][runNgi]")
{
  std::string output;
  std::string errors;
  const std::string source = std::string{"import prelude;\n"} + R"(
fun main() -> unit {
    print(runNgi("fun main() -> i64 { return missing(1); }"));
}
)";
  REQUIRE(run({"--source", source}, output, errors) == 0);
  REQUIRE_THAT(output, ContainsSubstring("name `missing` is not visible in this module"));
  REQUIRE_THAT(output, ContainsSubstring("[exit 1]"));
  REQUIRE(errors.empty());
}

TEST_CASE("runNgi native rejects non-string arguments at typecheck time", "[vNext][Driver][runNgi]")
{
  std::string output;
  std::string errors;
  const std::string source = std::string{"import prelude;\n"} + R"(
fun main() -> unit {
    print(runNgi("ok"));
}
fun broken() -> unit {
    runNgi(42);
}
)";
  REQUIRE(run({"--source", source}, output, errors) == 1);
  REQUIRE_THAT(errors, ContainsSubstring("type error"));
}

TEST_CASE("runDriverWithNatives injects an extra native callable from NG source", "[vNext][Driver][NativeRegistration]")
{
  static std::string lastMessage;
  const auto registerProbe = [](NG::vm::NativeRegistry &registry) {
    registry.registerNative("probe", [](const std::vector<NG::Value> &arguments, const std::vector<NG::typecheck::TypeId> &) {
      if (arguments.size() != 1 || !arguments.front().isString())
        throw NG::bytecode::BytecodeError("probe expects a string");
      lastMessage = arguments.front().asString();
      return NG::Value{};
    });
  };
  std::ostringstream outputStream;
  std::ostringstream errorStream;
  const std::string source = "import prelude;\n"
                             "export native fun probe(message: string) -> unit;\n"
                             "fun main() -> unit { probe(\"ping\"); }\n";
  REQUIRE(NG::runDriverWithNatives({"--source", source}, outputStream, errorStream, registerProbe) == 0);
  REQUIRE(lastMessage == "ping");
  REQUIRE(errorStream.str().empty());
}
