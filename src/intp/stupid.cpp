
#include <algorithm>
#include <ast.hpp>
#include <intp/intp.hpp>
#include <intp/runtime.hpp>
#include <intp/runtime_numerals.hpp>
#include <module.hpp>
#include <token.hpp>
#include <visitor.hpp>

#include <functional>
#include <iterator>

using namespace NG;
using namespace NG::ast;

namespace NG::runtime
{
  auto get_native_registry() -> Map<Str, Map<Str, NGInvocable>> &
  {
    static Map<Str, Map<Str, NGInvocable>> natives;
    return natives;
  }

  void register_native_library(Str moduleId, Map<Str, NGInvocable> handlers)
  {
    get_native_registry().insert_or_assign(moduleId, handlers);
  }
} // namespace NG::runtime

namespace NG::intp
{

  using namespace NG::runtime;
  using NG::module::FileBasedExternalModuleLoader;
  using NG::module::get_module_registry;
  using NG::module::standard_library_base_path;

  void ISummarizable::summary() {}

  ISummarizable::~ISummarizable() = default;

  static auto evaluateExpr(TokenType optr, const RuntimeRef<NGObject> &leftParam,
                           const RuntimeRef<NGObject> &rightParam) -> RuntimeRef<NGObject>
  {
    switch (optr)
    {
    case TokenType::PLUS:
      return leftParam->opPlus(rightParam);
    case TokenType::MINUS:
      return leftParam->opMinus(rightParam);
    case TokenType::TIMES:
      return leftParam->opTimes(rightParam);
    case TokenType::DIVIDE:
      return leftParam->opDividedBy(rightParam);
    case TokenType::MODULUS:
      return leftParam->opModulus(rightParam);
    case TokenType::EQUAL:
      return NGObject::boolean(leftParam->opEquals(rightParam));
    case TokenType::NOT_EQUAL:
      return NGObject::boolean(!leftParam->opEquals(rightParam));
    case TokenType::LE:
      return NGObject::boolean(leftParam->opLessEqual(rightParam));
    case TokenType::LT:
      return NGObject::boolean(leftParam->opLessThan(rightParam));
    case TokenType::GE:
      return NGObject::boolean(leftParam->opGreaterEqual(rightParam));
    case TokenType::GT:
      return NGObject::boolean(leftParam->opGreaterThan(rightParam));
    case TokenType::RSHIFT:
      return leftParam->opRShift(rightParam);
    case TokenType::LSHIFT:
      return leftParam->opLShift(rightParam);
    //            case TokenType::ASSIGN:
    case TokenType::BIND:
      throw RuntimeException("Operator = is not supported in expressions, perhaps you mean ':='?");
    default:
      throw RuntimeException("Unsupported binary operator");
      break;
    }
    return nullptr;
  }

  struct FunctionPathVisitor : public DummyVisitor
  {

    Str path;

    void visit(IdExpression *idExpr) override { this->path = idExpr->id; }
  };

  struct ExpressionVisitor : public DummyVisitor
  {

    RuntimeRef<NGObject> object = nullptr;

    RuntimeRef<Vec<RuntimeRef<NGObject>>> collection = nullptr;

    RuntimeRef<NGContext> context = nullptr;

    explicit ExpressionVisitor(RuntimeRef<NGContext> context) : context(context) {}

#pragma region Visit numeral literals

    void visit(IntegralValue<int8_t> *intVal) override { object = makert<NGIntegral<int8_t>>(intVal->value); }
    void visit(IntegralValue<uint8_t> *intVal) override
    {
      object = std::make_shared<NGIntegral<uint8_t>>(intVal->value);
    }
    void visit(IntegralValue<int16_t> *intVal) override
    {
      object = std::make_shared<NGIntegral<int16_t>>(intVal->value);
    }
    void visit(IntegralValue<uint16_t> *intVal) override
    {
      object = std::make_shared<NGIntegral<uint16_t>>(intVal->value);
    }
    void visit(IntegralValue<int32_t> *intVal) override
    {
      object = std::make_shared<NGIntegral<int32_t>>(intVal->value);
    }
    void visit(IntegralValue<uint32_t> *intVal) override
    {
      object = std::make_shared<NGIntegral<uint32_t>>(intVal->value);
    }
    void visit(IntegralValue<int64_t> *intVal) override
    {
      object = std::make_shared<NGIntegral<int64_t>>(intVal->value);
    }
    void visit(IntegralValue<uint64_t> *intVal) override
    {
      object = std::make_shared<NGIntegral<uint64_t>>(intVal->value);
    }

