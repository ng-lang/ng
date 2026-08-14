// AI-generated code; reviewed for this repository's vNext rewrite.
#include "vnext/typecheck.hpp"
#include "vnext/const_interp.hpp"

#include <algorithm>
#include <charconv>
#include <format>
#include <unordered_map>
#include <memory>
#include <unordered_set>

namespace NG::vnext::typecheck
{
  namespace
  {
    class Checker final
    {
    public:
      [[nodiscard]] auto check(const hir::Module &module) -> TypeCheckResult
      {
        module_ = &module;
        for (const auto &structure : module.structs) static_cast<void>(interner_.declareStruct(structure.id, structure.name));
        for (const auto &enumeration : module.enums)
        {
          static_cast<void>(interner_.declareEnum(enumeration.id, enumeration.name, enumeration.genericParameters));
          interner_.registerEnumTemplate(enumeration);
        }
        for (const auto &structure : module.structs)
        {
          std::vector<std::string> fields;
          std::vector<TypeId> types;
          for (const auto &field : structure.fields)
          {
            fields.push_back(field.name);
            types.push_back(interner_.resolve(field.type));
          }
          interner_.defineStruct(structure.id, std::move(fields), std::move(types));
        }
        for (const auto &enumeration : module.enums)
        {
          std::vector<std::string> variants;
          std::vector<TypeId> payloads;
          std::vector<bool> hasPayload;
          for (const auto &variant : enumeration.variants)
          {
            variants.push_back(variant.name);
            hasPayload.push_back(variant.payloadType != nullptr);
            payloads.push_back(variant.payloadType != nullptr && enumeration.genericParameters.empty()
                                   ? interner_.resolve(*variant.payloadType)
                                   : builtin::Unit);
          }
          interner_.defineEnum(enumeration.id, std::move(variants), std::move(payloads), std::move(hasPayload));
        }
        for (const auto &function : module.functions)
        {
          FunctionTypeIds signature;
          std::unordered_map<std::string, TypeId> genericBindings;
          TypeInterner::ConstParamBindings constBindings;
          for (size_t index = 0; index < function.genericParameters.size(); ++index)
          {
            const auto parameter = interner_.internTypeParameter(function.genericParameters[index], static_cast<uint32_t>(index));
            signature.genericParameters.push_back(parameter);
            signature.genericParameterNames.push_back(function.genericParameters[index]);
            genericBindings.emplace(function.genericParameters[index], parameter);
          }
          for (size_t index = 0; index < function.constParameters.size(); ++index)
          {
            const auto &parameter = function.constParameters[index];
            const TypeId type = interner_.resolve(parameter.type);
            if (type != builtin::I64)
              throw TypeError(std::format("const generic parameter `{}` must be i64, got {}", parameter.name,
                                          interner_.display(type)), parameter.span);
            signature.constParameters.push_back(type);
            signature.constParameterNames.push_back(parameter.name);
            constBindings.emplace(parameter.name, static_cast<uint32_t>(index));
          }
          FunctionType displaySignature;
          for (const auto &parameter : function.parameters)
          {
            const TypeId type = interner_.resolveInScope(parameter.type, genericBindings, constBindings);
            signature.parameters.push_back(type);
            displaySignature.parameters.push_back(interner_.display(type));
          }
          signature.returnType = function.returnType != nullptr
                                   ? interner_.resolveInScope(*function.returnType, genericBindings, constBindings)
                                   : builtin::Unit;
          displaySignature.returnType = interner_.display(signature.returnType);
          signatures_.emplace(function.id.value, signature);
          functionTypeIds_.emplace(function.id.value, signature);
          functionTypes_.emplace(function.id.value, std::move(displaySignature));
        }
        for (const auto &declaration : module.consts) checkConstDeclaration(declaration);
        for (const auto &function : module.functions)
        {
          if (function.constFunction) constFunctions_.insert(function.id.value);
        }
        interpreter_ = std::make_unique<const_eval::ConstInterpreter>(
            module, constFunctions_, interner_.constInterner(),
            [this](const hir::Expression &node) { return evaluateConstApplication(node); });
        for (const auto &function : module.functions) checkFunction(function);
        return TypeCheckResult{.expressionTypes = std::move(expressionTypes_),
                               .expressionTypeIds = std::move(expressionTypeIds_),
                               .localTypes = std::move(localDisplayTypes_),
                               .localTypeIds = std::move(localTypeIds_),
                               .functionTypes = std::move(functionTypes_),
                               .functionTypeIds = std::move(functionTypeIds_),
                               .callTargets = std::move(callTargets_),
                               .constIfSelections = std::move(constIfSelections_),
                               .typeDescriptors = interner_.descriptors()};
      }

    private:
      using LocalTypes = std::unordered_map<uint32_t, TypeId>;
      using LoopTypes = std::unordered_map<uint32_t, std::vector<TypeId>>;
      using MutableBindings = std::unordered_set<uint32_t>;

      struct Substitution
      {
        std::unordered_map<uint32_t, TypeId> types;
        TypeInterner::ConstSubstitution consts;
      };

      struct ConstMatch
      {
        bool matched{};
        /// Specialization priority per D-012: 3 exact, 2 pattern, 1 primary.
        int priority{};
        std::unordered_map<uint32_t, TypeId> bindings;
      };

      struct ConstDeclChecked
      {
        const hir::ConstDeclaration *declaration;
        std::vector<TypeId> parameters;
        std::vector<TypeId> pattern;
        TypeId targetType;
      };

      [[nodiscard]] auto isGeneric(const FunctionTypeIds &signature) const -> bool
      {
        return !signature.genericParameters.empty() || !signature.constParameters.empty();
      }

      void requireConstArguments(const FunctionTypeIds &signature, const Substitution &substitution,
                                 std::string_view functionName, syntax::SourceSpan span) const
      {
        for (size_t index = 0; index < signature.constParameters.size(); ++index)
        {
          if (!substitution.consts.contains(static_cast<uint32_t>(index)))
            throw TypeError(std::format("cannot infer const generic argument `{}` for function `{}`",
                                        signature.constParameterNames[index], functionName), span);
        }
      }

      void record(const hir::Expression &expression, TypeId type)
      {
        expressionTypeIds_.insert_or_assign(&expression, type);
        expressionTypes_.insert_or_assign(&expression, interner_.display(type));
      }

      void recordLocal(hir::LocalId local, TypeId type)
      {
        localTypeIds_.insert_or_assign(local.value, type);
        localDisplayTypes_.insert_or_assign(local.value, interner_.display(type));
      }

