// AI-generated code; reviewed for this repository's vNext rewrite.
//
// INC-1 lightweight property-based testing harness.
//
// Design (per docs/design/rearchitecture/09-increment-plan.md INC-1):
//   * Deterministic, seeded PRNG (xoshiro256**): a run is reproducible by
//     replaying `PBT_SEED=<seed> ./build/ng_test "<filter>"`; every property
//     reports its seed through Catch2 WARN output.
//   * Integer generators weight boundary values (each width's MIN/MAX, -1,
//     0, 1, and narrow sub-ranges) so off-by-one bugs surface early.
//   * String/array/NG-source-snippet generators compose from the core ones.
//   * Known limitation, documented deliberately: there is NO shrinking. A
//     failing trial aborts immediately and reports the seed, the trial
//     number, and the formatted counterexample; minimizing the input is a
//     manual step (rerun with the same seed and smaller bounds).
//
// Batched-probe runner (challenge review follow-up): NG source literals,
// width tables, driver-banner filtering, and the compile-run-compare loop
// used to be re-implemented (with drifting behavior) in every consumer test;
// they are consolidated here so a property test only states its probes.

#pragma once

#include <catch2/catch_test_macros.hpp>

#include <unistd.h>

#include "driver.hpp"

#include <algorithm>
#include <cctype>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <format>
#include <fstream>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>

namespace pbt
{
  /// xoshiro256**: small, fast, seedable, identical sequence across platforms.
  class Rng
  {
  public:
    explicit Rng(uint64_t seed) noexcept
    {
      // SplitMix64 expansion avoids the all-zero state for any seed.
      for (auto &state : state_)
      {
        seed += 0x9E3779B97F4A7C15ULL;
        uint64_t z = seed;
        z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ULL;
        z = (z ^ (z >> 27)) * 0x94D049BB133111EBULL;
        state = z ^ (z >> 31);
      }
    }

    [[nodiscard]] auto nextU64() noexcept -> uint64_t
    {
      const uint64_t result = rotl(state_[1] * 5, 7) * 9;
      const uint64_t t = state_[1] << 17;
      state_[2] ^= state_[0];
      state_[3] ^= state_[1];
      state_[1] ^= state_[2];
      state_[0] ^= state_[3];
      state_[2] ^= t;
      state_[3] = rotl(state_[3], 45);
      return result;
    }

    /// Uniform value in [low, high] (inclusive); high >= low required.
    [[nodiscard]] auto nextU64InRange(uint64_t low, uint64_t high) noexcept -> uint64_t
    {
      // The full u64 domain has span == UINT64_MAX; `span + 1` wraps to zero
      // and would divide by zero (SIGFPE on x86_64 CI). Sample directly.
      const uint64_t span = high - low;
      if (span == UINT64_MAX)
        return low + nextU64();
      return span == 0 ? low : low + nextU64() % (span + 1);
    }

    [[nodiscard]] auto below(double probability) noexcept -> bool
    {
      return (nextU64() >> 11) * 0x1.0p-53 < probability;
    }

  private:
    [[nodiscard]] static auto rotl(uint64_t x, int k) noexcept -> uint64_t { return (x << k) | (x >> (64 - k)); }

    uint64_t state_[4]{};
  };

  /// Resolves the seed for one test case: the PBT_SEED environment variable
  /// overrides; otherwise a clock-derived seed is generated. The effective
  /// seed is always reported so failures can be replayed exactly.
  [[nodiscard]] inline auto currentSeed(std::string_view label) -> uint64_t
  {
    if (const char *injected = std::getenv("PBT_SEED"); injected != nullptr && *injected != '\0')
    {
      const uint64_t seed = std::strtoull(injected, nullptr, 10);
      WARN("[pbt:" << label << "] using injected PBT_SEED=" << seed);
      return seed;
    }
    const uint64_t seed =
        static_cast<uint64_t>(std::chrono::steady_clock::now().time_since_epoch().count()) ^ (0xD1B54A32D192ED03ULL);
    WARN("[pbt:" << label << "] seed=" << seed << " (replay with PBT_SEED=" << seed << ")");
    return seed;
  }

  /// A failed trial: carries the counterexample description for reporting.
  struct Counterexample
  {
    size_t trial{};
    std::string detail;
  };

