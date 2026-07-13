// AI-generated code; reviewed for this repository's vNext rewrite.
#include "vnext/driver.hpp"

#include "vnext/syntax/module_parser.hpp"
#include "vnext/syntax/parser.hpp"
#include <fstream>
#include <ostream>
#include <string>

namespace NG::vnext
{
  namespace
  {
    void printUsage(std::ostream &output)
    {
      output << "Usage: ngi --expr <expression>\n"
             << "       ngi --source <source-unit>\n"
             << "       ngi <source-file>\n"
             << "\n"
             << "The vNext frontend currently accepts expression fragments and source units with function declarations.\n";
    }

    [[nodiscard]] auto parseExpressionAndReport(std::string_view source, std::ostream &output, std::ostream &errors) -> int
    {
      try
      {
        const auto expression = syntax::parseExpression(source);
        output << "parsed vNext expression at bytes [" << expression->span.begin << ", " << expression->span.end << ")\n";
        return 0;
      }
      catch (const syntax::ParseError &error)
      {
        errors << "syntax error at bytes [" << error.span().begin << ", " << error.span().end << "): " << error.what() << '\n';
        return 1;
      }
    }

    [[nodiscard]] auto parseSourceAndReport(std::string_view source, std::ostream &output, std::ostream &errors) -> int
    {
      try
      {
        const auto unit = syntax::parseSourceUnit(source);
        output << "parsed vNext source unit with " << unit.items.size() << " module item(s)\n";
        return 0;
      }
      catch (const syntax::ParseError &error)
      {
        errors << "syntax error at bytes [" << error.span().begin << ", " << error.span().end << "): " << error.what() << '\n';
        return 1;
      }
    }
  } // namespace

  auto runDriver(const std::vector<std::string_view> &arguments, std::ostream &output, std::ostream &errors) -> int
  {
    if (arguments.empty() || arguments[0] == "--help" || arguments[0] == "-h")
    {
      printUsage(output);
      return arguments.empty() ? 1 : 0;
    }

    if (arguments[0] == "--expr")
    {
      if (arguments.size() != 2)
      {
        errors << "--expr requires exactly one expression argument\n";
        return 1;
      }
      return parseExpressionAndReport(arguments[1], output, errors);
    }

    if (arguments[0] == "--source")
    {
      if (arguments.size() != 2)
      {
        errors << "--source requires exactly one source-unit argument\n";
        return 1;
      }
      return parseSourceAndReport(arguments[1], output, errors);
    }

    if (arguments[0].starts_with('-') || arguments.size() != 1)
    {
      errors << "unknown or incomplete command-line arguments\n";
      printUsage(errors);
      return 1;
    }

    std::ifstream input{std::string{arguments[0]}};
    if (!input)
    {
      errors << "cannot read source file `" << arguments[0] << "`\n";
      return 1;
    }

    const std::string source{std::istreambuf_iterator<char>{input}, std::istreambuf_iterator<char>{}};
    return parseSourceAndReport(source, output, errors);
  }
} // namespace NG::vnext