      /// Evaluates a where clause (D-014) under a call substitution: const
      /// predicate applications, const fun calls, `T is Type` tests, and
      /// `!` / `&&` / `||` combinations.
      [[nodiscard]] auto evaluateWhereCondition(const hir::Expression &condition, const Substitution &substitution,
                                                const FunctionTypeIds &signature) -> bool
      {
        std::unordered_map<std::string, TypeId> bindings;
        for (size_t index = 0; index < signature.genericParameters.size(); ++index)
        {
          const TypeId parameter = signature.genericParameters[index];
          const auto found = substitution.types.find(parameter.value);
          bindings.emplace(signature.genericParameterNames[index],
                           found != substitution.types.end() ? found->second : parameter);
        }
        const_eval::ConstBindings constBindings;
        for (size_t index = 0; index < signature.constParameters.size(); ++index)
        {
          const auto found = substitution.consts.find(static_cast<uint32_t>(index));
          if (found != substitution.consts.end()) constBindings.emplace(signature.constParameterNames[index], found->second);
        }
        const auto evaluate = [&](const auto &self, const hir::Expression &expression) -> const_eval::ConstValueId {
          switch (expression.kind)
          {
          case hir::ExpressionKind::BooleanLiteral:
            return interner_.constInterner().internBool(expression.text == "true");
          case hir::ExpressionKind::IntegerLiteral:
            return interner_.constInterner().internInteger(std::stoll(expression.text));
          case hir::ExpressionKind::Grouped: return self(self, *expression.operands[0]);
          case hir::ExpressionKind::Prefix:
            if (expression.text == "!")
              return interner_.constInterner().internBool(!interner_.constInterner().value(self(self, *expression.operands[0])).boolValue);
            throw TypeError(std::format("unsupported where clause operator `{}`", expression.text), expression.span);
          case hir::ExpressionKind::Binary:
          {
            const std::string &op = expression.text;
            if (op == "&&")
            {
              if (!interner_.constInterner().value(self(self, *expression.operands[0])).boolValue) return interner_.constInterner().internBool(false);
              return interner_.constInterner().internBool(interner_.constInterner().value(self(self, *expression.operands[1])).boolValue);
            }
            if (op == "||")
            {
              if (interner_.constInterner().value(self(self, *expression.operands[0])).boolValue) return interner_.constInterner().internBool(true);
              return interner_.constInterner().internBool(interner_.constInterner().value(self(self, *expression.operands[1])).boolValue);
            }
            throw TypeError(std::format("unsupported where clause operator `{}`", op), expression.span);
          }
          case hir::ExpressionKind::GenericApplication:
          {
            std::vector<TypeId> typeArguments;
            for (const auto &argument : expression.genericArguments)
            {
              if (argument.kind == syntax::GenericArgumentKind::Type)
                typeArguments.push_back(interner_.resolveInScope(*argument.type, bindings));
              else
                typeArguments.push_back(builtin::Unit);
            }
            for (const auto &argument : typeArguments)
              if (interner_.descriptor(argument).kind == TypeKind::TypeParameter)
                throw TypeError(std::format("cannot evaluate where clause of generic function with abstract type parameter `{}`",
                                            interner_.display(argument)), expression.span);
            const auto *selected = selectConstDeclaration(expression.text, typeArguments, expression.span);
            const const_eval::ConstValueId value = evaluateConstDeclaration(*selected, expression.span);
            if (interner_.constInterner().value(value).kind != const_eval::ConstValueKind::Bool)
              throw TypeError(std::format("const declaration `{}` must evaluate to bool in predicate position", expression.text),
                              expression.span);
            return value;
          }
          case hir::ExpressionKind::Call:
            return interpreter_->evaluateCall(expression, {}, constBindings, expression.span);
          case hir::ExpressionKind::TypeTest:
          {
            const auto found = bindings.find(expression.text);
            if (found == bindings.end())
              throw TypeError(std::format("where clause tests unknown type parameter `{}`", expression.text), expression.span);
            const TypeId actual = found->second;
            if (interner_.descriptor(actual).kind == TypeKind::TypeParameter)
              throw TypeError(std::format("cannot evaluate where clause of generic function with abstract type parameter `{}`",
                                          expression.text), expression.span);
            const TypeId expected = interner_.resolveInScope(*expression.testedType, bindings);
            return interner_.constInterner().internBool(actual == expected);
          }
          case hir::ExpressionKind::ResolvedName:
            if (expression.resolvedName.has_value() && expression.resolvedName->kind == hir::ResolvedNameKind::ConstParameter)
            {
              const auto found = constBindings.find(expression.text);
              if (found == constBindings.end())
                throw TypeError(std::format("unresolved const parameter `{}` in where clause", expression.text), expression.span);
              return found->second;
            }
            throw TypeError(std::format("`{}` is not a compile-time constant in a where clause", expression.text), expression.span);
          default:
            throw TypeError("unsupported where clause constraint", expression.span);
          }
        };
        return interner_.constInterner().value(evaluate(evaluate, condition)).boolValue;
      }

      void checkFunction(const hir::Function &function)
      {
        LocalTypes locals;
        mutableBindings_.clear();
        const auto &signature = signatures_.at(function.id.value);
        genericBindings_.clear();
        for (size_t index = 0; index < function.genericParameters.size(); ++index)
          genericBindings_.emplace(function.genericParameters[index], signature.genericParameters[index]);
        for (size_t index = 0; index < function.parameters.size(); ++index)
        {
          locals.emplace(function.parameters[index].local.value, signature.parameters[index]);
          recordLocal(function.parameters[index].local, signature.parameters[index]);
        }
        inConstGenericFunction_ = !signature.constParameters.empty();
        if (function.whereClause != nullptr && !isGeneric(signature))
        {
          if (!evaluateWhereCondition(*function.whereClause, {}, signature))
            throw TypeError(std::format("function `{}` does not satisfy its where clause", function.name), function.span);
        }
        checkBlock(function.body, locals, {}, signature.returnType);
        inConstGenericFunction_ = false;
        genericBindings_.clear();
      }

      void checkBlock(const hir::Block &block, LocalTypes locals, LoopTypes loops, TypeId returnType)
      {
        const MutableBindings saved = mutableBindings_;
        for (const auto &statement : block.statements) checkStatement(statement, locals, loops, returnType);
        if (block.tailExpression != nullptr) static_cast<void>(infer(*block.tailExpression, locals));
        mutableBindings_ = std::move(saved);
      }

