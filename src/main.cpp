

#include "ast.hpp"
#include "intp/intp.hpp"
#include "module.hpp"
#include "orgasm/compiler.hpp"
#include "orgasm/vm.hpp"
#include "parser.hpp"
#include "token.hpp"
#include "typecheck/typecheck.hpp"
#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iostream>
#include <streambuf>

using namespace NG;
using namespace NG::ast;
using namespace NG::parsing;

#ifdef _WIN32
#include <io.h>
#define isatty _isatty
#ifndef STDIN_FILENO
#define STDIN_FILENO 0
#endif
#else
#include <unistd.h>
#endif

static inline auto parse(const Str &source, const Str &file = "[noname]") -> ASTRef<ASTNode>
{
  return Parser(ParseState(Lexer(LexState{source}).lex())).parse(file);
}

// NOLINTNEXTLINE(bugprone-exception-escape)

auto showHint() -> bool
{
  return isatty(STDIN_FILENO);
}

auto repl() -> int
{

  if (showHint())
  {
    std::cout << ">> ";
  }
  std::string line{};
  std::getline(std::cin, line);
  std::string source{line};
  LexState state{source};
  Lexer lexer{LexState{line}};
  NG::intp::Interpreter *stupid = NG::intp::stupid();

  Token current;
  Vec<Token> tokens;
  Vec<ASTRef<ASTNode>> histories;

  bool shouldRestart = true;
  while (true)
  {
    shouldRestart = false;
    int braceCount = 0;
    tokens.clear();
    while (true)
    {
      if (lexer->eof())
      {
        if (showHint())
        {
          std::cout << ">> ";
        }
        if (std::getline(std::cin, line))
        {
          if (std::find_if(begin(line), end(line), [](unsigned char c) -> bool { return !std::isblank(c); }) !=
              line.end())
          {
            lexer->extend(line);
          }
        }
        else
        {
          exit(0);
        }
      }
      current = lexer.next();
      if (current.type == TokenType::NONE)
      {
        continue;
      }
      // debug_log(current);
      tokens.push_back(current);
      if (current.type == TokenType::LEFT_CURLY)
      {
        braceCount++;
      }
      if (current.type == TokenType::RIGHT_CURLY)
      {
        braceCount--;
        if (braceCount < 0)
        {
          debug_log("Invalid input, unbalanced curly brackets");
          shouldRestart = true;
          break;
        }
      }
      if (current.type == TokenType::SEMICOLON && braceCount == 0)
      {
        break;
      }
    }
    if (shouldRestart)
    {
      continue;
    }

    try
    {
      ParseState parse_state{tokens};
      auto ast = Parser(parse_state).parse("[interpreter]");
      tokens.clear();
      (ast)->accept(stupid);
      histories.push_back(ast);
    }
    catch (const ParseException &ex)
    {
      debug_log("Syntax error:", ex.what());
    }
    catch (const NG::RuntimeException &ex)
    {
      debug_log("Runtime error", ex.what());
    }
  }

  for (auto ast : histories)
  {
    destroyast(ast);
  }
}
auto main(int argc, char *argv[]) -> int
{
  bool use_stupid = false;
  const char *filename_ptr = nullptr;

  for (int i = 1; i < argc; ++i)
  {
    if (std::strcmp(argv[i], "--stupid") == 0)
    {
      use_stupid = true;
    }
    else if (filename_ptr == nullptr)
    {
      filename_ptr = argv[i];
    }
  }

  if (filename_ptr == nullptr)
  {
    return repl();
  }

  if (!std::filesystem::exists(filename_ptr))
  {
    std::cout << "file " << filename_ptr << " not found";
    return -1;
  }

  std::string filename{filename_ptr};

  std::ifstream file{filename};
  std::string source{std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>()};

  try
  {
    auto ast = parse(source, filename);

    using namespace NG::typecheck;
    TypeIndex prelude_types = build_prelude_type_index();

    NG::typecheck::type_check(ast, prelude_types);

    if (use_stupid)
    {
      NG::intp::Interpreter *stupid = NG::intp::stupid();
      ast->accept(stupid);
    }
    else
    {
      // Derive module search paths from the input file's directory
      Vec<Str> modulePaths;
      namespace fs = std::filesystem;
      fs::path inputPath{filename};
      auto parentDir = inputPath.parent_path();
      if (!parentDir.empty())
      {
        modulePaths.push_back(parentDir.string());
      }
      // Also add standard lib paths
      modulePaths.push_back("lib");
      modulePaths.push_back("../lib");

      NG::orgasm::Compiler compiler{modulePaths, NG::library::prelude::native_function_names()};
      auto bytecode = compiler.compile(dynamic_ast_cast<CompileUnit>(ast));
      NG::orgasm::VM vm;

      // Register native functions from the prelude
      NG::library::prelude::register_vm_natives(vm);

      vm.run(bytecode);
    }

    destroyast(ast);
  }
  catch (ParseException &ex)
  {
    std::cout << "Parse error: " << ex.what() << " at " << ex.pos.line << ":" << ex.pos.col << std::endl;
    return -1;
  }
  catch (TypeCheckingException &ex)
  {
    std::cout << "Type check error: " << ex.what() << " at " << ex.pos.line << ":" << ex.pos.col << std::endl;
    return -1;
  }
  catch (NG::RuntimeException &ex)
  {
    std::cout << "Runtime error: " << ex.what() << " at " << ex.pos.line << ":" << ex.pos.col << std::endl;
    return -1;
  }
  catch (const std::exception &ex)
  {
    std::cout << "Error: " << ex.what() << std::endl;
    return -1;
  }
}
