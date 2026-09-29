// AI-generated code; reviewed for this repository's vNext rewrite.
// HIR function cloning with fresh local/loop ids: generic instances
// (monomorphization re-checks cloned bodies) and synthesized closure code
// functions (INC-14).
#include "hir_internal.hpp"
#include "syntax/const_expr.hpp"

#include <format>

namespace NG::hir
{
  using detail::cloneType;

  namespace
  {
    struct CloneContext
    {
      std::unordered_map<uint32_t, uint32_t> locals;
      std::unordered_map<uint32_t, uint32_t> loops;
      uint32_t &nextLocal;
    };

    [[nodiscard]] auto cloneExpression(CloneContext &context, const hir::Expression &source) -> hir::ExpressionPtr;
    [[nodiscard]] auto cloneBlock(CloneContext &context, const hir::Block &source) -> hir::Block;

    [[nodiscard]] auto remapLocal(CloneContext &context, hir::LocalId local) -> hir::LocalId
    {
      const auto found = context.locals.find(local.value);
      if (found != context.locals.end())
        return hir::LocalId{found->second};
      const uint32_t fresh = context.nextLocal++;
      context.locals.emplace(local.value, fresh);
      return hir::LocalId{fresh};
    }

    [[nodiscard]] auto remapLoop(CloneContext &context, hir::LoopId loop) -> hir::LoopId
    {
      const auto found = context.loops.find(loop.value);
      if (found != context.loops.end())
        return hir::LoopId{found->second};
      const uint32_t fresh = context.nextLocal++;
      context.loops.emplace(loop.value, fresh);
      return hir::LoopId{fresh};
    }

    [[nodiscard]] auto cloneExpression(CloneContext &context, const hir::Expression &source) -> hir::ExpressionPtr
    {
      auto cloned = std::make_unique<hir::Expression>();
      cloned->kind = source.kind;
      cloned->span = source.span;
      cloned->text = source.text;
      cloned->numericSuffix = source.numericSuffix;
      cloned->resolvedName = source.resolvedName;
      cloned->functionCandidates = source.functionCandidates;
      cloned->structId = source.structId;
      cloned->enumId = source.enumId;
      cloned->variant = source.variant;
      cloned->memberNames = source.memberNames;
      cloned->genericArguments = detail::cloneTypeArguments(source.genericArguments);
      if (source.testedType != nullptr)
        cloned->testedType = std::make_unique<hir::Type>(cloneType(*source.testedType));
      cloned->traitNames = source.traitNames;
      cloned->methodCall = source.methodCall;
      if (cloned->resolvedName.has_value() && cloned->resolvedName->kind == hir::ResolvedNameKind::Local)
        cloned->resolvedName->id = remapLocal(context, LocalId{cloned->resolvedName->id}).value;
      if (source.kind == hir::ExpressionKind::Closure)
      {
        for (const auto &capture : source.closureCaptures)
          cloned->closureCaptures.push_back(
              Expression::CaptureInfo{.local = remapLocal(context, capture.local), .isMove = capture.isMove});
        for (const auto captureParam : source.closureCaptureParams)
          cloned->closureCaptureParams.push_back(remapLocal(context, captureParam));
        for (const auto &parameterName : source.closureParameterNames)
          cloned->closureParameterNames.push_back(parameterName);
        for (const auto parameterId : source.closureParameterIds)
          cloned->closureParameterIds.push_back(remapLocal(context, parameterId));
        for (const auto &parameterType : source.closureParameterTypes)
          cloned->closureParameterTypes.push_back(std::make_shared<hir::Type>(cloneType(*parameterType)));
        if (source.closureBody != nullptr)
          cloned->closureBody = std::make_unique<hir::Block>(cloneBlock(context, *source.closureBody));
      }
      for (const auto &operand : source.operands)
        cloned->operands.push_back(cloneExpression(context, *operand));
      return cloned;
    }