      void checkStatement(const hir::Statement &statement, LocalTypes &locals, LoopTypes &loops, TypeId returnType)
      {
        switch (statement.kind)
        {
        case hir::StatementKind::Let:
        {
          const TypeId type = statement.bindingType != nullptr
                                  ? inferExpected(*statement.expression, interner_.resolve(*statement.bindingType), locals, "let initializer")
                                  : infer(*statement.expression, locals);
          if (!statement.destructuredLocals.empty())
          {
            const auto &tuple = interner_.descriptor(type);
            if (tuple.kind != TypeKind::Tuple)
              throw TypeError(std::format("cannot destructure value of type {}", interner_.display(type)), statement.expression->span);
            if (tuple.elements.size() != statement.destructuredLocals.size())
              throw TypeError(std::format("tuple destructuring length mismatch: expected {}, got {}", tuple.elements.size(),
                                          statement.destructuredLocals.size()), statement.span);
            for (size_t index = 0; index < statement.destructuredLocals.size(); ++index)
            {
              locals.emplace(statement.destructuredLocals[index].value, tuple.elements[index]);
              recordLocal(statement.destructuredLocals[index], tuple.elements[index]);
              if (statement.mutableBinding) mutableBindings_.insert(statement.destructuredLocals[index].value);
            }
          }
          else
          {
            locals.emplace(statement.local->value, type);
            recordLocal(*statement.local, type);
            if (statement.mutableBinding) mutableBindings_.insert(statement.local->value);
          }
          return;
        }
        case hir::StatementKind::Assign:
          if (statement.assignmentTarget != nullptr)
          {
            requireMutableDeref(*statement.assignmentTarget, locals);
            const TypeId target = infer(*statement.assignmentTarget, locals);
            static_cast<void>(inferExpected(*statement.expression, target, locals, "assignment value"));
          }
          else
          {
            static_cast<void>(inferExpected(*statement.expression, locals.at(statement.local->value), locals, "assignment value"));
          }
          return;
        case hir::StatementKind::Return:
          if (statement.expression != nullptr) static_cast<void>(inferExpected(*statement.expression, returnType, locals, "return value"));
          else requireType(returnType, builtin::Unit, statement.span, "return value");
          return;
        case hir::StatementKind::If:
          requireType(builtin::Bool, infer(*statement.expression, locals), statement.expression->span, "if condition");
          checkBlock(*statement.consequence, locals, loops, returnType);
          if (statement.alternative != nullptr) checkBlock(*statement.alternative, locals, loops, returnType);
          return;
        case hir::StatementKind::ConstIf:
        {
          if (inConstGenericFunction_)
            throw TypeError("per-instance `const if` inside a const-generic function is not yet supported", statement.span);
          requireType(builtin::Bool, infer(*statement.expression, locals), statement.expression->span, "const if condition");
          bool selected{};
          try
          {
            const_eval::ConstEvaluator evaluator{interner_.constInterner()};
            selected = evaluator.evaluateBool(*statement.expression, [this](const hir::Expression &node) {
              if (node.kind == hir::ExpressionKind::GenericApplication) return evaluateConstApplication(node);
              if (node.kind == hir::ExpressionKind::Call)
                return interpreter_->evaluateCall(node, {}, {}, node.span);
              throw const_eval::ConstEvalError("const if condition is not a compile-time constant expression", node.span);
            });
          }
          catch (const const_eval::ConstEvalError &error)
          {
            throw TypeError(error.what(), error.span);
          }
          constIfSelections_.emplace(&statement, selected);
          if (selected) checkBlock(*statement.consequence, locals, loops, returnType);
          else if (statement.alternative != nullptr) checkBlock(*statement.alternative, locals, loops, returnType);
          return;
        }
        case hir::StatementKind::Loop:
        {
          std::vector<TypeId> types;
          for (const auto &initializer : statement.arguments) types.push_back(infer(*initializer, locals));
          LocalTypes loopLocals = locals;
          for (size_t index = 0; index < statement.loopBindings.size(); ++index)
          {
            loopLocals.emplace(statement.loopBindings[index].value, types[index]);
            recordLocal(statement.loopBindings[index], types[index]);
            mutableBindings_.insert(statement.loopBindings[index].value);
          }
          LoopTypes loopTypes = loops;
          loopTypes.emplace(statement.loop->value, types);
          checkBlock(*statement.body, std::move(loopLocals), std::move(loopTypes), returnType);
          return;
        }
        case hir::StatementKind::Next: checkNext(statement, locals, loops); return;
        case hir::StatementKind::Switch: checkSwitch(statement, locals, loops, returnType); return;
        case hir::StatementKind::Expression: static_cast<void>(infer(*statement.expression, locals)); return;
        }
      }

      void checkNext(const hir::Statement &statement, const LocalTypes &locals, const LoopTypes &loops)
      {
        const std::vector<TypeId> *expected = statement.nextTarget->kind == hir::NextTargetKind::Loop
                                                ? &loops.at(statement.nextTarget->id)
                                                : &signatures_.at(statement.nextTarget->id).parameters;
        if (statement.arguments.size() != expected->size())
          throw TypeError(std::format("next argument count mismatch: expected {}, got {}", expected->size(), statement.arguments.size()),
                          statement.span);
        for (size_t index = 0; index < expected->size(); ++index)
          static_cast<void>(inferExpected(*statement.arguments[index], (*expected)[index], locals,
                                          std::format("next argument {}", index + 1)));
      }

      void checkSwitch(const hir::Statement &statement, const LocalTypes &locals, const LoopTypes &loops, TypeId returnType)
      {
        const TypeId scrutinee = infer(*statement.expression, locals);
        const auto &descriptor = interner_.descriptor(scrutinee);
        if (descriptor.kind != TypeKind::Enum)
          throw TypeError(std::format("switch value must be an enum type, got {}", interner_.display(scrutinee)),
                          statement.expression->span);
        std::vector<bool> covered(descriptor.fieldNames.size());
        for (const auto &switchCase : statement.switchCases)
        {
          const auto found = std::find(descriptor.fieldNames.begin(), descriptor.fieldNames.end(), switchCase.variantName);
          if (found == descriptor.fieldNames.end())
            throw TypeError(std::format("unknown variant `{}` for enum `{}`", switchCase.variantName, descriptor.name),
                            switchCase.span);
          const size_t variant = static_cast<size_t>(std::distance(descriptor.fieldNames.begin(), found));
          if (covered[variant])
            throw TypeError(std::format("duplicate variant `{}` in switch", switchCase.variantName), switchCase.span);
          covered[variant] = true;
          if (!switchCase.binding.has_value())
          {
            checkBlock(*switchCase.body, locals, loops, returnType);
            continue;
          }
          if (!descriptor.variantHasPayload[variant])
            throw TypeError(std::format("variant `{}` has no payload to bind", switchCase.variantName), switchCase.span);
          LocalTypes caseLocals = locals;
          caseLocals.emplace(switchCase.binding->value, descriptor.elements[variant]);
          recordLocal(*switchCase.binding, descriptor.elements[variant]);
          checkBlock(*switchCase.body, std::move(caseLocals), loops, returnType);
        }
        if (statement.alternative != nullptr)
        {
          checkBlock(*statement.alternative, locals, loops, returnType);
        }
        else if (const auto missing = std::find(covered.begin(), covered.end(), false); missing != covered.end())
        {
          const size_t variant = static_cast<size_t>(std::distance(covered.begin(), missing));
          throw TypeError(std::format("switch is not exhaustive: missing variant `{}`", descriptor.fieldNames[variant]),
                          statement.span);
        }
      }

      void checkConstDeclaration(const hir::ConstDeclaration &declaration)
      {
        ConstDeclChecked checked{.declaration = &declaration};
        std::unordered_map<std::string, TypeId> bindings;
        for (size_t index = 0; index < declaration.typeParameters.size(); ++index)
        {
          const TypeId parameter = interner_.internTypeParameter(declaration.typeParameters[index], static_cast<uint32_t>(index));
          checked.parameters.push_back(parameter);
          bindings.emplace(declaration.typeParameters[index], parameter);
        }
        for (const auto &pattern : declaration.pattern) checked.pattern.push_back(interner_.resolveInScope(*pattern, bindings));
        checked.targetType = declaration.targetType != nullptr ? interner_.resolve(*declaration.targetType) : builtin::Bool;
        if (checked.targetType != builtin::Bool && checked.targetType != builtin::I64)
          throw TypeError(std::format("const declaration `{}` must declare bool or i64, got {}", declaration.name,
                                      interner_.display(checked.targetType)), declaration.span);
        constDeclarations_[declaration.name].push_back(std::move(checked));
      }

