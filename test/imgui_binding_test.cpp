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

  [[nodiscard]] auto examplePath(std::string_view filename) -> std::string
  {
    std::string path{filename};
    if (!std::filesystem::is_directory(std::filesystem::current_path() / "example")) path = std::string{"../"} + path;
    return path;
  }

  /// Headless stand-ins for the GUI natives: they satisfy the binding's
  /// signatures so programs can run without a window. `imguiAborted` reports
  /// "done" immediately, which ends the IDE's frame loop after one frame.
  void registerStubImguiNatives(NG::vm::NativeRegistry &registry)
  {
    const auto unit = [](const std::vector<NG::Value> &, const std::vector<NG::typecheck::TypeId> &) {
      return NG::Value{};
    };
    const auto no = [](const std::vector<NG::Value> &, const std::vector<NG::typecheck::TypeId> &) {
      return NG::Value::integer(0);
    };
    const auto echoString = [](const std::vector<NG::Value> &arguments, const std::vector<NG::typecheck::TypeId> &) {
      return arguments.size() > 1 ? arguments[1] : NG::Value::string("");
    };
    for (const auto *name : {"imguiInit", "imguiCleanup", "imguiEventLoop", "imguiNewFrame", "imguiRender", "imguiEnd",
                             "imguiEndChild", "imguiSetNextWindowSize", "imguiText", "imguiTextWrapped",
                             "imguiSeparator", "imguiStyleColorsDark", "imguiStyleColorsLight"})
    {
      registry.registerNative(name, unit);
    }
    for (const auto *name : {"imguiBegin", "imguiBeginChild", "imguiButton"})
    {
      registry.registerNative(name, no);
    }
    registry.registerNative("imguiAborted", [](const std::vector<NG::Value> &, const std::vector<NG::typecheck::TypeId> &) {
      return NG::Value::integer(1);
    });
    registry.registerNative("imguiCheckbox", echoString);
    registry.registerNative("imguiInputTextMultiline", echoString);
    registry.registerNative("imguiGetTime", [](const std::vector<NG::Value> &, const std::vector<NG::typecheck::TypeId> &) {
      return NG::Value::float_(0.0);
    });
  }
} // namespace

TEST_CASE("vNext imgui binding resolves and typechecks from lib/std", "[vNext][Imgui][Binding]")
{
  std::string output;
  std::string errors;
  REQUIRE(run({"--source", "import imgui; fun main() -> unit {}"}, output, errors) == 0);
  REQUIRE_THAT(output, ContainsSubstring("compiled"));
  REQUIRE(errors.empty());
}

TEST_CASE("vNext imgui binding rejects wrong call arity at typecheck time", "[vNext][Imgui][Binding]")
{
  std::string output;
  std::string errors;
  REQUIRE(run({"--source", "import imgui; fun main() -> unit { imguiButton(); }"}, output, errors) == 1);
  REQUIRE_THAT(errors, ContainsSubstring("type error"));
}

TEST_CASE("vNext NG IDE example runs headless against the stub imgui binding", "[vNext][Imgui][IDE]")
{
  std::ostringstream outputStream;
  std::ostringstream errorStream;
  REQUIRE(NG::runDriverWithNatives({examplePath("example/ng_ide.ng")}, outputStream, errorStream,
                                    registerStubImguiNatives) == 0);
  REQUIRE_THAT(outputStream.str(), ContainsSubstring("main returned"));
  REQUIRE(errorStream.str().empty());
}
