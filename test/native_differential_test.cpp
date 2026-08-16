// AI-generated code; reviewed for this repository's vNext rewrite.
// Differential sweep: every example runs through the VM and through
// `--native`; the VM is the oracle — its main return value (low 8 bits)
// must equal the native executable's exit code.
#include "test.hpp"

#include "driver.hpp"

#include <chrono>
#include <filesystem>
#include <iostream>
#include <regex>
#include <sstream>

#if defined(NG_QBE_PATH) && !defined(_WIN32)
#include <sys/wait.h>
#include <unistd.h>
#endif

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
    // Extern "C" calls have no VM oracle (the VM tier rejects them), so the
    // example is exercised by dedicated native tests (B3 first slice).
    if (path.ends_with("ffi_extern.ng")) continue;
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

TEST_CASE("vNext native tier benchmark suite", "[.benchmark][vNext][Native]")
{
  // VM: repeated full runs through the driver. Native: compile once, then
  // execute the binary directly so the qbe+cc toolchain is not measured.
  const std::vector<std::pair<std::string, std::string>> programs = {
    {"fib(22)",
     "fun fib(n: i64) -> i64 { if (n <= 1) { return n; } return fib(n - 1) + fib(n - 2); } "
     "fun main() -> i64 => fib(22);"},
    {"loop 200000",
     "fun main() -> i64 { let mut total = 0; loop (i = 0) { total := total + i; if (i == 200000) { return total; } next (i + 1); } }"},
    {"string concat 20000",
     "import prelude;\nfun main() -> i64 { let mut s = \"\"; loop (i = 0) { s := s + \"a\"; if (i == 2000) { return length(s); } next (i + 1); } }"},
    {"list walk 200",
     "enum List<T> { Nil, Cons(head: T, tail: ref<List<T>>) }\n"
     "fun walk(list: ref<List<i64>>) -> i64 { switch (*list) { case Nil { return 0; } case Cons(v, rest) { return v + walk(rest); } } }\n"
     "fun main() -> i64 { let a: List<i64> = List.Nil; let b: List<i64> = List.Cons(3, ref a); let c: List<i64> = List.Cons(2, ref b); let d: List<i64> = List.Cons(1, ref c); return walk(ref d); }"},
  };
  const auto executable = std::filesystem::temp_directory_path() /
                          std::format("ng_native_{}", static_cast<int>(getpid())) / "ng_out";
  std::cout << "=== native tier benchmark (user time) ===\n";
  for (const auto &[name, source] : programs)
  {
    std::string output;
    std::string errors;
    const int vmRuns = 3;
    const auto vmStart = std::chrono::steady_clock::now();
    for (int run = 0; run < vmRuns; ++run) REQUIRE(runDriverCaptured({"--source", source}, output, errors) == 0);
    const auto vmTime = std::chrono::steady_clock::now() - vmStart;
    REQUIRE(runDriverCaptured({"--native", "--source", source}, output, errors) == 0);
    const int nativeRuns = 50;
    const auto nativeStart = std::chrono::steady_clock::now();
    for (int run = 0; run < nativeRuns; ++run)
    {
      const int status = std::system(std::format("'{}' > /dev/null 2>&1", executable.string()).c_str());
      REQUIRE(WIFEXITED(status));
      static_cast<void>(status); // exit codes are the programs' own results
    }
    const auto nativeTime = std::chrono::steady_clock::now() - nativeStart;
    const double vmMs = std::chrono::duration<double, std::milli>(vmTime).count() / vmRuns;
    const double nativeMs = std::chrono::duration<double, std::milli>(nativeTime).count() / nativeRuns;
    std::cout << std::format("{:<20} vm {:>10.2f} ms   native {:>8.3f} ms   speedup {:>7.1f}x\n", name, vmMs,
                             nativeMs, vmMs / nativeMs);
  }
}
#endif