      struct PatternMatchState
      {
        bool containsParameter{};
        bool repeated{};
        std::unordered_map<uint32_t, TypeId> bindings;
      };

      [[nodiscard]] auto matchConstPatternType(TypeId pattern, TypeId actual, PatternMatchState &state) const -> bool
      {
        const auto &patternDescriptor = interner_.descriptor(pattern);
        const auto &actualDescriptor = interner_.descriptor(actual);
        if (actualDescriptor.kind == TypeKind::TypeParameter) return false;
        switch (patternDescriptor.kind)
        {
        case TypeKind::TypeParameter:
        {
          state.containsParameter = true;
          const uint32_t parameterIndex = *patternDescriptor.nominalId;
          const auto existing = state.bindings.find(parameterIndex);
          if (existing != state.bindings.end())
          {
            if (existing->second != actual) return false;
            state.repeated = true;
            return true;
          }
          state.bindings.emplace(parameterIndex, actual);
          return true;
        }
        case TypeKind::Builtin:
        case TypeKind::DependentArray:
          return pattern == actual;
        case TypeKind::Reference:
        case TypeKind::RawPointer:
          if (actualDescriptor.kind != patternDescriptor.kind ||
              actualDescriptor.referenceMutable != patternDescriptor.referenceMutable)
            return false;
          return matchConstPatternType(patternDescriptor.element, actualDescriptor.element, state);
        case TypeKind::DynamicArray:
          if (actualDescriptor.kind != TypeKind::DynamicArray) return false;
          return matchConstPatternType(patternDescriptor.element, actualDescriptor.element, state);
        case TypeKind::FixedArray:
          if (actualDescriptor.kind != TypeKind::FixedArray || *patternDescriptor.length != *actualDescriptor.length) return false;
          return matchConstPatternType(patternDescriptor.element, actualDescriptor.element, state);
        case TypeKind::Tuple:
        {
          if (actualDescriptor.kind != TypeKind::Tuple || patternDescriptor.elements.size() != actualDescriptor.elements.size())
            return false;
          for (size_t index = 0; index < patternDescriptor.elements.size(); ++index)
            if (!matchConstPatternType(patternDescriptor.elements[index], actualDescriptor.elements[index], state)) return false;
          return true;
        }
        case TypeKind::Struct:
        case TypeKind::Enum:
        {
          if (actualDescriptor.kind != patternDescriptor.kind || *patternDescriptor.nominalId != *actualDescriptor.nominalId)
            return false;
          for (size_t index = 0; index < patternDescriptor.elements.size(); ++index)
            if (!matchConstPatternType(patternDescriptor.elements[index], actualDescriptor.elements[index], state)) return false;
          return true;
        }
        }
        return false;
      }

      [[nodiscard]] auto matchConstPattern(const ConstDeclChecked &candidate, const std::vector<TypeId> &arguments) const
          -> ConstMatch
      {
        if (candidate.pattern.size() != arguments.size()) return {};
        PatternMatchState state;
        bool constructed{};
        for (size_t index = 0; index < arguments.size(); ++index)
        {
          if (!matchConstPatternType(candidate.pattern[index], arguments[index], state)) return {};
          if (interner_.descriptor(candidate.pattern[index]).kind != TypeKind::TypeParameter) constructed = true;
        }
        ConstMatch match{.matched = true, .bindings = std::move(state.bindings)};
        if (!state.containsParameter) match.priority = 3;
        else if (constructed || state.repeated) match.priority = 2;
        else match.priority = 1;
        return match;
      }

      [[nodiscard]] auto selectConstDeclaration(std::string_view name, const std::vector<TypeId> &arguments,
                                                syntax::SourceSpan span) const -> const ConstDeclChecked *
      {
        const auto found = constDeclarations_.find(std::string{name});
        if (found == constDeclarations_.end())
          throw TypeError(std::format("unknown const declaration `{}`", name), span);
        const ConstDeclChecked *best = nullptr;
        int bestPriority{};
        for (const auto &candidate : found->second)
        {
          ConstMatch match = matchConstPattern(candidate, arguments);
          if (!match.matched) continue;
          if (best == nullptr || match.priority > bestPriority)
          {
            best = &candidate;
            bestPriority = match.priority;
          }
          else if (match.priority == bestPriority)
          {
            throw TypeError(std::format("ambiguous const specialization `{}`", name), span);
          }
        }
        if (best == nullptr)
          throw TypeError(std::format("no const specialization of `{}` matches the given type arguments", name), span);
        return best;
      }

      [[nodiscard]] auto evaluateConstDeclaration(const ConstDeclChecked &selected, syntax::SourceSpan span)
          -> const_eval::ConstValueId
      {
        if (selected.declaration->bodyKind == hir::ConstDeclarationBodyKind::Delete)
          throw TypeError(std::format("const declaration `{}` is deleted for these type arguments",
                                      selected.declaration->name), selected.declaration->span);
        if (selected.declaration->bodyKind == hir::ConstDeclarationBodyKind::Native)
          throw TypeError(std::format("no const native registered for `{}`", selected.declaration->name), span);
        const_eval::ConstEvaluator evaluator{interner_.constInterner()};
        return evaluator.evaluate(*selected.declaration->body, {});
      }

      /// Evaluates a const predicate application (`name<types>`). Used by the
      /// `const if` extension and, later, by where clauses.
      [[nodiscard]] auto evaluateConstApplication(const hir::Expression &expression) -> const_eval::ConstValueId
      {
        std::vector<TypeId> typeArguments;
        for (const auto &argument : expression.genericArguments)
        {
          if (argument.kind != syntax::GenericArgumentKind::Type)
            throw TypeError("const arguments on const predicates are not yet supported", argument.span);
          const TypeId resolved = interner_.resolveInScope(*argument.type, genericBindings_);
          if (interner_.descriptor(resolved).kind == TypeKind::TypeParameter)
            throw TypeError(std::format("cannot evaluate const declaration `{}` for abstract type parameter `{}`",
                                        expression.text, interner_.display(resolved)), expression.span);
          typeArguments.push_back(resolved);
        }
        const auto *selected = selectConstDeclaration(expression.text, typeArguments, expression.span);
        const const_eval::ConstValueId value = evaluateConstDeclaration(*selected, expression.span);
        if (interner_.constInterner().value(value).kind != const_eval::ConstValueKind::Bool)
          throw TypeError(std::format("const declaration `{}` must evaluate to bool in predicate position", expression.text),
                          expression.span);
        return value;
      }

      [[nodiscard]] static auto isPlace(const hir::Expression &expression) -> bool
      {
        switch (expression.kind)
        {
        case hir::ExpressionKind::ResolvedName:
          return expression.resolvedName.has_value() && expression.resolvedName->kind == hir::ResolvedNameKind::Local;
        case hir::ExpressionKind::Index:
        case hir::ExpressionKind::Member:
          return isPlace(*expression.operands[0]);
        case hir::ExpressionKind::Grouped:
          return isPlace(*expression.operands[0]);
        default:
          return false;
        }
      }

