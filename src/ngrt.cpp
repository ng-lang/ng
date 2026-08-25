// AI-generated code; reviewed for this repository's vNext rewrite.
//
// Thin C++ wrappers over libngrt. The VM and const evaluator marshal their
// native values into the libngrt ABI (len-prefixed strings / array headers),
// call the C implementation, and marshal the result back.
#include "ngrt.hpp"

#include "ngrt.h"

#include <cstdlib>
#include <cstring>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace NG::ngrt
{
  namespace
  {
    extern "C" void swallowNgrtError(const char *) {}

    const bool errorHandlerInstalled = []
    {
      ngrt_set_error_handler(swallowNgrtError);
      return true;
    }();

    [[nodiscard]] auto failOr(const char *fallback) -> std::string
    {
      if (const char *message = ngrt_last_error(); message != nullptr)
        return message;
      return fallback;
    }

    [[nodiscard]] auto takeNgString(char *text) -> std::string
    {
      if (text == nullptr)
        throw std::runtime_error(failOr("libngrt string allocation failed"));
      std::string result = fromNgString(text);
      ngrt_free(text);
      return result;
    }

    struct NgString
    {
      char *pointer{};
      explicit NgString(std::string_view text)
          : pointer{ngrt_new_string(text.data(), static_cast<long>(text.size()))}
      {
      }
      NgString(const NgString &) = delete;
      NgString &operator=(const NgString &) = delete;
      ~NgString() { ngrt_free(pointer); }
      [[nodiscard]] operator const char *() const { return pointer; }
    };

    [[nodiscard]] auto makeI64Array(const std::vector<int64_t> &values) -> long *
    {
      auto *header = static_cast<long *>(ngrt_alloc(sizeof(long) * (values.size() + 2)));
      header[0] = static_cast<long>(values.size());
      header[1] = header[0];
      for (size_t index = 0; index < values.size(); ++index)
        header[2 + index] = values[index];
      return header;
    }

    [[nodiscard]] auto makeStringArray(const std::vector<std::string> &values) -> long *
    {
      auto *header = static_cast<long *>(ngrt_alloc(sizeof(long) * (values.size() + 2)));
      header[0] = static_cast<long>(values.size());
      header[1] = header[0];
      for (size_t index = 0; index < values.size(); ++index)
        header[2 + index] =
            reinterpret_cast<long>(ngrt_new_string(values[index].data(), static_cast<long>(values[index].size())));
      return header;
    }

    void freeStringArray(long *header)
    {
      const long count = header[0];
      for (long index = 0; index < count; ++index)
        ngrt_free(reinterpret_cast<void *>(header[2 + index]));
      ngrt_free(header);
    }

    [[nodiscard]] auto splitResultToVector(void *result) -> std::vector<std::string>
    {
      if (result == nullptr)
        throw std::runtime_error(failOr("libngrt split failed"));
      auto *header = static_cast<long *>(result);
      const long count = header[0];
      std::vector<std::string> parts;
      parts.reserve(static_cast<size_t>(count));
      for (long index = 0; index < count; ++index)
      {
        char *part = reinterpret_cast<char *>(header[2 + index]);
        parts.push_back(fromNgString(part));
        ngrt_free(part);
      }
      ngrt_free(header);
      return parts;
    }
  } // namespace

  auto length(std::string_view text) -> int64_t
  {
    const NgString value{text};
    return ngrt_length(value);
  }

  auto trim(std::string_view text) -> std::string
  {
    const NgString value{text};
    return takeNgString(ngrt_trim(value));
  }

  auto toUpper(std::string_view text) -> std::string
  {
    const NgString value{text};
    return takeNgString(ngrt_toUpper(value));
  }

  auto toLower(std::string_view text) -> std::string
  {
    const NgString value{text};
    return takeNgString(ngrt_toLower(value));
  }

  auto charAt(std::string_view text, int64_t index) -> std::string
  {
    const NgString value{text};
    ngrt_clear_error();
    char *result = ngrt_charAt(value, static_cast<long>(index));
    if (result == nullptr)
      throw std::out_of_range(failOr("charAt index out of bounds"));
    return takeNgString(result);
  }

  auto substring(std::string_view text, int64_t start, int64_t end) -> std::string
  {
    const NgString value{text};
    ngrt_clear_error();
    char *result = ngrt_substring(value, static_cast<long>(start), static_cast<long>(end));
    if (result == nullptr)
      throw std::out_of_range(failOr("substring bounds out of range"));
    return takeNgString(result);
  }

  auto contains(std::string_view text, std::string_view needle) -> bool
  {
    const NgString value{text};
    const NgString search{needle};
    return ngrt_contains(value, search) != 0;
  }

  auto startsWith(std::string_view text, std::string_view prefix) -> bool
  {
    const NgString value{text};
    const NgString search{prefix};
    return ngrt_startsWith(value, search) != 0;
  }

  auto endsWith(std::string_view text, std::string_view suffix) -> bool
  {
    const NgString value{text};
    const NgString search{suffix};
    return ngrt_endsWith(value, search) != 0;
  }

  auto replace(std::string_view text, std::string_view needle, std::string_view replacement) -> std::string
  {
    const NgString value{text};
    const NgString search{needle};
    const NgString substitute{replacement};
    return takeNgString(ngrt_replace(value, search, substitute));
  }

  auto split(std::string_view text, std::string_view delimiter) -> std::vector<std::string>
  {
    const NgString value{text};
    const NgString separator{delimiter};
    return splitResultToVector(ngrt_split(value, separator));
  }

  auto join(const std::vector<std::string> &items, std::string_view separator) -> std::string
  {
    long *header = makeStringArray(items);
    const NgString separatorNg{separator};
    char *result = ngrt_join(header, separatorNg);
    freeStringArray(header);
    return takeNgString(result);
  }

  auto regexMatch(std::string_view text, std::string_view pattern) -> bool
  {
    const NgString value{text};
    const NgString expression{pattern};
    ngrt_clear_error();
    const long result = ngrt_regexMatch(value, expression);
    if (result < 0)
      throw std::runtime_error(failOr("regexMatch: invalid pattern"));
    return result != 0;
  }

  auto readLine() -> std::string { return takeNgString(ngrt_readLine()); }

  auto readFile(std::string_view path) -> std::string
  {
    const NgString pathNg{path};
    ngrt_clear_error();
    char *result = ngrt_readFile(pathNg);
    if (result == nullptr)
      throw std::runtime_error(failOr("cannot read file"));
    return takeNgString(result);
  }

  auto writeFile(std::string_view path, std::string_view content) -> void
  {
    const NgString pathNg{path};
    const NgString contentNg{content};
    ngrt_clear_error();
    if (ngrt_writeFile(pathNg, contentNg) != 0)
      throw std::runtime_error(failOr("cannot write file"));
  }

  auto currentExecutablePath() -> std::string { return takeNgString(ngrt_currentExecutablePath()); }

  auto arrayLength(const std::vector<Value> &values) -> int64_t
  {
    return static_cast<int64_t>(values.size());
  }

  auto arraySum(const std::vector<Value> &values) -> int64_t
  {
    std::vector<int64_t> elements;
    elements.reserve(values.size());
    for (const auto &value : values)
    {
      if (!value.isInteger())
        throw std::runtime_error("sum expects an array of i64");
      elements.push_back(value.asInteger());
    }
    long *header = makeI64Array(elements);
    const long result = ngrt_sum(header);
    ngrt_free(header);
    return result;
  }

  auto arrayContains(const std::vector<Value> &values, int64_t searched) -> bool
  {
    std::vector<int64_t> elements;
    elements.reserve(values.size());
    for (const auto &value : values)
    {
      if (!value.isInteger())
        throw std::runtime_error("arrayContains expects an array of i64");
      elements.push_back(value.asInteger());
    }
    long *header = makeI64Array(elements);
    const long result = ngrt_arrayContains(header, searched);
    ngrt_free(header);
    return result != 0;
  }

  auto arrayReverse(const std::vector<Value> &values) -> Value
  {
    std::vector<int64_t> elements;
    elements.reserve(values.size());
    for (const auto &value : values)
    {
      if (!value.isInteger())
        throw std::runtime_error("reverse expects an array of i64");
      elements.push_back(value.asInteger());
    }
    long *header = makeI64Array(elements);
    ngrt_clear_error();
    void *result = ngrt_reverse(header);
    ngrt_free(header);
    if (result == nullptr)
      throw std::runtime_error(failOr("reverse failed"));
    auto *resultHeader = static_cast<long *>(result);
    const long count = resultHeader[0];
    std::vector<Value> reversed;
    reversed.reserve(static_cast<size_t>(count));
    for (long index = 0; index < count; ++index)
      reversed.push_back(Value::integer(resultHeader[2 + index]));
    ngrt_free(result);
    return Value::array(std::move(reversed));
  }

  auto allocate(int64_t value) -> uint64_t
  {
    return static_cast<uint64_t>(ngrt_allocate(value));
  }

  auto load(uint64_t handle) -> int64_t
  {
    ngrt_clear_error();
    const long result = ngrt_load(static_cast<long>(handle));
    if (const char *message = ngrt_last_error(); message != nullptr)
      throw std::runtime_error(message);
    return result;
  }

  auto store(uint64_t handle, int64_t value) -> void
  {
    ngrt_clear_error();
    static_cast<void>(ngrt_store(static_cast<long>(handle), value));
    if (const char *message = ngrt_last_error(); message != nullptr)
      throw std::runtime_error(message);
  }

  auto release(uint64_t handle) -> void
  {
    ngrt_clear_error();
    static_cast<void>(ngrt_release(static_cast<long>(handle)));
    if (const char *message = ngrt_last_error(); message != nullptr)
      throw std::runtime_error(message);
  }

  auto outstanding() -> int64_t { return ngrt_outstanding(); }
} // namespace NG::ngrt
