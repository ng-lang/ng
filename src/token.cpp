
#include <common.hpp>
#include <iostream>
#include <token.hpp>

namespace NG
{
  auto operator<<(std::ostream &stream, const Token &token) -> std::ostream &
  {
    return stream << "Token { " << token.repr << "[" << token.position.line << ", " << token.position.col << "]"
                  << code(token.type) << "}";
  }
} // namespace NG