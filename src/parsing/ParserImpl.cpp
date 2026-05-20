
#include "common.hpp"
#include "parser.hpp"
#include "token.hpp"
#include <ast.hpp>
#include <debug.hpp>
#include <filesystem>
#include <functional>
#include <regex>
#include <utility>
#include <visitor.hpp>

namespace fs = std::filesystem;

namespace NG::parsing
{
  using namespace NG;
  using namespace NG::ast;

  const std::regex IMPORT_DECL_PATTERN{"^[A-Za-z_][A-Za-z_\\-0-9\\.]+$"};

  static const Set<TokenType> unary_operators{
    TokenType::NOT,
    TokenType::MINUS,
    TokenType::QUERY,
    TokenType::AMPERSAND,
    TokenType::TIMES,
    TokenType::KEYWORD_REF,
    TokenType::KEYWORD_MOVE,
  };

  [[nodiscard]] inline auto isUnaryOperator(TokenType optr) -> bool
  {
    return unary_operators.contains(optr);
  }

  class ParserImpl
  {
    ParseState state;

        template <class T, class... Args>
        auto createNode(Args &&...args) -> ASTRef<T>
        {
          auto node = makeast<T>(std::forward<Args>(args)...);
          if (!state.eof())      {
        node->pos = state->position;
      }
      else if (!state.tokens.empty())
      {
        node->pos = state.tokens.back().position;
      }
      return node;
    }

    [[noreturn]]
    void unexpected(Str message)
    {
      if (message.empty())
      {
        message = std::string{"Unexpected token "} + state->repr;
      }
      throw ParseException(message, state->position);
    }

  public:
    explicit ParserImpl(ParseState &state) : state(state) {}

    auto parse(const Str &fileName) -> ASTRef<ASTNode>
    {
      // file as default module

      auto compileUnit = createNode<CompileUnit>();

      compileUnit->fileName = fileName;
      fs::path filePath{fileName};
      if (fs::exists(filePath))
      {
        compileUnit->path = filePath.parent_path().string();
      }

      Str fileWithoutPath{filePath.filename()};
      Str moudleName = fileWithoutPath;
      if (moudleName.ends_with(".ng"))
      {
        moudleName = fileWithoutPath.substr(0, fileWithoutPath.size() - 3);
      }

      auto mod = createNode<Module>();
      mod->name = moudleName;
      ASTRef<Module> current_mod = mod;
      compileUnit->module = mod;
      bool moduleDeclared = false;

      while (!state.eof())
      {
        bool exported = false;
        if (expect(TokenType::KEYWORD_EXPORT))
        {
          exported = true;
          accept(TokenType::KEYWORD_EXPORT);
          // todo: export single identifiers
        }
        switch (state->type)
        {
        case TokenType::KEYWORD_FUN:
        {
          auto fn = funDef();
          if (exported)
          {
            for (auto &&name : fn->names())
            {
              mod->exports.push_back(name);
            }
          }
          current_mod->definitions.push_back(std::move(fn));
          break;
        }
        case TokenType::KEYWORD_VAL:
        {
          auto value = valDef();
          if (exported)
          {
            for (auto &&name : value->names())
            {
              mod->exports.push_back(name);
            }
          }
          current_mod->definitions.push_back(std::move(value));
          break;
        }
        case TokenType::KEYWORD_CONST:
        {
          if (peekTokenType(1) == TokenType::KEYWORD_IF)
          {
            current_mod->statements.push_back(statement());
            break;
          }
          auto constant = constDef();
          if (exported)
          {
            for (auto &&name : constant->names())
            {
              mod->exports.push_back(name);
            }
          }
          current_mod->definitions.push_back(std::move(constant));
          break;
        }
        case TokenType::KEYWORD_TYPE:
        {
          auto type = typeDef();
          if (exported)
          {
            for (auto &&name : type->names())
            {
              mod->exports.push_back(name);
            }
          }
          current_mod->definitions.push_back(std::move(type));
          break;
        }
        case TokenType::KEYWORD_TRAIT:
        {
          auto trait = traitDef();
          if (exported)
          {
            for (auto &&name : trait->names())
            {
              mod->exports.push_back(name);
            }
          }
          current_mod->definitions.push_back(std::move(trait));
          break;
        }
        case TokenType::KEYWORD_IMPL:
        {
          current_mod->definitions.push_back(implDef());
          break;
        }
        case TokenType::KEYWORD_MODULE:
        {
          if (moduleDeclared)
          {
            unexpected("Redeclare a module");
          }
          moduleDeclared = true;
          moduleDecl(mod);
          break;
        }
        // todo: export an import.
        case TokenType::KEYWORD_IMPORT:
        {
          current_mod->imports.push_back(importDecl());
          break;
        }
          // case TokenType::KEYWORD_IF:
        case TokenType::KEYWORD_CASE:
        case TokenType::KEYWORD_LOOP:
        case TokenType::KEYWORD_COLLECT:
        case TokenType::KEYWORD_NEXT:
        default:
        {
          if (exported)
          {
            unexpected("Invalid export: only definitions can be exported");
          }
          current_mod->statements.push_back(statement());
        }
        }
      }
      return compileUnit;
    }

  private:
    auto expect(TokenType type) -> bool { return !state.eof() && state->type == type; }

    auto expectTypeSuffixKeyword() -> bool
    {
      return expect(TokenType::ID) || expect(TokenType::KEYWORD_REF);
    }

    auto isDerefExpression(const ASTRef<Expression> &expr) -> bool
    {
      auto unaryExpr = dynamic_ast_cast<UnaryExpression>(expr);
      return unaryExpr != nullptr && unaryExpr->optr != nullptr && unaryExpr->optr->type == TokenType::TIMES;
    }

    auto expressionTerminatorAfterGenericArgs(TokenType type) -> bool
    {
      return type == TokenType::LEFT_PAREN || type == TokenType::RIGHT_PAREN || type == TokenType::LEFT_CURLY ||
             type == TokenType::RIGHT_CURLY || type == TokenType::RIGHT_SQUARE || type == TokenType::SEMICOLON ||
             type == TokenType::COMMA || type == TokenType::COLON || type == TokenType::BIND ||
             type == TokenType::AND || type == TokenType::OR;
    }

    auto genericExpressionArgsAhead() -> bool
    {
      if (!expect(TokenType::LT))
      {
        return false;
      }
      int depth = 0;
      for (size_t i = state.index; i < state.size; ++i)
      {
        auto tokenType = state.tokens[i].type;
        if (tokenType == TokenType::LT)
        {
          ++depth;
        }
        else if (tokenType == TokenType::GT)
        {
          --depth;
        }
        else if (tokenType == TokenType::RSHIFT)
        {
          depth -= 2;
        }
        if (depth == 0)
        {
          auto nextIndex = i + 1;
          return nextIndex >= state.size || expressionTerminatorAfterGenericArgs(state.tokens[nextIndex].type);
        }
        if (depth < 0)
        {
          return false;
        }
      }
      return false;
    }

    auto genericConstExpression(ASTRef<Expression> primaryExpression,
                                Vec<std::shared_ptr<TypeAnnotation>> genericArgs) -> ASTRef<FunCallExpression>
    {
      auto expr = createNode<FunCallExpression>();
      expr->primaryExpression = std::move(primaryExpression);
      expr->genericArgs = std::move(genericArgs);
      return expr;
    }

    auto postfixExpression(ASTRef<Expression> expr) -> ASTRef<Expression>
    {
      while (expect(TokenType::LEFT_PAREN) || expect(TokenType::LT) || expect(TokenType::DOT) || expect(TokenType::LEFT_SQUARE) ||
             expect(TokenType::SEPARATOR))
      {
        if (expect(TokenType::LEFT_PAREN))
        {
          expr = std::move(funCallExpression(std::move(expr)));
        }
        else if (expect(TokenType::LT) && dynamic_ast_cast<IdExpression>(expr) && genericExpressionArgsAhead())
        {
          auto genericTypeArgs = genericArgs();
          if (expect(TokenType::LEFT_PAREN))
          {
            expr = std::move(funCallExpression(std::move(expr), std::move(genericTypeArgs)));
          }
          else
          {
            expr = std::move(genericConstExpression(std::move(expr), std::move(genericTypeArgs)));
          }
        }
        else if (expect(TokenType::DOT))
        {
          expr = std::move(idAccessorExpression(std::move(expr)));
        }
        else if (expect(TokenType::LEFT_SQUARE))
        {
          expr = std::move(indexAccessorExpression(std::move(expr)));
        }
        else if (expect(TokenType::SEPARATOR))
        {
          expr = std::move(staticQualifiedTraitCallExpression(std::move(expr)));
        }
        else
        {
          break;
        }
      }
      return expr;
    }

