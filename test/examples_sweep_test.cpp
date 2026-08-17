// AI-generated code; reviewed for this repository's vNext rewrite.
// Sweeps every example file end to end through ngi so the corpus stays fully
// runnable; individual feature tests cover semantics in depth.
#include "test.hpp"
#include "driver.hpp"

#include <filesystem>
#include <sstream>

namespace
{
  [[nodiscard]] auto runExample(std::string_view filename, std::string &output, std::string &errors,
                                bool nativeMode = false) -> int
  {
    std::string path{filename};
    if (!std::filesystem::is_directory(std::filesystem::current_path() / "example")) path = std::string{"../"} + path;
    std::ostringstream outputStream;
    std::ostringstream errorStream;
    std::vector<std::string_view> args;
    if (nativeMode) args.push_back("--native");
    args.push_back(path);
    const int status = NG::runDriver(args, outputStream, errorStream);
    output = std::move(outputStream).str();
    errors = std::move(errorStream).str();
    return status;
  }
} // namespace

TEST_CASE("vNext example corpus runs end to end through ngi", "[vNext][Examples][Sweep]")
{
  // Repo-relative source directory; the helper prefixes `../` when the
  // tests run from the build directory.
  std::string examplesDirectory{"example"};
  if (!std::filesystem::is_directory(std::filesystem::current_path() / "example")) examplesDirectory = "../example";

  std::vector<std::string> files;
  for (const auto &entry : std::filesystem::directory_iterator{examplesDirectory})
  {
    if (!entry.is_regular_file() || entry.path().extension() != ".ng") continue;
    files.push_back(entry.path().filename().string());
  }
  if (std::filesystem::is_directory(examplesDirectory + "/modules"))
    for (const auto &entry : std::filesystem::directory_iterator{examplesDirectory + "/modules"})
      if (entry.is_regular_file() && entry.path().extension() == ".ng")
        files.push_back("modules/" + entry.path().filename().string());
  std::sort(files.begin(), files.end());

  size_t runCount = 0;
  for (const auto &filename : files)
  {
    // The IDE opens a real SDL/ImGui window; it is not headless-testable.
    if (filename == "ng_ide.ng") continue;
    // `modules/hello.ng` is an import-only module with no main entry point.
    if (filename == "modules/hello.ng") continue;
    // The extern "C" example calls real C symbols and is exercised through
    // the normal AOT path.
    const bool isNativeOnly = (filename == "ffi_extern.ng");
    std::string output;
    std::string errors;
    const int status = runExample("example/" + filename, output, errors, isNativeOnly);
    INFO("example: " << filename);
    INFO("errors: " << errors);
    REQUIRE(status == 0);
    REQUIRE(errors.empty());
    if (!isNativeOnly)
      REQUIRE(output.find("compiled") != std::string::npos);
    ++runCount;
  }
  REQUIRE(runCount >= 40);
}