      void requireMutableRoot(const hir::Expression &expression, syntax::SourceSpan span) const
      {
        const hir::Expression *root = &expression;
        while (true)
        {
          if (root->kind == hir::ExpressionKind::Index || root->kind == hir::ExpressionKind::Member ||
              root->kind == hir::ExpressionKind::Grouped)
            root = root->operands[0].get();
          else
            break;
        }
        if (root->kind != hir::ExpressionKind::ResolvedName || root->resolvedName->kind != hir::ResolvedNameKind::Local) return;
        if (!mutableBindings_.contains(root->resolvedName->id))
          throw TypeError("cannot create a mutable reference to an immutable binding", span);
      }

      void requireMutableDeref(const hir::Expression &expression, const LocalTypes &locals)
      {
        switch (expression.kind)
        {
        case hir::ExpressionKind::Index:
        case hir::ExpressionKind::Member:
        case hir::ExpressionKind::Grouped:
          requireMutableDeref(*expression.operands[0], locals);
          return;
        case hir::ExpressionKind::Prefix:
          if (expression.text == "*")
          {
            const TypeId operand = infer(*expression.operands[0], locals);
            const auto &descriptor = interner_.descriptor(operand);
            if (descriptor.kind != TypeKind::Reference)
              throw TypeError(std::format("cannot assign through value of type {}", interner_.display(operand)), expression.span);
            if (!descriptor.referenceMutable)
              throw TypeError("cannot assign through an immutable reference", expression.span);
          }
          return;
        default:
          return;
        }
      }

      /// Builds a substitution from explicit generic arguments written on a
      /// call expression (`name<types>(...)`), filling declared parameters in
      /// declaration order: type parameters first, then const parameters.
      [[nodiscard]] auto explicitSubstitution(const hir::Expression &expression, const FunctionTypeIds &signature) -> Substitution
      {
        Substitution substitution;
        if (expression.genericArguments.size() != signature.genericParameters.size() + signature.constParameters.size())
          throw TypeError(std::format("generic argument count mismatch: expected {}, got {}",
                                      signature.genericParameters.size() + signature.constParameters.size(),
                                      expression.genericArguments.size()), expression.span);
        for (size_t index = 0; index < expression.genericArguments.size(); ++index)
        {
          const auto &argument = expression.genericArguments[index];
          if (index < signature.genericParameters.size())
          {
            if (argument.kind != syntax::GenericArgumentKind::Type)
              throw TypeError(std::format("generic argument {} must be a type", index + 1), argument.span);
            substitution.types.emplace(signature.genericParameters[index].value,
                                       interner_.resolveInScope(*argument.type, genericBindings_));
          }
          else
          {
            if (argument.kind != syntax::GenericArgumentKind::ConstExpr)
              throw TypeError(std::format("generic argument {} must be a const expression", index + 1), argument.span);
            const const_eval::ConstValueId value =
                const_eval::ConstEvaluator{interner_.constInterner()}.evaluate(*argument.constExpr, {});
            substitution.consts.emplace(static_cast<uint32_t>(index - signature.genericParameters.size()), value);
          }
        }
        return substitution;
      }

      [[nodiscard]] auto inferExpected(const hir::Expression &expression, TypeId expected, const LocalTypes &locals,
                                       std::string_view context) -> TypeId
      {
        const auto &descriptor = interner_.descriptor(expected);
        if (expression.kind == hir::ExpressionKind::ArrayLiteral &&
            (descriptor.kind == TypeKind::DynamicArray || descriptor.kind == TypeKind::FixedArray ||
             descriptor.kind == TypeKind::DependentArray))
        {
          if (descriptor.kind == TypeKind::FixedArray && expression.operands.size() != *descriptor.length)
            throw TypeError(std::format("fixed array length mismatch: expected {}, got {}", *descriptor.length,
                                        expression.operands.size()), expression.span);
          for (const auto &element : expression.operands)
            static_cast<void>(inferExpected(*element, descriptor.element, locals, "array element"));
          record(expression, expected);
          return expected;
        }
        if (expression.kind == hir::ExpressionKind::EnumLiteral && descriptor.kind == TypeKind::Enum)
          return inferExpectedEnum(expression, expected, locals);
        if (expression.kind == hir::ExpressionKind::Call && !expression.operands.empty() &&
            expression.operands[0]->resolvedName.has_value() &&
            expression.operands[0]->resolvedName->kind == hir::ResolvedNameKind::Function)
        {
          hir::DefId selected{expression.operands[0]->resolvedName->id};
          if (expression.operands[0]->functionCandidates.size() > 1)
          {
            int bestScore{-1};
            for (const auto candidate : expression.operands[0]->functionCandidates)
            {
              const auto &candidateSignature = signatures_.at(candidate.value);
              if (candidateSignature.parameters.size() != expression.operands.size() - 1) continue;
              int score = isGeneric(candidateSignature) ? 0 : 100;
              for (const auto parameter : candidateSignature.parameters) score += specificity(parameter);
              if (score > bestScore)
              {
                bestScore = score;
                selected = candidate;
              }
              else if (score == bestScore && candidate != selected)
              {
                throw TypeError("ambiguous function specialization", expression.span);
              }
            }
          }
          const auto &signature = signatures_.at(selected.value);
          if (isGeneric(signature))
          {
            Substitution substitution;
            const size_t supplied = expression.operands.size() - 1;
            if (supplied != signature.parameters.size())
              throw TypeError(std::format("call argument count mismatch: expected {}, got {}", signature.parameters.size(), supplied), expression.span);
            if (!expression.genericArguments.empty())
            {
              substitution = explicitSubstitution(expression, signature);
              for (size_t index = 0; index < supplied; ++index)
                static_cast<void>(inferExpected(*expression.operands[index + 1],
                                                specialize(signature.parameters[index], substitution), locals,
                                                std::format("call argument {}", index + 1)));
            }
            else
            {
              unify(signature.returnType, expected, substitution, expression.span);
              for (size_t index = 0; index < supplied; ++index)
              {
                const auto &argument = *expression.operands[index + 1];
                const auto &parameterDescriptor = interner_.descriptor(signature.parameters[index]);
                if (argument.kind == hir::ExpressionKind::EnumLiteral && parameterDescriptor.kind == TypeKind::Enum)
                {
                  const uint32_t variant = argument.variant.value();
                  if (argument.operands.size() != (parameterDescriptor.variantHasPayload[variant] ? 1u : 0u))
                    throw TypeError("generic enum constructor payload arity mismatch", argument.span);
                  if (!argument.operands.empty())
                    unify(parameterDescriptor.elements[variant], infer(*argument.operands.front(), locals), substitution,
                          argument.operands.front()->span);
                }
                else
                {
                  unify(signature.parameters[index], infer(argument, locals), substitution, argument.span);
                }
              }
            }
            const TypeId specialized = specialize(signature.returnType, substitution);
            requireConstArguments(signature, substitution, expression.operands[0]->text, expression.span);
            if (module_ != nullptr && selected.value < module_->functions.size() &&
                module_->functions.at(selected.value).whereClause != nullptr &&
                !evaluateWhereCondition(*module_->functions.at(selected.value).whereClause, substitution, signature))
              throw TypeError(std::format("call to `{}` does not satisfy its where clause", module_->functions.at(selected.value).name),
                              expression.span);
            requireType(expected, specialized, expression.span, context);
            record(expression, specialized);
            callTargets_.insert_or_assign(&expression, selected);
            return specialized;
          }
        }
        if (expression.kind == hir::ExpressionKind::StructLiteral && descriptor.kind == TypeKind::Struct)
        {
          return inferExpectedStruct(expression, expected, locals);
        }
        if (expression.kind == hir::ExpressionKind::TupleLiteral && descriptor.kind == TypeKind::Tuple)
        {
          if (expression.operands.size() != descriptor.elements.size())
            throw TypeError(std::format("tuple length mismatch: expected {}, got {}", descriptor.elements.size(),
                                        expression.operands.size()), expression.span);
          for (size_t index = 0; index < expression.operands.size(); ++index)
            static_cast<void>(inferExpected(*expression.operands[index], descriptor.elements[index], locals,
                                            std::format("tuple element {}", index)));
          record(expression, expected);
          return expected;
        }
        const TypeId actual = infer(expression, locals);
        requireType(expected, actual, expression.span, context);
        return actual;
      }