    // void visit(FloatingPointValue<float16_t> *floatVal) override
    // {
    //     object = std::make_shared<FloatingPointValue<float16_t>>(floatVal->value);
    // }

    void visit(FloatingPointValue<float /*float32_t*/> *floatVal) override
    {
      object = std::make_shared<NGFloatingPoint<float /* float32_t */>>(floatVal->value);
    }

    void visit(FloatingPointValue<double /*float64_t*/> *floatVal) override
    {
      object = std::make_shared<NGFloatingPoint<double /* float64_t */>>(floatVal->value);
    }

    // void visit(FloatingPointValue<float128_t> *floatVal) override
    // {
    //     object = std::make_shared<FloatingPointValue<float128_t>>(floatVal->value);
    // }

#pragma endregion

    void visit(StringValue *strVal) override { object = makert<NGString>(strVal->value); }

    void visit(BooleanValue *boolVal) override { object = makert<NGBoolean>(boolVal->value); }

    void visit(TupleLiteral *tuple) override
    {
      Vec<RuntimeRef<NGObject>> objects;

      ExpressionVisitor vis{context};

      for (const auto &element : tuple->elements)
      {
        element->accept(&vis);
        if (auto spread = dynamic_ast_cast<SpreadExpression>(element); spread)
        {
          auto collection = vis.collection;
          for (auto &&item : *collection)
          {
            objects.push_back(item);
          }
        }
        else
        {
          objects.push_back(vis.object);
        }
      }

      object = makert<NGTuple>(objects);
    }

    void visit(FunCallExpression *funCallExpr) override
    {
      FunctionPathVisitor fpVis{};
      funCallExpr->primaryExpression->accept(&fpVis);
      NGInvCtx invocationContext = makert<NGInvocationContext>();

      for (auto &param : funCallExpr->arguments)
      {
        ExpressionVisitor vis{context};
        param->accept(&vis);
        if (auto spread = dynamic_ast_cast<SpreadExpression>(param))
        {
          auto collection = vis.collection;
          for (auto &&item : *collection)
          {
            invocationContext->params.push_back(item);
          }
        }
        else
        {
          invocationContext->params.push_back(vis.object);
        }
      }

      RuntimeRef<NGObject> dummy = makert<NGObject>();
      invocationContext->target = dummy;

      if (!context->has_function(fpVis.path, true))
      {
        throw RuntimeException("No such function: " + fpVis.path, funCallExpr->pos);
      }

      context->get_function(fpVis.path)(dummy, context, invocationContext);

      this->object = context->retVal;
      context->retVal = nullptr;
    }

    void visit(UnaryExpression *unoExpr) override
    {
      ExpressionVisitor operandVisitor{context};
      unoExpr->operand->accept(&operandVisitor);
      auto result = operandVisitor.object;

      switch (unoExpr->optr->type)
      {
      case TokenType::MINUS:
      {
        auto numeric = dynamic_cast<NumeralBase *>(result.get());
        if (numeric)
        {
          this->object = numeric->opNegate();
          return;
        }
        throw RuntimeException("Cannot negate a non-number");
      }
      case TokenType::NOT:
        this->object = NGObject::boolean(!result->boolValue());
        return;
      case TokenType::QUERY:
        throw NotImplementedException("Operator QUERY (?) not implemented yet");
      default:
        throw RuntimeException("Unsupported Operator");
      }
    }

    void visit(BinaryExpression *binExpr) override
    {
      ExpressionVisitor leftVisitor{context};
      ExpressionVisitor rightVisitor{context};
      binExpr->left->accept(&leftVisitor);
      binExpr->right->accept(&rightVisitor);

      object = evaluateExpr(binExpr->optr->type, leftVisitor.object, rightVisitor.object);
    }

    void visit(IdExpression *idExpr) override { object = context->get(idExpr->id); }

    void visit(ArrayLiteral *array) override
    {
      Vec<RuntimeRef<NGObject>> objects;

      ExpressionVisitor vis{context};

      for (const auto &element : array->elements)
      {
        element->accept(&vis);
        if (auto spread = dynamic_ast_cast<SpreadExpression>(element); spread)
        {
          auto collection = vis.collection;
          for (auto &&item : *collection)
          {
            objects.push_back(item);
          }
        }
        else
        {
          objects.push_back(vis.object);
        }
      }

      object = makert<NGArray>(objects);
    }

