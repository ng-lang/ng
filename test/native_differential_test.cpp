// AI-generated code; reviewed for this repository's vNext rewrite.
// Differential sweep: every example runs through the VM and through
// `--native`; the VM is the oracle — its main return value (low 8 bits)
// must equal the native executable's exit code.
#include "test.hpp"

#include "driver.hpp"

#include <chrono>
#include <filesystem>
#include <regex>
#include <sstream>

#if defined(NG_QBE_PATH) && !defined(_WIN32)
namespace
{
  [[nodiscard]] auto runDriverCaptured(const std::vector<std::string_view> &arguments, std::string &output,
                                       std::string &errors) -> int
  {
    std::ostringstream outputStream;
    std::ostringstream errorStream;
    const int status = NG::runDriver(arguments, outputStream, errorStream);
    output = std::move(outputStream).str();
    errors = std::move(errorStream).str();
    return status;
  }

  /// Extracts the VM main return value from the driver output; unit returns
  /// (no value printed) are 0.
  [[nodiscard]] auto vmReturnValue(const std::string &output) -> int64_t
  {
    std::regex pattern{"with value (-?[0-9]+)"};
    std::smatch match;
    if (std::regex_search(output, match, pattern)) return std::stoll(match[1].str());
    return 0;
  }

  [[nodiscard]] auto nativeExitCode(const std::string &output, std::optional<int64_t> &code) -> bool
  {
    std::regex pattern{"native main exited with code (-?[0-9]+)"};
    std::smatch match;
    if (!std::regex_search(output, match, pattern)) return false;
    code = std::stoll(match[1].str());
    return true;
  }

  [[nodiscard]] auto exampleFiles() -> std::vector<std::string>
  {
    std::string examplesDirectory{"example"};
    if (!std::filesystem::is_directory(std::filesystem::current_path() / "example")) examplesDirectory = "../example";
    std::vector<std::string> files;
    for (const auto &entry : std::filesystem::directory_iterator{examplesDirectory})
      if (entry.is_regular_file() && entry.path().extension() == ".ng") files.push_back(entry.path().string());
    std::sort(files.begin(), files.end());
    return files;
  }
} // namespace

TEST_CASE("vNext example corpus matches the VM under the native tier", "[vNext][Native][Differential]")
{
  size_t runCount = 0;
  for (const auto &path : exampleFiles())
  {
    // The IDE drives the imgui binding (exercised headless in the imgui
    // suite); its native shims arrive with the R9 std-module work.
    if (path.ends_with("ng_ide.ng")) continue;
    std::string vmOutput;
    std::string vmErrors;
    const int vmStatus = runDriverCaptured({path}, vmOutput, vmErrors);
    INFO("example: " << path);
    INFO("vm errors: " << vmErrors);
    REQUIRE(vmStatus == 0);
    // Module files have no main; they compile under the VM but cannot
    // produce a native executable on their own.
    if (vmOutput.find("main returned") == std::string::npos) continue;
    const int64_t vmValue = vmReturnValue(vmOutput);
    std::string nativeOutput;
    std::string nativeErrors;
    const int nativeStatus = runDriverCaptured({"--native", path}, nativeOutput, nativeErrors);
    INFO("native errors: " << nativeErrors);
    REQUIRE(nativeStatus == 0);
    std::optional<int64_t> exitCode;
    INFO("native output: " << nativeOutput);
    REQUIRE(nativeExitCode(nativeOutput, exitCode));
    CHECK((vmValue & 0xFF) == *exitCode);
    ++runCount;
  }
  REQUIRE(runCount >= 40);
}

TEST_CASE("vNext native tier fib benchmark", "[.benchmark][vNext][Native]")
{
  constexpr std::string_view source =
      "fun fib(n: i64) -> i64 { if (n <= 1) { return n; } return fib(n - 1) + fib(n - 2); } "
      "fun main() -> i64 => fib(24);";
  std::string output;
  std::string errors;
  const auto start = std::chrono::steady_clock::now();
  REQUIRE(runDriverCaptured({"--source", source}, output, errors) == 0);
  const auto vmTime = std::chrono::steady_clock::now() - start;
  const auto nativeStart = std::chrono::steady_clock::now();
  REQUIRE(runDriverCaptured({"--native", "--source", source}, output, errors) == 0);
  const auto nativeTime = std::chrono::steady_clock::now() - nativeStart;
  INFO("vm fib(24): " << std::chrono::duration_cast<std::chrono::milliseconds>(vmTime).count() << " ms");
  INFO("native fib(24): " << std::chrono::duration_cast<std::chrono::milliseconds>(nativeTime).count() << " ms");
}
#endif