      [[nodiscard]] auto inferExpectedEnum(const hir::Expression &expression, TypeId expected, const LocalTypes &locals) -> TypeId
      {
        const auto &descriptor = interner_.descriptor(expected);
        if (!expression.enumId.has_value() || descriptor.nominalId != expression.enumId->value)
          throw TypeError(std::format("enum constructor type mismatch: expected {}, got {}", interner_.display(expected), expression.text), expression.span);
        const uint32_t variant = expression.variant.value();
        if (variant >= descriptor.elements.size()) throw TypeError("enum variant is out of range", expression.span);
        const size_t wanted = descriptor.variantHasPayload[variant] ? 1 : 0;
        if (expression.operands.size() != wanted)
          throw TypeError(std::format("enum variant `{}` expects {} payload values, got {}", descriptor.fieldNames[variant], wanted,
                                      expression.operands.size()), expression.span);
        if (wanted == 1) static_cast<void>(inferExpected(*expression.operands[0], descriptor.elements[variant], locals, "variant payload"));
        record(expression, expected);
        return expected;
      }

      [[nodiscard]] auto inferExpectedStruct(const hir::Expression &expression, TypeId expected, const LocalTypes &locals) -> TypeId
      {
        const auto &descriptor = interner_.descriptor(expected);
        if (!expression.structId.has_value() || descriptor.nominalId != expression.structId->value)
          throw TypeError(std::format("struct literal type mismatch: expected {}, got {}", interner_.display(expected), expression.text),
                          expression.span);
        std::vector<bool> seen(descriptor.fieldNames.size());
        for (size_t index = 0; index < expression.operands.size(); ++index)
        {
          const auto found = std::find(descriptor.fieldNames.begin(), descriptor.fieldNames.end(), expression.memberNames[index]);
          if (found == descriptor.fieldNames.end())
            throw TypeError(std::format("unknown field `{}` in struct `{}`", expression.memberNames[index], descriptor.name),
                            expression.span);
          const size_t field = static_cast<size_t>(std::distance(descriptor.fieldNames.begin(), found));
          if (seen[field]) throw TypeError(std::format("duplicate field `{}` in struct literal", expression.memberNames[index]), expression.span);
          seen[field] = true;
          static_cast<void>(inferExpected(*expression.operands[index], descriptor.elements[field], locals,
                                          std::format("field `{}`", expression.memberNames[index])));
        }
        if (std::find(seen.begin(), seen.end(), false) != seen.end())
          throw TypeError(std::format("missing field in struct `{}`", descriptor.name), expression.span);
        record(expression, expected);
        return expected;
      }