    void visit(IndexAccessorExpression *index) override
    {
      ExpressionVisitor vis{context};

      index->primary->accept(&vis);

      RuntimeRef<NGObject> primaryObject = vis.object;

      index->accessor->accept(&vis);

      RuntimeRef<NGObject> accessorObject = vis.object;

      object = primaryObject->opIndex(accessorObject);
    }

    void visit(IndexAssignmentExpression *index) override
    {
      ExpressionVisitor vis{context};

      index->primary->accept(&vis);

      RuntimeRef<NGObject> primaryObject = vis.object;

      index->accessor->accept(&vis);

      RuntimeRef<NGObject> accessorObject = vis.object;

      index->value->accept(&vis);

      RuntimeRef<NGObject> valueObject = vis.object;

      object = primaryObject->opIndex(accessorObject, valueObject);
    }

    void visit(IdAccessorExpression *idAccExpr) override
    {
      const Str &repr = idAccExpr->accessor->repr();

      ExpressionVisitor vis{context};

      idAccExpr->primaryExpression->accept(&vis);

      RuntimeRef<NGObject> main = vis.object;

      auto invCtx = makert<NGInvocationContext>();
      invCtx->target = main;
      for (const auto &argument : idAccExpr->arguments)
      {
        argument->accept(&vis);
        invCtx->params.push_back(vis.object);
      }

      object = main->respond(repr, context, invCtx);
    }

    void visit(NewObjectExpression *newObj) override
    {
      Str &typeName = newObj->typeName;
      auto ngType = context->get_type(typeName);

      auto structural = makert<NGStructuralObject>();

      structural->customizedType = ngType;

      RuntimeRef<NGContext> newContext = context->fork();
      ExpressionVisitor visitor{newContext};

      for (auto &&[name, expr] : newObj->properties)
      {
        expr->accept(&visitor);
        RuntimeRef<NGObject> result = visitor.object;

        visitor.context->define(name, result);

        structural->properties[name] = result;
      }

      for (const auto &property : ngType->properties)
      {
        if (!structural->properties.contains(property))
        {
          structural->properties[property] = makert<NGObject>();
        }
      }

      object = structural;
    }

    void visit(AssignmentExpression *assignmentExpr) override
    {
      ExpressionVisitor vis{context};

      assignmentExpr->value->accept(&vis);
      auto result = vis.object;
      if (auto idexpr = dynamic_ast_cast<IdExpression>(assignmentExpr->target); idexpr)
      {
        context->set(idexpr->id, result);
        object = result;
      }
      else if (auto idAcc = dynamic_ast_cast<IdAccessorExpression>(assignmentExpr->target); idAcc)
      {
        idAcc->primaryExpression->accept(&vis);
        if (auto obj = std::dynamic_pointer_cast<NGStructuralObject>(vis.object); obj)
        {
          obj->properties[idAcc->accessor->repr()] = result;
          object = result;
          return;
        }
        if (auto tup = std::dynamic_pointer_cast<NGTuple>(vis.object); tup)
        {
          tup->opIndex(makert<NGIntegral<int>>(std::stoi(idAcc->accessor->repr())), result);
          object = result;
          return;
        }
        throw RuntimeException("Left-hand side of member assignment is not assignable: " +
                               idAcc->primaryExpression->repr());
      }
      else
      {
        throw RuntimeException("Invalid assignment: " + assignmentExpr->repr());
      }
    }