    auto peekTokenType(int offset) -> TokenType
    {
      size_t target = state.index + offset;
      if (target >= state.size) return TokenType::NONE;
      return state.tokens[target].type;
    }

    void accept(TokenType type)
    {
      if (!expect(type))
      {
        return unexpected("Unexpected token " + state->repr);
      }
      state.next();
    }

    /**
     * Accept a GT token, but also handle RIGHT_SHIFT (>>) by splitting it
     * into two GT tokens. This handles nested generics like Option<Option<int>>.
     */
    void acceptGT()
    {
      if (expect(TokenType::GT))
      {
        state.next();
      }
      else if (!state.eof() && state->type == TokenType::RSHIFT)
      {
        // Split >> into two > tokens: replace current >> with >, insert second > after it
        state.tokens[state.index].type = TokenType::GT;
        state.tokens[state.index].repr = ">";
        Token secondGT;
        secondGT.type = TokenType::GT;
        secondGT.repr = ">";
        secondGT.position = state.tokens[state.index].position;
        state.tokens.insert(state.tokens.begin() + state.index + 1, secondGT);
        state.size = state.tokens.size();
        state.next(); // consume the first >
      }
      else
      {
        unexpected("Unexpected token " + state->repr + ", expected '>'");
      }
    }

    /**
     * Parse generic type parameters: <T, U, T..., T: Comparable>
     */
    auto genericParams() -> Vec<ASTRef<GenericParam>>
    {
      Vec<ASTRef<GenericParam>> params;
      accept(TokenType::LT);
      while (!expect(TokenType::GT) && !state.eof())
      {
        if (!expect(TokenType::ID))
        {
          unexpected("Expected generic type parameter name");
        }
        auto param = createNode<GenericParam>(state->repr);
        accept(TokenType::ID);

        // Higher-kinded type constructor parameter: F<_>, F<_, _>, ...
        if (expect(TokenType::LT))
        {
          accept(TokenType::LT);
          size_t arity = 0;
          while (!expect(TokenType::GT) && !state.eof() && !expect(TokenType::RSHIFT))
          {
            if (!expect(TokenType::ID) || state->repr != "_")
            {
              unexpected("Expected '_' placeholder in generic type constructor parameter");
            }
            accept(TokenType::ID);
            ++arity;
            if (expect(TokenType::COMMA))
            {
              accept(TokenType::COMMA);
            }
          }
          if (arity == 0)
          {
            unexpected("Generic type constructor parameter must declare at least one '_' placeholder");
          }
          param->kindArity = arity;
          acceptGT();
        }

        // Check for parameter pack: T...
        if (expect(TokenType::SPREAD))
        {
          accept(TokenType::SPREAD);
          param->isPack = true;
        }

        // Check for type bound: T: Comparable
        if (expect(TokenType::COLON))
        {
          accept(TokenType::COLON);
          param->bound = typeAnnotation();
        }

        params.push_back(std::move(param));

        if (expect(TokenType::COMMA))
        {
          accept(TokenType::COMMA);
        }
      }
      acceptGT();
      return params;
    }

    /**
     * Parse generic type arguments: <int, string, bool>
     */
    auto genericArgs() -> Vec<std::shared_ptr<TypeAnnotation>>
    {
      Vec<std::shared_ptr<TypeAnnotation>> args;
      accept(TokenType::LT);
      while (!expect(TokenType::GT) && !state.eof() && !expect(TokenType::RSHIFT))
      {
        auto arg = typeAnnotation();
        // Convert ASTRef to shared_ptr for genericArgs field
        args.push_back(std::shared_ptr<TypeAnnotation>(std::move(arg)));

        if (expect(TokenType::COMMA))
        {
          accept(TokenType::COMMA);
        }
      }
      acceptGT();
      return args;
    }

    auto cloneSimpleTypeAnnotation(const TypeAnnotation &annotation) -> ASTRef<TypeAnnotation>
    {
      if (!annotation.genericArgs.empty() || !annotation.arguments.empty())
      {
        unexpected("Repeated trait bounds only support a simple generic parameter subject");
      }
      auto clone = createNode<TypeAnnotation>(annotation.name);
      clone->type = annotation.type;
      return clone;
    }

    auto whereBounds() -> Vec<ASTRef<TraitBound>>
    {
      Vec<ASTRef<TraitBound>> bounds;
      accept(TokenType::KEYWORD_WHERE);
      while (!state.eof())
      {
        if (expect(TokenType::ID) && peekTokenType(1) == TokenType::COLON)
        {
          auto subject = typeAnnotation();
          accept(TokenType::COLON);
          auto trait = typeAnnotation();
          bounds.push_back(createNode<TraitBound>(std::move(subject), std::move(trait)));
          while (expect(TokenType::PLUS))
          {
            accept(TokenType::PLUS);
            auto repeatedSubject = cloneSimpleTypeAnnotation(*bounds.back()->subject);
            bounds.push_back(createNode<TraitBound>(std::move(repeatedSubject), typeAnnotation()));
          }
        }
        else
        {
          bounds.push_back(createNode<TraitBound>(expression(true)));
        }
        if (!expect(TokenType::AND))
        {
          break;
        }
        accept(TokenType::AND);
      }
      return bounds;
    }

    auto funDef(bool allowSignatureOnly = false) -> ASTRef<FunctionDef>
    {
      accept(TokenType::KEYWORD_FUN);

      // Parse generic parameters: fun<T, U> name(...) or fun<T...> name(...)
      Vec<ASTRef<GenericParam>> pendingGenericParams;
      if (expect(TokenType::LT))
      {
        pendingGenericParams = std::move(genericParams());
      }

      if (expect(TokenType::ID))
      {
        auto def = createNode<FunctionDef>();
        def->funName = state->repr;
        accept(TokenType::ID);

        def->genericParams = std::move(pendingGenericParams);

        // Also support generic params after function name: fun name<T, U>(...)
        if (def->genericParams.empty() && expect(TokenType::LT))
        {
          def->genericParams = std::move(genericParams());
        }

        def->params = std::move(funParams());

        if (expect(TokenType::SINGLE_ARROW))
        {
          accept(TokenType::SINGLE_ARROW);
          def->returnType = std::move(typeAnnotation());
        }

        if (expect(TokenType::KEYWORD_WHERE))
        {
          def->whereBounds = whereBounds();
        }

        if (allowSignatureOnly && expect(TokenType::SEMICOLON))
        {
          accept(TokenType::SEMICOLON);
          return def;
        }

        if (expect(TokenType::BIND))
        {
          accept(TokenType::BIND);
          if (expect(TokenType::KEYWORD_NATIVE))
          {
            accept(TokenType::KEYWORD_NATIVE);
            def->native = true;
            if (def->returnType == nullptr)
            {
              unexpected("Native function '" + def->funName + "' must declare a return type.");
            }
            accept(TokenType::SEMICOLON);
            return def;
          }
          else
          {
            auto expressionBody = expression();
            auto body = createNode<ReturnStatement>();
            body->expression = expressionBody;
            def->body = body;
            accept(TokenType::SEMICOLON);
            return def;
          }
        }
        def->body = std::move(statement());
        return def;
      }
      unexpected("Expected function name");
    }

