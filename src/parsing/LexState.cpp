
#include <parser.hpp>
#include <token.hpp>
#include <algorithm>

namespace NG::parsing
{

  LexState::LexState(const Str &_source) : source(_source), size(_source.size()), index(0), line(1), col(1) {}

  auto LexState::current() const -> char
  {
    if (!eof())
    {
      return source.at(index);
    }
    return '\0';
  }

  auto LexState::eof() const -> bool
  {
    return index >= size;
  }

  void LexState::extend(const Str &source)
  {
    this->source += source;
    this->size += source.size();
  }

  auto LexState::lookAhead() const -> char
  {
    if (index + 1 >= size)
    {
      return '\0';
    }
    return source.at(index + 1);
  }

  void LexState::next(size_t n)
  {
    if (!eof())
    {
      index += n;
      col += n;
    }
  }

  void LexState::revert(size_t n)
  {
    if (n > index)
    {
      return;
    }
    // Binary search lineStarts to find the line for position n.
    // lineStarts[i] is the index of the first character of line i+1.
    auto it = std::upper_bound(lineStarts.begin(), lineStarts.end(), n);
    if (it == lineStarts.begin())
    {
      line = 1;
      col = n + 1; // 1-based column
    }
    else
    {
      --it;
      line = static_cast<size_t>(std::distance(lineStarts.begin(), it)) + 1;
      col = n - *it + 1; // 1-based column
    }
    index = n;
  }

  void LexState::nextLine()
  {
    line++;
    col = 0;
    // Record the next character's index as a line start.
    if (index < size)
    {
      lineStarts.push_back(index + 1);
    }
  }

} // namespace NG::parsing