    void visit(TypeCheckingExpression *typeCheckExpr) override
    {
      ExpressionVisitor vis{context};
      typeCheckExpr->value->accept(&vis);
      RuntimeRef<NGObject> value = vis.object;

      if (auto anno = dynamic_ast_cast<TypeAnnotation>(typeCheckExpr->type); anno)
      {
        auto name = anno->name;
        auto targetType = context->get_type(name);
        if (targetType)
        {
          this->object = makert<NGBoolean>(*(value->type()) == *(targetType));
        }
        else
        {
          // todo: simply fix this, and will migrate to typechecker later.
          this->object = makert<NGBoolean>((value->type()->name == anno->name));
        }
      }
      else if (auto idAcccessor = dynamic_ast_cast<IdAccessorExpression>(typeCheckExpr->type); idAcccessor)
      {
        idAcccessor->primaryExpression->accept(&vis);
        auto name = idAcccessor->accessor->repr();
        auto result = vis.object;
        auto mod = std::dynamic_pointer_cast<NGModule>(result);
        if (!mod)
        {
          throw RuntimeException("Invalid module to locate type: " + name);
        }
        auto typeIt = mod->types.find(name);
        if (typeIt == mod->types.end() || !typeIt->second)
        {
          throw RuntimeException("Invalid type name, cannot find: " + name);
        }
        auto targetType = typeIt->second;
        this->object = makert<NGBoolean>(*(value->type()) == *(targetType));
      }
      else
      {
        throw RuntimeException("Invalid target expression for type checking: " + typeCheckExpr->type->repr());
      }
    }
    void visit(SpreadExpression *spreadExpression) override
    {
      ExpressionVisitor vis{context};

      spreadExpression->expression->accept(&vis);
      auto result = vis.object;

      if (auto tup = std::dynamic_pointer_cast<NGTuple>(result); tup)
      {
        collection = tup->items;
        object = result;
        return;
      }
      if (auto arr = std::dynamic_pointer_cast<NGArray>(result); arr)
      {
        collection = arr->items;
        object = result;
        return;
      }
      throw RuntimeException("Invalid spread expression, expect array or tuple, but got: " +
                             spreadExpression->expression->repr());
    }

    void visit(UnitLiteral *unit) override { object = makert<NGUnit>(); }

    void visit(CastExpression *castExpr) override
    {
      ExpressionVisitor exprVis{context};
      castExpr->expression->accept(&exprVis);
      RuntimeRef<NGObject> value = exprVis.object;

      // Resolve target type name
      Str targetTypeName;
      if (auto anno = dynamic_ast_cast<TypeAnnotation>(castExpr->targetType))
      {
        targetTypeName = anno->name;
      }
      else
      {
        throw RuntimeException("Invalid cast target type: " + castExpr->targetType->repr());
      }

      // Check if target is a newtype
      if (context->has_type(targetTypeName, true))
      {
        auto targetType = context->get_type(targetTypeName);
        // If the value is already an NGNewType with the same type, unwrap it
        if (auto newTypeVal = std::dynamic_pointer_cast<NGNewType>(value); newTypeVal)
        {
          if (newTypeVal->newType->name == targetTypeName)
          {
            // Same newtype — no-op
            object = value;
            return;
          }
          // Unwrap then potentially rewrap
          value = newTypeVal->wrapped;
        }

        // Wrap into newtype
        auto newType = makert<NGType>();
        newType->name = targetTypeName;
        object = makert<NGNewType>(newType, value);
      }
      else
      {
        // For primitive casts or unknown types, just pass through
        object = value;
      }
    }
  };

  struct StatementVisitor : public DummyVisitor
  {
    RuntimeRef<NGContext> context;

    explicit StatementVisitor(RuntimeRef<NGContext> context) : context(context) {}

    void visit(ReturnStatement *returnStatement) override
    {
      if (returnStatement->expression)
      {
        ExpressionVisitor vis{context};
        returnStatement->expression->accept(&vis);
        context->retVal = vis.object;
      }
      else
      {
        // todo: implement Unit type
        context->retVal = makert<NGUnit>(); // or an equivalent Unit singleton/null per your design
      }
    }

    void visit(IfStatement *ifStmt) override
    {
      ExpressionVisitor vis{context};
      ifStmt->testing->accept(&vis);

      StatementVisitor stmtVis{context};
      if (vis.object->boolValue())
      {
        ifStmt->consequence->accept(&stmtVis);
      }
      else if (ifStmt->alternative != nullptr)
      {
        ifStmt->alternative->accept(&stmtVis);
      }
    }

    void visit(CompoundStatement *stmt) override
    {
      auto newContext = context->fork();
      StatementVisitor vis{newContext};
      for (const auto &innerStmt : stmt->statements)
      {
        innerStmt->accept(&vis);
        if (vis.context->retVal != nullptr)
        {
          context->retVal = vis.context->retVal;
          break;
        }
      }
    }

    void visit(ValDefStatement *valDef) override
    {
      ExpressionVisitor vis{context};
      valDef->value->accept(&vis);

      context->define(valDef->name, vis.object);
    }