    auto constDef() -> ASTRef<ConstDef>
    {
      accept(TokenType::KEYWORD_CONST);
      Vec<ASTRef<GenericParam>> constGenericParams;
      if (expect(TokenType::LT))
      {
        constGenericParams = std::move(genericParams());
      }
      if (!expect(TokenType::ID))
      {
        unexpected("Expected const name");
      }
      auto constName = state->repr;
      accept(TokenType::ID);
      ASTRef<TypeAnnotation> specializationPattern = nullptr;
      if (expect(TokenType::LT))
      {
        if (constGenericParams.empty())
        {
          constGenericParams = std::move(genericParams());
        }
        else
        {
          specializationPattern = createNode<TypeAnnotation>(constName);
          specializationPattern->type = TypeAnnotationType::CUSTOMIZED;
          specializationPattern->genericArgs = std::move(genericArgs());
        }
      }
      auto def = createNode<ConstDef>(constName);
      def->genericParams = std::move(constGenericParams);
      def->specializationPattern = std::move(specializationPattern);
      if (expect(TokenType::KEYWORD_WHERE))
      {
        def->whereBounds = whereBounds();
      }
      accept(TokenType::COLON);
      def->returnType = std::move(typeAnnotation());
      accept(TokenType::BIND);
      if (expect(TokenType::KEYWORD_NATIVE))
      {
        accept(TokenType::KEYWORD_NATIVE);
        def->native = true;
      }
      else
      {
        def->value = std::move(expression());
      }
      accept(TokenType::SEMICOLON);
      return def;
    }

    /**
     * syntax:
     *   import a.b.c;
     *   import "a"."b"."c";
     *   import "abc" c;
     *   import abcdef c;
     *   import a.b.c (a, b, c);
     *   import a.b.c (*);
     *   import a.b.c abc (a, b, c);
     */
    auto importDecl() -> ASTRef<ImportDecl> // NOLINT(readability-function-cognitive-complexity)
    {
      accept(TokenType::KEYWORD_IMPORT);
      auto imp = createNode<ImportDecl>();

      Vec<Str> modulePath{};

      while (expect(TokenType::ID) || expect(TokenType::STRING))
      {
        Str moduleSegment = state->repr;
        accept(state->type);
        if (std::regex_match(moduleSegment, IMPORT_DECL_PATTERN))
        {
          modulePath.push_back(moduleSegment);
          // always use latest
          imp->module = moduleSegment;
        }
        else
        {
          unexpected("Invalid module path when import.");
        }
        if (expect(TokenType::DOT))
        {
          accept(TokenType::DOT);
          continue;
        }
        else
        {
          imp->modulePath = modulePath;
          break;
        }
      }

      // alias import
      if (expect(TokenType::ID))
      {
        Str alias = state->repr;
        accept(TokenType::ID);
        imp->alias = alias;
      }

      // symbol import
      if (expect(TokenType::LEFT_PAREN))
      {
        accept(TokenType::LEFT_PAREN);

        while (expect(TokenType::ID))
        {
          imp->imports.push_back(state->repr);
          accept(TokenType::ID);
          if (!expect(TokenType::COMMA))
          {
            break;
          }
          accept(TokenType::COMMA);
        }
        if (imp->imports.empty() && expect(TokenType::TIMES))
        {
          accept(TokenType::TIMES);
          imp->imports.emplace_back("*");
        }
        accept(TokenType::RIGHT_PAREN);
      }
      else
      {
        if (expect(TokenType::TIMES))
        {
          accept(TokenType::TIMES);
          imp->imports.emplace_back("*");
        }
      }
      if (imp->imports.empty() && imp->alias.empty())
      {
        imp->alias = imp->module;
      }
      accept(TokenType::SEMICOLON);

      return imp;
    }

    auto valDef() -> ASTRef<ValDef> { return createNode<ValDef>(valDefStatement()); }

    auto propertyDef() -> ASTRef<PropertyDef>
    {
      accept(TokenType::KEYWORD_PROPERTY);

      auto name = idExpression();
      ASTRef<TypeAnnotation> type = nullptr;

      if (expect(TokenType::COLON))
      {
        accept(TokenType::COLON);
        type = typeAnnotation();
      }

      accept(TokenType::SEMICOLON);
      return createNode<PropertyDef>((name)->repr(), std::move(type));
    }

    auto traitDef() -> ASTRef<TraitDef>
    {
      accept(TokenType::KEYWORD_TRAIT);
      auto trait = createNode<TraitDef>();
      trait->traitName = idExpression()->repr();

      if (expect(TokenType::LT))
      {
        trait->genericParams = genericParams();
      }

      if (expect(TokenType::COLON))
      {
        accept(TokenType::COLON);
        while (!state.eof())
        {
          trait->superTraits.push_back(typeAnnotation());
          if (!expect(TokenType::PLUS))
          {
            break;
          }
          accept(TokenType::PLUS);
        }
      }

      accept(TokenType::LEFT_CURLY);
      while (!expect(TokenType::RIGHT_CURLY))
      {
        if (!expect(TokenType::KEYWORD_FUN))
        {
          unexpected("Expected trait method declaration");
        }
        auto method = funDef(true);
        trait->methods.push_back(std::move(method));
      }
      accept(TokenType::RIGHT_CURLY);
      return trait;
    }

    auto implDef() -> ASTRef<ImplDef>
    {
      accept(TokenType::KEYWORD_IMPL);
      auto impl = createNode<ImplDef>();

      if (expect(TokenType::LT))
      {
        impl->genericParams = genericParams();
      }

      impl->trait = typeAnnotation();
      accept(TokenType::KEYWORD_FOR);
      impl->targetType = typeAnnotation();

      if (expect(TokenType::KEYWORD_WHERE))
      {
        impl->whereBounds = whereBounds();
      }

      accept(TokenType::LEFT_CURLY);
      while (!expect(TokenType::RIGHT_CURLY))
      {
        if (!expect(TokenType::KEYWORD_FUN))
        {
          unexpected("Expected impl method definition");
        }
        impl->methods.push_back(funDef());
      }
      accept(TokenType::RIGHT_CURLY);
      return impl;
    }

    void parseTypeAliasConstraintSection(Vec<ASTRef<TraitBound>> &whereBoundsOut)
    {
      if (!expect(TokenType::COLON))
      {
        return;
      }
      accept(TokenType::COLON);
      if (!expect(TokenType::KEYWORD_WHERE))
      {
        unexpected("Type alias constraint section only supports `: where ...`");
      }
      whereBoundsOut = whereBounds();
    }