      [[nodiscard]] auto infer(const hir::Expression &expression, const LocalTypes &locals) -> TypeId
      {
        TypeId type;
        switch (expression.kind)
        {
        case hir::ExpressionKind::IntegerLiteral: type = builtin::I64; break;
        case hir::ExpressionKind::StringLiteral: type = builtin::String; break;
        case hir::ExpressionKind::BooleanLiteral: type = builtin::Bool; break;
        case hir::ExpressionKind::ArrayLiteral:
        {
          if (expression.operands.empty()) throw TypeError("cannot infer the type of an empty array literal", expression.span);
          const TypeId element = infer(*expression.operands.front(), locals);
          for (size_t index = 1; index < expression.operands.size(); ++index)
            requireType(element, infer(*expression.operands[index], locals), expression.operands[index]->span, "array element");
          type = interner_.internDynamicArray(element);
          break;
        }
        case hir::ExpressionKind::StructLiteral:
        {
          if (!expression.structId.has_value()) throw TypeError("struct literal has no resolved type", expression.span);
          type = interner_.typeForStruct(*expression.structId);
          return inferExpectedStruct(expression, type, locals);
        }
        case hir::ExpressionKind::EnumLiteral:
        {
          if (!expression.enumId.has_value() || !expression.variant.has_value())
            throw TypeError("enum constructor has no resolved variant", expression.span);
          if (interner_.enumGenericArity(*expression.enumId) != 0)
            throw TypeError(std::format("cannot infer generic arguments for enum constructor `{}`", expression.text), expression.span);
          type = interner_.typeForEnum(*expression.enumId);
          const auto &descriptor = interner_.descriptor(type);
          const uint32_t variant = *expression.variant;
          const size_t expected = descriptor.variantHasPayload.at(variant) ? 1 : 0;
          if (expression.operands.size() != expected)
            throw TypeError(std::format("enum variant `{}` expects {} payload values, got {}", descriptor.fieldNames.at(variant),
                                        expected, expression.operands.size()), expression.span);
          if (expected == 1)
            static_cast<void>(inferExpected(*expression.operands[0], descriptor.elements.at(variant), locals,
                                            std::format("variant `{}` payload", descriptor.fieldNames.at(variant))));
          break;
        }
        case hir::ExpressionKind::TupleLiteral:
        {
          std::vector<TypeId> elements;
          elements.reserve(expression.operands.size());
          for (const auto &element : expression.operands) elements.push_back(infer(*element, locals));
          type = interner_.internTuple(elements);
          break;
        }
        case hir::ExpressionKind::ResolvedName:
          if (expression.resolvedName->kind == hir::ResolvedNameKind::Function)
            throw TypeError("function name cannot be used as a value", expression.span);
          if (expression.resolvedName->kind == hir::ResolvedNameKind::ConstParameter)
            throw TypeError(std::format("const parameter `{}` is not a runtime value", expression.text), expression.span);
          type = locals.at(expression.resolvedName->id);
          break;
        case hir::ExpressionKind::Grouped: type = infer(*expression.operands[0], locals); break;
        case hir::ExpressionKind::GenericApplication:
        {
          std::vector<TypeId> typeArguments;
          for (const auto &argument : expression.genericArguments)
          {
            if (argument.kind != syntax::GenericArgumentKind::Type)
              throw TypeError("const arguments on const predicates are not yet supported", argument.span);
            const TypeId resolved = interner_.resolveInScope(*argument.type, genericBindings_);
            if (interner_.descriptor(resolved).kind == TypeKind::TypeParameter)
              throw TypeError(std::format("cannot evaluate const declaration `{}` for abstract type parameter `{}`",
                                          expression.text, interner_.display(resolved)), expression.span);
            typeArguments.push_back(resolved);
          }
          static_cast<void>(selectConstDeclaration(expression.text, typeArguments, expression.span));
          type = builtin::Bool;
          break;
        }
        case hir::ExpressionKind::Prefix:
        {
          const TypeId operand = infer(*expression.operands[0], locals);
          if (expression.text == "ref" || expression.text == "ref mut")
          {
            if (!isPlace(*expression.operands[0]))
              throw TypeError("reference operand is not a place", expression.span);
            const auto &operandDescriptor = interner_.descriptor(operand);
            if (operandDescriptor.kind == TypeKind::Reference)
              throw TypeError("cannot create a reference to a reference", expression.span);
            if (expression.text == "ref mut") requireMutableRoot(*expression.operands[0], expression.span);
            type = interner_.internReference(operand, expression.text == "ref mut");
            break;
          }
          if (expression.text == "*")
          {
            const auto &descriptor = interner_.descriptor(operand);
            if (descriptor.kind != TypeKind::Reference)
              throw TypeError(std::format("cannot dereference value of type {}", interner_.display(operand)), expression.span);
            type = descriptor.element;
            break;
          }
          type = expression.text == "!" ? builtin::Bool : builtin::I64;
          requireType(type, operand, expression.span, "prefix operand");
          break;
        }
        case hir::ExpressionKind::Binary:
        {
          const TypeId left = infer(*expression.operands[0], locals);
          const TypeId right = infer(*expression.operands[1], locals);
          requireType(left, right, expression.span, "binary operands");
          if (expression.text == "==" || expression.text == "!=") type = builtin::Bool;
          else if (expression.text == "<" || expression.text == "<=" || expression.text == ">" || expression.text == ">=")
          {
            requireType(builtin::I64, left, expression.span, "comparison operand");
            type = builtin::Bool;
          }
          else if (expression.text == "&&" || expression.text == "||")
          {
            requireType(builtin::Bool, left, expression.span, "logical operand");
            type = builtin::Bool;
          }
          else
          {
            if (expression.text != "+" || left != builtin::String) requireType(builtin::I64, left, expression.span, "binary operand");
            type = left;
          }
          break;
        }
        case hir::ExpressionKind::Call:
        {
          if (!expression.operands[0]->resolvedName.has_value() ||
              expression.operands[0]->resolvedName->kind != hir::ResolvedNameKind::Function)
            throw TypeError("call target is not a function", expression.operands[0]->span);
          hir::DefId selected{expression.operands[0]->resolvedName->id};
          if (expression.operands[0]->functionCandidates.size() > 1)
          {
            int bestScore{-1};
            for (const auto candidate : expression.operands[0]->functionCandidates)
            {
              const auto &candidateSignature = signatures_.at(candidate.value);
              if (candidateSignature.parameters.size() != expression.operands.size() - 1) continue;
              int score = isGeneric(candidateSignature) ? 0 : 100;
              for (const auto parameter : candidateSignature.parameters) score += specificity(parameter);
              if (!isGeneric(candidateSignature))
              {
                try
                {
                  for (size_t index = 0; index < candidateSignature.parameters.size(); ++index)
                    requireType(candidateSignature.parameters[index], infer(*expression.operands[index + 1], locals),
                                expression.operands[index + 1]->span, "specialization argument");
                }
                catch (const TypeError &) { continue; }
              }
              if (score > bestScore)
              {
                bestScore = score;
                selected = candidate;
              }
              else if (score == bestScore && candidate != selected)
              {
                throw TypeError("ambiguous function specialization", expression.span);
              }
            }
            if (bestScore < 0) throw TypeError("no matching function specialization", expression.span);
          }
          const auto &signature = signatures_.at(selected.value);
          const size_t supplied = expression.operands.size() - 1;
          if (supplied != signature.parameters.size())
            throw TypeError(std::format("call argument count mismatch: expected {}, got {}", signature.parameters.size(), supplied),
                            expression.span);
          Substitution substitution;
          if (!expression.genericArguments.empty())
          {
            substitution = explicitSubstitution(expression, signature);
            for (size_t index = 0; index < supplied; ++index)
              static_cast<void>(inferExpected(*expression.operands[index + 1],
                                              specialize(signature.parameters[index], substitution), locals,
                                              std::format("call argument {}", index + 1)));
          }
          else
          {
            for (size_t index = 0; index < supplied; ++index)
            {
              const auto &argument = *expression.operands[index + 1];
              if (!isGeneric(signature))
              {
                static_cast<void>(inferExpected(argument, signature.parameters[index], locals,
                                                std::format("call argument {}", index + 1)));
                continue;
              }
              const auto &expectedDescriptor = interner_.descriptor(signature.parameters[index]);
              if (argument.kind == hir::ExpressionKind::EnumLiteral && expectedDescriptor.kind == TypeKind::Enum)
              {
                if (!argument.enumId.has_value() || !argument.variant.has_value() ||
                    expectedDescriptor.nominalId != argument.enumId->value)
                  throw TypeError("generic enum constructor type mismatch", argument.span);
                const uint32_t variant = *argument.variant;
                if (variant >= expectedDescriptor.elements.size()) throw TypeError("enum variant is out of range", argument.span);
                if (argument.operands.size() != (expectedDescriptor.variantHasPayload[variant] ? 1u : 0u))
                  throw TypeError(std::format("enum variant `{}` payload arity mismatch", expectedDescriptor.fieldNames[variant]), argument.span);
                if (!argument.operands.empty())
                  unify(expectedDescriptor.elements[variant], infer(*argument.operands.front(), locals), substitution,
                        argument.operands.front()->span);
              }
              else
              {
                unify(signature.parameters[index], infer(argument, locals), substitution, argument.span);
              }
            }
          }
          type = specialize(signature.returnType, substitution);
          if (!signature.genericParameters.empty() && interner_.descriptor(type).kind == TypeKind::TypeParameter)
            throw TypeError(std::format("cannot infer generic arguments for function `{}`", expression.operands[0]->text), expression.span);
          requireConstArguments(signature, substitution, expression.operands[0]->text, expression.span);
          if (module_ != nullptr && selected.value < module_->functions.size() &&
              module_->functions.at(selected.value).whereClause != nullptr &&
              !evaluateWhereCondition(*module_->functions.at(selected.value).whereClause, substitution, signature))
            throw TypeError(std::format("call to `{}` does not satisfy its where clause", module_->functions.at(selected.value).name),
                            expression.span);
          callTargets_.insert_or_assign(&expression, selected);
          break;
        }
        case hir::ExpressionKind::Index:
        {
          const TypeId receiver = infer(*expression.operands[0], locals);
          requireType(builtin::I64, infer(*expression.operands[1], locals), expression.operands[1]->span, "array index");
          const auto &descriptor = interner_.descriptor(receiver);
          if (descriptor.kind == TypeKind::Tuple)
          {
            if (expression.text.empty()) throw TypeError("tuple index must be an integer literal", expression.operands[1]->span);
            uint64_t index{};
            const auto [end, error] = std::from_chars(expression.text.data(), expression.text.data() + expression.text.size(), index);
            if (error != std::errc{} || end != expression.text.data() + expression.text.size() || index >= descriptor.elements.size())
              throw TypeError(std::format("tuple index {} is out of range for length {}", expression.text,
                                          descriptor.elements.size()), expression.operands[1]->span);
            type = descriptor.elements[index];
          }
          else
          {
            if (descriptor.kind != TypeKind::DynamicArray && descriptor.kind != TypeKind::FixedArray &&
                descriptor.kind != TypeKind::DependentArray)
              throw TypeError(std::format("cannot index value of type {}", interner_.display(receiver)), expression.span);
            type = descriptor.element;
          }
          break;
        }
        case hir::ExpressionKind::Member:
        {
          const TypeId receiver = infer(*expression.operands[0], locals);
          const auto &descriptor = interner_.descriptor(receiver);
          if (descriptor.kind != TypeKind::Struct)
            throw TypeError(std::format("cannot access member `{}` on value of type {}", expression.text,
                                        interner_.display(receiver)), expression.span);
          const auto found = std::find(descriptor.fieldNames.begin(), descriptor.fieldNames.end(), expression.text);
          if (found == descriptor.fieldNames.end())
            throw TypeError(std::format("unknown field `{}` in struct `{}`", expression.text, descriptor.name), expression.span);
          type = descriptor.elements[static_cast<size_t>(std::distance(descriptor.fieldNames.begin(), found))];
          break;
        }
        }
        record(expression, type);
        return type;
      }

