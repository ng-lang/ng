// AI-generated code; reviewed for this repository's vNext rewrite.
#include "typecheck.hpp"
#include "const_interp.hpp"

#include <algorithm>
#include <charconv>
#include <deque>
#include <format>
#include <iterator>
#include <memory>
#include <unordered_map>
#include <unordered_set>

namespace NG::typecheck
{
  namespace
  {
    class Checker final
    {
    public:
      [[nodiscard]] auto check(const hir::Module &module) -> TypeCheckResult
      {
        module_ = &module;
        {
          std::unordered_map<std::string, syntax::SourceSpan> typeNames;
          const auto declareName = [&](const std::string &name, syntax::SourceSpan span) {
            if (!typeNames.emplace(name, span).second)
              throw TypeError(std::format("duplicate type declaration `{}`", name), span);
          };
          for (const auto &structure : module.structs) declareName(structure.name, structure.span);
          for (const auto &enumeration : module.enums) declareName(enumeration.name, enumeration.span);
          for (const auto &opaque : module.opaqueTypes) declareName(opaque.name, opaque.span);
        }
        for (const auto &opaque : module.opaqueTypes)
          static_cast<void>(interner_.declareOpaqueType(opaque));
        for (const auto &structure : module.structs) static_cast<void>(interner_.declareStruct(structure.id, structure.name));
        for (const auto &enumeration : module.enums)
        {
          static_cast<void>(interner_.declareEnum(enumeration.id, enumeration.name, enumeration.genericParameters));
          interner_.registerEnumTemplate(enumeration);
        }
        for (const auto &structure : module.structs) interner_.registerStructTemplate(structure);
        for (const auto &structure : module.structs)
        {
          if (!structure.genericParameters.empty()) continue;
          std::vector<std::string> fields;
          std::vector<TypeId> types;
          for (const auto &field : structure.fields)
          {
            fields.push_back(field.name);
            const TypeId fieldType = interner_.resolve(field.type);
            if (interner_.descriptor(fieldType).kind == TypeKind::Reference)
              throw TypeError("references cannot be stored in struct fields", field.span);
            if (interner_.descriptor(fieldType).kind == TypeKind::Trait)
              throw TypeError(std::format("trait `{}` is not a value type; use `ref<{}>`", interner_.display(fieldType),
                                          interner_.display(fieldType)), field.span);
            types.push_back(fieldType);
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
        registerTraits();
        registerImpls();
        for (const auto &structure : module.structs)
        {
          if (structure.derivedTraits.empty()) continue;
          const TypeId target = interner_.typeForStruct(structure.id);
          std::unordered_set<std::string> seen;
          for (const auto &traitName : structure.derivedTraits)
          {
            if (!seen.insert(traitName).second)
              throw TypeError(std::format("duplicate derive for trait `{}` on type `{}`", traitName, structure.name),
                              structure.span);
            if (traitName != "Copy" && traitName != "Clone")
              throw TypeError(std::format("derive currently supports Copy and Clone only: {}", traitName),
                              structure.span);
            for (const auto &impl : impls_)
              if (impl.target == target && impl.traitName == traitName)
                throw TypeError(std::format("derive conflicts with explicit impl for trait `{}` on type `{}`",
                                            traitName, structure.name), structure.span);
            impls_.push_back(ImplInfo{.traitName = traitName, .target = target, .methods = {}});
            if (traitName == "Clone") derivedCloneTypes_.insert(target.value);
          }
        }
        for (const auto &function : module.functions)
        {
          if (function.name.starts_with("impl$") || function.name.starts_with("default$")) continue;
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
          for (size_t index = 0; index < function.packParameters.size(); ++index)
          {
            const auto parameter =
                interner_.internTypeParameter(function.packParameters[index],
                                              static_cast<uint32_t>(function.genericParameters.size() + index));
            signature.packParameters.push_back(parameter);
            signature.packParameterNames.push_back(function.packParameters[index]);
            genericBindings.emplace(function.packParameters[index], parameter);
          }
          for (size_t index = 0; index < function.constructorParameters.size(); ++index)
          {
            const auto parameter = interner_.internTypeConstructor(function.constructorParameters[index],
                                                                   static_cast<uint32_t>(index));
            signature.constructorParameters.push_back(parameter);
            signature.constructorParameterNames.push_back(function.constructorParameters[index]);
            genericBindings.emplace(function.constructorParameters[index], parameter);
          }
          for (size_t index = 0; index < function.variadicConstructorParameters.size(); ++index)
          {
            const auto parameter = interner_.internTypeConstructor(function.variadicConstructorParameters[index],
                                                                   static_cast<uint32_t>(function.constructorParameters.size() + index),
                                                                   true);
            signature.constructorParameters.push_back(parameter);
            signature.constructorParameterNames.push_back(function.variadicConstructorParameters[index]);
            genericBindings.emplace(function.variadicConstructorParameters[index], parameter);
          }
          for (const auto kind : function.genericParameterOrder)
            if (kind != syntax::GenericParameterKind::Pack) signature.explicitParameterOrder.push_back(kind);
          for (const auto &[parameterName, bounds] : function.traitBounds)
          {
            for (const auto &traitName : bounds)
              if (!traits_.contains(traitName))
                throw TypeError(std::format("unknown trait bound `{}` on `{}`", traitName, parameterName), function.span);
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
            if (interner_.descriptor(type).kind == TypeKind::Trait)
              throw TypeError(std::format("trait `{}` is not a value type; use `ref<{}>`", interner_.display(type),
                                          interner_.display(type)), parameter.span);
            signature.parameters.push_back(type);
            displaySignature.parameters.push_back(interner_.display(type));
          }
          signature.returnType = function.returnType != nullptr
                                   ? interner_.resolveInScope(*function.returnType, genericBindings, constBindings)
                                   : builtin::Unit;
          if (interner_.descriptor(signature.returnType).kind == TypeKind::Trait)
            throw TypeError(std::format("trait `{}` is not a value type; use `ref<{}>`", interner_.display(signature.returnType),
                                        interner_.display(signature.returnType)), function.span);
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
                               .callPackArgCounts = std::move(callPackArgCounts_),
                               .callPackTupleTypes = std::move(callPackTupleTypes_),
                               .callSpreadPositions = std::move(callSpreadPositions_),
                               .callFoldSpreadPositions = std::move(callFoldSpreadPositions_),
                               .callFoldAccumulatorPositions = std::move(callFoldAccumulatorPositions_),
                               .returnDrops = std::move(returnDrops_),
                               .fallthroughDrops = std::move(fallthroughDrops_),
                               .blockDrops = std::move(blockDrops_),
                               .placeholderFunctions = std::move(placeholderFunctions_),
                               .methodReceiverMutable = std::move(methodReceiverMutable_),
                               .methodReceiverRefTypes = std::move(methodReceiverRefTypes_),
                               .constIfSelections = std::move(constIfSelections_),
                               .instances = std::vector<hir::Function>{std::make_move_iterator(instances_.begin()),
                                                                       std::make_move_iterator(instances_.end())},
                               .deferredMethodFunctions = std::move(deferredMethodFunctions_),
                               .derivedCloneCalls = std::move(derivedCloneCalls_),
                               .traitViewCoercions = std::move(traitViewCoercions_),
                               .traitViewCalls = std::move(traitViewCalls_),
                               .traitViewTables = std::move(traitViewTables_),
                               .typeDescriptors = interner_.descriptors()};
      }

    private:
      using LocalTypes = std::unordered_map<uint32_t, TypeId>;
      using LoopTypes = std::unordered_map<uint32_t, std::vector<TypeId>>;
      using MutableBindings = std::unordered_set<uint32_t>;

      struct Substitution
      {
        std::unordered_map<uint32_t, TypeId> types;
        std::unordered_map<uint32_t, std::vector<TypeId>> packs;
        TypeInterner::ConstSubstitution consts;
        /// Constructor-parameter index -> template base type (struct).
        TypeInterner::ConstructorSubstitution constructors;
      };

      struct ConstMatch
      {
        bool matched{};
        /// Specialization priority per D-012: 3 exact, 2 pattern, 1 primary.
        int priority{};
        std::unordered_map<uint32_t, TypeId> bindings;
      };

      struct TraitInfo
      {
        std::string name;
        std::vector<std::string> supertraits;
        bool autoTrait{};
        TypeId selfParameter;
        /// Method names in declaration order (dynamic dispatch indexes).
        std::vector<std::string> methodOrder;
        std::unordered_map<std::string, std::vector<TypeId>> methodParameters;
        std::unordered_map<std::string, TypeId> methodReturns;
        std::unordered_map<std::string, hir::DefId> methodDefaults;
      };

      struct ImplInfo
      {
        std::string traitName;
        TypeId target;
        std::unordered_map<std::string, hir::DefId> methods;
      };

      /// Simple borrow conflict state (D-015 rule 5): shared and mutable
      /// reference counts per root local, conservatively scoped to the
      /// enclosing block.
      struct BorrowState
      {
        struct Counts
        {
          size_t shared{};
          size_t mutableRefs{};
        };
        std::unordered_map<uint32_t, Counts> counts;
        static constexpr size_t MaxShared{16};

        [[nodiscard]] auto mergedWith(const BorrowState &other) const -> BorrowState
        {
          BorrowState merged = *this;
          for (const auto &[local, otherCounts] : other.counts)
          {
            auto &mine = merged.counts[local];
            mine.shared = std::max(mine.shared, otherCounts.shared);
            mine.mutableRefs = std::max(mine.mutableRefs, otherCounts.mutableRefs);
          }
          return merged;
        }
      };

      /// Per-local move tracking (D-015): `whole` marks a fully moved
      /// binding; `fields` marks partially moved struct/enum fields.
      struct MoveState
      {
        std::unordered_map<uint32_t, bool> whole;
        std::unordered_map<uint32_t, std::unordered_set<uint32_t>> fields;

        [[nodiscard]] auto isWholeMoved(uint32_t local) const -> bool
        {
          return whole.contains(local) && (!fields.contains(local) || fields.at(local).empty());
        }
        [[nodiscard]] auto isFieldMoved(uint32_t local, uint32_t field) const -> bool
        {
          if (whole.contains(local)) return !fields.contains(local) || !fields.at(local).contains(field);
          return fields.contains(local) && fields.at(local).contains(field);
        }
        [[nodiscard]] auto hasMovedField(uint32_t local) const -> bool
        {
          return fields.contains(local) && !fields.at(local).empty();
        }
        void markWhole(uint32_t local)
        {
          whole[local] = true;
          fields.erase(local);
        }
        void markField(uint32_t local, uint32_t field)
        {
          if (!whole.contains(local)) fields[local].insert(field);
        }
        void reinitialize(uint32_t local)
        {
          whole.erase(local);
          fields.erase(local);
        }
        void reinitializeField(uint32_t local, uint32_t field)
        {
          if (whole.contains(local)) fields[local].insert(field);
          else fields[local].erase(field);
        }
        [[nodiscard]] auto mergedWith(const MoveState &other) const -> MoveState
        {
          MoveState merged = *this;
          for (const auto &[local, _] : other.whole) merged.markWhole(local);
          for (const auto &[local, movedFields] : other.fields)
            for (const auto field : movedFields) merged.markField(local, field);
          return merged;
        }
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
        return !signature.genericParameters.empty() || !signature.packParameters.empty() ||
               !signature.constParameters.empty() || !signature.constructorParameters.empty();
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
            if (const auto builtin = evaluateTraitIntrospection(expression.text, expression.genericArguments, expression.span);
                builtin.has_value())
              return *builtin;
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
            if (const auto builtin = evaluateTupleIntrospection(expression.text, typeArguments, expression.span);
                builtin.has_value())
            {
              if (interner_.constInterner().value(*builtin).kind != const_eval::ConstValueKind::Bool)
                throw TypeError(std::format("const predicate `{}` must evaluate to bool in predicate position",
                                            expression.text), expression.span);
              return *builtin;
            }
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
          case hir::ExpressionKind::TraitBound:
          {
            const auto found = bindings.find(expression.text);
            if (found == bindings.end())
              throw TypeError(std::format("where clause tests unknown type parameter `{}`", expression.text), expression.span);
            const TypeId actual = found->second;
            if (interner_.descriptor(actual).kind == TypeKind::TypeParameter)
              throw TypeError(std::format("cannot evaluate where clause of generic function with abstract type parameter `{}`",
                                          expression.text), expression.span);
            for (const auto &traitName : expression.traitNames)
            {
              if (!traits_.contains(traitName))
                throw TypeError(std::format("unknown trait `{}` in where clause", traitName), expression.span);
              if (!hasImpl(traitName, actual)) return interner_.constInterner().internBool(false);
            }
            return interner_.constInterner().internBool(true);
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

      void registerTraits()
      {
        for (const auto &trait : module_->traits)
        {
          TraitInfo info{.name = trait.name, .supertraits = trait.supertraits, .autoTrait = trait.autoTrait,
                         .selfParameter = interner_.internTypeParameter("Self", 0)};
          for (const auto &method : trait.methods) info.methodOrder.push_back(method.name);
          static_cast<void>(interner_.declareTraitType(trait.name));
          const std::unordered_map<std::string, TypeId> selfBindings{{"Self", info.selfParameter}};
          for (size_t index = 0; index < trait.methods.size(); ++index)
          {
            const auto &method = trait.methods[index];
            // Default methods were lowered into module functions; their
            // parameters were moved there.
            const std::vector<hir::Parameter> &sourceParameters = method.body.has_value()
                                                                     ? module_->functions.at(trait.methodIds.at(index).value).parameters
                                                                     : method.parameters;
            std::vector<TypeId> parameters;
            for (const auto &parameter : sourceParameters)
              parameters.push_back(interner_.resolveInScope(parameter.type, selfBindings));
            if (parameters.empty() || interner_.descriptor(parameters.front()).kind != TypeKind::Reference ||
                interner_.descriptor(parameters.front()).element != info.selfParameter)
              throw TypeError(std::format("trait method `{}` must take `self: Self ref`", method.name), method.span);
            info.methodParameters.emplace(method.name, parameters);
            const hir::Type *sourceReturnType = method.returnType.get();
            if (method.body.has_value()) sourceReturnType = module_->functions.at(trait.methodIds.at(index).value).returnType.get();
            info.methodReturns.emplace(method.name, sourceReturnType != nullptr
                                                       ? interner_.resolveInScope(*sourceReturnType, selfBindings)
                                                       : builtin::Unit);
            if (method.body.has_value())
            {
              info.methodDefaults.emplace(method.name, trait.methodIds.at(index));
              FunctionTypeIds defaultSignature;
              defaultSignature.parameters = parameters;
              defaultSignature.returnType = info.methodReturns.at(method.name);
              FunctionType displaySignature;
              for (const auto parameter : parameters) displaySignature.parameters.push_back(interner_.display(parameter));
              displaySignature.returnType = interner_.display(defaultSignature.returnType);
              signatures_.emplace(trait.methodIds.at(index).value, std::move(defaultSignature));
              functionTypes_.emplace(trait.methodIds.at(index).value, std::move(displaySignature));
            }
          }
          traits_.emplace(trait.name, std::move(info));
        }
        // Compiler-known lifecycle traits (legacy 55 derive targets): Copy is
        // a marker; Clone carries the `clone(self: Self ref) -> Self` contract.
        if (!traits_.contains("Copy"))
        {
          TraitInfo copy{.name = "Copy", .selfParameter = interner_.internTypeParameter("Self", 0)};
          traits_.emplace("Copy", std::move(copy));
        }
        if (!traits_.contains("Clone"))
        {
          TraitInfo clone{.name = "Clone", .selfParameter = interner_.internTypeParameter("Self", 0)};
          clone.methodParameters.emplace("clone", std::vector<TypeId>{interner_.internReference(clone.selfParameter, false)});
          clone.methodReturns.emplace("clone", clone.selfParameter);
          traits_.emplace("Clone", std::move(clone));
        }
      }

      void collectSupertraits(const std::string &traitName, std::vector<const TraitInfo *> &out,
                              std::unordered_set<std::string> &seen) const
      {
        const auto found = traits_.find(traitName);
        if (found == traits_.end() || !seen.insert(traitName).second) return;
        out.push_back(&found->second);
        for (const auto &supertrait : found->second.supertraits) collectSupertraits(supertrait, out, seen);
      }

      void resolveImplMethodSignature(hir::DefId id, const hir::Function &function, TypeId target)
      {
        FunctionTypeIds signature;
        const std::unordered_map<std::string, TypeId> bindings{{"Self", target}};
        for (const auto &parameter : function.parameters)
          signature.parameters.push_back(interner_.resolveInScope(parameter.type, bindings));
        if (signature.parameters.empty() || interner_.descriptor(signature.parameters.front()).kind != TypeKind::Reference ||
            interner_.descriptor(signature.parameters.front()).element != target)
          throw TypeError(std::format("trait method `{}` must take `self: Self ref`", function.name), function.span);
        signature.returnType = function.returnType != nullptr ? interner_.resolveInScope(*function.returnType, bindings) : builtin::Unit;
        FunctionType displaySignature;
        for (const auto parameter : signature.parameters) displaySignature.parameters.push_back(interner_.display(parameter));
        displaySignature.returnType = interner_.display(signature.returnType);
        functionTypeIds_.emplace(id.value, signature);
        signatures_.emplace(id.value, std::move(signature));
        functionTypes_.emplace(id.value, std::move(displaySignature));
      }

      /// Walks a drop impl body for `move (*self).field` / `move self.field`
      /// expressions, collecting the field names the destructor takes over.
      void collectDropMovedFields(const hir::Block &block, uint32_t selfLocal, std::unordered_set<std::string> &fields)
      {
        for (const auto &statement : block.statements) collectDropMovedFields(statement, selfLocal, fields);
        if (block.tailExpression != nullptr) collectDropMovedFields(*block.tailExpression, selfLocal, fields);
      }

      void collectDropMovedFields(const hir::Statement &statement, uint32_t selfLocal, std::unordered_set<std::string> &fields)
      {
        switch (statement.kind)
        {
        case hir::StatementKind::Let:
        case hir::StatementKind::Return:
          if (statement.expression != nullptr) collectDropMovedFields(*statement.expression, selfLocal, fields);
          return;
        case hir::StatementKind::Assign:
          if (statement.expression != nullptr) collectDropMovedFields(*statement.expression, selfLocal, fields);
          if (statement.assignmentTarget != nullptr) collectDropMovedFields(*statement.assignmentTarget, selfLocal, fields);
          return;
        case hir::StatementKind::Expression:
          if (statement.expression != nullptr) collectDropMovedFields(*statement.expression, selfLocal, fields);
          return;
        case hir::StatementKind::If:
        case hir::StatementKind::ConstIf:
          if (statement.expression != nullptr) collectDropMovedFields(*statement.expression, selfLocal, fields);
          if (statement.consequence != nullptr) collectDropMovedFields(*statement.consequence, selfLocal, fields);
          if (statement.alternative != nullptr) collectDropMovedFields(*statement.alternative, selfLocal, fields);
          return;
        case hir::StatementKind::Loop:
          if (statement.body != nullptr) collectDropMovedFields(*statement.body, selfLocal, fields);
          for (const auto &argument : statement.arguments) collectDropMovedFields(*argument, selfLocal, fields);
          return;
        case hir::StatementKind::Next:
          for (const auto &argument : statement.arguments) collectDropMovedFields(*argument, selfLocal, fields);
          return;
        case hir::StatementKind::Switch:
          if (statement.expression != nullptr) collectDropMovedFields(*statement.expression, selfLocal, fields);
          for (const auto &switchCase : statement.switchCases)
            if (switchCase.body != nullptr) collectDropMovedFields(*switchCase.body, selfLocal, fields);
          return;
        }
      }

      void collectDropMovedFields(const hir::Expression &expression, uint32_t selfLocal,
                                  std::unordered_set<std::string> &fields)
      {
        if (expression.kind == hir::ExpressionKind::Prefix && expression.text == "move" && !expression.operands.empty())
        {
          const auto &moved = *expression.operands[0];
          if (moved.kind == hir::ExpressionKind::Member && !moved.operands.empty())
          {
            // `(*self).field` parses as Member over a Grouped deref.
            const hir::Expression *root = moved.operands[0].get();
            while (root->kind == hir::ExpressionKind::Grouped && !root->operands.empty())
              root = root->operands[0].get();
            const hir::Expression *selfNode = root;
            if (root->kind == hir::ExpressionKind::Prefix && root->text == "*" && !root->operands.empty())
              selfNode = root->operands[0].get();
            if (selfNode->kind == hir::ExpressionKind::ResolvedName && selfNode->resolvedName.has_value() &&
                selfNode->resolvedName->kind == hir::ResolvedNameKind::Local && selfNode->resolvedName->id == selfLocal)
            {
              fields.insert(moved.text);
              return;
            }
          }
        }
        for (const auto &child : expression.operands) collectDropMovedFields(*child, selfLocal, fields);
      }

      /// Builds a (local, drop method) drop edge for a live drop-typed local,
      /// validating field-aware partial moves against the Drop impl contract:
      /// dropping a value whose field the destructor moves is a double-own.
      [[nodiscard]] auto dropEdge(uint32_t local, TypeId type, syntax::SourceSpan span) -> std::pair<uint32_t, uint32_t>
      {
        const auto &impl = *std::find_if(impls_.begin(), impls_.end(), [&](const ImplInfo &candidate) {
          return candidate.traitName == "Drop" && candidate.target.value == type.value;
        });
        if (const auto moved = dropMovedFields_.find(type.value); moved != dropMovedFields_.end())
        {
          const auto &descriptor = interner_.descriptor(type);
          for (const auto &fieldName : moved->second)
          {
            const auto found = std::find(descriptor.fieldNames.begin(), descriptor.fieldNames.end(), fieldName);
            if (found == descriptor.fieldNames.end()) continue;
            const uint32_t field = static_cast<uint32_t>(std::distance(descriptor.fieldNames.begin(), found));
            if (moveState_.isFieldMoved(local, field))
              throw TypeError(std::format("cannot drop a value with field `{}` moved out", fieldName), span);
          }
        }
        return {local, impl.methods.at("drop").value};
      }

      void registerImpls()
      {
        for (const auto &impl : module_->impls)
        {
          if (impl.traitName == "Drop")
          {
            // Compiler-known lifecycle contract (D-015 rule 6): no trait
            // declaration; the single `drop` method runs when a value's
            // scope ends while it is still initialized.
            const TypeId target = interner_.resolve(*impl.targetType);
            if (interner_.descriptor(target).kind != TypeKind::Struct)
              throw TypeError("Drop impl target must be a struct type", impl.span);
            if (impl.methods.size() != 1 || impl.methods.front().name != "drop")
              throw TypeError("Drop impl must define exactly one `drop` method", impl.span);
            resolveImplMethodSignature(impl.methodIds.at(0), module_->functions.at(impl.methodIds.at(0).value), target);
            {
              const auto &dropFunction = module_->functions.at(impl.methodIds.at(0).value);
              std::unordered_set<std::string> movedFields;
              collectDropMovedFields(dropFunction.body, dropFunction.parameters.front().local.value, movedFields);
              if (!movedFields.empty())
                dropMovedFields_.emplace(target.value, std::vector<std::string>{movedFields.begin(), movedFields.end()});
            }
            ImplInfo info{.traitName = "Drop", .target = target};
            info.methods.emplace("drop", impl.methodIds.at(0));
            dropTypes_.insert(target.value);
            impls_.push_back(std::move(info));
            continue;
          }
          if (!traits_.contains(impl.traitName))
            throw TypeError(std::format("unknown trait `{}` in impl", impl.traitName), impl.span);
          const TypeId target = interner_.resolve(*impl.targetType);
          if (interner_.descriptor(target).kind == TypeKind::TypeParameter)
            throw TypeError("impl target must be a concrete type", impl.span);
          for (const auto &existing : impls_)
            if (existing.traitName == impl.traitName && existing.target == target)
              throw TypeError(std::format("duplicate impl for trait `{}`", impl.traitName), impl.span);
          std::vector<const TraitInfo *> closure;
          std::unordered_set<std::string> seen;
          collectSupertraits(impl.traitName, closure, seen);
          std::unordered_map<std::string, const TraitInfo *> required;
          for (const auto *trait : closure)
            for (const auto &[methodName, _] : trait->methodParameters) required.emplace(methodName, trait);
          ImplInfo info{.traitName = impl.traitName, .target = target};
          for (size_t index = 0; index < impl.methods.size(); ++index)
          {
            const auto &method = impl.methods[index];
            if (!required.contains(method.name))
              throw TypeError(std::format("impl for trait `{}` provides unknown method `{}`", impl.traitName, method.name),
                              method.span);
            resolveImplMethodSignature(impl.methodIds.at(index), module_->functions.at(impl.methodIds.at(index).value), target);
            info.methods.emplace(method.name, impl.methodIds.at(index));
          }
          for (const auto &[methodName, owner] : required)
          {
            if (info.methods.contains(methodName)) continue;
            const auto defaultMethod = owner->methodDefaults.find(methodName);
            if (defaultMethod == owner->methodDefaults.end())
              throw TypeError(std::format("impl for trait `{}` is missing method `{}`", impl.traitName, methodName), impl.span);
            info.methods.emplace(methodName, defaultMethod->second);
          }
          impls_.push_back(std::move(info));
        }
      }

      [[nodiscard]] auto hasImpl(const std::string &traitName, TypeId target) const -> bool
      {
        const auto declared = traits_.find(traitName);
        if (declared != traits_.end() && declared->second.autoTrait)
        {
          const auto kind = interner_.descriptor(target).kind;
          return kind != TypeKind::TypeParameter && kind != TypeKind::TypeConstructor &&
                 kind != TypeKind::TypeApplication;
        }
        for (const auto &impl : impls_)
        {
          if (impl.target != target) continue;
          std::vector<const TraitInfo *> closure;
          std::unordered_set<std::string> seen;
          collectSupertraits(impl.traitName, closure, seen);
          for (const auto *trait : closure)
            if (trait->name == traitName) return true;
        }
        return false;
      }

      [[nodiscard]] auto inferMethodCall(const hir::Expression &expression, const LocalTypes &locals) -> TypeId
      {
        const auto &callee = *expression.operands[0];
        const auto &receiverNode = *callee.operands[0];
        // Qualified calls (`Trait.method(...)`) carry an unresolved trait name
        // as the callee receiver; every other receiver form is a value place.
        const bool qualified = receiverNode.kind == hir::ExpressionKind::ResolvedName &&
                               !receiverNode.resolvedName.has_value() && traits_.contains(receiverNode.text);
        if (receiverNode.kind == hir::ExpressionKind::ResolvedName && !receiverNode.resolvedName.has_value() &&
            !traits_.contains(receiverNode.text))
          throw TypeError(std::format("unknown trait `{}` in qualified call", receiverNode.text), receiverNode.span);
        const hir::Expression &receiver = qualified ? *expression.operands[1] : receiverNode;
        const size_t firstArgument = qualified ? 2 : 1;
        const TypeId receiverType = infer(receiver, locals);
        TypeId dispatchType = receiverType;
        bool passReferenceThrough = false;
        if (interner_.descriptor(receiverType).kind == TypeKind::Reference)
        {
          dispatchType = interner_.descriptor(receiverType).element;
          passReferenceThrough = true;
        }
        if (interner_.descriptor(dispatchType).kind == TypeKind::TypeParameter)
        {
          // Abstract dispatch through a trait bound: resolve the signature
          // against the bound and defer the target to monomorphization.
          const std::string parameterName = interner_.descriptor(dispatchType).name;
          const hir::Function *current = currentFunctionId_.value < module_->functions.size()
                                             ? &module_->functions.at(currentFunctionId_.value)
                                             : nullptr;
          const TraitInfo *owning = nullptr;
          if (current != nullptr)
          {
            for (const auto &[boundParameter, boundTraits] : current->traitBounds)
            {
              if (boundParameter != parameterName) continue;
              for (const auto &traitName : boundTraits)
              {
                const auto found = traits_.find(traitName);
                if (found != traits_.end() && found->second.methodParameters.contains(expression.text))
                {
                  owning = &found->second;
                  break;
                }
              }
              if (owning != nullptr) break;
            }
          }
          if (owning == nullptr)
            throw TypeError(std::format("no trait bound provides method `{}` for type parameter `{}`", expression.text,
                                        parameterName), expression.span);
          deferredMethodFunctions_.insert(currentFunctionId_.value);
          const TypeId returnType = interner_.specialize(owning->methodReturns.at(expression.text),
                                                         {{owning->selfParameter.value, dispatchType}});
          record(expression, returnType);
          return returnType;
        }
        if (interner_.descriptor(dispatchType).kind == TypeKind::TraitReference)
        {
          // Dynamic dispatch through a trait view (D-011 stage 2): resolve the
          // method index in declaration order and the erased return type.
          const std::string traitName = interner_.descriptor(dispatchType).name;
          const auto &trait = traits_.at(traitName);
          const auto foundMethod = std::find(trait.methodOrder.begin(), trait.methodOrder.end(), expression.text);
          if (foundMethod == trait.methodOrder.end())
            throw TypeError(std::format("trait `{}` has no method `{}`", traitName, expression.text), expression.span);
          const size_t methodIndex = static_cast<size_t>(std::distance(trait.methodOrder.begin(), foundMethod));
          const TypeId returnType = trait.methodReturns.at(expression.text);
          if (typeContains(returnType, trait.selfParameter))
            throw TypeError("trait object methods with Self in the return type are not yet supported", expression.span);
          const auto &parameters = trait.methodParameters.at(expression.text);
          const size_t supplied = expression.operands.size() - firstArgument;
          if (supplied + 1 != parameters.size())
            throw TypeError(std::format("method argument count mismatch: expected {}, got {}", parameters.size() - 1,
                                        supplied), expression.span);
          for (size_t index = 0; index < supplied; ++index)
          {
            const TypeId parameter = parameters[index + 1];
            if (typeContains(parameter, trait.selfParameter))
              throw TypeError("trait object method arguments typed with Self are not yet supported", expression.span);
            static_cast<void>(inferExpected(*expression.operands[firstArgument + index], parameter, locals,
                                            std::format("method argument {}", index + 1)));
          }
          traitViewCalls_.insert_or_assign(&expression, std::pair<std::string, size_t>{traitName, methodIndex});
          record(expression, returnType);
          return returnType;
        }
        if (expression.text == "clone" && derivedCloneTypes_.contains(dispatchType.value))
        {
          if (expression.operands.size() != firstArgument)
            throw TypeError("derived clone takes no arguments", expression.span);
          methodReceiverMutable_.insert_or_assign(&expression, false);
          methodReceiverRefTypes_.insert_or_assign(&expression, interner_.internReference(dispatchType, false));
          derivedCloneCalls_.insert_or_assign(&expression, dispatchType);
          record(expression, dispatchType);
          return dispatchType;
        }
        const hir::DefId *selected = nullptr;
        for (const auto &impl : impls_)
        {
          if (impl.target != dispatchType) continue;
          if (const auto found = impl.methods.find(expression.text); found != impl.methods.end())
          {
            selected = &found->second;
            break;
          }
        }
        if (selected == nullptr)
          throw TypeError(std::format("no method `{}` for value of type {}", expression.text, interner_.display(dispatchType)),
                          expression.span);
        const auto &signature = signatures_.at(selected->value);
        const size_t supplied = expression.operands.size() - firstArgument;
        if (supplied != signature.parameters.size() - 1)
          throw TypeError(std::format("method argument count mismatch: expected {}, got {}", signature.parameters.size() - 1,
                                      supplied), expression.span);
        const TypeId receiverParameter = signature.parameters.front();
        const auto &receiverDescriptor = interner_.descriptor(receiverParameter);
        if (passReferenceThrough)
        {
          requireType(receiverParameter, receiverType, receiver.span, "method receiver");
        }
        else
        {
          if (interner_.descriptor(receiverDescriptor.element).kind != TypeKind::TypeParameter)
            requireType(receiverDescriptor.element, receiverType, receiver.span, "method receiver");
          if (receiverDescriptor.referenceMutable) requireMutableRoot(receiver, receiver.span);
        }
        for (size_t index = 0; index < supplied; ++index)
        {
          const auto &parameterDescriptor = interner_.descriptor(signature.parameters[index + 1]);
          if (parameterDescriptor.kind != TypeKind::Reference && parameterDescriptor.kind != TypeKind::RawPointer)
            trackConsumption(*expression.operands[firstArgument + index], locals);
        }
        methodReceiverMutable_.insert_or_assign(&expression, receiverDescriptor.referenceMutable);
        TypeId recordedReference = receiverParameter;
        if (interner_.descriptor(receiverDescriptor.element).kind == TypeKind::TypeParameter)
          recordedReference = interner_.internReference(dispatchType, receiverDescriptor.referenceMutable);
        methodReceiverRefTypes_.insert_or_assign(&expression, recordedReference);
        for (size_t index = 0; index < supplied; ++index)
          static_cast<void>(inferExpected(*expression.operands[firstArgument + index], signature.parameters[index + 1], locals,
                                          std::format("method argument {}", index + 1)));
        record(expression, signature.returnType);
        callTargets_.insert_or_assign(&expression, *selected);
        return signature.returnType;
      }

      /// Instantiates a generic function with a fully concrete substitution:
      /// the clone is renumbered, registered with specialized types, and its
      /// body re-checked under concrete bindings. Non-generic functions and
      /// non-concrete substitutions return the original id (type-erased).
      [[nodiscard]] auto specializeReturnType(const FunctionTypeIds &signature, const Substitution &substitution,
                                              size_t packCount) -> TypeId
      {
        const auto &descriptor = interner_.descriptor(signature.returnType);
        if (descriptor.kind != TypeKind::Tuple) return specialize(signature.returnType, substitution);
        std::vector<TypeId> elements;
        for (const auto element : descriptor.elements)
        {
          if (interner_.descriptor(element).kind == TypeKind::TypePack)
          {
            const auto foundPack = substitution.packs.find(interner_.descriptor(element).element.value);
            if (foundPack == substitution.packs.end()) continue;
            elements.insert(elements.end(), foundPack->second.begin(), foundPack->second.end());
          }
          else
          {
            elements.push_back(specialize(element, substitution));
          }
        }
        return interner_.internTuple(elements);
      }

      [[nodiscard]] auto instantiateFunction(hir::DefId original, const Substitution &substitution, size_t packCount,
                                             syntax::SourceSpan span) -> hir::DefId
      {
        const auto &source = module_->functions.at(original.value);
        const auto &signature = signatures_.at(original.value);
        if (!isGeneric(signature)) return original;
        std::string key = source.name;
        std::vector<TypeId> concreteTypes;
        bool concrete = true;
        for (size_t index = 0; index < signature.genericParameters.size(); ++index)
        {
          const TypeId parameter = signature.genericParameters[index];
          const auto found = substitution.types.find(parameter.value);
          const TypeId instantiated = found != substitution.types.end() ? found->second : parameter;
          if (interner_.descriptor(instantiated).kind == TypeKind::TypeParameter) concrete = false;
          key += "|" + interner_.display(instantiated);
          concreteTypes.push_back(instantiated);
        }
        for (size_t index = 0; index < signature.constParameters.size(); ++index)
        {
          const auto found = substitution.consts.find(static_cast<uint32_t>(index));
          if (found == substitution.consts.end())
          {
            concrete = false;
            continue;
          }
          const auto &value = interner_.constInterner().value(found->second);
          if (value.kind != const_eval::ConstValueKind::Integer)
            throw TypeError("const generic argument is not an integer", span);
          key += "|" + std::to_string(value.integerValue);
        }
        std::vector<TypeId> concreteConstructors;
        for (size_t index = 0; index < signature.constructorParameters.size(); ++index)
        {
          const TypeId parameter = signature.constructorParameters[index];
          const auto found = substitution.constructors.find(*interner_.descriptor(parameter).nominalId);
          if (found == substitution.constructors.end())
          {
            concrete = false;
            concreteConstructors.push_back(parameter);
            continue;
          }
          key += "|ctor=" + interner_.display(found->second);
          concreteConstructors.push_back(found->second);
        }
        std::vector<TypeId> packElements;
        if (!signature.packParameters.empty())
        {
          const auto packDescriptor = interner_.descriptor(signature.parameters.back());
          const auto foundPack = substitution.packs.find(packDescriptor.element.value);
          if (foundPack == substitution.packs.end()) concrete = false;
          else
          {
            key += "|pack=" + std::to_string(foundPack->second.size());
            for (const auto element : foundPack->second)
            {
              if (interner_.descriptor(element).kind == TypeKind::TypeParameter) concrete = false;
              key += "x" + interner_.display(element);
              packElements.push_back(element);
            }
          }
        }
        const bool variadicParameter = !signature.packParameters.empty();
        if (!concrete) return original;
        if (const auto existing = instanceTable_.find(key); existing != instanceTable_.end()) return existing->second;

        hir::Function clone = hir::cloneFunction(source, nextInstanceLocal_);
        const hir::DefId instanceId{static_cast<uint32_t>(module_->functions.size() + instances_.size())};
        clone.id = instanceId;
        clone.name = std::format("{}#{}", source.name, instances_.size());
        FunctionTypeIds instanceSignature;
        instanceSignature.parameters.reserve(signature.parameters.size());
        for (size_t index = 0; index < signature.parameters.size(); ++index)
        {
          if (variadicParameter && index + 1 == signature.parameters.size())
          {
            instanceSignature.parameters.push_back(interner_.internTuple(packElements));
            continue;
          }
          instanceSignature.parameters.push_back(specialize(signature.parameters[index], substitution));
        }
        instanceSignature.genericParameters = std::move(concreteTypes);
        instanceSignature.genericParameterNames = signature.genericParameterNames;
        instanceSignature.constructorParameters = concreteConstructors;
        instanceSignature.constructorParameterNames = signature.constructorParameterNames;
        instanceSignature.constParameters = signature.constParameters;
        instanceSignature.constParameterNames = signature.constParameterNames;
        instanceSignature.returnType = specializeReturnType(signature, substitution, packCount);
        FunctionType displaySignature;
        for (const auto parameter : instanceSignature.parameters) displaySignature.parameters.push_back(interner_.display(parameter));
        displaySignature.returnType = interner_.display(instanceSignature.returnType);
        signatures_.emplace(instanceId.value, std::move(instanceSignature));
        functionTypes_.emplace(instanceId.value, std::move(displaySignature));
        instanceTable_.emplace(std::move(key), instanceId);
        instances_.push_back(std::move(clone));
        const_eval::ConstBindings instanceConstBindings;
        for (size_t index = 0; index < signature.constParameters.size(); ++index)
        {
          const auto found = substitution.consts.find(static_cast<uint32_t>(index));
          if (found != substitution.consts.end())
            instanceConstBindings.emplace(signature.constParameterNames[index], found->second);
        }
        checkFunction(instances_.back(), instanceConstBindings);
        return instanceId;
      }

      void checkFunction(const hir::Function &function, const const_eval::ConstBindings &constBindings = {})
      {
        currentFunctionId_ = function.id;
        const const_eval::ConstBindings previousBindings = std::move(activeConstBindings_);
        activeConstBindings_ = constBindings;
        LocalTypes locals;
        mutableBindings_.clear();
        moveState_ = MoveState{};
        borrowState_ = BorrowState{};
        const auto &signature = signatures_.at(function.id.value);
        if (!signature.packParameters.empty()) placeholderFunctions_.insert(function.id.value);
        genericBindings_.clear();
        for (size_t index = 0; index < function.genericParameters.size(); ++index)
          genericBindings_.emplace(function.genericParameters[index], signature.genericParameters[index]);
        for (size_t index = 0; index < function.constructorParameters.size(); ++index)
          genericBindings_.emplace(function.constructorParameters[index], signature.constructorParameters[index]);
        for (size_t index = 0; index < function.variadicConstructorParameters.size(); ++index)
          genericBindings_.emplace(function.variadicConstructorParameters[index],
                                   signature.constructorParameters[function.constructorParameters.size() + index]);
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
        recordFallthroughDrops(function, locals);
        genericBindings_.clear();
        activeConstBindings_ = previousBindings;
      }

      void recordFallthroughDrops(const hir::Function &function, const LocalTypes &locals)
      {
        for (const auto &[local, type] : locals)
        {
          if (!dropTypes_.contains(type.value) || moveState_.isWholeMoved(local)) continue;
          fallthroughDrops_[function.id.value].push_back(dropEdge(local, type, function.span));
        }
      }

      void checkBlock(const hir::Block &block, LocalTypes locals, LoopTypes loops, TypeId returnType,
                      bool nestedScope = false)
      {
        std::unordered_set<uint32_t> incomingLocals;
        for (const auto &[local, type] : locals) incomingLocals.insert(local);
        const MutableBindings saved = mutableBindings_;
        const BorrowState borrowsSaved = borrowState_;
        for (const auto &statement : block.statements) checkStatement(statement, locals, loops, returnType);
        if (block.tailExpression != nullptr) static_cast<void>(infer(*block.tailExpression, locals));
        if (nestedScope)
        {
          // Block-scoped drop edges (D-015): locals declared in this block
          // drop when the scope exits unless they were wholly moved out.
          std::vector<std::pair<uint32_t, uint32_t>> drops;
          for (const auto &[local, type] : locals)
          {
            if (incomingLocals.contains(local)) continue;
            if (!dropTypes_.contains(type.value) || moveState_.isWholeMoved(local)) continue;
            drops.push_back(dropEdge(local, type, block.span));
          }
          if (!drops.empty()) blockDrops_.emplace(&block, std::move(drops));
        }
        mutableBindings_ = std::move(saved);
        borrowState_ = std::move(borrowsSaved);
      }

      void checkStatement(const hir::Statement &statement, LocalTypes &locals, LoopTypes &loops, TypeId returnType)
      {
        switch (statement.kind)
        {
        case hir::StatementKind::Let:
        {
          const TypeId bindingType = statement.bindingType != nullptr ? interner_.resolve(*statement.bindingType) : TypeId{};
          if (bindingType.value != 0 && interner_.descriptor(bindingType).kind == TypeKind::Trait)
            throw TypeError(std::format("trait `{}` is not a value type; use `ref<{}>`", interner_.display(bindingType),
                                        interner_.display(bindingType)), statement.span);
          const TypeId type = statement.bindingType != nullptr
                                  ? inferExpected(*statement.expression, bindingType, locals, "let initializer")
                                  : infer(*statement.expression, locals);
          if (interner_.descriptor(type).kind != TypeKind::TraitReference)
            trackConsumption(*statement.expression, locals);
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
            // Assignment writes revive the target place, but the assigned
            // value reads against the pre-assignment move state.
            const MoveState beforeAssignment = moveState_;
            const auto reviveTarget = [&]() {
              if (const auto root = rootLocalOf(*statement.assignmentTarget); root.has_value())
              {
                if (statement.assignmentTarget->kind == hir::ExpressionKind::Member)
                {
                  const auto &descriptor = interner_.descriptor(locals.at(root->value));
                  const auto found = std::find(descriptor.fieldNames.begin(), descriptor.fieldNames.end(),
                                               statement.assignmentTarget->text);
                  if (found != descriptor.fieldNames.end())
                    moveState_.reinitializeField(root->value,
                                                 static_cast<uint32_t>(std::distance(descriptor.fieldNames.begin(), found)));
                }
                else
                {
                  moveState_.reinitialize(root->value);
                }
              }
            };
            reviveTarget();
            requireMutableDeref(*statement.assignmentTarget, locals);
            const TypeId target = infer(*statement.assignmentTarget, locals);
            moveState_ = beforeAssignment;
            static_cast<void>(inferExpected(*statement.expression, target, locals, "assignment value"));
            trackConsumption(*statement.expression, locals);
            reviveTarget();
          }
          else
          {
            static_cast<void>(inferExpected(*statement.expression, locals.at(statement.local->value), locals, "assignment value"));
            trackConsumption(*statement.expression, locals);
            moveState_.reinitialize(statement.local->value);
          }
          return;
        case hir::StatementKind::Return:
          if (statement.expression != nullptr)
          {
            const TypeId returned = inferExpected(*statement.expression, returnType, locals, "return value");
            if (interner_.descriptor(returned).kind == TypeKind::Reference)
              throw TypeError("references cannot be returned from a function", statement.expression->span);
          }
          else requireType(returnType, builtin::Unit, statement.span, "return value");
          for (const auto &[local, type] : locals)
          {
            if (!dropTypes_.contains(type.value) || moveState_.isWholeMoved(local)) continue;
            returnDrops_[&statement].push_back(dropEdge(local, type, statement.span));
          }
          return;
        case hir::StatementKind::If:
        {
          requireType(builtin::Bool, infer(*statement.expression, locals), statement.expression->span, "if condition");
          const MoveState before = moveState_;
          const BorrowState borrowsBefore = borrowState_;
          checkBlock(*statement.consequence, locals, loops, returnType, true);
          const MoveState afterConsequence = moveState_;
          const BorrowState borrowsAfterConsequence = borrowState_;
          moveState_ = before;
          borrowState_ = borrowsBefore;
          if (statement.alternative != nullptr) checkBlock(*statement.alternative, locals, loops, returnType, true);
          moveState_ = afterConsequence.mergedWith(moveState_);
          borrowState_ = borrowsAfterConsequence.mergedWith(borrowState_);
          return;
        }
        case hir::StatementKind::ConstIf:
        {
          bool selected{};
          if (inConstGenericFunction_)
          {
            // Per-instance `const if` (D-012): conditions may reference const
            // parameters; they evaluate against the active instance bindings.
            try
            {
              const_eval::ConstEvaluator evaluator{interner_.constInterner()};
              selected = evaluator.evaluateBool(*statement.expression, [this](const hir::Expression &node) {
                if (node.kind == hir::ExpressionKind::ResolvedName && node.resolvedName.has_value() &&
                    node.resolvedName->kind == hir::ResolvedNameKind::ConstParameter)
                {
                  const auto found = activeConstBindings_.find(node.text);
                  if (found == activeConstBindings_.end())
                    throw const_eval::ConstEvalError(
                        std::format("const if condition references abstract const parameter `{}`", node.text),
                        node.span);
                  return found->second;
                }
                if (node.kind == hir::ExpressionKind::GenericApplication) return evaluateConstApplication(node);
                if (node.kind == hir::ExpressionKind::Call)
                  return interpreter_->evaluateCall(node, {}, activeConstBindings_, node.span);
                throw const_eval::ConstEvalError("const if condition is not a compile-time constant expression", node.span);
              });
            }
            catch (const const_eval::ConstEvalError &error)
            {
              // At the generic (abstract) level neither branch is checked and
              // the type-erased body is not lowerable; monomorphized instances
              // re-check with concrete bindings.
              if (activeConstBindings_.empty())
              {
                placeholderFunctions_.insert(currentFunctionId_.value);
                return;
              }
              throw TypeError(error.what(), error.span);
            }
          }
          else
          {
            requireType(builtin::Bool, infer(*statement.expression, locals), statement.expression->span, "const if condition");
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
          }
          constIfSelections_.emplace(&statement, selected);
          if (selected) checkBlock(*statement.consequence, locals, loops, returnType, true);
          else if (statement.alternative != nullptr) checkBlock(*statement.alternative, locals, loops, returnType, true);
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
          const MoveState before = moveState_;
          const BorrowState borrowsBefore = borrowState_;
          checkBlock(*statement.body, std::move(loopLocals), std::move(loopTypes), returnType, true);
          moveState_ = before.mergedWith(moveState_);
          borrowState_ = borrowsBefore.mergedWith(borrowState_);
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
        const MoveState beforeSwitch = moveState_;
        const BorrowState borrowsBeforeSwitch = borrowState_;
        MoveState merged;
        BorrowState borrowsMerged;
        bool anyBranch = false;
        const auto checkBranch = [&](const hir::Block &branch, const LocalTypes &branchLocals) {
          const MoveState beforeBranch = moveState_;
          const BorrowState borrowsBeforeBranch = borrowState_;
          checkBlock(branch, branchLocals, loops, returnType, true);
          merged = merged.mergedWith(moveState_);
          borrowsMerged = borrowsMerged.mergedWith(borrowState_);
          moveState_ = beforeBranch;
          borrowState_ = borrowsBeforeBranch;
          anyBranch = true;
        };
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
            checkBranch(*switchCase.body, locals);
            continue;
          }
          if (!descriptor.variantHasPayload[variant])
            throw TypeError(std::format("variant `{}` has no payload to bind", switchCase.variantName), switchCase.span);
          LocalTypes caseLocals = locals;
          const TypeId payloadType = descriptor.elements[variant];
          if (!switchCase.bindings.empty())
          {
            const auto &payloadDescriptor = interner_.descriptor(payloadType);
            if (payloadDescriptor.kind != TypeKind::Tuple ||
                payloadDescriptor.elements.size() != switchCase.bindings.size() + 1)
              throw TypeError(std::format("variant `{}` payload bindings do not match its tuple payload", switchCase.variantName),
                              switchCase.span);
            caseLocals.emplace(switchCase.binding->value, payloadDescriptor.elements.front());
            recordLocal(*switchCase.binding, payloadDescriptor.elements.front());
            for (size_t index = 0; index < switchCase.bindings.size(); ++index)
            {
              caseLocals.emplace(switchCase.bindings[index].value, payloadDescriptor.elements[index + 1]);
              recordLocal(switchCase.bindings[index], payloadDescriptor.elements[index + 1]);
            }
          }
          else
          {
            caseLocals.emplace(switchCase.binding->value, payloadType);
            recordLocal(*switchCase.binding, payloadType);
          }
          checkBranch(*switchCase.body, caseLocals);
        }
        if (statement.alternative != nullptr) checkBranch(*statement.alternative, locals);
        if (anyBranch)
        {
          moveState_ = beforeSwitch.mergedWith(merged);
          borrowState_ = borrowsBeforeSwitch.mergedWith(borrowsMerged);
        }
        if (statement.alternative == nullptr)
        {
          if (const auto missing = std::find(covered.begin(), covered.end(), false); missing != covered.end())
          {
            const size_t variant = static_cast<size_t>(std::distance(covered.begin(), missing));
            throw TypeError(std::format("switch is not exhaustive: missing variant `{}`", descriptor.fieldNames[variant]),
                            statement.span);
          }
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
        case TypeKind::Opaque:
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

      /// Evaluates the built-in tuple/pack introspection const predicates
      /// `is_tuple<T>`, `tuple_size<T>`, and `sizeof_pack<T...>`. Returns
      /// nullopt for any other name so declared const specializations stay
      /// authoritative.
      [[nodiscard]] auto evaluateTupleIntrospection(std::string_view name, const std::vector<TypeId> &arguments,
                                                    syntax::SourceSpan span) -> std::optional<const_eval::ConstValueId>
      {
        if (name == "is_tuple")
        {
          if (arguments.size() != 1) throw TypeError("is_tuple<T> expects exactly 1 type argument", span);
          return interner_.constInterner().internBool(interner_.descriptor(arguments[0]).kind == TypeKind::Tuple);
        }
        if (name == "tuple_size")
        {
          if (arguments.size() != 1) throw TypeError("tuple_size<T> expects exactly 1 type argument", span);
          const auto &descriptor = interner_.descriptor(arguments[0]);
          if (descriptor.kind != TypeKind::Tuple)
            throw TypeError(std::format("tuple_size<T> expects a tuple type, got {}", interner_.display(arguments[0])),
                            span);
          return interner_.constInterner().internInteger(static_cast<int64_t>(descriptor.elements.size()));
        }
        if (name == "sizeof_pack")
        {
          if (arguments.empty()) throw TypeError("sizeof_pack requires at least one type argument", span);
          return interner_.constInterner().internInteger(static_cast<int64_t>(arguments.size()));
        }
        return std::nullopt;
      }

      /// Evaluates the built-in trait/abstractness const predicates
      /// `is_trait<T>` and `is_abstract<T>`. Trait names are looked up
      /// textually (traits are not first-class types); other names resolve
      /// through the interner. Returns nullopt for any other name.
      [[nodiscard]] auto evaluateTraitIntrospection(std::string_view name,
                                                    const std::vector<hir::TypeArgument> &arguments,
                                                    syntax::SourceSpan span) -> std::optional<const_eval::ConstValueId>
      {
        if (name != "is_trait" && name != "is_abstract") return std::nullopt;
        if (arguments.size() != 1 || arguments[0].type == nullptr)
          throw TypeError(std::format("{}<T> expects exactly 1 type argument", name), span);
        const auto &type = *arguments[0].type;
        const bool isTrait = type.kind == hir::TypeKind::Named && traits_.contains(type.name);
        if (isTrait) return interner_.constInterner().internBool(true);
        const TypeId resolved = interner_.resolveInScope(type, genericBindings_);
        const auto &descriptor = interner_.descriptor(resolved);
        if (descriptor.kind == TypeKind::TypeParameter)
          throw TypeError(std::format("cannot evaluate const declaration `{}` for abstract type parameter `{}`",
                                      name, interner_.display(resolved)), span);
        if (name == "is_trait") return interner_.constInterner().internBool(false);
        return interner_.constInterner().internBool(descriptor.kind == TypeKind::Opaque && descriptor.abstractType);
      }

      /// Evaluates a const predicate application (`name<types>`). Used by the
      /// `const if` extension and, later, by where clauses.
      [[nodiscard]] auto evaluateConstApplication(const hir::Expression &expression) -> const_eval::ConstValueId
      {
        if (const auto builtin = evaluateTraitIntrospection(expression.text, expression.genericArguments, expression.span);
            builtin.has_value())
          return *builtin;
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
        if (const auto builtin = evaluateTupleIntrospection(expression.text, typeArguments, expression.span);
            builtin.has_value())
          return *builtin;
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

      /// Collects the static spread structure of a call argument list:
      /// returns the number of expanded arguments and records, per spread
      /// argument, its positional index for lowering.
      struct CallSpreadInfo
      {
        size_t expandedCount{};
        std::vector<size_t> spreadPositions;
        std::vector<TypeId> expandedTypes;
        std::vector<bool> spreadSlot;
        std::vector<const hir::Expression *> argumentExpressions;
      };

      [[nodiscard]] auto analyzeCallSpread(const hir::Expression &expression, size_t firstArgument,
                                           const LocalTypes &locals, CallSpreadInfo &info) -> bool
      {
        bool anySpread = false;
        for (size_t index = firstArgument; index < expression.operands.size(); ++index)
        {
          const auto &argument = *expression.operands[index];
          if (argument.kind == hir::ExpressionKind::Prefix && argument.text == "...")
          {
            const TypeId operand = infer(*argument.operands[0], locals);
            const auto &descriptor = interner_.descriptor(operand);
            if (descriptor.kind != TypeKind::Tuple)
              throw TypeError(std::format("cannot spread value of type {}", interner_.display(operand)), argument.span);
            info.spreadPositions.push_back(index - firstArgument);
            info.expandedTypes.insert(info.expandedTypes.end(), descriptor.elements.begin(), descriptor.elements.end());
            for (const auto element : descriptor.elements)
            {
              static_cast<void>(element);
              info.spreadSlot.push_back(true);
              info.argumentExpressions.push_back(argument.operands[0].get());
            }
            anySpread = true;
          }
          else
          {
            info.expandedTypes.push_back(infer(argument, locals));
            info.spreadSlot.push_back(false);
            info.argumentExpressions.push_back(&argument);
          }
        }
        info.expandedCount = info.expandedTypes.size();
        return anySpread;
      }

      [[nodiscard]] auto isAffine(TypeId type) const -> bool
      {
        const auto &descriptor = interner_.descriptor(type);
        if (descriptor.kind == TypeKind::Struct || descriptor.kind == TypeKind::Enum) return true;
        if (descriptor.kind == TypeKind::Tuple)
          return std::any_of(descriptor.elements.begin(), descriptor.elements.end(),
                             [this](TypeId element) { return isAffine(element); });
        return false;
      }

      [[nodiscard]] auto rootLocalOf(const hir::Expression &expression) const -> std::optional<hir::LocalId>
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
        if (root->kind == hir::ExpressionKind::ResolvedName && root->resolvedName.has_value() &&
            root->resolvedName->kind == hir::ResolvedNameKind::Local)
          return hir::LocalId{root->resolvedName->id};
        return std::nullopt;
      }

      /// Records a consuming use: affine bindings and explicitly moved
      /// bindings are marked moved (whole or by field).
      void trackConsumption(const hir::Expression &expression, const LocalTypes &locals)
      {
        if (expression.kind == hir::ExpressionKind::Prefix && expression.text == "clone") return;
        if (expression.kind == hir::ExpressionKind::Prefix && expression.text == "move")
        {
          // Explicit moves invalidate the source place regardless of whether
          // its type is affine (legacy #24 semantics). Moving a struct/enum
          // field marks the field, keeping the rest of the value usable and
          // letting Drop edges validate partial moves field by field.
          const auto root = rootLocalOf(*expression.operands[0]);
          if (!root.has_value()) return;
          const auto &operand = *expression.operands[0];
          if (operand.kind == hir::ExpressionKind::Member)
          {
            const auto &descriptor = interner_.descriptor(locals.at(root->value));
            if ((descriptor.kind == TypeKind::Struct || descriptor.kind == TypeKind::Enum) &&
                !descriptor.fieldNames.empty())
            {
              const auto found = std::find(descriptor.fieldNames.begin(), descriptor.fieldNames.end(), operand.text);
              if (found != descriptor.fieldNames.end())
              {
                moveState_.markField(root->value,
                                     static_cast<uint32_t>(std::distance(descriptor.fieldNames.begin(), found)));
                return;
              }
            }
          }
          moveState_.markWhole(root->value);
          return;
        }
        if (expression.kind == hir::ExpressionKind::Grouped)
        {
          trackConsumption(*expression.operands[0], locals);
          return;
        }
        if (expression.kind == hir::ExpressionKind::Member)
        {
          const auto root = rootLocalOf(expression);
          if (!root.has_value()) return;
          const auto fieldType = locals.at(root->value);
          if (!isAffine(fieldType)) return;
          const auto &descriptor = interner_.descriptor(fieldType);
          const auto found = std::find(descriptor.fieldNames.begin(), descriptor.fieldNames.end(), expression.text);
          if (found == descriptor.fieldNames.end()) return;
          const uint32_t field = static_cast<uint32_t>(std::distance(descriptor.fieldNames.begin(), found));
          if (moveState_.hasMovedField(root->value))
            throw TypeError(std::format("use of partially moved value `{}`", expression.operands[0]->text), expression.span);
          moveState_.markField(root->value, field);
          return;
        }
        if (expression.kind == hir::ExpressionKind::ResolvedName && expression.resolvedName->kind == hir::ResolvedNameKind::Local)
        {
          const uint32_t local = expression.resolvedName->id;
          if (!isAffine(locals.at(local))) return;
          if (moveState_.hasMovedField(local))
            throw TypeError(std::format("use of partially moved value `{}`", expression.text), expression.span);
          moveState_.markWhole(local);
        }
      }

      /// Builds a substitution from explicit generic arguments written on a
      /// call expression (`name<types>(...)`), filling declared parameters in
      /// declaration order: type parameters first, then const parameters.
      [[nodiscard]] auto explicitSubstitution(const hir::Expression &expression, const FunctionTypeIds &signature) -> Substitution
      {
        Substitution substitution;
        const size_t expected = signature.explicitParameterOrder.size();
        if (expression.genericArguments.size() != expected)
          throw TypeError(std::format("generic argument count mismatch: expected {}, got {}", expected,
                                      expression.genericArguments.size()), expression.span);
        size_t typeIndex{};
        size_t constructorIndex{};
        size_t constIndex{};
        for (size_t index = 0; index < expression.genericArguments.size(); ++index)
        {
          const auto &argument = expression.genericArguments[index];
          switch (signature.explicitParameterOrder[index])
          {
          case syntax::GenericParameterKind::Type:
          {
            if (argument.kind != syntax::GenericArgumentKind::Type)
              throw TypeError(std::format("generic argument {} must be a type", index + 1), argument.span);
            const TypeId resolved = interner_.resolveInScope(*argument.type, genericBindings_);
            const auto &descriptor = interner_.descriptor(resolved);
            // Kind check: a bare generic struct/enum template is a type
            // constructor, not a type, and cannot bind a type parameter.
            if (descriptor.typeArguments.empty() && descriptor.nominalId.has_value() &&
                ((descriptor.kind == TypeKind::Struct &&
                  interner_.structGenericArity(hir::StructId{*descriptor.nominalId}) != 0) ||
                 (descriptor.kind == TypeKind::Enum &&
                  interner_.enumGenericArity(hir::EnumId{*descriptor.nominalId}) != 0)))
              throw TypeError(std::format("generic argument {} is a type constructor, not a type", index + 1),
                              argument.span);
            substitution.types.emplace(signature.genericParameters[typeIndex].value, resolved);
            ++typeIndex;
            break;
          }
          case syntax::GenericParameterKind::TypeConstructor:
          case syntax::GenericParameterKind::VariadicTypeConstructor:
          {
            if (argument.kind != syntax::GenericArgumentKind::Type)
              throw TypeError(std::format("generic argument {} must be a type constructor", index + 1), argument.span);
            const TypeId templateType = interner_.templateForName(argument.type->name, argument.span);
            const auto &descriptor = interner_.descriptor(templateType);
            if ((descriptor.kind != TypeKind::Struct && descriptor.kind != TypeKind::Opaque) ||
                !descriptor.nominalId.has_value())
              throw TypeError(std::format("generic argument {} must be a struct or opaque type constructor", index + 1),
                              argument.span);
            substitution.constructors.emplace(*interner_.descriptor(signature.constructorParameters[constructorIndex]).nominalId,
                                             templateType);
            ++constructorIndex;
            break;
          }
          case syntax::GenericParameterKind::Const:
          {
            if (argument.kind != syntax::GenericArgumentKind::ConstExpr)
              throw TypeError(std::format("generic argument {} must be a const expression", index + 1), argument.span);
            const const_eval::ConstValueId value =
                const_eval::ConstEvaluator{interner_.constInterner()}.evaluate(*argument.constExpr, {});
            substitution.consts.emplace(static_cast<uint32_t>(constIndex), value);
            ++constIndex;
            break;
          }
          case syntax::GenericParameterKind::Pack:
            break;
          }
        }
        return substitution;
      }

      /// Detects a fold call (`f(acc, xs...)` / `f(xs..., acc)`): exactly one
      /// argument is an array spread and exactly one other argument is the
      /// accumulator. Returns spread/accumulator positions and the element type.
      struct FoldInfo
      {
        bool fold{};
        size_t spreadPosition{};
        size_t accumulatorPosition{};
        TypeId elementType{};
      };

      [[nodiscard]] auto analyzeFoldCall(const hir::Expression &expression, const LocalTypes &locals) -> FoldInfo
      {
        FoldInfo info;
        if (expression.operands.size() < 2) return info;
        for (size_t index = 1; index < expression.operands.size(); ++index)
        {
          const auto &argument = *expression.operands[index];
          if (argument.kind != hir::ExpressionKind::Prefix || argument.text != "...") continue;
          const TypeId operand = infer(*argument.operands[0], locals);
          const auto &descriptor = interner_.descriptor(operand);
          if (descriptor.kind != TypeKind::DynamicArray && descriptor.kind != TypeKind::FixedArray &&
              descriptor.kind != TypeKind::DependentArray && descriptor.kind != TypeKind::Range)
            continue;
          if (descriptor.kind == TypeKind::Range && descriptor.element != builtin::I64)
            throw TypeError("fold over ranges currently requires i64 elements", argument.span);
          info.fold = true;
          info.spreadPosition = index - 1;
          info.elementType = descriptor.element;
          break;
        }
        if (info.fold && expression.operands.size() == 3) info.accumulatorPosition = 1 - info.spreadPosition;
        return info;
      }

      /// Resolves a numeric literal suffix (`i8`, `u8`, `f32`, ...) to its
      /// builtin type; empty suffixes default to i64 / f64 at the call sites.
      [[nodiscard]] auto typeFromNumericSuffix(std::string_view suffix) const -> TypeId
      {
        if (suffix == "i8") return builtin::I8;
        if (suffix == "i16") return builtin::I16;
        if (suffix == "i32") return builtin::I32;
        if (suffix == "i64") return builtin::I64;
        if (suffix == "u8") return builtin::U8;
        if (suffix == "u16") return builtin::U16;
        if (suffix == "u32") return builtin::U32;
        if (suffix == "u64") return builtin::U64;
        if (suffix == "f32") return builtin::F32;
        if (suffix == "f64") return builtin::F64;
        return TypeId{};
      }

      /// D-008 range check for an integer literal value against a fixed-width
      /// integer builtin type.
      void checkIntegerLiteral(int64_t value, std::string_view text, TypeId type, syntax::SourceSpan span) const
      {
        int64_t minimum{};
        uint64_t maximum{};
        switch (type.value)
        {
        case builtin::I8.value: minimum = std::numeric_limits<int8_t>::min(); maximum = std::numeric_limits<int8_t>::max(); break;
        case builtin::I16.value: minimum = std::numeric_limits<int16_t>::min(); maximum = std::numeric_limits<int16_t>::max(); break;
        case builtin::I32.value: minimum = std::numeric_limits<int32_t>::min(); maximum = std::numeric_limits<int32_t>::max(); break;
        case builtin::I64.value: minimum = std::numeric_limits<int64_t>::min(); maximum = std::numeric_limits<int64_t>::max(); break;
        case builtin::U8.value: minimum = 0; maximum = std::numeric_limits<uint8_t>::max(); break;
        case builtin::U16.value: minimum = 0; maximum = std::numeric_limits<uint16_t>::max(); break;
        case builtin::U32.value: minimum = 0; maximum = std::numeric_limits<uint32_t>::max(); break;
        case builtin::U64.value: minimum = 0; maximum = std::numeric_limits<uint64_t>::max(); break;
        default: break;
        }
        if (value < minimum || (value >= 0 && static_cast<uint64_t>(value) > maximum))
          throw TypeError(std::format("integer literal `{}` is out of range for type {}", text, interner_.display(type)),
                          span);
      }

      /// True when a type mentions the given Self type parameter anywhere.
      [[nodiscard]] auto typeContains(TypeId type, TypeId needle) const -> bool
      {
        if (type == needle) return true;
        const auto &descriptor = interner_.descriptor(type);
        if (descriptor.kind == TypeKind::Reference || descriptor.kind == TypeKind::RawPointer ||
            descriptor.kind == TypeKind::DynamicArray || descriptor.kind == TypeKind::FixedArray ||
            descriptor.kind == TypeKind::DependentArray || descriptor.kind == TypeKind::TypeApplication ||
            descriptor.kind == TypeKind::TypePack || descriptor.kind == TypeKind::Range)
          return typeContains(descriptor.element, needle);
        for (const auto element : descriptor.elements)
          if (typeContains(element, needle)) return true;
        return false;
      }

      /// Builds the dispatch table for (trait, concrete type) if missing:
      /// declaration-order entries from the concrete impl, falling back to
      /// trait default methods. Returns false when no impl covers the trait.
      [[nodiscard]] auto ensureTraitViewTable(const std::string &traitName, TypeId concreteType) -> bool
      {
        if (traitViewTables_.contains(traitName) && traitViewTables_.at(traitName).contains(concreteType.value))
          return true;
        const auto &trait = traits_.at(traitName);
        for (const auto &impl : impls_)
        {
          if (impl.target != concreteType) continue;
          std::vector<const TraitInfo *> closure;
          std::unordered_set<std::string> seen;
          collectSupertraits(impl.traitName, closure, seen);
          const bool covers = std::any_of(closure.begin(), closure.end(),
                                          [&](const TraitInfo *candidate) { return candidate->name == traitName; });
          if (!covers) continue;
          std::vector<hir::DefId> table;
          for (const auto &methodName : trait.methodOrder)
          {
            if (const auto found = impl.methods.find(methodName); found != impl.methods.end())
              table.push_back(found->second);
            else if (const auto fallback = trait.methodDefaults.find(methodName); fallback != trait.methodDefaults.end())
              table.push_back(fallback->second);
            else
              return false;
          }
          traitViewTables_[traitName].emplace(concreteType.value, std::move(table));
          return true;
        }
        return false;
      }

      [[nodiscard]] auto inferExpected(const hir::Expression &expression, TypeId expected, const LocalTypes &locals,
                                       std::string_view context) -> TypeId
      {
        const auto &descriptor = interner_.descriptor(expected);
        if (descriptor.kind != TypeKind::Union && expression.kind == hir::ExpressionKind::ResolvedName)
        {
          const TypeId valueType = infer(expression, locals);
          if (interner_.descriptor(valueType).kind == TypeKind::Union &&
              std::find(interner_.descriptor(valueType).elements.begin(), interner_.descriptor(valueType).elements.end(),
                        expected) != interner_.descriptor(valueType).elements.end())
          {
            record(expression, expected);
            return expected;
          }
        }
        if (expression.kind == hir::ExpressionKind::IntegerLiteral && isIntegerBuiltin(expected))
        {
          // D-008: integer literal text is preserved exactly until contextual
          // type selection; adopt the expected integer type with a range check.
          if (!expression.numericSuffix.empty())
          {
            const TypeId suffixType = typeFromNumericSuffix(expression.numericSuffix);
            if (suffixType != expected)
              throw TypeError(std::format("integer literal suffix `{}` conflicts with expected type {}", expression.numericSuffix,
                                          interner_.display(expected)), expression.span);
          }
          checkIntegerLiteral(std::stoll(expression.text), expression.text, expected, expression.span);
          record(expression, expected);
          return expected;
        }
        if (expression.kind == hir::ExpressionKind::FloatLiteral && isFloatBuiltin(expected))
        {
          if (!expression.numericSuffix.empty() && typeFromNumericSuffix(expression.numericSuffix) != expected)
            throw TypeError(std::format("float literal suffix `{}` conflicts with expected type {}", expression.numericSuffix,
                                        interner_.display(expected)), expression.span);
          record(expression, expected);
          return expected;
        }
        if (expression.kind == hir::ExpressionKind::Prefix &&
            (expression.text == "-" || expression.text == "+") && isIntegerBuiltin(expected) &&
            expression.operands[0]->kind == hir::ExpressionKind::IntegerLiteral)
        {
          const std::string text = std::format("{}{}", expression.text, expression.operands[0]->text);
          static_cast<void>(inferExpected(*expression.operands[0], expected, locals, "integer literal"));
          const int64_t value = expression.text == "-" ? -std::stoll(expression.operands[0]->text)
                                                        : std::stoll(expression.operands[0]->text);
          checkIntegerLiteral(value, text, expected, expression.span);
          record(expression, expected);
          return expected;
        }
        if (expression.kind == hir::ExpressionKind::Prefix &&
            (expression.text == "-" || expression.text == "+") && isFloatBuiltin(expected) &&
            expression.operands[0]->kind == hir::ExpressionKind::FloatLiteral)
        {
          static_cast<void>(inferExpected(*expression.operands[0], expected, locals, "float literal"));
          record(expression, expected);
          return expected;
        }
        if (descriptor.kind == TypeKind::Union)
        {
          // Union coercion (legacy 19): the value's type must be one of the
          // members; the runtime representation stays the plain member value.
          const TypeId valueType = infer(expression, locals);
          if (valueType == expected)
          {
            record(expression, expected);
            return expected;
          }
          const auto member = std::find(descriptor.elements.begin(), descriptor.elements.end(), valueType);
          if (member == descriptor.elements.end())
            throw TypeError(std::format("value of type {} is not a member of union {}", interner_.display(valueType),
                                        interner_.display(expected)), expression.span);
          record(expression, expected);
          return expected;
        }
        if (descriptor.kind == TypeKind::TraitReference)
        {
          const std::string traitName = descriptor.name;
          const TypeId valueType = infer(expression, locals);
          TypeId concrete = valueType;
          if (interner_.descriptor(valueType).kind == TypeKind::TraitReference)
          {
            requireType(expected, valueType, expression.span, context);
            record(expression, expected);
            return expected;
          }
          if (interner_.descriptor(valueType).kind == TypeKind::Reference)
            concrete = interner_.descriptor(valueType).element;
          if (interner_.descriptor(concrete).kind == TypeKind::TraitReference ||
              interner_.descriptor(concrete).kind == TypeKind::Trait)
            throw TypeError(std::format("cannot build a `{}` view from {}", interner_.display(expected),
                                        interner_.display(valueType)), expression.span);
          if (!hasImpl(traitName, concrete) || !ensureTraitViewTable(traitName, concrete))
            throw TypeError(std::format("value of type {} does not implement trait `{}`", interner_.display(concrete),
                                        traitName), expression.span);
          traitViewCoercions_.insert_or_assign(&expression, std::pair<std::string, TypeId>{traitName, concrete});
          record(expression, expected);
          return expected;
        }
        if (expression.kind == hir::ExpressionKind::Binary && expression.text == ".." &&
            descriptor.kind == TypeKind::Range)
        {
          static_cast<void>(inferExpected(*expression.operands[0], descriptor.element, locals, "range start"));
          static_cast<void>(inferExpected(*expression.operands[1], descriptor.element, locals, "range end"));
          record(expression, expected);
          return expected;
        }
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
          {
            const FoldInfo fold = analyzeFoldCall(expression, locals);
            if (fold.fold)
            {
              if (expression.operands.size() != 3)
                throw TypeError("fold calls require exactly one accumulator argument", expression.span);
              if (signature.parameters.size() != 2)
                throw TypeError("fold function must take exactly two arguments", expression.span);
              Substitution substitution;
              unify(signature.parameters[fold.spreadPosition], fold.elementType, substitution, expression.span);
              const TypeId accumulatorType = infer(*expression.operands[fold.accumulatorPosition + 1], locals);
              unify(signature.parameters[fold.accumulatorPosition], accumulatorType, substitution, expression.span);
              const TypeId result = specializeReturnType(signature, substitution, 0);
              unify(signature.parameters[fold.accumulatorPosition], result, substitution, expression.span);
              requireConstArguments(signature, substitution, expression.operands[0]->text, expression.span);
              requireType(expected, result, expression.span, context);
              callFoldSpreadPositions_.insert_or_assign(&expression, std::vector<size_t>{fold.spreadPosition});
              callFoldAccumulatorPositions_.insert_or_assign(&expression, std::vector<size_t>{fold.accumulatorPosition});
              callTargets_.insert_or_assign(&expression, selected);
              record(expression, result);
              return result;
            }
          }
          if (isGeneric(signature))
          {
            Substitution substitution;
            const size_t supplied = expression.operands.size() - 1;
            const bool variadic = !signature.packParameters.empty();
            const size_t fixedParameters = signature.parameters.size() - (variadic ? 1 : 0);
            if (variadic ? supplied < fixedParameters : supplied != signature.parameters.size())
              throw TypeError(std::format("call argument count mismatch: expected {}, got {}", signature.parameters.size(), supplied), expression.span);
            const size_t packCount = variadic ? supplied - fixedParameters : 0;
            if (!expression.genericArguments.empty())
            {
              substitution = explicitSubstitution(expression, signature);
              for (size_t index = 0; index < supplied; ++index)
              {
                const TypeId parameterType = index < fixedParameters ? signature.parameters[index]
                                                                     : interner_.descriptor(signature.parameters.back()).element;
                static_cast<void>(inferExpected(*expression.operands[index + 1],
                                                specialize(parameterType, substitution), locals,
                                                std::format("call argument {}", index + 1)));
              }
            }
            else
            {
              unify(signature.returnType, expected, substitution, expression.span);
              for (size_t index = 0; index < supplied; ++index)
              {
                const auto &argument = *expression.operands[index + 1];
                const TypeId parameterType = index < fixedParameters ? signature.parameters[index]
                                                                     : interner_.descriptor(signature.parameters.back()).element;
                if (variadic && index >= fixedParameters)
                {
                  const TypeId argumentType = infer(argument, locals);
                  auto &pack = substitution.packs[parameterType.value];
                  const size_t packPosition = index - fixedParameters;
                  if (packPosition < pack.size())
                  {
                    if (pack[packPosition] != argumentType)
                      throw TypeError(std::format("generic argument type mismatch: expected {}, got {}", interner_.display(pack[packPosition]),
                                                  interner_.display(argumentType)), argument.span);
                  }
                  else
                  {
                    pack.push_back(argumentType);
                  }
                  continue;
                }
                const auto &parameterDescriptor = interner_.descriptor(parameterType);
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
                  unify(parameterType, infer(argument, locals), substitution, argument.span);
                }
              }
            }
            const TypeId specialized = specializeReturnType(signature, substitution, packCount);
            requireConstArguments(signature, substitution, expression.operands[0]->text, expression.span);
            if (module_ != nullptr && selected.value < module_->functions.size() &&
                module_->functions.at(selected.value).whereClause != nullptr &&
                !evaluateWhereCondition(*module_->functions.at(selected.value).whereClause, substitution, signature))
              throw TypeError(std::format("call to `{}` does not satisfy its where clause", module_->functions.at(selected.value).name),
                              expression.span);
            for (size_t index = 0; index < supplied; ++index)
            {
              const TypeId parameterType = index < fixedParameters ? signature.parameters[index]
                                                                   : interner_.descriptor(signature.parameters.back()).element;
              const auto &parameterDescriptor = interner_.descriptor(parameterType);
              if (parameterDescriptor.kind != TypeKind::Reference && parameterDescriptor.kind != TypeKind::RawPointer)
                trackConsumption(*expression.operands[index + 1], locals);
            }
            if (variadic)
            {
              callPackArgCounts_.insert_or_assign(&expression, packCount);
              std::vector<TypeId> packedTypes;
              for (size_t index = fixedParameters; index < supplied; ++index)
                packedTypes.push_back(infer(*expression.operands[index + 1], locals));
              callPackTupleTypes_.insert_or_assign(&expression, interner_.internTuple(std::move(packedTypes)));
            }
            selected = instantiateFunction(selected, substitution, packCount, expression.span);
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
          size_t expectedIndex = 0;
          for (const auto &element : expression.operands)
          {
            if (element->kind == hir::ExpressionKind::Prefix && element->text == "...")
            {
              const TypeId operand = infer(*element->operands[0], locals);
              const auto &operandDescriptor = interner_.descriptor(operand);
              if (expectedIndex < descriptor.elements.size() &&
                  interner_.descriptor(descriptor.elements[expectedIndex]).kind == TypeKind::TypePack)
              {
                // Declaration-side pack marker: the spread consumes it.
                ++expectedIndex;
                continue;
              }
              if (operandDescriptor.kind == TypeKind::TypePack)
              {
                expectedIndex = descriptor.elements.size();
                continue;
              }
              if (operandDescriptor.kind != TypeKind::Tuple)
                throw TypeError(std::format("cannot spread value of type {}", interner_.display(operand)), element->span);
              if (expectedIndex + operandDescriptor.elements.size() > descriptor.elements.size())
                throw TypeError("tuple splice exceeds the expected tuple length", element->span);
              for (size_t index = 0; index < operandDescriptor.elements.size(); ++index)
                requireType(descriptor.elements[expectedIndex + index], operandDescriptor.elements[index], element->span,
                            "tuple splice element");
              expectedIndex += operandDescriptor.elements.size();
              continue;
            }
            if (expectedIndex >= descriptor.elements.size())
              throw TypeError(std::format("tuple length mismatch: expected {}, got {}", descriptor.elements.size(),
                                          expression.operands.size()), expression.span);
            static_cast<void>(inferExpected(*element, descriptor.elements[expectedIndex++], locals,
                                            std::format("tuple element {}", expectedIndex)));
          }
          if (expectedIndex != descriptor.elements.size())
            throw TypeError(std::format("tuple length mismatch: expected {}, got {}", descriptor.elements.size(),
                                        expectedIndex), expression.span);
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
        if (descriptor.variantHasPayload[variant] &&
            interner_.descriptor(descriptor.elements[variant]).kind == TypeKind::Tuple)
        {
          const auto &tuple = interner_.descriptor(descriptor.elements[variant]);
          if (expression.operands.size() != tuple.elements.size())
            throw TypeError(std::format("enum variant `{}` expects {} payload values, got {}",
                                        descriptor.fieldNames[variant], tuple.elements.size(), expression.operands.size()),
                            expression.span);
          for (size_t index = 0; index < expression.operands.size(); ++index)
            static_cast<void>(inferExpected(*expression.operands[index], tuple.elements[index], locals, "variant payload"));
          record(expression, expected);
          return expected;
        }
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
          const TypeId fieldType = inferExpected(*expression.operands[index], descriptor.elements[field], locals,
                                                  std::format("field `{}`", expression.memberNames[index]));
          if (interner_.descriptor(fieldType).kind == TypeKind::Reference)
            throw TypeError("references cannot be stored in struct fields", expression.operands[index]->span);
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
        case hir::ExpressionKind::IntegerLiteral:
          type = expression.numericSuffix.empty() ? builtin::I64 : typeFromNumericSuffix(expression.numericSuffix);
          if (isIntegerBuiltin(type)) checkIntegerLiteral(std::stoll(expression.text), expression.text, type, expression.span);
          break;
        case hir::ExpressionKind::FloatLiteral:
          type = expression.numericSuffix.empty() ? builtin::F64 : typeFromNumericSuffix(expression.numericSuffix);
          static_cast<void>(std::stod(expression.text));
          break;
        case hir::ExpressionKind::StringLiteral: type = builtin::String; break;
        case hir::ExpressionKind::BooleanLiteral: type = builtin::Bool; break;
        case hir::ExpressionKind::ArrayLiteral:
        {
          if (expression.operands.empty()) throw TypeError("cannot infer the type of an empty array literal", expression.span);
          size_t mapSpreads = 0;
          size_t valueSpreads = 0;
          TypeId element{};
          for (const auto &candidate : expression.operands)
          {
            if (candidate->kind == hir::ExpressionKind::Prefix && candidate->text == "...")
            {
              bool filterMode = false;
              const hir::Expression *inner = candidate->operands[0].get();
              if (inner->kind == hir::ExpressionKind::Prefix && inner->text == "?")
              {
                filterMode = true;
                inner = inner->operands[0].get();
              }
              const bool mapSpread = inner->kind == hir::ExpressionKind::Call && !inner->operands.empty() &&
                                     inner->operands[0]->resolvedName.has_value() &&
                                     inner->operands[0]->resolvedName->kind == hir::ResolvedNameKind::Function;
              if (!mapSpread)
              {
                // Value spread: `[...array]` / `[...(1..5)]` splices the source's
                // elements into the literal.
                ++valueSpreads;
                if (filterMode)
                  throw TypeError("the filter marker `?` requires a map spread", candidate->span);
                const TypeId source = infer(*inner, locals);
                const auto &sourceDescriptor = interner_.descriptor(source);
                const bool arraySource = sourceDescriptor.kind == TypeKind::DynamicArray ||
                                         sourceDescriptor.kind == TypeKind::FixedArray ||
                                         sourceDescriptor.kind == TypeKind::DependentArray;
                const bool rangeSource = sourceDescriptor.kind == TypeKind::Range;
                if (!arraySource && !rangeSource)
                  throw TypeError(std::format("cannot spread value of type {}", interner_.display(source)),
                                  candidate->span);
                if (rangeSource && sourceDescriptor.element != builtin::I64)
                  throw TypeError("spreading ranges currently requires i64 elements", candidate->span);
                const TypeId sourceElement = sourceDescriptor.element;
                if (element.value == 0) element = sourceElement;
                else requireType(element, sourceElement, candidate->span, "array spread");
                continue;
              }
              ++mapSpreads;
              if (inner->operands.size() != 2)
                throw TypeError("map spread function must take exactly one argument", candidate->span);
              const TypeId source = infer(*inner->operands[1], locals);
              const auto &sourceDescriptor = interner_.descriptor(source);
              const bool arraySource = sourceDescriptor.kind == TypeKind::DynamicArray ||
                                       sourceDescriptor.kind == TypeKind::FixedArray ||
                                       sourceDescriptor.kind == TypeKind::DependentArray;
              const bool rangeSource = sourceDescriptor.kind == TypeKind::Range;
              if (!arraySource && !rangeSource)
                throw TypeError(std::format("map spread source must be an array or range, got {}", interner_.display(source)),
                                inner->operands[1]->span);
              if (rangeSource && sourceDescriptor.element != builtin::I64)
                throw TypeError("map spread over ranges currently requires i64 elements", inner->operands[1]->span);
              const TypeId sourceElement = sourceDescriptor.element;
              const auto &innerSignature = signatures_.at(inner->operands[0]->resolvedName->id);
              if (innerSignature.parameters.size() != 1)
                throw TypeError("map spread function must take exactly one argument", candidate->span);
              Substitution mapSubstitution;
              unify(innerSignature.parameters.front(), sourceElement, mapSubstitution, inner->operands[1]->span);
              requireConstArguments(innerSignature, mapSubstitution, inner->operands[0]->text, inner->span);
              if (module_ != nullptr && inner->operands[0]->resolvedName->id < module_->functions.size() &&
                  module_->functions.at(inner->operands[0]->resolvedName->id).whereClause != nullptr &&
                  !evaluateWhereCondition(*module_->functions.at(inner->operands[0]->resolvedName->id).whereClause,
                                          mapSubstitution, innerSignature))
                throw TypeError(std::format("call to `{}` does not satisfy its where clause",
                                            module_->functions.at(inner->operands[0]->resolvedName->id).name), inner->span);
              const TypeId mapped = specializeReturnType(innerSignature, mapSubstitution, 0);
              if (filterMode) requireType(builtin::Bool, mapped, candidate->span, "filter predicate result");
              callTargets_.insert_or_assign(inner, hir::DefId{inner->operands[0]->resolvedName->id});
              record(*inner, mapped);
              if (mapSpreads == 1) element = filterMode ? sourceElement : mapped;
              else requireType(element, filterMode ? sourceElement : mapped, candidate->span, "map spread result");
              continue;
            }
            const TypeId candidateType = infer(*candidate, locals);
            if (interner_.descriptor(candidateType).kind == TypeKind::Reference)
              throw TypeError("references cannot be stored in arrays", candidate->span);
            if (element.value == 0) element = candidateType;
            requireType(element, candidateType, candidate->span, "array element");
          }
          if (mapSpreads + valueSpreads > 1)
            throw TypeError("array literals support at most one spread", expression.span);
          type = interner_.internDynamicArray(element);
          break;
        }
        case hir::ExpressionKind::StructLiteral:
        {
          if (!expression.structId.has_value()) throw TypeError("struct literal has no resolved type", expression.span);
          if (interner_.structGenericArity(*expression.structId) != 0)
            throw TypeError(std::format("cannot infer generic arguments for struct literal `{}`; annotate the binding type",
                                        expression.text), expression.span);
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
          if (descriptor.variantHasPayload.at(variant) &&
              interner_.descriptor(descriptor.elements.at(variant)).kind == TypeKind::Tuple)
          {
            // Multi-field variants: each constructor argument fills one tuple
            // element of the payload.
            const auto &tuple = interner_.descriptor(descriptor.elements.at(variant));
            if (expression.operands.size() != tuple.elements.size())
              throw TypeError(std::format("enum variant `{}` expects {} payload values, got {}",
                                          descriptor.fieldNames.at(variant), tuple.elements.size(),
                                          expression.operands.size()), expression.span);
            for (size_t index = 0; index < expression.operands.size(); ++index)
              static_cast<void>(inferExpected(*expression.operands[index], tuple.elements[index], locals,
                                              std::format("variant `{}` payload", descriptor.fieldNames.at(variant))));
            break;
          }
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
          for (const auto &element : expression.operands)
          {
            if (element->kind == hir::ExpressionKind::Prefix && element->text == "...")
            {
              const TypeId operand = infer(*element->operands[0], locals);
              const auto &descriptor = interner_.descriptor(operand);
              if (descriptor.kind == TypeKind::Tuple)
                elements.insert(elements.end(), descriptor.elements.begin(), descriptor.elements.end());
              else if (descriptor.kind == TypeKind::TypePack)
                elements.push_back(descriptor.element);
              else
                throw TypeError(std::format("cannot spread value of type {}", interner_.display(operand)), element->span);
              continue;
            }
            const TypeId elementType = infer(*element, locals);
            if (interner_.descriptor(elementType).kind == TypeKind::Reference)
              throw TypeError("references cannot be stored in tuples", element->span);
            elements.push_back(elementType);
          }
          type = interner_.internTuple(elements);
          break;
        }
        case hir::ExpressionKind::ResolvedName:
          if (expression.resolvedName->kind == hir::ResolvedNameKind::Function)
            throw TypeError("function name cannot be used as a value", expression.span);
          if (expression.resolvedName->kind == hir::ResolvedNameKind::ConstParameter)
            throw TypeError(std::format("const parameter `{}` is not a runtime value", expression.text), expression.span);
          if (moveState_.isWholeMoved(expression.resolvedName->id))
            throw TypeError(std::format("use of moved value `{}`", expression.text), expression.span);
          type = locals.at(expression.resolvedName->id);
          break;
        case hir::ExpressionKind::Grouped: type = infer(*expression.operands[0], locals); break;
        case hir::ExpressionKind::GenericApplication:
        {
          if (const auto builtin = evaluateTraitIntrospection(expression.text, expression.genericArguments, expression.span);
              builtin.has_value())
          {
            type = builtin::Bool;
            break;
          }
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
          if (const auto builtin = evaluateTupleIntrospection(expression.text, typeArguments, expression.span);
              builtin.has_value())
          {
            type = interner_.constInterner().value(*builtin).kind == const_eval::ConstValueKind::Bool ? builtin::Bool
                                                                                                     : builtin::I64;
            break;
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
            if (const auto root = rootLocalOf(*expression.operands[0]); root.has_value())
            {
              // Borrows are counted once per expression node: the same
              // `ref`/`ref mut` may be re-inferred by the call paths.
              if (borrowExpressions_.insert(&expression).second)
              {
                auto &counts = borrowState_.counts[root->value];
                if (expression.text == "ref mut")
                {
                  if (counts.shared != 0)
                    throw TypeError(std::format("cannot mutably borrow `{}` while it is shared-borrowed",
                                                expression.operands[0]->text), expression.span);
                  if (counts.mutableRefs != 0)
                    throw TypeError(std::format("cannot mutably borrow `{}` while it is already mutably borrowed",
                                                expression.operands[0]->text), expression.span);
                  ++counts.mutableRefs;
                }
                else
                {
                  if (counts.mutableRefs != 0)
                    throw TypeError(std::format("cannot shared-borrow `{}` while it is mutably borrowed",
                                                expression.operands[0]->text), expression.span);
                  if (counts.shared >= BorrowState::MaxShared)
                    throw TypeError("too many shared borrows of one binding", expression.span);
                  ++counts.shared;
                }
              }
            }
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
          if (expression.text == "move" || expression.text == "clone")
          {
            type = operand;
            break;
          }
          if (expression.text == "...")
          {
            throw TypeError("spread is only valid inside tuple literals", expression.span);
          }
          if (expression.text == "!")
          {
            type = builtin::Bool;
            requireType(type, operand, expression.span, "prefix operand");
          }
          else if (isIntegerBuiltin(operand) || isFloatBuiltin(operand))
          {
            type = operand;
          }
          else
          {
            type = builtin::I64;
            requireType(type, operand, expression.span, "prefix operand");
          }
          break;
        }
        case hir::ExpressionKind::Binary:
        {
          TypeId left = infer(*expression.operands[0], locals);
          TypeId right = infer(*expression.operands[1], locals);
          const bool equality = expression.text == "==" || expression.text == "!=";
          const bool ordering = expression.text == "<" || expression.text == "<=" || expression.text == ">" ||
                                expression.text == ">=";
          // Contextual numeric literals adopt the other operand's type for
          // arithmetic; equality and ordering compare across numeric widths,
          // so literals keep their own types there.
          if (!equality && !ordering)
          {
            if (expression.operands[1]->kind == hir::ExpressionKind::IntegerLiteral && isIntegerBuiltin(left) &&
                right != left)
              right = inferExpected(*expression.operands[1], left, locals, "integer literal");
            if (expression.operands[0]->kind == hir::ExpressionKind::IntegerLiteral && isIntegerBuiltin(right) &&
                left != right)
              left = inferExpected(*expression.operands[0], right, locals, "integer literal");
            if (expression.operands[1]->kind == hir::ExpressionKind::FloatLiteral && isFloatBuiltin(left) &&
                right != left)
              right = inferExpected(*expression.operands[1], left, locals, "float literal");
            if (expression.operands[0]->kind == hir::ExpressionKind::FloatLiteral && isFloatBuiltin(right) &&
                left != right)
              left = inferExpected(*expression.operands[0], right, locals, "float literal");
          }
          if (expression.text == "..")
          {
            if (!isIntegerBuiltin(left)) requireType(builtin::I64, left, expression.operands[0]->span, "range start");
            if (!isIntegerBuiltin(right)) requireType(builtin::I64, right, expression.operands[1]->span, "range end");
            requireType(left, right, expression.span, "range bounds");
            type = interner_.internRange(left);
            break;
          }
          // Union sides narrow to the other operand's member type for
          // equality and ordering comparisons.
          if (equality || ordering)
          {
            if (interner_.descriptor(left).kind == TypeKind::Union)
            {
              const auto &members = interner_.descriptor(left).elements;
              if (std::find(members.begin(), members.end(), right) != members.end())
              {
                record(*expression.operands[0], right);
                left = right;
              }
            }
            if (interner_.descriptor(right).kind == TypeKind::Union)
            {
              const auto &members = interner_.descriptor(right).elements;
              if (std::find(members.begin(), members.end(), left) != members.end())
              {
                record(*expression.operands[1], left);
                right = left;
              }
            }
          }
          const bool numericPair = isNumericBuiltin(left) && isNumericBuiltin(right);
          // Equality and ordering compare across numeric widths; everything
          // else requires identical operand types.
          if (!((equality || ordering) && numericPair)) requireType(left, right, expression.span, "binary operands");
          if (equality) type = builtin::Bool;
          else if (ordering)
          {
            if (!numericPair) requireType(builtin::I64, left, expression.span, "comparison operand");
            type = builtin::Bool;
          }
          else if (expression.text == "&&" || expression.text == "||")
          {
            requireType(builtin::Bool, left, expression.span, "logical operand");
            type = builtin::Bool;
          }
          else
          {
            if (expression.text != "+" || left != builtin::String)
            {
              if (!isIntegerBuiltin(left) && !isFloatBuiltin(left))
                requireType(builtin::I64, left, expression.span, "binary operand");
            }
            type = left;
          }
          break;
        }
        case hir::ExpressionKind::Call:
        {
          if (expression.methodCall) return inferMethodCall(expression, locals);
          if (!expression.operands[0]->resolvedName.has_value() ||
              expression.operands[0]->resolvedName->kind != hir::ResolvedNameKind::Function)
            throw TypeError("call target is not a function", expression.operands[0]->span);
          {
            const FoldInfo fold = analyzeFoldCall(expression, locals);
            if (fold.fold)
            {
              if (expression.operands.size() != 3)
                throw TypeError("fold calls require exactly one accumulator argument", expression.span);
              const auto &signature = signatures_.at(expression.operands[0]->resolvedName->id);
              if (signature.parameters.size() != 2)
                throw TypeError("fold function must take exactly two arguments", expression.span);
              Substitution substitution;
              unify(signature.parameters[fold.spreadPosition], fold.elementType, substitution, expression.span);
              const TypeId accumulatorType = infer(*expression.operands[fold.accumulatorPosition + 1], locals);
              unify(signature.parameters[fold.accumulatorPosition], accumulatorType, substitution, expression.span);
              const TypeId result = specialize(signature.returnType, substitution);
              unify(signature.parameters[fold.accumulatorPosition], result, substitution, expression.span);
              requireConstArguments(signature, substitution, expression.operands[0]->text, expression.span);
              if (module_ != nullptr && expression.operands[0]->resolvedName->id < module_->functions.size() &&
                  module_->functions.at(expression.operands[0]->resolvedName->id).whereClause != nullptr &&
                  !evaluateWhereCondition(*module_->functions.at(expression.operands[0]->resolvedName->id).whereClause,
                                          substitution, signature))
                throw TypeError(std::format("call to `{}` does not satisfy its where clause",
                                            module_->functions.at(expression.operands[0]->resolvedName->id).name),
                                expression.span);
              callFoldSpreadPositions_.insert_or_assign(&expression, std::vector<size_t>{fold.spreadPosition});
              callFoldAccumulatorPositions_.insert_or_assign(&expression, std::vector<size_t>{fold.accumulatorPosition});
              callTargets_.insert_or_assign(&expression, hir::DefId{expression.operands[0]->resolvedName->id});
              type = specialize(signature.returnType, substitution);
              break;
            }
          }
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
          CallSpreadInfo spreadInfo;
          const bool hasSpread = analyzeCallSpread(expression, 1, locals, spreadInfo);
          const size_t supplied = hasSpread ? spreadInfo.expandedCount : expression.operands.size() - 1;
          const bool variadic = !signature.packParameters.empty();
          const size_t fixedParameters = signature.parameters.size() - (variadic ? 1 : 0);
          if (variadic ? supplied < fixedParameters : supplied != signature.parameters.size())
            throw TypeError(std::format("call argument count mismatch: expected {}, got {}", signature.parameters.size(), supplied),
                            expression.span);
          const size_t packCount = variadic ? supplied - fixedParameters : 0;
          if (hasSpread) callSpreadPositions_.insert_or_assign(&expression, spreadInfo.spreadPositions);
          Substitution substitution;
          const auto argumentTypeAt = [&](size_t index) -> TypeId {
            return hasSpread ? spreadInfo.expandedTypes[index] : infer(*expression.operands[index + 1], locals);
          };
          if (!expression.genericArguments.empty())
          {
            substitution = explicitSubstitution(expression, signature);
            for (size_t index = 0; index < supplied; ++index)
            {
              const TypeId parameterType = index < fixedParameters ? signature.parameters[index]
                                                                   : interner_.descriptor(signature.parameters.back()).element;
              if (hasSpread && spreadInfo.spreadSlot[index])
                requireType(specialize(parameterType, substitution), argumentTypeAt(index),
                            spreadInfo.argumentExpressions[index]->span, std::format("call argument {}", index + 1));
              else
                static_cast<void>(inferExpected(*expression.operands[index + 1],
                                                specialize(parameterType, substitution), locals,
                                                std::format("call argument {}", index + 1)));
            }
          }
          else
          {
            for (size_t index = 0; index < supplied; ++index)
            {
              const hir::Expression &argument = hasSpread ? *spreadInfo.argumentExpressions[index]
                                                            : *expression.operands[index + 1];
              const TypeId parameterType = index < fixedParameters ? signature.parameters[index]
                                                                   : interner_.descriptor(signature.parameters.back()).element;
              const TypeId argumentType = argumentTypeAt(index);
              if (!isGeneric(signature))
              {
                if (hasSpread && spreadInfo.spreadSlot[index])
                  requireType(parameterType, argumentType, argument.span, std::format("call argument {}", index + 1));
                else
                  static_cast<void>(inferExpected(argument, parameterType, locals,
                                                  std::format("call argument {}", index + 1)));
                continue;
              }
              if (variadic && index >= fixedParameters)
              {
                auto &pack = substitution.packs[parameterType.value];
                const size_t packPosition = index - fixedParameters;
                if (packPosition < pack.size())
                {
                  if (pack[packPosition] != argumentType)
                    throw TypeError(std::format("generic argument type mismatch: expected {}, got {}", interner_.display(pack[packPosition]),
                                                interner_.display(argumentType)), argument.span);
                }
                else
                {
                  pack.push_back(argumentType);
                }
                continue;
              }
              const auto &expectedDescriptor = interner_.descriptor(parameterType);
              if (hasSpread && spreadInfo.spreadSlot[index])
              {
                unify(parameterType, argumentType, substitution, argument.span);
                continue;
              }
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
                unify(parameterType, argumentType, substitution, argument.span);
              }
            }
          }
          type = specializeReturnType(signature, substitution, packCount);
          if (!signature.genericParameters.empty() && interner_.descriptor(type).kind == TypeKind::TypeParameter)
            throw TypeError(std::format("cannot infer generic arguments for function `{}`", expression.operands[0]->text), expression.span);
          requireConstArguments(signature, substitution, expression.operands[0]->text, expression.span);
          if (module_ != nullptr && selected.value < module_->functions.size() &&
              module_->functions.at(selected.value).whereClause != nullptr &&
              !evaluateWhereCondition(*module_->functions.at(selected.value).whereClause, substitution, signature))
            throw TypeError(std::format("call to `{}` does not satisfy its where clause", module_->functions.at(selected.value).name),
                            expression.span);
          if (!hasSpread)
          {
            for (size_t index = 0; index < supplied; ++index)
            {
              const TypeId parameterType = index < fixedParameters ? signature.parameters[index]
                                                                   : interner_.descriptor(signature.parameters.back()).element;
              const auto &parameterDescriptor = interner_.descriptor(parameterType);
              if (parameterDescriptor.kind != TypeKind::Reference && parameterDescriptor.kind != TypeKind::RawPointer)
                trackConsumption(*expression.operands[index + 1], locals);
            }
          }
          else
          {
            for (size_t index = 0; index < supplied; ++index)
            {
              if (!spreadInfo.spreadSlot[index]) continue;
              const TypeId parameterType = index < fixedParameters ? signature.parameters[index]
                                                                   : interner_.descriptor(signature.parameters.back()).element;
              const auto &parameterDescriptor = interner_.descriptor(parameterType);
              if (parameterDescriptor.kind != TypeKind::Reference && parameterDescriptor.kind != TypeKind::RawPointer)
                trackConsumption(*spreadInfo.argumentExpressions[index], locals);
            }
          }
          if (variadic)
          {
            callPackArgCounts_.insert_or_assign(&expression, packCount);
            std::vector<TypeId> packedTypes;
            for (size_t index = fixedParameters; index < supplied; ++index) packedTypes.push_back(argumentTypeAt(index));
            callPackTupleTypes_.insert_or_assign(&expression, interner_.internTuple(std::move(packedTypes)));
          }
          selected = instantiateFunction(selected, substitution, packCount, expression.span);
          callTargets_.insert_or_assign(&expression, selected);
          break;
        }
        case hir::ExpressionKind::Index:
        {
          const TypeId receiver = infer(*expression.operands[0], locals);
          const TypeId indexType = infer(*expression.operands[1], locals);
          if (interner_.descriptor(indexType).kind == TypeKind::Range)
          {
            const auto &descriptor = interner_.descriptor(receiver);
            if (descriptor.kind != TypeKind::DynamicArray && descriptor.kind != TypeKind::FixedArray &&
                descriptor.kind != TypeKind::DependentArray)
              throw TypeError(std::format("cannot slice value of type {}", interner_.display(receiver)), expression.span);
            type = interner_.internDynamicArray(descriptor.element);
            break;
          }
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
          if (const auto root = rootLocalOf(expression); root.has_value())
          {
            const auto fieldNames = descriptor.fieldNames;
            const auto found = std::find(fieldNames.begin(), fieldNames.end(), expression.text);
            if (found != fieldNames.end())
            {
              const uint32_t field = static_cast<uint32_t>(std::distance(fieldNames.begin(), found));
              if (moveState_.isFieldMoved(root->value, field))
                throw TypeError(std::format("use of moved field `{}.{}`", expression.operands[0]->text, expression.text),
                                expression.span);
            }
          }
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
        std::unordered_set<uint32_t> visited;
        return specificityGuarded(type, visited);
      }

      /// Guarded specificity: recursive payload types are regular trees, so a
      /// repeated type contributes once.
      [[nodiscard]] auto specificityGuarded(TypeId type, std::unordered_set<uint32_t> &visited) const -> int
      {
        if (!visited.insert(type.value).second) return 0;
        const auto &descriptor = interner_.descriptor(type);
        if (descriptor.kind == TypeKind::TypeParameter) return 0;
        int score = descriptor.kind == TypeKind::Builtin ? 1 : 0;
        for (const auto element : descriptor.elements) score += specificityGuarded(element, visited);
        for (const auto argument : descriptor.typeArguments) score += specificityGuarded(argument, visited);
        if (descriptor.kind == TypeKind::DynamicArray || descriptor.kind == TypeKind::FixedArray ||
            descriptor.kind == TypeKind::DependentArray || descriptor.kind == TypeKind::Reference ||
            descriptor.kind == TypeKind::RawPointer)
          score += specificityGuarded(descriptor.element, visited);
        return score;
      }
      auto unify(TypeId expected, TypeId actual, Substitution &substitution, syntax::SourceSpan span) -> void
      {
        std::unordered_set<uint64_t> visited;
        unifyGuarded(expected, actual, substitution, span, visited);
      }

      /// Guarded unification: a repeated (expected, actual) pair means the
      /// types were already compared in this call (regular recursive types),
      /// which terminates unification of self-referential payloads.
      auto unifyGuarded(TypeId expected, TypeId actual, Substitution &substitution, syntax::SourceSpan span,
                        std::unordered_set<uint64_t> &visited) -> void
      {
        if (!visited.insert((static_cast<uint64_t>(expected.value) << 32) | actual.value).second) return;
        const auto &expectedDescriptor = interner_.descriptor(expected);
        if (expectedDescriptor.kind == TypeKind::TypeParameter)
        {
          if (const auto found = substitution.types.find(expected.value); found != substitution.types.end())
            requireType(found->second, actual, span, "generic argument");
          else
          {
            // Kind check: a bare generic struct/enum template is a type
            // constructor, not a type, and cannot bind a type parameter.
            const auto &bound = interner_.descriptor(actual);
            if (bound.typeArguments.empty() && bound.nominalId.has_value() &&
                ((bound.kind == TypeKind::Struct && interner_.structGenericArity(hir::StructId{*bound.nominalId}) != 0) ||
                 (bound.kind == TypeKind::Enum && interner_.enumGenericArity(hir::EnumId{*bound.nominalId}) != 0)))
              throw TypeError(std::format("generic argument is a type constructor, not a type: {}",
                                          interner_.display(actual)), span);
            substitution.types.emplace(expected.value, actual);
          }
          return;
        }
        const auto &actualDescriptor = interner_.descriptor(actual);
        if (expectedDescriptor.kind == TypeKind::TypeApplication)
        {
          const uint32_t constructorIndex = *expectedDescriptor.nominalId;
          if (actualDescriptor.kind == TypeKind::TypeApplication)
          {
            if (*actualDescriptor.nominalId != constructorIndex ||
                expectedDescriptor.elements.size() != actualDescriptor.elements.size())
              throw TypeError(std::format("generic argument type mismatch: expected {}, got {}", interner_.display(expected),
                                          interner_.display(actual)), span);
            for (size_t index = 0; index < expectedDescriptor.elements.size(); ++index)
              unifyGuarded(expectedDescriptor.elements[index], actualDescriptor.elements[index], substitution, span, visited);
            return;
          }
          if (actualDescriptor.kind != TypeKind::Struct && actualDescriptor.kind != TypeKind::Opaque)
            throw TypeError(std::format("generic argument type mismatch: expected {}, got {}", interner_.display(expected),
                                        interner_.display(actual)), span);
          if (!actualDescriptor.nominalId.has_value() ||
              actualDescriptor.typeArguments.size() != expectedDescriptor.elements.size())
            throw TypeError(std::format("generic argument type mismatch: expected {}, got {}", interner_.display(expected),
                                        interner_.display(actual)), span);
          const TypeId templateType = actualDescriptor.kind == TypeKind::Struct
                                          ? interner_.typeForStruct(hir::StructId{*actualDescriptor.nominalId})
                                          : TypeId{*actualDescriptor.nominalId};
          if (const auto bound = substitution.constructors.find(constructorIndex); bound != substitution.constructors.end())
          {
            if (bound->second != templateType)
              throw TypeError(std::format("generic argument type mismatch: expected {}, got {}", interner_.display(expected),
                                          interner_.display(actual)), span);
          }
          else
          {
            substitution.constructors.emplace(constructorIndex, templateType);
          }
          for (size_t index = 0; index < expectedDescriptor.elements.size(); ++index)
            unifyGuarded(expectedDescriptor.elements[index], actualDescriptor.typeArguments[index], substitution, span, visited);
          return;
        }
        if (expected == actual) return;
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
          unifyGuarded(expectedDescriptor.element, actualDescriptor.element, substitution, span, visited);
          return;
        }
        if (expectedDescriptor.kind != actualDescriptor.kind)
          throw TypeError(std::format("generic argument type mismatch: expected {}, got {}", interner_.display(expected),
                                      interner_.display(actual)), span);
        if (expectedDescriptor.kind == TypeKind::Enum)
        {
          if (expectedDescriptor.nominalId != actualDescriptor.nominalId ||
              expectedDescriptor.elements.size() != actualDescriptor.elements.size())
            throw TypeError(std::format("generic argument type mismatch: expected {}, got {}", interner_.display(expected),
                                        interner_.display(actual)), span);
          for (size_t index = 0; index < expectedDescriptor.elements.size(); ++index)
            unifyGuarded(expectedDescriptor.elements[index], actualDescriptor.elements[index], substitution, span, visited);
          return;
        }
        if (expectedDescriptor.kind == TypeKind::Tuple)
        {
          const bool hasPack = std::any_of(expectedDescriptor.elements.begin(), expectedDescriptor.elements.end(),
                                           [this](TypeId element) { return interner_.descriptor(element).kind == TypeKind::TypePack; });
          if (!hasPack)
          {
            if (expectedDescriptor.elements.size() != actualDescriptor.elements.size())
              throw TypeError(std::format("generic argument type mismatch: expected {}, got {}", interner_.display(expected),
                                          interner_.display(actual)), span);
            for (size_t index = 0; index < expectedDescriptor.elements.size(); ++index)
              unifyGuarded(expectedDescriptor.elements[index], actualDescriptor.elements[index], substitution, span, visited);
            return;
          }
          if (actualDescriptor.kind != TypeKind::Tuple)
            throw TypeError(std::format("generic argument type mismatch: expected {}, got {}", interner_.display(expected),
                                        interner_.display(actual)), span);
          size_t expectedIndex = 0;
          while (interner_.descriptor(expectedDescriptor.elements[expectedIndex]).kind != TypeKind::TypePack) ++expectedIndex;
          const size_t trailing = expectedDescriptor.elements.size() - expectedIndex - 1;
          if (actualDescriptor.elements.size() + 1 < expectedDescriptor.elements.size())
            throw TypeError(std::format("generic argument type mismatch: expected {}, got {}", interner_.display(expected),
                                        interner_.display(actual)), span);
          for (size_t index = 0; index < expectedIndex; ++index)
            unifyGuarded(expectedDescriptor.elements[index], actualDescriptor.elements[index], substitution, span, visited);
          const TypeId packElement = interner_.descriptor(expectedDescriptor.elements[expectedIndex]).element;
          const size_t packSize = actualDescriptor.elements.size() - expectedIndex - trailing;
          auto &pack = substitution.packs[packElement.value];
          if (pack.empty())
          {
            for (size_t index = 0; index < packSize; ++index)
              pack.push_back(actualDescriptor.elements[expectedIndex + index]);
          }
          else if (pack.size() != packSize)
          {
            throw TypeError("generic argument type mismatch: expected {}, got {}", span);
          }
          for (size_t index = 0; index < trailing; ++index)
            unifyGuarded(expectedDescriptor.elements[expectedIndex + 1 + index],
                  actualDescriptor.elements[actualDescriptor.elements.size() - trailing + index], substitution, span, visited);
          return;
        }
        if (expectedDescriptor.kind == TypeKind::DynamicArray || expectedDescriptor.kind == TypeKind::FixedArray)
        {
          if (expectedDescriptor.length != actualDescriptor.length)
            throw TypeError("generic array length mismatch", span);
          unifyGuarded(expectedDescriptor.element, actualDescriptor.element, substitution, span, visited);
          return;
        }
        if (expectedDescriptor.kind == TypeKind::Reference || expectedDescriptor.kind == TypeKind::RawPointer)
        {
          if (expectedDescriptor.referenceMutable != actualDescriptor.referenceMutable)
            throw TypeError("generic reference mutability mismatch", span);
          unifyGuarded(expectedDescriptor.element, actualDescriptor.element, substitution, span, visited);
          return;
        }
        throw TypeError(std::format("generic argument type mismatch: expected {}, got {}", interner_.display(expected),
                                    interner_.display(actual)), span);
      }

      [[nodiscard]] auto specialize(TypeId type, const Substitution &substitution) -> TypeId
      {
        return interner_.specialize(type, substitution.types, substitution.consts, substitution.constructors);
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
      std::unordered_map<std::string, TraitInfo> traits_;
      std::vector<ImplInfo> impls_;
      std::unordered_map<const hir::Expression *, size_t> callPackArgCounts_;
      std::unordered_map<const hir::Expression *, TypeId> callPackTupleTypes_;
      std::unordered_map<const hir::Expression *, std::vector<size_t>> callSpreadPositions_;
      std::unordered_map<const hir::Expression *, std::vector<size_t>> callFoldSpreadPositions_;
      std::unordered_map<const hir::Expression *, std::vector<size_t>> callFoldAccumulatorPositions_;
      std::unordered_set<uint32_t> dropTypes_;
      std::unordered_map<const hir::Statement *, std::vector<std::pair<uint32_t, uint32_t>>> returnDrops_;
      std::unordered_map<uint32_t, std::vector<std::pair<uint32_t, uint32_t>>> fallthroughDrops_;
      std::unordered_map<const hir::Block *, std::vector<std::pair<uint32_t, uint32_t>>> blockDrops_;
      std::unordered_set<uint32_t> placeholderFunctions_;
      std::unordered_set<uint32_t> derivedCloneTypes_;
      std::unordered_map<uint32_t, std::vector<std::string>> dropMovedFields_;
      std::unordered_map<const hir::Expression *, std::pair<std::string, TypeId>> traitViewCoercions_;
      std::unordered_map<const hir::Expression *, std::pair<std::string, size_t>> traitViewCalls_;
      std::unordered_map<std::string, std::unordered_map<uint32_t, std::vector<hir::DefId>>> traitViewTables_;
      std::unordered_map<const hir::Expression *, TypeId> derivedCloneCalls_;
      std::unordered_map<const hir::Expression *, bool> methodReceiverMutable_;
      std::unordered_map<const hir::Expression *, TypeId> methodReceiverRefTypes_;
      std::unordered_map<std::string, hir::DefId> instanceTable_;
      std::deque<hir::Function> instances_;
      std::unordered_set<uint32_t> deferredMethodFunctions_;
      hir::DefId currentFunctionId_{};
      uint32_t nextInstanceLocal_{1'000'000};
      std::unordered_set<uint32_t> constFunctions_;
      std::unique_ptr<const_eval::ConstInterpreter> interpreter_;
      /// Generic parameter bindings of the function currently being checked;
      /// used to resolve in-body const predicate arguments.
      std::unordered_map<std::string, TypeId> genericBindings_;
      /// Bindings declared with `let mut` (and loop bindings, which `next`
      /// rebinds) in the current lexical path; restored at block boundaries.
      MutableBindings mutableBindings_;
      /// Move tracking for affine bindings (D-015): whole and per-field moved
      /// state, merged at branch and loop boundaries.
      MoveState moveState_;
      /// Active shared/mutable borrow counts per root local (D-015 rule 5).
      BorrowState borrowState_;
      /// Borrow expressions already counted (re-inference is idempotent).
      std::unordered_set<const hir::Expression *> borrowExpressions_;
      bool inConstGenericFunction_{};
      const_eval::ConstBindings activeConstBindings_;
    };
  } // namespace

  auto TypeChecker::check(const hir::Module &module) -> TypeCheckResult { return Checker{}.check(module); }
} // namespace NG::typecheck