    void visit(ValueBindingStatement *valBind) override
    {
      ExpressionVisitor vis{context};
      valBind->value->accept(&vis);
      auto result = vis.object;

      switch (valBind->type)
      {
      // case BindingType::DIRECT:
      // {
      //     if (valBind->bindings.size() != 1)
      //     {
      //         throw RuntimeException("Invalid binding type: direct binding only allow exactly 1.");
      //     }
      //     auto binding = valBind->bindings[0];
      //     if (binding->name.empty())
      //     {
      //         throw RuntimeException("Direct binding requires a name");
      //     }
      //     context->define(binding->name, result);
      // }
      // break;
      case BindingType::TUPLE_UNPACK:
      {
        auto tuple = std::dynamic_pointer_cast<NGTuple>(result);
        if (!tuple)
        {
          throw RuntimeException("Tuple unpacking requires a tuple value");
        }
        auto items = tuple->items;
        for (auto &&binding : valBind->bindings)
        {
          if (!binding->spreadReceiver)
          {
            context->define(binding->name, items->at(binding->index));
          }
          else if (!binding->name.empty()) // empty spread receiver just ignores everything
          {
            Vec<RuntimeRef<NGObject>> values{items->begin() + binding->index, items->end()};
            context->define(binding->name, makert<NGTuple>(values));
          }
        }
      }
      break;

      case BindingType::ARRAY_UNPACK:
      {
        auto array = std::dynamic_pointer_cast<NGArray>(result);
        if (!array)
        {
          throw RuntimeException("Array unpacking requires an array value");
        }
        auto items = array->items;
        for (auto &&binding : valBind->bindings)
        {
          if (!binding->spreadReceiver)
          {
            context->define(binding->name, items->at(binding->index));
          }
          else if (!binding->name.empty()) // empty spread receiver just ignores everything
          {
            Vec<RuntimeRef<NGObject>> values{items->begin() + binding->index, items->end()};
            context->define(binding->name, makert<NGArray>(values));
          }
        }
      }
      break;
      default:
        throw RuntimeException("Invalid binding type: unsupported");
        break;
      }
    }

    void visit(SimpleStatement *simpleStmt) override
    {
      ExpressionVisitor vis{context};
      simpleStmt->expression->accept(&vis);
    }

    void visit(SwitchStatement *switchStmt) override
    {
      ExpressionVisitor vis{context};
      switchStmt->scrutinee->accept(&vis);
      auto scrutinee = vis.object;

      auto *tagged = dynamic_cast<NGTaggedValue *>(scrutinee.get());
      if (!tagged)
      {
        throw RuntimeException("Switch scrutinee is not a tagged value");
      }

      const CaseClause *otherwise = nullptr;
      for (auto &c : switchStmt->cases)
      {
        if (c.isOtherwise)
        {
          otherwise = &c;
          continue;
        }
        if (c.variantName == tagged->variantName)
        {
          auto caseContext = context->fork();
          // Bind payload variables
          for (size_t j = 0; j < c.bindings.size() && j < tagged->payload.size(); ++j)
          {
            if (!c.bindings[j].empty())
            {
              caseContext->define(c.bindings[j], tagged->payload[j]);
            }
          }
          StatementVisitor caseVis{caseContext};
          c.body->accept(&caseVis);
          if (caseContext->retVal != nullptr)
          {
            context->retVal = caseContext->retVal;
          }
          return;
        }
      }

      if (otherwise != nullptr)
      {
        auto caseContext = context->fork();
        StatementVisitor caseVis{caseContext};
        otherwise->body->accept(&caseVis);
        if (caseContext->retVal != nullptr)
        {
          context->retVal = caseContext->retVal;
        }
        return;
      }

      throw RuntimeException("No matching case for variant: " + tagged->variantName);
    }

    void visit(LoopStatement *loopStatement) override
    {
      auto context = this->context->fork();
      ExpressionVisitor vis{context};
      for (auto &&binding : loopStatement->bindings)
      {
        binding.target->accept(&vis);
        switch (binding.type)
        {
        case LoopBindingType::LOOP_ASSIGN:
          context->define(binding.name, vis.object);
          break;
        default:
          throw RuntimeException("Unsupported loop binding");
        }
      }

      StatementVisitor stmtVis{context};
      bool stopLoop = false;
      while (!stopLoop)
      {
        try
        {
          loopStatement->loopBody->accept(&stmtVis);
          if (vis.context->retVal != nullptr)
          {
            this->context->retVal = vis.context->retVal;
          }
          stopLoop = true;
        }
        catch (NextIteration iter)
        {
          int i = 0;
          for (auto &&object : iter.slotValues)
          {
            context->set(loopStatement->bindings[i].name, object);
            i++;
          }
        }
      }
    }