    auto typeDef() -> ASTRef<Definition>
    {
      accept(TokenType::KEYWORD_TYPE);

      Vec<ASTRef<GenericParam>> typeGenericParams;
      if (expect(TokenType::LT))
      {
        typeGenericParams = std::move(genericParams());
      }

      Str nameStr;
      if (expect(TokenType::KEYWORD_REF))
      {
        nameStr = "ref";
        accept(TokenType::KEYWORD_REF);
      }
      else
      {
        auto typeName = idExpression();
        nameStr = typeName->repr();
      }
      ASTRef<TypeAnnotation> specializationPattern = nullptr;

      // Parse generic parameters on type name: type Option<T> { ... }
      // For `type<T> Name<Pattern> = ...`, the annotation is a specialization pattern.
      if (expect(TokenType::LT))
      {
        if (typeGenericParams.empty())
        {
          typeGenericParams = std::move(genericParams());
        }
        else
        {
          specializationPattern = createNode<TypeAnnotation>(nameStr);
          specializationPattern->type = TypeAnnotationType::CUSTOMIZED;
          specializationPattern->genericArgs = std::move(genericArgs());
        }
      }

      Vec<ASTRef<TraitBound>> aliasWhereBounds;
      parseTypeAliasConstraintSection(aliasWhereBounds);

      // Check for type alias or tagged union syntax: type A = B; or type A = V1(T) | V2(T);
      if (expect(TokenType::BIND))
      {
        accept(TokenType::BIND);

        // Check if this is a tagged union or type alias.
        // Heuristic: if next token after ID is `(` or `|`, it's a tagged union.
        // If it's `;` or `=` or `,`, it's a type alias.
        if (expect(TokenType::ID) &&
            ((peekTokenType(1) == TokenType::LEFT_PAREN) || (peekTokenType(1) == TokenType::PIPE)))
        {
          auto taggedUnion = createNode<TaggedUnionDef>();
          taggedUnion->typeName = nameStr;
          taggedUnion->genericParams = std::move(typeGenericParams);

          // Parse first variant
          VariantDef variant;
          variant.variantName = state->repr;
          accept(TokenType::ID);
          if (expect(TokenType::LEFT_PAREN))
          {
            accept(TokenType::LEFT_PAREN);
            while (!expect(TokenType::RIGHT_PAREN))
            {
              // Skip optional field name: "name: type" or just "type"
              if (expect(TokenType::ID) && peekTokenType(1) == TokenType::COLON)
              {
                variant.payloadNames.push_back(state->repr); // store field name
                accept(TokenType::ID);   // skip field name
                accept(TokenType::COLON); // skip colon
              }
              variant.payloadTypes.push_back(typeAnnotation());
              if (expect(TokenType::COMMA)) accept(TokenType::COMMA);
            }
            accept(TokenType::RIGHT_PAREN);
          }
          taggedUnion->variants.push_back(std::move(variant));

          // Parse additional variants separated by PIPE
          while (expect(TokenType::PIPE))
          {
            accept(TokenType::PIPE);
            VariantDef v;
            v.variantName = state->repr;
            accept(TokenType::ID);
            if (expect(TokenType::LEFT_PAREN))
            {
              accept(TokenType::LEFT_PAREN);
              while (!expect(TokenType::RIGHT_PAREN))
              {
                if (expect(TokenType::ID) && peekTokenType(1) == TokenType::COLON)
                {
                  v.payloadNames.push_back(state->repr); // store field name
                  accept(TokenType::ID);
                  accept(TokenType::COLON);
                }
                v.payloadTypes.push_back(typeAnnotation());
                if (expect(TokenType::COMMA)) accept(TokenType::COMMA);
              }
              accept(TokenType::RIGHT_PAREN);
            }
            taggedUnion->variants.push_back(std::move(v));
          }

          accept(TokenType::SEMICOLON);
          return taggedUnion;
        }

        // Otherwise it's a type alias
        auto aliasDef = createNode<TypeAliasDef>(nameStr);
        aliasDef->genericParams = std::move(typeGenericParams);
        aliasDef->specializationPattern = std::move(specializationPattern);
        aliasDef->whereBounds = std::move(aliasWhereBounds);
        if (expect(TokenType::KEYWORD_NATIVE))
        {
          accept(TokenType::KEYWORD_NATIVE);
          aliasDef->nativeOpaque = true;
        }
        else if (expect(TokenType::KEYWORD_DELETE))
        {
          accept(TokenType::KEYWORD_DELETE);
          aliasDef->deleted = true;
        }
        else
        {
          aliasDef->underlyingType = std::move(typeAnnotation());
        }
        accept(TokenType::SEMICOLON);
        return aliasDef;
      }

      if (expect(TokenType::SEMICOLON))
      {
        auto aliasDef = createNode<TypeAliasDef>(nameStr);
        aliasDef->genericParams = std::move(typeGenericParams);
        aliasDef->specializationPattern = std::move(specializationPattern);
        aliasDef->whereBounds = std::move(aliasWhereBounds);
        aliasDef->abstract = true;
        accept(TokenType::SEMICOLON);
        return aliasDef;
      }

      // Check for newtype syntax: type A wraps B;
      if (expect(TokenType::KEYWORD_WRAPS))
      {
        accept(TokenType::KEYWORD_WRAPS);
        auto ntDef = createNode<NewTypeDef>(nameStr);
        ntDef->genericParams = std::move(typeGenericParams);
        ntDef->wrappedType = std::move(typeAnnotation());
        accept(TokenType::SEMICOLON);
        return ntDef;
      }

      // Regular type definition: type X { ... }
      auto typeDef = createNode<TypeDef>();
      typeDef->typeName = nameStr;
      typeDef->genericParams = std::move(typeGenericParams);

      accept(TokenType::LEFT_CURLY);

      while (!expect(TokenType::RIGHT_CURLY))
      {
        if (expect(TokenType::KEYWORD_PROPERTY))
        {
          typeDef->properties.push_back(propertyDef());
        }
        else if (expect(TokenType::KEYWORD_FUN))
        {
          typeDef->memberFunctions.push_back(funDef());
        }
        else if (expect(TokenType::ID))
        {
          // Shorthand property: name: type;
          auto name = idExpression();
          ASTRef<TypeAnnotation> type = nullptr;
          if (expect(TokenType::COLON))
          {
            accept(TokenType::COLON);
            type = typeAnnotation();
          }
          accept(TokenType::SEMICOLON);
          typeDef->properties.push_back(createNode<PropertyDef>((name)->repr(), std::move(type)));
        }
        else
        {
          unexpected("Expected property or function definition in type body");
        }
      }
      accept(TokenType::RIGHT_CURLY);
      return typeDef;
    }

    auto moduleDecl(ASTRef<Module> mod) -> ASTRef<Module>
    {
      accept(TokenType::KEYWORD_MODULE);
      if (expect(TokenType::ID))
      {
        auto moduleName = state->repr;
        accept(TokenType::ID);
        if (mod->name == moduleName)
        {
        }
        else if (mod->name == "[noname]" || mod->name == "[interpreter]")
        {
          mod->name = moduleName;
        }
        else
        {
          unexpected("Invalid module name for module: " + moduleName + ", expected: " + mod->name);
        }
      }
      if (expect(TokenType::KEYWORD_EXPORTS))
      {
        accept(TokenType::KEYWORD_EXPORTS);
        if (state->type == TokenType::TIMES)
        {
          accept(TokenType::TIMES);
          mod->exports.emplace_back("*");
        }
        else
        {
          mod->exports = std::move(exportList());
        }
      }
      accept(TokenType::SEMICOLON);
      return mod;
    }

    auto exportList() -> Vec<Str>
    {
      bool withParen = false;

      Vec<Str> exports{};

      if (expect(TokenType::LEFT_PAREN))
      {
        withParen = true;
        accept(TokenType::LEFT_PAREN);
      }

      while (expect(TokenType::ID))
      {
        auto &&symbol = state->repr;
        exports.push_back(symbol);
        accept(TokenType::ID);
        if (!expect(TokenType::COMMA))
        {
          break;
        }
        accept(TokenType::COMMA);
      }
      if (withParen)
      {
        accept(TokenType::RIGHT_PAREN);
      }

      return exports;
    }

    auto funParams() -> Vec<ASTRef<Param>>
    {
      Vec<ASTRef<Param>> params{};
      if (expect(TokenType::LEFT_PAREN))
      {
        accept(TokenType::LEFT_PAREN);

        while (expect(TokenType::ID))
        {
          const Str &name = state->repr;
          ASTRef<Param> param = createNode<Param>(name);
          accept(TokenType::ID);
          if (expect(TokenType::COLON))
          {
            accept(TokenType::COLON);
            auto anno = typeAnnotation();

            // Handle variadic parameter syntax: args: T...
            if (expect(TokenType::SPREAD))
            {
              accept(TokenType::SPREAD);
              // Create a new type annotation with "..." suffix to signal variadic to the type checker
              auto variadicAnno = createNode<TypeAnnotation>(anno->repr() + "...");
              variadicAnno->type = anno->type;
              variadicAnno->arguments = std::move(anno->arguments);
              variadicAnno->genericArgs = std::move(anno->genericArgs);
              anno = std::move(variadicAnno);
            }

            param = createNode<Param>(name, anno);
          }
          params.push_back(param);
          if (expect(TokenType::BIND))
          {
            accept(TokenType::BIND);
            param->value = expression();
          }

          if (!expect(TokenType::COMMA))
          {
            break;
          }
          accept(TokenType::COMMA);
        }

        accept(TokenType::RIGHT_PAREN);
      }
      return params;
    }

    auto statement() -> ASTRef<Statement>
    {
      switch (state->type)
      {
      case TokenType::LEFT_CURLY:
        return compoundStatement();
      case TokenType::KEYWORD_RETURN:
        return returnStatement();
      case TokenType::DUAL_ARROW:
        return arrowReturn();
      case TokenType::KEYWORD_IF:
        return ifStatement(false);
      case TokenType::KEYWORD_CONST:
        if (peekTokenType(1) == TokenType::KEYWORD_IF)
        {
          return ifStatement(true);
        }
        return simpleStatement();
      case TokenType::KEYWORD_VAL:
        return valDefStatement();
      case TokenType::KEYWORD_LOOP:
        return loopStatement();
      case TokenType::KEYWORD_SWITCH:
        return switchStatement();
      case TokenType::KEYWORD_NEXT:
        return nextStatement();
      case TokenType::SEMICOLON:
        accept(TokenType::SEMICOLON);
        return createNode<EmptyStatement>();
      default:
        return simpleStatement();
      }
    }