      [[nodiscard]] auto specificity(TypeId type) const -> int
      {
        const auto &descriptor = interner_.descriptor(type);
        if (descriptor.kind == TypeKind::TypeParameter) return 0;
        int score = descriptor.kind == TypeKind::Builtin ? 1 : 0;
        for (const auto element : descriptor.elements) score += specificity(element);
        for (const auto argument : descriptor.typeArguments) score += specificity(argument);
        if (descriptor.kind == TypeKind::DynamicArray || descriptor.kind == TypeKind::FixedArray ||
            descriptor.kind == TypeKind::DependentArray || descriptor.kind == TypeKind::Reference ||
            descriptor.kind == TypeKind::RawPointer)
          score += specificity(descriptor.element);
        return score;
      }
      auto unify(TypeId expected, TypeId actual, Substitution &substitution, syntax::SourceSpan span) -> void
      {
        const auto &expectedDescriptor = interner_.descriptor(expected);
        if (expectedDescriptor.kind == TypeKind::TypeParameter)
        {
          if (const auto found = substitution.types.find(expected.value); found != substitution.types.end())
            requireType(found->second, actual, span, "generic argument");
          else substitution.types.emplace(expected.value, actual);
          return;
        }
        if (expected == actual) return;
        const auto &actualDescriptor = interner_.descriptor(actual);
        if (expectedDescriptor.kind == TypeKind::DependentArray || actualDescriptor.kind == TypeKind::DependentArray)
        {
          const auto &dependent = expectedDescriptor.kind == TypeKind::DependentArray ? expectedDescriptor : actualDescriptor;
          const auto &concrete = expectedDescriptor.kind == TypeKind::DependentArray ? actualDescriptor : expectedDescriptor;
          if (concrete.kind != TypeKind::FixedArray)
            throw TypeError(std::format("generic argument type mismatch: expected {}, got {}", interner_.display(expected),
                                        interner_.display(actual)), span);
          const auto value = interner_.internConstInteger(static_cast<int64_t>(*concrete.length));
          if (const auto bound = substitution.consts.find(*dependent.constParameterIndex); bound != substitution.consts.end())
          {
            if (bound->second != value) throw TypeError("generic array length mismatch", span);
          }
          else
          {
            substitution.consts.emplace(*dependent.constParameterIndex, value);
          }
          unify(expectedDescriptor.element, actualDescriptor.element, substitution, span);
          return;
        }
        if (expectedDescriptor.kind != actualDescriptor.kind)
          throw TypeError(std::format("generic argument type mismatch: expected {}, got {}", interner_.display(expected),
                                      interner_.display(actual)), span);
        if (expectedDescriptor.kind == TypeKind::Enum || expectedDescriptor.kind == TypeKind::Tuple)
        {
          if (expectedDescriptor.nominalId != actualDescriptor.nominalId ||
              expectedDescriptor.elements.size() != actualDescriptor.elements.size())
            throw TypeError(std::format("generic argument type mismatch: expected {}, got {}", interner_.display(expected),
                                        interner_.display(actual)), span);
          for (size_t index = 0; index < expectedDescriptor.elements.size(); ++index)
            unify(expectedDescriptor.elements[index], actualDescriptor.elements[index], substitution, span);
          return;
        }
        if (expectedDescriptor.kind == TypeKind::DynamicArray || expectedDescriptor.kind == TypeKind::FixedArray)
        {
          if (expectedDescriptor.length != actualDescriptor.length)
            throw TypeError("generic array length mismatch", span);
          unify(expectedDescriptor.element, actualDescriptor.element, substitution, span);
          return;
        }
        if (expectedDescriptor.kind == TypeKind::Reference || expectedDescriptor.kind == TypeKind::RawPointer)
        {
          if (expectedDescriptor.referenceMutable != actualDescriptor.referenceMutable)
            throw TypeError("generic reference mutability mismatch", span);
          unify(expectedDescriptor.element, actualDescriptor.element, substitution, span);
          return;
        }
        throw TypeError(std::format("generic argument type mismatch: expected {}, got {}", interner_.display(expected),
                                    interner_.display(actual)), span);
      }

      [[nodiscard]] auto specialize(TypeId type, const Substitution &substitution) -> TypeId
      {
        return interner_.specialize(type, substitution.types, substitution.consts);
      }
      void requireType(TypeId expected, TypeId actual, syntax::SourceSpan span, std::string_view context) const
      {
        if (expected != actual)
          throw TypeError(std::format("{} type mismatch: expected {}, got {}", context, interner_.display(expected),
                                      interner_.display(actual)), span);
      }

      TypeInterner interner_;
      std::unordered_map<uint32_t, FunctionTypeIds> signatures_;
      std::unordered_map<const hir::Expression *, std::string> expressionTypes_;
      std::unordered_map<const hir::Expression *, TypeId> expressionTypeIds_;
      std::unordered_map<uint32_t, std::string> localDisplayTypes_;
      std::unordered_map<uint32_t, TypeId> localTypeIds_;
      std::unordered_map<uint32_t, FunctionType> functionTypes_;
      std::unordered_map<uint32_t, FunctionTypeIds> functionTypeIds_;
      std::unordered_map<const hir::Expression *, hir::DefId> callTargets_;
      std::unordered_map<const hir::Statement *, bool> constIfSelections_;
      const hir::Module *module_{};
      std::unordered_map<std::string, std::vector<ConstDeclChecked>> constDeclarations_;
      std::unordered_set<uint32_t> constFunctions_;
      std::unique_ptr<const_eval::ConstInterpreter> interpreter_;
      /// Generic parameter bindings of the function currently being checked;
      /// used to resolve in-body const predicate arguments.
      std::unordered_map<std::string, TypeId> genericBindings_;
      /// Bindings declared with `let mut` (and loop bindings, which `next`
      /// rebinds) in the current lexical path; restored at block boundaries.
      MutableBindings mutableBindings_;
      bool inConstGenericFunction_{};
    };
  } // namespace

  auto TypeChecker::check(const hir::Module &module) -> TypeCheckResult { return Checker{}.check(module); }
} // namespace NG::vnext::typecheck