    void visit(NextStatement *nextStatement) override
    {
      ExpressionVisitor vis{context};
      try
      {
        Vec<RuntimeRef<NGObject>> slotValues{};
        for (auto &&expr : nextStatement->expressions)
        {
          expr->accept(&vis);
          if (auto spread = dynamic_ast_cast<SpreadExpression>(expr); spread)
          {
            auto collection = vis.collection;
            for (auto &&item : *collection)
            {
              slotValues.push_back(item);
            }
          }
          else
          {
            slotValues.push_back(vis.object);
          }
        }
        throw NextIteration{slotValues};
      }
      catch (StopIteration)
      {
        // do nothing
      }
    }
  };

  struct Stupid : public Interpreter,
                  DummyVisitor // NOLINT(cppcoreguidelines-special-member-functions)
  {
    RuntimeRef<NGContext> context;

    Vec<Str> modulePaths;

    explicit Stupid(Vec<Str> modulePaths, bool loadingPrelude = false)
        : context(makert<NGContext>()), modulePaths(modulePaths)
    {
      if (!loadingPrelude)
      {
        loadPrelude();
      }
    }

    RuntimeRef<NGModule> asModule() { return makert<NGModule>(context); }

    void loadPrelude()
    {
      if (context->has_module("prelude"))
      {
        return;
      }
      if (auto target = get_module_registry().queryModuleById("std.prelude"); target)
      {
        auto targetModule = target->runtimeModule;
        context->define_module(target->moduleName, target->runtimeModule);
        importInto(context, {"*"}, targetModule);
      }
      else
      {
        // do actual import
        auto importPrelude = makeast<ImportDecl>();
        importPrelude->module = "prelude";
        importPrelude->modulePath.push_back("std");
        importPrelude->modulePath.push_back("prelude");
        importPrelude->imports.push_back("*");
        importPrelude->accept(this);
      }
      // load std.prelude by default.
    }

    void appendModulePath(Str path)
    {
      if (std::ranges::find(modulePaths, path) == std::end(modulePaths))
      {
        modulePaths.push_back(path);
      }
    }
    void visit(CompileUnit *compileUnit) override
    {
      if (!compileUnit->path.empty())
      {
        appendModulePath(compileUnit->path);
      }
      compileUnit->module->accept(this);
    }

    void visit(Module *mod) override
    {
      for (auto &&import : mod->imports)
      {
        import->accept(this);
      }

      Set<Str> definedSymbols = {};
      for (auto &&defs : mod->definitions)
      {
        for (auto &&name : defs->names())
        {
          definedSymbols.insert(name);
        }
        defs->accept(this);
      }

      for (auto &&exp : mod->exports)
      {
        if (!definedSymbols.contains(exp) && exp != "*")
        {
          throw RuntimeException("Export undefined symbol: " + exp);
        }
      }
      context->exports = mod->exports;
      StatementVisitor vis{context};

      for (const auto &stmt : mod->statements)
      {
        stmt->accept(&vis);
      }
    }

    void visit(ImportDecl *importDecl) override
    {
      if (!context->has_module(importDecl->module))
      {
        // load module
        NG::module::FileBasedExternalModuleLoader loader{this->modulePaths};
        auto &&moduleInfo = loader.load(importDecl->modulePath);
        if (auto target = get_module_registry().queryModuleById(moduleInfo->moduleId); target)
        {
          context->define_module(importDecl->module, target->runtimeModule);
        }
        else
        {
          auto &&ast = moduleInfo->moduleAst;
          bool loadingPrelude = moduleInfo->moduleId == "std.prelude";
          Stupid stupid{modulePaths, loadingPrelude};
          ast->accept(&stupid);
          auto runtimeModule = stupid.asModule();
          if (get_native_registry().contains(moduleInfo->moduleId))
          {
            auto &n = get_native_registry()[moduleInfo->moduleId];
            runtimeModule->native_functions.insert(n.begin(), n.end());
          }
          moduleInfo->runtimeModule = runtimeModule;
          get_module_registry().addModuleInfo(moduleInfo);
          context->define_module(importDecl->module, moduleInfo->runtimeModule);
        }
      }
      RuntimeRef<NGModule> targetModule = context->get_module(importDecl->module);

      importInto(context, importDecl->imports, targetModule);

      if (!importDecl->alias.empty())
      {
        context->define(importDecl->alias, targetModule);
      }
    }

    static void importInto(RuntimeRef<NGContext> context, Vec<Str> declaredImports,
                           const RuntimeRef<NGModule> &fromModule)
    {
      Set<Str> imports = resolveImports(declaredImports, fromModule);
      std::copy(imports.begin(), imports.end(), std::back_inserter(context->imported));

      for (auto &&imp : imports)
      {
        if (fromModule->functions.contains(imp))
        {
          context->define_function(imp, fromModule->functions[imp]);
        }
        if (fromModule->types.contains(imp))
        {
          context->define_type(imp, fromModule->types[imp]);
        }
        if (fromModule->objects.contains(imp))
        {
          context->define(imp, fromModule->objects[imp]);
        }
        if (fromModule->native_functions.contains(imp))
        {
          context->define_function(imp, fromModule->native_functions[imp]);
        }
      }
    }

    static auto resolveImports(const Vec<Str> &imports, const RuntimeRef<NGModule> &targetModule) -> Set<Str>
    {
      bool importAll = (std::ranges::find(imports, "*") != end(imports));

      bool exportsAll =
          (std::find(begin(targetModule->exports), end(targetModule->exports), "*") != end(targetModule->exports));
      Set<Str> exported{};
      if (exportsAll)
      {
        for (auto &&[fnName, _ignored] : targetModule->functions)
        {
          if (!targetModule->imports.contains(fnName) || targetModule->exports.contains(fnName))
          {
            exported.insert(fnName);
          }
        }
        for (auto &&[typeName, _ignored] : targetModule->types)
        {
          if (!targetModule->imports.contains(typeName) || targetModule->exports.contains(typeName))
          {
            exported.insert(typeName);
          }
        }
        for (auto &&[objName, _ignored] : targetModule->objects)
        {
          if (!targetModule->imports.contains(objName) || targetModule->exports.contains(objName))
          {
            exported.insert(objName);
          }
        }
        for (auto &&[fnName, _ignored] : targetModule->native_functions)
        {
          if (!targetModule->imports.contains(fnName) || targetModule->exports.contains(fnName))
          {
            exported.insert(fnName);
          }
        }
      }
      else
      {
        exported.insert(begin(targetModule->exports), end(targetModule->exports));
      }

      if (importAll)
      {
        return exported;
      }

      for (auto &&imp : imports)
      {
        if (!exported.contains(imp))
        {
          throw RuntimeException("Cannot found symbol " + imp + " in module");
        }
      }
      return Set<Str>{begin(imports), end(imports)};
    }

    // virtual void visit(Definition *def);
    // virtual void visit(Param *param);
    void visit(FunctionDef *funDef) override
    {
      if (funDef->native)
      {
        return;
      }

      auto functionInvoker =
          [this, funDef](const NGSelf &dummy, const NGCtx &ngContext, const NGInvCtx &invocationContext)
      {
        // Determine if there's a pack parameter and at which position
        int packIndex = -1;
        for (size_t g = 0; g < funDef->genericParams.size(); ++g)
        {
          if (funDef->genericParams[g]->isPack)
          {
            packIndex = static_cast<int>(g);
            break;
          }
        }
        RuntimeRef<NGContext> newContext = ngContext->fork();
        for (size_t i = 0; i < funDef->params.size(); ++i)
        {
          if (static_cast<int>(i) == packIndex)
          {
            // Pack remaining args into a tuple
            Vec<RuntimeRef<NGObject>> packItems;
            for (size_t j = i; j < invocationContext->params.size(); ++j)
            {
              packItems.push_back(invocationContext->params[j]);
            }
            newContext->define(funDef->params[i]->paramName, makert<NGTuple>(packItems));
            break; // pack parameter is always the last one
          }
          else if (invocationContext->params.size() > i)
          {
            newContext->define(funDef->params[i]->paramName, invocationContext->params[i]);
          }
          else if (funDef->params[i]->value != nullptr)
          {
            ExpressionVisitor vis{ngContext};
            funDef->params[i]->value->accept(&vis);
            auto value = vis.object;
            newContext->define(funDef->params[i]->paramName, value);
          }
          else
          {
            throw RuntimeException("Missing argument for parameter '" + funDef->params[i]->paramName +
                                   "' in function '" + funDef->funName + "'");
          }
        }
        bool tailRecur = true;
        while (tailRecur)
        {
          try
          {
            StatementVisitor vis{newContext};
            funDef->body->accept(&vis);
            tailRecur = false;
          }
          catch (NextIteration nextIter)
          {
            tailRecur = true;
            if (packIndex >= 0)
            {
              // For pack parameters: the next values need to be rebound correctly.
              // Non-pack params take individual values from the front;
              // the pack parameter gets the remaining values packed into a tuple.
              for (size_t i = 0; i < funDef->params.size(); ++i)
              {
                if (static_cast<int>(i) == packIndex)
                {
                  // Collect all remaining slot values for the pack
                  Vec<RuntimeRef<NGObject>> packItems;
                  for (size_t j = i; j < nextIter.slotValues.size(); ++j)
                  {
                    packItems.push_back(nextIter.slotValues[j]);
                  }
                  newContext->set(funDef->params[i]->paramName, makert<NGTuple>(packItems));
                }
                else if (i < nextIter.slotValues.size())
                {
                  newContext->set(funDef->params[i]->paramName, nextIter.slotValues[i]);
                }
              }
            }
            else
            {
              for (size_t i = 0; i < nextIter.slotValues.size(); i++)
              {
                newContext->set(funDef->params[i]->paramName, nextIter.slotValues[i]);
              }
            }
          }
        }
        ngContext->retVal = newContext->retVal;
      };

      context->define_function(funDef->funName, functionInvoker);
    }

    void visit(Statement *stmt) override
    {
      StatementVisitor vis{context};
      stmt->accept(&vis);
    }

    void visit(ValDef *valDef) override
    {
      StatementVisitor vis{context};

      valDef->body->accept(&vis);
    }

    void visit(TypeDef *typeDef) override
    {
      auto type = makert<NGType>();

      type->name = typeDef->typeName;

      for (const auto &property : typeDef->properties)
      {
        type->properties.push_back(property->propertyName);
      }

      for (const auto &memFn : typeDef->memberFunctions)
      {
        type->memberFunctions[memFn->funName] =
            [this, memFn](const NGSelf &dummy, const NGCtx &ngContext, const NGInvCtx &invocationContext)
        {
          RuntimeRef<NGContext> newContext = ngContext->fork();
          for (size_t i = 0; i < memFn->params.size(); ++i)
          {
            newContext->define(memFn->params[i]->paramName, invocationContext->params[i]);
          }

          newContext->define("self", dummy);

          if (auto structural = std::dynamic_pointer_cast<NGStructuralObject>(dummy); structural != nullptr)
          {
            for (const auto &property : structural->properties)
            {
              newContext->define(property.first, property.second);
            }
          }

          StatementVisitor vis{newContext};
          memFn->body->accept(&vis);
          ngContext->retVal = newContext->retVal;
        };
      }

      context->define_type(type->name, type);
    }

    void visit(TypeAliasDef *typeAliasDef) override
    {
      // Type alias is transparent — just register the underlying type under the alias name
      // The type checker resolves aliases; at runtime we store the underlying type directly
      auto underlyingType = makert<NGType>();
      underlyingType->name = typeAliasDef->aliasName;
      context->define_type(typeAliasDef->aliasName, underlyingType);
    }

    void visit(NewTypeDef *newTypeDef) override
    {
      // Create a new nominal type for the newtype
      auto newType = makert<NGType>();
      newType->name = newTypeDef->typeName;
      context->define_type(newTypeDef->typeName, newType);
    }

    void visit(TaggedUnionDef *taggedUnion) override
    {
      auto type = makert<NGType>();
      type->name = taggedUnion->typeName;
      context->define_type(taggedUnion->typeName, type);

      // Register each variant as a constructor function
      for (int32_t i = 0; i < static_cast<int32_t>(taggedUnion->variants.size()); ++i)
      {
        auto &variant = taggedUnion->variants[i];
        Str unionName = taggedUnion->typeName;
        Str variantName = variant.variantName;
        int32_t variantIndex = i;
        Vec<Str> payloadNames = variant.payloadNames;

        context->define_function(variantName,
          [unionName, variantName, variantIndex, payloadNames](const NGSelf &self, const NGCtx &ctx, const NGInvCtx &invCtx)
          {
            Vec<RuntimeRef<NGObject>> payload;
            for (auto &arg : invCtx->params)
            {
              payload.push_back(arg);
            }
            ctx->retVal = makert<NGTaggedValue>(unionName, variantName, variantIndex, std::move(payload), payloadNames);
          });
      }
    }

    void summary() override { context->summary(); }

    ~Stupid() override = default;
  };

  auto stupid() -> Interpreter *
  {
    NG::library::prelude::do_register();
    NG::library::imgui::do_register();

    return new Stupid(Vec<Str>{
      "",
      NG::module::standard_library_base_path(),
    }); // NOLINT(cppcoreguidelines-owning-memory)
  }

} // namespace NG::intp