    auto simpleStatement() -> ASTRef<SimpleStatement>
    {
      auto expr = expression();
      accept(TokenType::SEMICOLON);
      auto stmt = createNode<SimpleStatement>();
      stmt->expression = std::move(expr);
      return stmt;
    }

    auto typeAnnotation() -> ASTRef<TypeAnnotation>
    {
      ASTRef<TypeAnnotation> result = typeAnnotationBase();
      // Apply suffix generic syntax for all type bases:
      // `i32 array` => array<i32>, `bool Optional` => Optional<bool>
      // `(string, i32) Map` => Map<string, i32>
      // Left-associative: `i32 array Optional` => Optional<array<i32>>
      result = parseSuffixGeneric(std::move(result));

      // Parse union type annotations: `i32 | string | bool`
      // Right-associative: `i32 | string | bool` => Union(i32, string, bool)
      if (expect(TokenType::PIPE))
      {
        auto unionNode = createNode<TypeAnnotation>("union");
        unionNode->type = TypeAnnotationType::UNION;
        unionNode->arguments.push_back(result);

        while (expect(TokenType::PIPE))
        {
          accept(TokenType::PIPE);
          auto member = typeAnnotationBase();
          member = parseSuffixGeneric(std::move(member));
          unionNode->arguments.push_back(member);
        }
        result = std::move(unionNode);
      }

      return result;
    }

    /**
     * Core type annotation parsing without suffix generic application.
     * typeAnnotation() wraps this with suffix generic support.
     */
    auto typeAnnotationBase() -> ASTRef<TypeAnnotation>
    {
      TokenType maybeBuiltin = state->type;
      if (code(TokenType::KEYWORD_INT) <= code(maybeBuiltin) && code(TokenType::KEYWORD_F128) >= code(maybeBuiltin))
      {
        ASTRef<TypeAnnotation> anno = createNode<TypeAnnotation>(state->repr);
        size_t builtin_type_code =
            code(maybeBuiltin) - code(TokenType::KEYWORD_INT) + code(TypeAnnotationType::BUILTIN_INT);
        anno->type = from_code<TypeAnnotationType>(builtin_type_code);
        accept(maybeBuiltin);
        return anno;
      }
      if (maybeBuiltin == TokenType::KEYWORD_UNIT)
      {
        ASTRef<TypeAnnotation> anno = createNode<TypeAnnotation>(state->repr);
        anno->type = TypeAnnotationType::BUILTIN_UNIT;
        accept(maybeBuiltin);
        return anno;
      }
      if (maybeBuiltin == TokenType::LEFT_SQUARE)
      {
        auto array = createNode<TypeAnnotation>("array");
        accept(TokenType::LEFT_SQUARE);
        array->type = TypeAnnotationType::ARRAY;
        auto argumentRst = typeAnnotation();
        array->arguments.push_back(argumentRst);
        accept(TokenType::RIGHT_SQUARE);
        return array;
      }
      if (maybeBuiltin == TokenType::LEFT_PAREN)
      {
        auto tuple = createNode<TypeAnnotation>("tuple");
        accept(TokenType::LEFT_PAREN);
        tuple->type = TypeAnnotationType::TUPLE;

        // Parse tuple element types
        while (!expect(TokenType::RIGHT_PAREN))
        {
          auto elementType = typeAnnotation();
          tuple->arguments.push_back(elementType);

          if (!expect(TokenType::COMMA))
          {
            break;
          }
          accept(TokenType::COMMA);
        }
        accept(TokenType::RIGHT_PAREN);
        return tuple;
      }
      if (maybeBuiltin == TokenType::KEYWORD_REF)
      {
        ASTRef<TypeAnnotation> anno = createNode<TypeAnnotation>("ref");
        anno->type = TypeAnnotationType::CUSTOMIZED;
        accept(TokenType::KEYWORD_REF);
        if (!expect(TokenType::LT))
        {
          unexpected("Expected '<' after ref in type annotation");
        }
        anno->genericArgs = std::move(genericArgs());
        return anno;
      }
      if (maybeBuiltin == TokenType::ID)
      {
        if (state->repr == "_")
        {
          unexpected("Type placeholder '_' is only allowed in generic parameter kind declarations");
        }
        ASTRef<TypeAnnotation> anno = createNode<TypeAnnotation>(state->repr);
        accept(TokenType::ID);
        anno->type = TypeAnnotationType::CUSTOMIZED;

        // Check for generic type arguments: TypeName<T, U>
        // In a type annotation context, `<` is unambiguous (not a comparison).
        if (expect(TokenType::LT) && !state.eof())
        {
          // Look ahead to see if this is a type argument list (not comparison).
          // Type arg lists: ID < type | ID , type , ... >
          // We try to parse it: if we see a valid type after `<`, it's generic args.
          // Save current state in case we need to backtrack.
          auto savedState = state;
          state.next(); // consume `<`
          bool isGenericArgs = false;
          if (!expect(TokenType::GT))
          {
            // Try to see if next token could start a type annotation
            TokenType next = state->type;
            if (next == TokenType::ID ||
                (code(TokenType::KEYWORD_INT) <= code(next) && code(TokenType::KEYWORD_F128) >= code(next)) ||
                next == TokenType::KEYWORD_UNIT || next == TokenType::KEYWORD_REF || next == TokenType::LEFT_SQUARE ||
                next == TokenType::LEFT_PAREN)
            {
              isGenericArgs = true;
            }
          }
          else
          {
            // Empty generic args like `Foo<>` - treat as generic
            isGenericArgs = true;
          }
          // Restore state
          state = savedState;

          if (isGenericArgs)
          {
            anno->genericArgs = std::move(genericArgs());
          }
        }
        return anno;
      }
      unexpected("Unknown type annotation");
    }

    // Parse suffix generic type application: `T TypeName` => TypeName<T>
    // Left-associative: `T A B` => B<A<T>>
    // Multi-param: `(T1, T2) Map` => Map<T1, T2>  (tuple elements spread as generic args)
    auto parseSuffixGeneric(ASTRef<TypeAnnotation> base) -> ASTRef<TypeAnnotation>
    {
      while (expectTypeSuffixKeyword())
      {
        auto suffixName = expect(TokenType::KEYWORD_REF) ? Str{"ref"} : state->repr;
        accept(state->type);

        auto wrapper = createNode<TypeAnnotation>(suffixName);
        wrapper->type = TypeAnnotationType::CUSTOMIZED;

        if (base->type == TypeAnnotationType::TUPLE)
        {
          // Multi-param suffix: `(T1, T2) Map` => Map<T1, T2>
          // Spread tuple elements as individual generic args
          for (auto &elem : base->arguments)
          {
            // arguments are shared_ptr<ASTNode> but hold TypeAnnotation nodes
            wrapper->genericArgs.push_back(std::static_pointer_cast<TypeAnnotation>(elem));
          }
        }
        else
        {
          // Single-param suffix: `i32 array` => array<i32>
          wrapper->genericArgs.push_back(std::shared_ptr<TypeAnnotation>(std::move(base)));
        }
        base = std::move(wrapper);
      }
      return base;
    }