  /// Runs `property(rng, trialIndex)` n times; the first counterexample
  /// aborts the run and is returned. A property signals failure either by
  /// returning a description (preferred: include the generated inputs) or
  /// via PBT_CHECK, which wraps plain boolean conditions.
  template <typename Property>
  [[nodiscard]] auto forAll(const Rng &seeded, uint32_t n, Property &&property) -> std::optional<Counterexample>
  {
    Rng rng = seeded;
    for (uint32_t trial = 0; trial < n; ++trial)
    {
      if (auto failure = property(rng, trial); failure.has_value())
        return Counterexample{trial, std::move(*failure)};
    }
    return std::nullopt;
  }

  /// Renders a forAll outcome for a Catch2 REQUIRE_MESSAGE.
  [[nodiscard]] inline auto describe(uint64_t seed, const std::optional<Counterexample> &failure,
                                     uint32_t requestedTrials) -> std::string
  {
    if (!failure.has_value())
      return std::format("all {} trials passed", requestedTrials);
    return std::format("counterexample at trial {} of {} (replay: PBT_SEED={}): {}", failure->trial, requestedTrials,
                       seed, failure->detail);
  }
} // namespace pbt

/// Inside a forAll property body: fail the trial with the condition text
/// when it does not hold. Append generated-input context by returning a
/// formatted description directly for anything non-trivial.
#define PBT_CHECK(condition)                                                                                           \
  do                                                                                                                   \
  {                                                                                                                    \
    if (!(condition))                                                                                                  \
      return std::format("violated: {}", #condition);                                                                  \
  } while (false)

namespace pbt
{
  namespace detail
  {
    template <typename W>
    struct WidthTraits;

    template <>
    struct WidthTraits<int8_t>
    {
      static constexpr int64_t kMin = INT64_C(-128), kMax = INT64_C(127);
    };
    template <>
    struct WidthTraits<int16_t>
    {
      static constexpr int64_t kMin = INT64_C(-32768), kMax = INT64_C(32767);
    };
    template <>
    struct WidthTraits<int32_t>
    {
      static constexpr int64_t kMin = INT64_C(-2147483648), kMax = INT64_C(2147483647);
    };
    template <>
    struct WidthTraits<int64_t>
    {
      static constexpr int64_t kMin = INT64_MIN, kMax = INT64_MAX;
    };
    template <>
    struct WidthTraits<uint8_t>
    {
      static constexpr uint64_t kMin = 0, kMax = UINT64_C(255);
    };
    template <>
    struct WidthTraits<uint16_t>
    {
      static constexpr uint64_t kMin = 0, kMax = UINT64_C(65535);
    };
    template <>
    struct WidthTraits<uint32_t>
    {
      static constexpr uint64_t kMin = 0, kMax = UINT64_C(4294967295);
    };
    template <>
    struct WidthTraits<uint64_t>
    {
      static constexpr uint64_t kMin = 0, kMax = UINT64_MAX;
    };

    template <typename W>
    concept SignedWidth = std::is_signed_v<W>;
    template <typename W>
    concept UnsignedWidth = std::is_unsigned_v<W>;
  } // namespace detail

  /// Generates a signed-width integer with boundary weighting:
  /// ~40% edges/near-edges, ~60% uniform over the width's domain.
  template <detail::SignedWidth W>
  [[nodiscard]] auto signedInt(Rng &rng) -> W
  {
    using T = detail::WidthTraits<W>;
    const int64_t roll = static_cast<int64_t>(rng.nextU64InRange(0, 99));
    int64_t value = 0;
    if (roll < 15)
      value = roll < 5 ? T::kMin : T::kMax;
    else if (roll < 25)
      value = roll < 20 ? -1 : 1;
    else if (roll < 30)
      value = roll < 27 ? 0 : T::kMin / 2;
    else if (roll < 35)
      value = T::kMax / 2;
    else if (roll < 40)
      value = T::kMin + static_cast<int64_t>(rng.nextU64InRange(0, 3));
    else if (roll < 45)
      value = T::kMax - static_cast<int64_t>(rng.nextU64InRange(0, 3));
    else
    {
      // Unsigned arithmetic: kMin == INT64_MIN makes `kMax - kMin` overflow
      // as int64_t, and adding the offset to kMin overflows too.
      const uint64_t span = static_cast<uint64_t>(T::kMax) - static_cast<uint64_t>(T::kMin);
      const uint64_t raw = static_cast<uint64_t>(T::kMin) + rng.nextU64InRange(0, span);
      value = static_cast<int64_t>(raw);
    }
    return static_cast<W>(value);
  }

  /// Unsigned counterpart of signedInt (0, max, max-1, small values).
  template <detail::UnsignedWidth W>
  [[nodiscard]] auto unsignedInt(Rng &rng) -> W
  {
    using T = detail::WidthTraits<W>;
    const uint64_t roll = rng.nextU64InRange(0, 99);
    uint64_t value = 0;
    if (roll < 20)
      value = roll < 10 ? T::kMin : T::kMax;
    else if (roll < 30)
      value = roll < 25 ? 0 : T::kMax - 1;
    else if (roll < 40)
      value = rng.nextU64InRange(T::kMin, T::kMax < 8 ? T::kMax : 8);
    else if (roll < 50)
      value = T::kMax - rng.nextU64InRange(0, T::kMax < 16 ? 1 : 16);
    else
      value = rng.nextU64InRange(T::kMin, T::kMax);
    return static_cast<W>(value);
  }

  /// Printable ASCII string (letters/digits/punctuation/space) of bounded
  /// length; empty strings occur with realistic frequency.
  [[nodiscard]] inline auto asciiString(Rng &rng, size_t maxLength) -> std::string
  {
    static constexpr std::string_view alphabet =
        "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789 .,!?-_+/\\\"'@#%^&*()[]{}<>=;:";
    const size_t length = rng.below(0.1) ? 0 : rng.nextU64InRange(0, static_cast<uint64_t>(maxLength));
    std::string text;
    text.reserve(length);
    for (size_t index = 0; index < length; ++index)
      text.push_back(alphabet[rng.nextU64InRange(0, alphabet.size() - 1)]);
    return text;
  }

  /// Vector with boundary-weighted length in [minLength, maxLength].
  template <typename T, typename Generator>
  [[nodiscard]] auto vectorOf(Rng &rng, size_t minLength, size_t maxLength, Generator &&generator) -> std::vector<T>
  {
    const size_t length = rng.nextU64InRange(static_cast<uint64_t>(minLength), static_cast<uint64_t>(maxLength));
    std::vector<T> items;
    items.reserve(length);
    for (size_t index = 0; index < length; ++index)
      items.push_back(generator(rng));
    return items;
  }

  // ---- Shared NG-source oracles (challenge-review consolidation) ----
  //
  // These used to exist as three drifting `literalFor` variants and two
  // width tables across consumer tests; the drift produced invalid literals
  // (`--N`, u64 rendered negative) that silently weakened properties.

  struct IntegerWidth
  {
    const char *name;
    bool isSigned;
    int64_t min;
    uint64_t max;
  };

  /// The D-008 integer tower, ordered i8..u64.
  [[nodiscard]] inline auto integerWidths() -> const std::vector<IntegerWidth> &
  {
    static const std::vector<IntegerWidth> widths = {
        {"i8", true, -128, 127},
        {"i16", true, -32768, 32767},
        {"i32", true, -2147483648LL, 2147483647ULL},
        {"i64", true, -9223372036854775807LL - 1, 9223372036854775807ULL},
        {"u8", false, 0, 255},
        {"u16", false, 0, 65535},
        {"u32", false, 0, 4294967295ULL},
        {"u64", false, 0, 18446744073709551615ULL},
    };
    return widths;
  }

  /// Boundary-weighted operand inside one width's domain. The signed minimum
  /// is never produced: its only spelling negates an overflowing literal,
  /// and naive emission yields invalid text like `--9223372036854775808`.
  /// INT64_MIN's expressibility has dedicated tests.
  [[nodiscard]] inline auto drawOperand(Rng &rng, const IntegerWidth &width) -> uint64_t
  {
    const auto roll = rng.nextU64InRange(0, 99);
    if (roll < 15)
      return width.max;
    if (roll < 25)
      return width.isSigned ? static_cast<uint64_t>(width.min) + 1 : static_cast<uint64_t>(width.min);
    if (roll < 35)
      return rng.below(0.5) ? 1 : width.max / 2;
    if (!width.isSigned && roll < 45)
      return rng.nextU64InRange(0, std::min<uint64_t>(width.max, 8));
    if (width.isSigned)
    {
      const auto span = static_cast<uint64_t>(width.max) - static_cast<uint64_t>(width.min);
      return static_cast<uint64_t>(width.min) + rng.nextU64InRange(1, span - 1);
    }
    return rng.nextU64InRange(0, width.max);
  }

  /// Decimal literal for an in-domain value of `width`. Negatives spell a
  /// bare `-N` (a bare negative literal adopts the declared width;
  /// parenthesizing would turn it into an i64 unary-minus expression).
  /// Unsigned widths always render plain digits regardless of how the bit
  /// pattern reads as signed — rendering them via the int64 interpretation
  /// was a real bug this consolidation removes.
  [[nodiscard]] inline auto widthLiteral(const IntegerWidth &width, uint64_t raw) -> std::string
  {
    if (!width.isSigned || static_cast<int64_t>(raw) >= 0)
      return std::to_string(raw);
    return "-" + std::to_string(-static_cast<int64_t>(raw));
  }

  /// Signed i64 decimal literal. INT64_MIN itself is not spellable this way;
  /// spell it as `-9223372036854775807 - 1` where needed.
  [[nodiscard]] inline auto decimalLiteral(int64_t raw) -> std::string
  {
    if (raw >= 0)
      return std::to_string(raw);
    return "-" + std::to_string(-raw);
  }

  // ---- Batched-probe runner ----

  /// True for driver-internal stdout lines ("compiled ... vNext function(s)"
  /// and the trailing exit-code line); everything else is program output.
  [[nodiscard]] inline auto isDriverBanner(std::string_view line) -> bool
  {
    return (line.starts_with("compiled ") && line.ends_with(" vNext function(s)")) ||
           line.starts_with("native main exited with code ");
  }

  /// Program stdout lines from a captured driver output stream.
  [[nodiscard]] inline auto programOutputLines(const std::string &driverOutput) -> std::vector<std::string>
  {
    std::vector<std::string> kept;
    size_t begin = 0;
    while (begin <= driverOutput.size())
    {
      const size_t end = driverOutput.find('\n', begin);
      const auto line = driverOutput.substr(begin, end == std::string::npos ? std::string::npos : end - begin);
      if (!isDriverBanner(line) && !line.empty())
        kept.emplace_back(line);
      if (end == std::string::npos)
        break;
      begin = end + 1;
    }
    while (!kept.empty() && kept.back().empty())
      kept.pop_back();
    return kept;
  }

  struct BatchProbe
  {
    std::string source;                 // statements; must print exactly one line
    std::string expect{"true"};         // the line that must be printed
    std::string label;                  // echoed verbatim when the probe fails
  };

  /// Compiles and runs one batched module (every probe in a single main) and
  /// compares program output lines against probe expectations. Returns a
  /// description of the first failing probe — echoing its label and source —
  /// or nullopt when every line matched. This replaces the per-test
  /// compile/run/parse loops whose duplication hid a false-green regression.
  [[nodiscard]] inline auto runBatchProbes(std::string_view label, std::string_view preamble,
                                           const std::vector<BatchProbe> &probes) -> std::optional<std::string>
  {
    std::string sanitized(label);
    for (auto &character : sanitized)
      if (!std::isalnum(static_cast<unsigned char>(character)) && character != '_')
        character = '_';
    const auto modulePath =
        std::filesystem::temp_directory_path() /
        std::format("ng_batch_{}_{}.ng", sanitized, static_cast<unsigned>(::getpid()));
    {
      std::ofstream stream(modulePath);
      stream << preamble << "\nfun main() -> unit {\n";
      for (const auto &probe : probes)
        stream << probe.source << "\n";
      stream << "}\n";
    }
    std::ostringstream output;
    std::ostringstream errors;
    const int status = NG::runDriver({modulePath.string()}, output, errors);
    if (status != 0)
    {
      auto message = std::format("batch `{}` failed to compile/run (status {}):\n{}", label, status, errors.str());
      for (const auto &probe : probes)
        message += probe.source + "\n";
      std::filesystem::remove(modulePath);
      return message;
    }
    const auto lines = programOutputLines(output.str());
    std::filesystem::remove(modulePath);
    if (lines.size() != probes.size())
      return std::format("batch `{}` produced {} output lines for {} probes", label, lines.size(), probes.size());
    for (size_t index = 0; index < probes.size(); ++index)
    {
      if (lines[index] != probes[index].expect)
        return std::format("probe #{} ({}) failed: expected `{}`, got `{}`\nsource:\n{}", index, probes[index].label,
                           probes[index].expect, lines[index], probes[index].source);
    }
    return std::nullopt;
  }
} // namespace pbt
