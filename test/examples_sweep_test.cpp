// AI-generated code; reviewed for this repository's vNext rewrite.
// Sweeps every example file end to end through ngi so the corpus stays fully
// runnable; individual feature tests cover semantics in depth.
#include "test.hpp"
#include "driver.hpp"

#include <filesystem>
#include <sstream>

namespace
{
  [[nodiscard]] auto runExample(std::string_view filename, std::string &output, std::string &errors) -> int
  {
    std::string path{filename};
    if (!std::filesystem::is_directory(std::filesystem::current_path() / "example")) path = std::string{"../"} + path;
    std::ostringstream outputStream;
    std::ostringstream errorStream;
    const int status = NG::runDriver({path}, outputStream, errorStream);
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
    // The IDE drives the imgui binding; it is exercised headless through
    // stub natives in the imgui suite instead.
    if (filename == "ng_ide.ng") continue;
    // The extern "C" example calls real C symbols; the VM tier rejects
    // extern calls with a tier diagnostic, so it is exercised natively by
    // the native suite (B3 first slice).
    if (filename == "ffi_extern.ng") continue;
    std::string output;
    std::string errors;
    const int status = runExample("example/" + filename, output, errors);
    INFO("example: " << filename);
    INFO("errors: " << errors);
    REQUIRE(status == 0);
    REQUIRE(errors.empty());
    REQUIRE(output.find("compiled") != std::string::npos);
    ++runCount;
  }
  REQUIRE(runCount >= 40);
}