    auto valDefStatement() -> ASTRef<Statement>
    {

      accept(TokenType::KEYWORD_VAL);
      auto name = state->repr;
      ASTRef<TypeAnnotation> anno{};
      if (expect(TokenType::LEFT_PAREN) || expect(TokenType::LEFT_SQUARE))
      {
        BindingType bindingType =
            state->type == TokenType::LEFT_PAREN ? BindingType::TUPLE_UNPACK : BindingType::ARRAY_UNPACK;
        const auto closing =
            bindingType == BindingType::TUPLE_UNPACK ? TokenType::RIGHT_PAREN : TokenType::RIGHT_SQUARE;

        accept(state->type);
        auto valBind = createNode<ValueBindingStatement>();
        valBind->type = bindingType;
        int index = 0;
        while (!expect(closing))
        {
          auto binding = createNode<Binding>();
          if (expect(TokenType::SPREAD))
          {
            if (valBind->bindings.size() > 0 && valBind->bindings.back()->spreadReceiver) [[unlikely]]
            {
              unexpected("Invalid unpack operator, only 1 allowed");
            }
            accept(TokenType::SPREAD);
            binding->spreadReceiver = true;
          }
          if (expect(TokenType::ID))
          {
            binding->name = state->repr;
            binding->index = index;
            accept(TokenType::ID);

            if (expect(TokenType::COLON))
            {
              accept(TokenType::COLON);
              binding->annotation = typeAnnotation();
            }
          }
          else if (binding->spreadReceiver)
          {
            binding->name = "";
            binding->index = index;
          }
          else
          {
            unexpected("Expected identifier or unpacking in value binding.");
          }
          valBind->bindings.push_back(std::move(binding));
          if (expect(TokenType::COMMA))
          {
            if (valBind->bindings.back()->spreadReceiver)
            {
              unexpected("Unpacking binding must be last one");
            }
            index += 1;
            accept(TokenType::COMMA);
            continue;
          }
          else
          {
            break;
          }
        }
        if (bindingType == BindingType::TUPLE_UNPACK)
        {
          accept(TokenType::RIGHT_PAREN);
        }
        else
        {
          accept(TokenType::RIGHT_SQUARE);
        }
        if (!expect(TokenType::BIND))
        {
          unexpected("Unexpected token " + state->repr + ", expect bind operator `=`.");
        }
        accept(TokenType::BIND);
        auto value = expression();
        valBind->value = value;
        accept(TokenType::SEMICOLON);
        return valBind;
      }
      else
      {
        accept(TokenType::ID);
        if (expect(TokenType::COLON))

        {
          accept(TokenType::COLON);
          anno = typeAnnotation();
        }
        if (!expect(TokenType::BIND))
        {
          unexpected("Unexpected token " + state->repr + ", expect bind operator `=`.");
        }
        accept(TokenType::BIND);
        auto value = expression();
        accept(TokenType::SEMICOLON);
        auto def = createNode<ValDefStatement>(name);
        def->value = std::move(value);
        def->typeAnnotation = std::move(anno);
        return def;
      }
    }

    auto ifStatement(bool isConst) -> ASTRef<IfStatement>
    {

      auto ifstmt = createNode<IfStatement>();
      ifstmt->isConst = isConst;
      if (isConst)
      {
        accept(TokenType::KEYWORD_CONST); // consume 'const'
      }
      accept(TokenType::KEYWORD_IF);
      accept(TokenType::LEFT_PAREN);

      auto testing = expression();
      ifstmt->testing = std::move(testing);
      accept(TokenType::RIGHT_PAREN);
      ;
      ifstmt->consequence = std::move(statement());

      if (expect(TokenType::KEYWORD_ELSE))
      {
        accept(TokenType::KEYWORD_ELSE);
        ifstmt->alternative = std::move(statement());
      }

      return ifstmt;
    }

    auto loopStatement() -> ASTRef<LoopStatement>
    {
      accept(TokenType::KEYWORD_LOOP);
      auto loopStmt = createNode<LoopStatement>();
      while (expect(TokenType::ID))
      {
        auto identifier = state->repr;
        accept(TokenType::ID);
        auto loopBindingType = LoopBindingType::LOOP_ASSIGN;
        ASTRef<TypeAnnotation> loopBindingAnnotation = nullptr;
        ASTRef<Expression> bindingTarget{createNode<IdExpression>(identifier)};
        if (state->type == TokenType::COLON)
        {
          accept(TokenType::COLON);
          loopBindingAnnotation = typeAnnotation();
        }
        if (state->type == TokenType::BIND)
        {
          accept(TokenType::BIND);
          loopBindingType = LoopBindingType::LOOP_ASSIGN;
          bindingTarget = expression(); // NOLINT(*-unused-variable)
        }
        if (bindingTarget)
        {
          loopStmt->bindings.emplace_back(LoopBinding{
            .name = identifier,
            .type = loopBindingType,
            .target = bindingTarget,
            .annotation = loopBindingAnnotation,
          });
        }
        else
        {
          unexpected("Invalid loop binding target for " + identifier);
        }
        if (expect(TokenType::COMMA))
        {
          accept(TokenType::COMMA);
          continue;
        }
        else
        {
          break;
        }
      }
      loopStmt->loopBody = std::move(statement());
      return loopStmt;
    }

    auto nextStatement() -> ASTRef<NextStatement>
    {
      accept(TokenType::KEYWORD_NEXT);
      auto nextStmt = createNode<NextStatement>();

      while (!expect(TokenType::SEMICOLON))
      {
        auto expr = expression();
        if (expr)
        {
          nextStmt->expressions.push_back(expr);
        }
        if (!expect(TokenType::SEMICOLON))
        {
          accept(TokenType::COMMA);
        }
      }
      accept(TokenType::SEMICOLON);

      return nextStmt;
    }

    auto switchStatement() -> ASTRef<SwitchStatement>
    {
      accept(TokenType::KEYWORD_SWITCH);
      accept(TokenType::LEFT_PAREN);
      auto scrutinee = expression();
      accept(TokenType::RIGHT_PAREN);
      accept(TokenType::LEFT_CURLY);

      auto switchStmt = createNode<SwitchStatement>();
      switchStmt->scrutinee = std::move(scrutinee);

      while (expect(TokenType::KEYWORD_CASE) || expect(TokenType::KEYWORD_OTHERWISE))
      {
        if (expect(TokenType::KEYWORD_OTHERWISE))
        {
          accept(TokenType::KEYWORD_OTHERWISE);
          CaseClause clause;
          clause.isOtherwise = true;
          clause.body = compoundStatement();
          switchStmt->cases.push_back(std::move(clause));
          // otherwise must be last
          break;
        }
        accept(TokenType::KEYWORD_CASE);
        CaseClause clause;
        if (expect(TokenType::ID))
        {
          clause.variantName = state->repr;
          accept(TokenType::ID);
        }
        // Parse optional bindings: case Ok(value) or case Ok(value, index)
        if (expect(TokenType::LEFT_PAREN))
        {
          accept(TokenType::LEFT_PAREN);
          while (!expect(TokenType::RIGHT_PAREN))
          {
            clause.bindings.push_back(state->repr);
            accept(TokenType::ID);
            if (expect(TokenType::COMMA)) accept(TokenType::COMMA);
          }
          accept(TokenType::RIGHT_PAREN);
        }
        clause.body = compoundStatement();
        switchStmt->cases.push_back(std::move(clause));
      }

      accept(TokenType::RIGHT_CURLY);
      return switchStmt;
    }

    auto compoundStatement() -> ASTRef<CompoundStatement>
    {
      accept(TokenType::LEFT_CURLY);
      auto stmt = createNode<CompoundStatement>();
      while (!expect(TokenType::RIGHT_CURLY))
      {
        stmt->statements.push_back(std::move(statement()));
      }
      accept(TokenType::RIGHT_CURLY);

      return stmt;
    }

    auto returnBy(TokenType type) -> ASTRef<ReturnStatement>
    {
      accept(type);
      if (expect(TokenType::SEMICOLON))
      {
        accept(TokenType::SEMICOLON);
        return createNode<ReturnStatement>();
      }
      auto ret = createNode<ReturnStatement>();
      ret->expression = std::move(expression());
      accept(TokenType::SEMICOLON);
      return ret;
    }

    auto returnStatement() -> ASTRef<ReturnStatement> { return returnBy(TokenType::KEYWORD_RETURN); }

    auto arrowReturn() -> ASTRef<ReturnStatement> { return returnBy(TokenType::DUAL_ARROW); }

    auto typeCheckExpr(ASTRef<Expression> expr) -> ASTRef<TypeCheckingExpression>
    {
      accept(TokenType::KEYWORD_IS);
      auto type = typeAnnotation();
      return createNode<TypeCheckingExpression>(expr, type);
    }