    [[nodiscard]] auto cloneBlock(CloneContext &context, const hir::Block &source) -> hir::Block
    {
      hir::Block cloned{.span = source.span};
      for (const auto &statement : source.statements)
      {
        hir::Statement copy{.kind = statement.kind, .span = statement.span};
        if (statement.local.has_value())
          copy.local = remapLocal(context, *statement.local);
        if (statement.bindingType != nullptr)
          copy.bindingType = std::make_shared<hir::Type>(cloneType(*statement.bindingType));
        for (const auto local : statement.destructuredLocals)
          copy.destructuredLocals.push_back(remapLocal(context, local));
        copy.destructuredIndices = statement.destructuredIndices;
        if (statement.restLocal.has_value())
          copy.restLocal = remapLocal(context, *statement.restLocal);
        copy.mutableBinding = statement.mutableBinding;
        if (statement.loop.has_value())
          copy.loop = remapLoop(context, *statement.loop);
        if (statement.nextTarget.has_value())
          copy.nextTarget = statement.nextTarget->kind == hir::NextTargetKind::Loop
                                ? NextTarget{.kind = NextTargetKind::Loop,
                                             .id = remapLoop(context, LoopId{statement.nextTarget->id}).value}
                                : *statement.nextTarget;
        if (statement.expression != nullptr)
          copy.expression = cloneExpression(context, *statement.expression);
        if (statement.assignmentTarget != nullptr)
          copy.assignmentTarget = cloneExpression(context, *statement.assignmentTarget);
        for (const auto &argument : statement.arguments)
          copy.arguments.push_back(cloneExpression(context, *argument));
        for (const auto local : statement.loopBindings)
          copy.loopBindings.push_back(remapLocal(context, local));
        for (const auto &switchCase : statement.switchCases)
        {
          hir::SwitchCase clonedCase{
            .variantName = switchCase.variantName, .literalTexts = switchCase.literalTexts, .span = switchCase.span};
          if (switchCase.memberType != nullptr)
            clonedCase.memberType = std::make_unique<hir::Type>(cloneType(*switchCase.memberType));
          if (switchCase.binding.has_value())
            clonedCase.binding = remapLocal(context, *switchCase.binding);
          for (const auto binding : switchCase.bindings)
            clonedCase.bindings.push_back(remapLocal(context, binding));
          clonedCase.body = std::make_unique<hir::Block>(cloneBlock(context, *switchCase.body));
          copy.switchCases.push_back(std::move(clonedCase));
        }
        if (statement.consequence != nullptr)
          copy.consequence = std::make_unique<hir::Block>(cloneBlock(context, *statement.consequence));
        if (statement.alternative != nullptr)
          copy.alternative = std::make_unique<hir::Block>(cloneBlock(context, *statement.alternative));
        if (statement.body != nullptr)
          copy.body = std::make_unique<hir::Block>(cloneBlock(context, *statement.body));
        cloned.statements.push_back(std::move(copy));
      }
      if (source.tailExpression != nullptr)
        cloned.tailExpression = cloneExpression(context, *source.tailExpression);
      return cloned;
    }
  } // namespace

  auto cloneFunction(const Function &source, uint32_t &nextLocal) -> Function
  {
    CloneContext context{.nextLocal = nextLocal};
    Function cloned{.id = source.id,
                    .name = source.name,
                    .span = source.span,
                    .genericParameters = source.genericParameters,
                    .packParameters = source.packParameters,
                    .constructorParameters = source.constructorParameters,
                    .variadicConstructorParameters = source.variadicConstructorParameters,
                    .genericParameterOrder = source.genericParameterOrder,
                    .constFunction = source.constFunction,
                    .exported = source.exported,
                    .nativeFunction = source.nativeFunction,
                    .externC = source.externC,
                    .traitBounds = source.traitBounds};
    for (const auto &parameter : source.constParameters)
      cloned.constParameters.push_back(ConstParameter{.name = parameter.name,
                                                      .typeName = parameter.typeName,
                                                      .type = cloneType(parameter.type),
                                                      .span = parameter.span});
    if (source.returnType != nullptr)
      cloned.returnType = std::make_unique<Type>(cloneType(*source.returnType));
    for (const auto &parameter : source.parameters)
    {
      Parameter copy{.name = parameter.name, .typeName = parameter.typeName, .span = parameter.span};
      copy.type = cloneType(parameter.type);
      copy.local = remapLocal(context, parameter.local);
      cloned.parameters.push_back(std::move(copy));
    }
    if (source.whereClause != nullptr)
      cloned.whereClause = cloneExpression(context, *source.whereClause);
    cloned.body = cloneBlock(context, source.body);
    return cloned;
  }

  auto makeClosureFunction(const Expression &closure, const std::string &name, DefId id, uint32_t &nextLocal)
      -> Function
  {
    CloneContext context{.nextLocal = nextLocal};
    Function result;
    result.id = id;
    result.name = name;
    result.span = closure.span;
    // INC-14 S3: capture params are prepended (hidden leading params).
    result.parameters.reserve(closure.closureCaptures.size() + closure.closureParameterNames.size());
    for (size_t index = 0; index < closure.closureCaptureParams.size(); ++index)
    {
      const auto &capture = closure.closureCaptures[index];
      Parameter parameter{.name = std::format("__cap{}", index), .span = closure.span};
      parameter.local = remapLocal(context, closure.closureCaptureParams[index]);
      result.parameters.push_back(std::move(parameter));
    }
    for (size_t index = 0; index < closure.closureParameterNames.size(); ++index)
    {
      // Untyped closure parameters (INC-14 S2) are resolved at the inference
      // site and recorded in the closure's synthesized signature; the HIR
      // parameter carries only the declared annotation when present.
      Parameter parameter{.name = closure.closureParameterNames[index], .span = closure.span};
      if (closure.closureParameterTypes[index] != nullptr)
      {
        parameter.typeName = closure.closureParameterTypes[index]->name;
        parameter.type = cloneType(*closure.closureParameterTypes[index]);
      }
      parameter.local = remapLocal(context, closure.closureParameterIds[index]);
      result.parameters.push_back(std::move(parameter));
    }
    result.body = cloneBlock(context, *closure.closureBody);
    nextLocal = context.nextLocal;
    return result;
  }
} // namespace NG::hir
