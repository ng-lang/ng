Internals
========


ng has few stages, including:

  - Lexical analysis
  - Grammar analysis
  - Semantic analysis
  - Optimization
  - Code generating

As ng implemented in C++ (for now), this document shows how these stages implemented and
some internal data structure.

## 1. Lexical analysis

ng internally using the `NG::Token` struct to describe the lexical tokens, you can find
the details in `src/include/token.hpp`
```C++
struct Token
{
    TokenType type;
    std::string repr;
    TokenPosition position;
    Operators operatorType;
};
```

ng has few token types: keywords (including both actual keywords and reserved words),
identifiers, literals, special symbols and operators. You can goto [Lexical](Lexical.md) to
get full list of these definitions.

Lexer manipulates a structure `NG::LexState`, to visit and perform lexical analysis:
```C++
// see src/include/parser.hpp
struct LexState
{
    const std::string source;
    const size_t size;
    size_t index;

    size_t line;
    size_t col;

    LexState(std::string _source);

    char current() const;
    bool eof() const;
    void next(int n = 1);
    void revert(size_t n = 1);
    void nextLine();

    char lookAhead() const;
};
```

After whole lexical process, Lexer::lex will produce a list of tokens (`std::vector<NG::Token>`) for parser as its input.

## 2. Grammar analysis

Grammar analysis will produce an AST for a ng source file. AST is a noncopyable object
tree and all its definitions in `src/include/ast.hpp`. The basic structure of AST is the
`ASTRef<T>` type, it is currently just a basic alias of `T*`. You can replace it with
self-defined reference/pointer type to make it more convienent to use.

`ASTRef` must be created by `makeast` and destroyed by `destroyast` functions, and make
sure you are calling `destroyast` in the parent AST node destructor.
```C++
template<class T>
using ASTRef<T> = ...;

template<class T, class Args...>
ASTRef makeast(Args... args);

template<class T>
void destroyast(ASTRef<T> ast);
```

### 2.1 AST node structure

This is current definition of few useful basic:
```C++
/** pure abstract **/
struct ASTNode : NonCopyable
{
    ASTNode() {}
    virtual void accept(AstVisitor *visitor);
    virtual ~ASTNode() = 0;
};

struct Statement : ASTNode
{
};

struct Definition : ASTNode
{
    virtual Str name() const = 0;
};

struct Expression : ASTNode
{
};
```

### 2.2 AST visitor

AST will be analyzed by visitors, all AST visitor must follow the `AstVisitor` interface.
```C++
class AstVisitor : NonCopyable
{
  public:
    virtual void visit(ASTRef<ASTNode> astNode) = 0;

    virtual void visit(ASTRef<Statement> statement) = 0;
    ...; // other visit function definitions

    virtual ~AstVisitor() = 0;
};
```

ng provides a default implementation `DummyVisitor` which set all `visit` functions
to empty, you can directly inherite it just modify what you need.

There is an exmaple `NG::ASTDumper` in `src/ast_dump.cpp`.