    auto expression(bool stopAtBind = false) -> ASTRef<Expression>
    {
      auto expr = std::move(primaryExpression());

      while (!expectExpressionTerminator(stopAtBind))
      {
        if (expect(TokenType::LEFT_PAREN))
        {
          expr = std::move(funCallExpression(std::move(expr)));
        }
        else if (expect(TokenType::LT) && dynamic_ast_cast<IdExpression>(expr) && genericExpressionArgsAhead())
        {
          auto genericTypeArgs = genericArgs();
          if (expect(TokenType::LEFT_PAREN))
          {
            expr = std::move(funCallExpression(std::move(expr), std::move(genericTypeArgs)));
          }
          else
          {
            expr = std::move(genericConstExpression(std::move(expr), std::move(genericTypeArgs)));
          }
        }
        else if (expect(TokenType::DOT))
        {
          expr = std::move(idAccessorExpression(std::move(expr)));
        }
        else if (expect(TokenType::SEPARATOR))
        {
          expr = std::move(staticQualifiedTraitCallExpression(std::move(expr)));
        }
        else if (expect(TokenType::KEYWORD_IS))
        {
          expr = std::move(typeCheckExpr(std::move(expr)));
        }
        else if (expect(TokenType::ASSIGN_EQUAL))
        {
          expr = std::move(assignmentExpression(std::move(expr)));
        }
        else if (is_operator(state->type))
        {
          expr = std::move(binaryExpression(std::move(expr)));
        }

        else if (expect(TokenType::LEFT_SQUARE))
        {
          expr = std::move(indexAccessorExpression(std::move(expr)));
        }
        else
        {
          unexpected("Unexpected token " + state->repr);
        }
      }

      return expr;
    }

    auto assignmentExpression(ASTRef<Expression> ref) -> ASTRef<AssignmentExpression>
    {
      auto idExpr = dynamic_ast_cast<IdExpression>(ref);
      auto idAccessor = dynamic_ast_cast<IdAccessorExpression>(ref);
      if (!idExpr && !idAccessor && !isDerefExpression(ref))
      {
        unexpected("Unexpected expression, expect identifier to assign");
      }

      if (!expect(TokenType::ASSIGN_EQUAL))
      {
        unexpected("Only assignment operator can assign");
      }

      accept(TokenType::ASSIGN_EQUAL);
      auto assignmentExpr = createNode<AssignmentExpression>(ref);
      assignmentExpr->value = std::move(expression());

      return assignmentExpr;
    }

    auto expectExpressionTerminator(bool stopAtBind = false) -> bool
    {
      return expect(TokenType::COLON) ||        // :
             (stopAtBind && expect(TokenType::BIND)) || // =
             expect(TokenType::COMMA) ||        // ,
             expect(TokenType::DUAL_ARROW) ||   // =>
             expect(TokenType::SINGLE_ARROW) || // ->
             expect(TokenType::RIGHT_PAREN) ||  // )
             expect(TokenType::RIGHT_CURLY) ||  // }
             expect(TokenType::RIGHT_SQUARE) || // ]
             expect(TokenType::SEMICOLON) ||    // ;
             expect(TokenType::LEFT_CURLY) ||   // {
             state.eof();
    }

    auto binaryExpression(ASTRef<Expression> expr) -> ASTRef<BinaryExpression>
    {
      auto &&token = state.current();
      if (state->type == TokenType::BIND)
      {
        unexpected("Invalid use of binding operator `=` in expression.");
      }
      accept(state->type);
      auto binexpr = createNode<BinaryExpression>();
      binexpr->optr = std::make_shared<Token>(token);
      binexpr->left = std::move(expr);
      binexpr->right = std::move(expression());
      return binexpr;
    }

    auto funCallExpression(ASTRef<Expression> primaryExpression,
                           Vec<std::shared_ptr<TypeAnnotation>> genericArgs = {}) -> ASTRef<FunCallExpression>
    {
      accept(TokenType::LEFT_PAREN);
      Vec<ASTRef<Expression>> args{};
      while (!expect(TokenType::RIGHT_PAREN))
      {
        args.push_back(std::move(expression()));
        if (!expect(TokenType::COMMA))
        {
          break;
        }
        accept(TokenType::COMMA);
      }
      accept(TokenType::RIGHT_PAREN);
      auto funcall = createNode<FunCallExpression>();
      funcall->primaryExpression = std::move(primaryExpression);
      funcall->genericArgs = std::move(genericArgs);
      funcall->arguments = std::move(args);
      return funcall;
    }

    auto idAccessorExpression(ASTRef<Expression> expr) -> ASTRef<Expression>
    {
      accept(TokenType::DOT);
      if (expect(TokenType::ID) && peekTokenType(1) == TokenType::SEPARATOR)
      {
        auto qualified = createNode<QualifiedTraitCallExpression>();
        qualified->receiver = std::move(expr);
        qualified->traitName = state->repr;
        accept(TokenType::ID);
        accept(TokenType::SEPARATOR);
        if (!expect(TokenType::ID))
        {
          unexpected("Expect method identifier after trait qualifier");
        }
        qualified->methodName = state->repr;
        accept(TokenType::ID);
        accept(TokenType::LEFT_PAREN);
        while (!expect(TokenType::RIGHT_PAREN))
        {
          qualified->arguments.push_back(std::move(expression()));
          if (!expect(TokenType::COMMA))
          {
            break;
          }
          accept(TokenType::COMMA);
        }
        accept(TokenType::RIGHT_PAREN);
        return qualified;
      }

      auto idacc = createNode<IdAccessorExpression>();
      idacc->primaryExpression = std::move(expr);

      if (expect(TokenType::ID))
      {
        idacc->accessor = std::move(idExpression());
      }
      else if (expect(TokenType::STRING))
      {
        idacc->accessor = createNode<IdExpression>(stringValue()->value);
      }
      else if (expect(TokenType::NUMBER))
      {
        idacc->accessor = createNode<IdExpression>(numberLiteral()->repr());
        return idacc;
      }
      else
      {
        unexpected("Expect identifier after '.'");
      }

      if (expect(TokenType::LEFT_PAREN))
      {
        accept(TokenType::LEFT_PAREN);

        Vec<ASTRef<Expression>> args{};

        while (!expect(TokenType::RIGHT_PAREN))
        {
          args.push_back(std::move(expression()));
          if (!expect(TokenType::COMMA))
          {
            break;
          }
          accept(TokenType::COMMA);
        }
        accept(TokenType::RIGHT_PAREN);

        idacc->arguments = std::move(args);
      }
      return idacc;
    }

    auto staticQualifiedTraitCallExpression(ASTRef<Expression> expr) -> ASTRef<QualifiedTraitCallExpression>
    {
      auto traitExpr = dynamic_ast_cast<IdExpression>(expr);
      if (!traitExpr)
      {
        unexpected("Trait-qualified call must start with a trait identifier");
      }
      auto qualified = createNode<QualifiedTraitCallExpression>();
      qualified->traitName = traitExpr->id;
      accept(TokenType::SEPARATOR);
      if (!expect(TokenType::ID))
      {
        unexpected("Expect method identifier after trait qualifier");
      }
      qualified->methodName = state->repr;
      accept(TokenType::ID);
      accept(TokenType::LEFT_PAREN);
      while (!expect(TokenType::RIGHT_PAREN))
      {
        qualified->arguments.push_back(std::move(expression()));
        if (!expect(TokenType::COMMA))
        {
          break;
        }
        accept(TokenType::COMMA);
      }
      accept(TokenType::RIGHT_PAREN);
      destroyast(expr);
      return qualified;
    }

    auto indexAccessorExpression(ASTRef<Expression> primary) -> ASTRef<Expression>
    {
      accept(TokenType::LEFT_SQUARE);

      auto accessor = expression();
      accept(TokenType::RIGHT_SQUARE);

      if (expect(TokenType::ASSIGN_EQUAL))
      {
        accept(TokenType::ASSIGN_EQUAL);
        auto value = expression();
        return createNode<IndexAssignmentExpression>(std::move(primary), std::move(accessor), std::move(value));
      }
      return createNode<IndexAccessorExpression>(std::move(primary), std::move(accessor));
    }

    auto newObjectExpression() -> ASTRef<NewObjectExpression>
    {
      ASTRef<NewObjectExpression> newObj = createNode<NewObjectExpression>();

      accept(TokenType::KEYWORD_NEW);

      auto targetType = typeAnnotation();
      newObj->typeName = targetType->name;
      newObj->targetType = targetType;

      accept(TokenType::LEFT_CURLY);

      while (!expect(TokenType::RIGHT_CURLY))
      {
        auto propertyName = idExpression();
        accept(TokenType::COLON);
        newObj->properties[propertyName->repr()] = std::move(expression());
        if (!expect(TokenType::COMMA))
        {
          break;
        }
        accept(TokenType::COMMA);
      }
      accept(TokenType::RIGHT_CURLY);

      return newObj;
    }

