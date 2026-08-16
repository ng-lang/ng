// AI-generated code; reviewed for this repository's vNext rewrite.
#include "string_ops.hpp"

#include <cctype>
#include <regex>
#include <stdexcept>

namespace NG::string_ops
{
  namespace
  {
    [[nodiscard]] auto castIndex(int64_t index) -> size_t
    {
      if (index < 0) throw std::out_of_range("index is negative");
      return static_cast<size_t>(index);
    }
  } // namespace

  auto length(std::string_view text) -> int64_t { return static_cast<int64_t>(text.size()); }

  auto trim(std::string_view text) -> std::string
  {
    const auto first = text.find_first_not_of(" \t\n\r");
    if (first == std::string_view::npos) return "";
    const auto last = text.find_last_not_of(" \t\n\r");
    return std::string{text.substr(first, last - first + 1)};
  }

  namespace
  {
    [[nodiscard]] auto transformCase(std::string_view text, bool upper) -> std::string
    {
      std::string result{text};
      for (auto &character : result)
        character = static_cast<char>(upper ? std::toupper(static_cast<unsigned char>(character))
                                            : std::tolower(static_cast<unsigned char>(character)));
      return result;
    }
  } // namespace

  auto toUpper(std::string_view text) -> std::string { return transformCase(text, true); }

  auto toLower(std::string_view text) -> std::string { return transformCase(text, false); }

  auto charAt(std::string_view text, int64_t index) -> std::string
  {
    const auto position = castIndex(index);
    if (position >= text.size())
      throw std::out_of_range("charAt index out of bounds: index " + std::to_string(index) + ", length " +
                              std::to_string(text.size()));
    return std::string(1, text[position]);
  }

  auto substring(std::string_view text, int64_t start, int64_t end) -> std::string
  {
    const auto first = castIndex(start);
    const auto last = castIndex(end);
    if (last < first || last > text.size())
      throw std::out_of_range("substring bounds out of range: [" + std::to_string(start) + ".." + std::to_string(end) +
                              ") of length " + std::to_string(text.size()));
    return std::string{text.substr(first, last - first)};
  }

  auto contains(std::string_view text, std::string_view needle) -> bool { return text.find(needle) != std::string_view::npos; }

  auto startsWith(std::string_view text, std::string_view prefix) -> bool { return text.starts_with(prefix); }

  auto endsWith(std::string_view text, std::string_view suffix) -> bool { return text.ends_with(suffix); }

  auto replace(std::string_view text, std::string_view needle, std::string_view replacement) -> std::string
  {
    std::string result{text};
    if (needle.empty()) return result;
    size_t position = 0;
    while ((position = result.find(needle, position)) != std::string::npos)
    {
      result.replace(position, needle.size(), replacement);
      position += replacement.size();
    }
    return result;
  }

  auto split(std::string_view text, std::string_view delimiter) -> std::vector<std::string>
  {
    std::vector<std::string> parts;
    size_t start = 0;
    while (start <= text.size())
    {
      const auto found = text.find(delimiter, start);
      if (found == std::string_view::npos)
      {
        parts.emplace_back(text.substr(start));
        break;
      }
      parts.emplace_back(text.substr(start, found - start));
      start = found + delimiter.size();
    }
    return parts;
  }

  auto join(const std::vector<std::string> &items, std::string_view separator) -> std::string
  {
    std::string joined;
    for (size_t index = 0; index < items.size(); ++index)
    {
      if (index != 0) joined += separator;
      joined += items[index];
    }
    return joined;
  }

  auto regexMatch(std::string_view text, std::string_view pattern) -> bool
  {
    try
    {
      return std::regex_search(std::string{text}, std::regex{std::string{pattern}});
    }
    catch (const std::regex_error &)
    {
      throw std::runtime_error("regexMatch: invalid pattern");
    }
  }
} // namespace NG::string_ops