    auto primaryExpression() -> ASTRef<Expression>
    {
      if (expect(TokenType::LEFT_PAREN))
      {
        accept(TokenType::LEFT_PAREN);
        auto expr = expression();
        if (expect(TokenType::COMMA))
        {
          // Parse tuple literal
          Vec<ASTRef<Expression>> elements{};
          elements.push_back(std::move(expr));

          while (expect(TokenType::COMMA))
          {
            accept(TokenType::COMMA);
            if (expect(TokenType::RIGHT_PAREN))
            {
              break; // trailing comma
            }
            elements.push_back(std::move(expression()));
          }
          accept(TokenType::RIGHT_PAREN);
          return createNode<TupleLiteral>(std::move(elements));
        }
        accept(TokenType::RIGHT_PAREN);

        return expr;
      }
      if (expect(TokenType::ID))
      {
        return idExpression();
      }
      if (expect(TokenType::NUMBER) ||
          (code(state->type) >= code(TokenType::NUMBER) && code(state->type) <= code(TokenType::NUMBER_F128)))
      {
        return numberLiteral();
      }
      if (expect(TokenType::STRING))
      {
        return stringValue();
      }
      if (expect(TokenType::KEYWORD_TRUE))
      {
        accept(TokenType::KEYWORD_TRUE);

        return createNode<BooleanValue>(true);
      }
      if (expect(TokenType::KEYWORD_FALSE))
      {
        accept(TokenType::KEYWORD_FALSE);
        return createNode<BooleanValue>(false);
      }
      if (expect(TokenType::LEFT_SQUARE))
      {
        return arrayLiteral();
      }
      if (expect(TokenType::KEYWORD_NEW))
      {
        return newObjectExpression();
      }
      if (expect(TokenType::KEYWORD_UNIT))
      {
        accept(TokenType::KEYWORD_UNIT);
        return createNode<UnitLiteral>();
      }
      if (expect(TokenType::KEYWORD_CAST))
      {
        accept(TokenType::KEYWORD_CAST);
        accept(TokenType::LT);
        auto targetType = typeAnnotation();
        accept(TokenType::GT);
        accept(TokenType::LEFT_PAREN);
        auto expr = expression();
        accept(TokenType::RIGHT_PAREN);
        return createNode<CastExpression>(std::move(expr), std::move(targetType));
      }
      if (expect(TokenType::KEYWORD_TYPEOF))
      {
        accept(TokenType::KEYWORD_TYPEOF);
        accept(TokenType::LEFT_PAREN);
        auto expr = expression();
        accept(TokenType::RIGHT_PAREN);
        return createNode<TypeOfExpression>(std::move(expr));
      }
      if (expect(TokenType::SPREAD))
      {
        accept(TokenType::SPREAD);
        auto expr = expression();
        return createNode<SpreadExpression>(std::move(expr));
      }
      if (expect(TokenType::KEYWORD_REF) || expect(TokenType::KEYWORD_MOVE))
      {
        return unaryExpression();
      }
      if (is_operator(state->type))
      {
        if (isUnaryOperator(state->type))
        {
          return unaryExpression();
        }
        else
        {
          unexpected("Unexpected operator as unary operator");
        }
      }
      unexpected("Unexpected primary expression: " + state->repr);
    }

    auto unaryExpression() -> ASTRef<UnaryExpression>
    {
      auto optrToken = state.current();

      switch (optrToken.type)
      {
      case TokenType::NOT:
        [[fallthrough]];
      case TokenType::MINUS:
        [[fallthrough]];
      case TokenType::QUERY:
        [[fallthrough]];
      case TokenType::AMPERSAND:
        [[fallthrough]];
      case TokenType::TIMES:
        [[fallthrough]];
      case TokenType::KEYWORD_REF:
        [[fallthrough]];
      case TokenType::KEYWORD_MOVE:
      {

        accept(optrToken.type);
        auto expr = createNode<UnaryExpression>();
        expr->optr = std::make_shared<Token>(optrToken);
        expr->operand = postfixExpression(std::move(primaryExpression()));
        return expr;
      }
      default:
        unexpected("Invalid unary operator.");
      }
    }

    auto stringValue() -> ASTRef<StringValue>
    {
      const auto &str = state->repr;
      accept(TokenType::STRING);
      return createNode<StringValue>(str);
    }

    auto numberLiteral() -> ASTRef<Expression>
    {
      auto integer = state->repr;
      switch (state->type)
      {
      case TokenType::NUMBER:
        accept(state->type);
        return createNode<IntegralValue<int32_t>>(std::stoi(integer));
      case TokenType::INTEGRAL:
        accept(state->type);
        return createNode<IntegralValue<int32_t>>(static_cast<int32_t>(std::stoi(integer)));
      case TokenType::NUMBER_I8:
        accept(state->type);
        return createNode<IntegralValue<int8_t>>(static_cast<int8_t>(std::stoi(integer)));
      case TokenType::NUMBER_U8:
        accept(state->type);
        return createNode<IntegralValue<uint8_t>>(static_cast<uint8_t>(std::stoi(integer)));
      case TokenType::NUMBER_I16:
        accept(state->type);
        return createNode<IntegralValue<int16_t>>(static_cast<int16_t>(std::stoi(integer)));
      case TokenType::NUMBER_U16:
        accept(state->type);
        return createNode<IntegralValue<uint16_t>>(static_cast<uint16_t>(std::stoi(integer)));
      case TokenType::NUMBER_I32:
        accept(state->type);
        return createNode<IntegralValue<int32_t>>(static_cast<int32_t>(std::stoi(integer)));
      case TokenType::NUMBER_U32:
        accept(state->type);
        return createNode<IntegralValue<uint32_t>>(static_cast<uint32_t>(std::stoul(integer)));
      case TokenType::NUMBER_I64:
        accept(state->type);
        return createNode<IntegralValue<int64_t>>(static_cast<int64_t>(std::stoll(integer)));
      case TokenType::NUMBER_U64:
        accept(state->type);
        return createNode<IntegralValue<uint64_t>>(static_cast<uint64_t>(std::stoull(integer)));
      case TokenType::NUMBER_I128:
        accept(state->type);
        // todo: support i128 parsing properly
        return createNode<IntegralValue<int64_t>>(static_cast<int64_t>(std::stoll(integer)));
      case TokenType::NUMBER_U128:
        accept(state->type);
        // todo: support u128 parsing properly
        return createNode<IntegralValue<uint64_t>>(static_cast<uint64_t>(std::stoull(integer)));
      case TokenType::FLOATING_POINT:
        accept(state->type);
        return createNode<FloatingPointValue<float>>(std::stof(integer));
      case TokenType::NUMBER_F16:
        unexpected("Float16 not supported");
      case TokenType::NUMBER_F32:
        accept(state->type);
        return createNode<FloatingPointValue<float>>(std::stof(integer));
      case TokenType::NUMBER_F64:
        accept(state->type);
        return createNode<FloatingPointValue<double>>(std::stod(integer));
      case TokenType::NUMBER_F128:
        accept(state->type);
        return createNode<FloatingPointValue<double>>(std::stod(integer));
      case TokenType::NUMBER_F256:
        unexpected("Float256 not supported");
      default:
        unexpected("Invalid number literal");
      }
    }

    auto idExpression() -> ASTRef<IdExpression>
    {
      auto identifier = state->repr;
      accept(TokenType::ID);
      return createNode<IdExpression>(identifier);
    }

    auto arrayLiteral() -> ASTRef<Expression>
    {
      accept(TokenType::LEFT_SQUARE);

      Vec<ASTRef<Expression>> elements{};

      while (!expect(TokenType::RIGHT_SQUARE))
      {
        elements.push_back(std::move(expression()));
        if (!expect(TokenType::COMMA))
        {
          break;
        }
        accept(TokenType::COMMA);
      }
      accept(TokenType::RIGHT_SQUARE);

      return createNode<ArrayLiteral>(std::move(elements));
    }
  };

  auto Parser::parse(const Str &fileName) -> ASTRef<ASTNode>
  {
    return ParserImpl(state).parse(fileName);
  }

} // namespace NG::parsing
