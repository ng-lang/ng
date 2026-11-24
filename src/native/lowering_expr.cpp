// AI-generated code; reviewed for this repository's vNext rewrite.
// ARCH-2: Expression lowering: instruction dispatch, unary/binary ops, terminators.
#include "function_lowerer.hpp"

namespace NG::native::detail
{

  void FunctionLowerer::emitCheckedOp(const Instruction &instruction, std::string_view helper,
                                      std::string_view arguments)
  {
    line(std::format("{} =l call $ngrt_{}({})", valueTemps_.at(instruction.result.value), helper, arguments));
    applyWidthCheck(instruction);
  }
  void FunctionLowerer::applyWidthCheck(const Instruction &instruction)
  {
    const auto type = function_.valueTypes.at(instruction.result.value);
    const char *name = nullptr;
    switch (type.value)
    {
    case typecheck::builtin::I8.value:
      name = "check_w_i8";
      break;
    case typecheck::builtin::I16.value:
      name = "check_w_i16";
      break;
    case typecheck::builtin::I32.value:
      name = "check_w_i32";
      break;
    case typecheck::builtin::U8.value:
      name = "check_w_u8";
      break;
    case typecheck::builtin::U16.value:
      name = "check_w_u16";
      break;
    case typecheck::builtin::U32.value:
      name = "check_w_u32";
      break;
    case typecheck::builtin::Isize.value:
      name = "check_w_i64";
      break;
    case typecheck::builtin::Usize.value:
      name = "check_w_u64";
      break;
    default:
      return;
    }
    const auto temp = valueTemps_.at(instruction.result.value);
    line(std::format("{} =l call $ngrt_{}(l {})", temp, name, temp));
  }
  void FunctionLowerer::lowerInstruction(const Instruction &instruction)
  {
    switch (instruction.kind)
    {
    case InstructionKind::Evaluate:
      lowerEvaluate(instruction);
      return;
    case InstructionKind::ExtractTuple:
    {
      // Tuple representation == array representation.
      const auto source = operandTemp(*instruction.source);
      const auto loaded = fresh();
      line(std::format("{} =l call $ngrt_arr_get(l {}, l {})", loaded, source, instruction.payload));
      const auto resultType = qtypeOf(function_.valueTypes.at(instruction.result.value));
      bindResult(instruction, resultType, std::format("copy {}", castFromSlot(loaded, resultType)));
      return;
    }
    case InstructionKind::AppendArray:
    {
      const auto array = operandTemp(instruction.operands[0]);
      const auto elementType = qtypeOf(function_.valueTypes.at(instruction.operands[1].value));
      const auto element = castForSlot(operandTemp(instruction.operands[1]), elementType);
      bindResult(instruction, QType::Long, std::format("call $ngrt_arr_append(l {}, l {})", array, element));
      return;
    }
    case InstructionKind::ArrayLength:
    {
      const auto source = operandTemp(*instruction.source);
      const auto sourceType = function_.valueTypes.at(instruction.source->value);
      const auto &descriptor = function_.typeDescriptors[sourceType.value];
      if (descriptor.kind == TypeKind::Range)
      {
        const auto start = fresh();
        line(std::format("{} =l loadl {}", start, source));
        const auto endAddress = fresh();
        line(std::format("{} =l add {}, 8", endAddress, source));
        const auto end = fresh();
        line(std::format("{} =l loadl {}", end, endAddress));
        bindResult(instruction, QType::Long, std::format("sub {}, {}", end, start));
      }
      else
      {
        bindResult(instruction, QType::Long, std::format("loadl {}", source));
      }
      return;
    }
    case InstructionKind::RangeStart:
      bindResult(instruction, QType::Long, std::format("loadl {}", operandTemp(*instruction.source)));
      return;
    case InstructionKind::Slice:
    {
      const auto array = operandTemp(instruction.operands[0]);
      const auto range = operandTemp(instruction.operands[1]);
      const auto start = fresh();
      line(std::format("{} =l loadl {}", start, range));
      const auto endAddress = fresh();
      line(std::format("{} =l add {}, 8", endAddress, range));
      const auto end = fresh();
      line(std::format("{} =l loadl {}", end, endAddress));
      bindResult(instruction, QType::Long, std::format("call $ngrt_arr_slice(l {}, l {}, l {})", array, start, end));
      return;
    }
    case InstructionKind::BindLocal:
    {
      const auto source = operandTemp(*instruction.source);
      const auto sourceType = function_.valueTypes.at(instruction.source->value);
      const auto localType = function_.localTypes.at(instruction.local->value);
      const auto type = localQType(instruction.local->value);
      std::string boundValue = source;
      // Copy-first semantics: aggregates are deep-copied on bind so
      // later in-place mutation cannot alias the source.
      if (needsClone(sourceType))
      {
        const bool aggregateSlot = qtypeOf(sourceType) == QType::Aggregate && !isCellLocal(instruction.local->value) &&
                                   !ssaLocals_.contains(instruction.local->value);
        if (aggregateSlot)
        {
          // Copy the value straight into the local's stack slot.
          emitAggregateCopy(localSlots_.at(instruction.local->value), source, sourceType);
          bindResult(instruction, QType::Aggregate, std::format("copy {}", localSlots_.at(instruction.local->value)));
          return;
        }
        boundValue = emitClone(source, sourceType);
      }
      if (isUnionType(localType) && !isUnionType(sourceType))
      {
        // Values flowing into a union slot are wrapped in a tagged box.
        bindUnionBox(instruction, localType, unionMemberFor(localType, sourceType), boundValue, qtypeOf(sourceType));
      }
      else
      {
        bindResult(instruction, type, std::format("copy {}", boundValue));
      }
      if (ssaLocals_.contains(instruction.local->value))
      {
        // SSA locals are pure temps: record the current temp (QBE's
        // non-SSA fixup handles later rebinding) and skip slot traffic.
        currentTemps_[instruction.local->value] = valueTemps_.at(instruction.result.value);
        return;
      }
      if (isCellLocal(instruction.local->value))
        bindFreshCell(instruction.local->value, valueTemps_.at(instruction.result.value));
      else
        line(std::format("store{} {}, {}", suffix(type), valueTemps_.at(instruction.result.value),
                         localSlots_.at(instruction.local->value)));
      return;
    }
    case InstructionKind::EnumVariantIndex:
      bindResult(instruction, QType::Long, std::format("loadl {}", operandTemp(*instruction.source)));
      return;
    case InstructionKind::ExtractEnumPayload:
    {
      const auto pointer = operandTemp(*instruction.source);
      const auto address = fresh();
      line(std::format("{} =l add {}, 8", address, pointer));
      const auto loaded = fresh();
      line(std::format("{} =l loadl {}", loaded, address));
      const auto resultType = qtypeOf(function_.valueTypes.at(instruction.result.value));
      if (resultType == QType::None)
        return; // Payloadless variant: unit, no temp is consumed.
      bindResult(instruction, resultType, std::format("copy {}", castFromSlot(loaded, resultType)));
      return;
    }
    case InstructionKind::TupleSplice:
      lowerTupleSplice(instruction);
      return;
    case InstructionKind::MakeRef:
    {
      // Ref-rooted chaining (placeRootRef) is deferred.
      if (!instruction.placeRootLocal || instruction.placeRootRef)
        throw LoweringError(
            std::format("native lowering (M2): only local-rooted references are supported in `{}`", function_.name));
      const TypeId boxResult = function_.valueTypes.at(instruction.result.value);
      if (isBoxType(boxResult))
      {
        // A MakeRef whose result type is a Box builds the box VALUE: an
        // envelope whose handle is a *registered* cell ($ngrt_allocate) so
        // ngrt_load()/ngrt_release() accept it. The cell holds the node word.
        const auto valueSlot = fresh();
        line(std::format("{} =l loadl {}", valueSlot, localSlots_.at(instruction.placeRootLocal->value)));
        const auto root = fresh();
        line(std::format("{} =l loadl {}", root, valueSlot));
        const auto cell = fresh();
        line(std::format("{} =l call $ngrt_allocate(l {})", cell, root));
        const auto envelope = fresh();
        line(std::format("{} =l call $ngrt_alloc(l 16)", envelope));
        line(std::format("storel {}, {}", cell, envelope));
        const auto countAddress = fresh();
        line(std::format("{} =l add {}, 8", countAddress, envelope));
        line(std::format("storel 0, {}", countAddress));
        bindResult(instruction, QType::Long, std::format("copy {}", envelope));
        return;
      }
      bindResult(instruction, QType::Long,
                 std::format("copy {}", emitRefObject(instruction.placeRootLocal->value, instruction.placeSteps)));
      return;
    }
    case InstructionKind::MakeTraitView:
    {
      // A trait view is { l ref-object, l trait-id, l concrete-id };
      // the ref object carries the (cell + path) receiver so method
      // bodies observe root rebinding exactly like the VM. Ref-rooted
      // views are deferred.
      if (!instruction.placeRootLocal || instruction.placeRootRef)
        throw LoweringError(
            std::format("native lowering (M2): only local-rooted trait views are supported in `{}`", function_.name));
      const auto reference = emitRefObject(instruction.placeRootLocal->value, instruction.placeSteps);
      const auto view = fresh();
      line(std::format("{} =l call $ngrt_alloc(l 24)", view));
      line(std::format("storel {}, {}", reference, view));
      const auto traitAddress = fresh();
      line(std::format("{} =l add {}, 8", traitAddress, view));
      line(std::format("storel {}, {}", instruction.traitType, traitAddress));
      const auto concreteAddress = fresh();
      line(std::format("{} =l add {}, 16", concreteAddress, view));
      line(std::format("storel {}, {}", instruction.payload, concreteAddress));
      bindResult(instruction, QType::Long, std::format("copy {}", view));
      return;
    }
    case InstructionKind::MakeSpan:
    {
      // A span is a 16-byte blob { data, len } over an array place (local,
      // struct field, or field reached through a `T ref`).
      if (instruction.placeRootLocal.has_value() == instruction.placeRootRef.has_value())
        throw LoweringError(std::format("native lowering (M2): malformed span place in `{}`", function_.name));
      const auto array = loadArrayAtPlace(instruction);
      std::string start;
      std::string count;
      if (instruction.operands.empty())
      {
        start = "0";
        const auto length = fresh();
        line(std::format("{} =l loadl {}", length, array));
        count = length;
      }
      else
      {
        start = operandTemp(instruction.operands[0]);
        const auto end = fresh();
        line(std::format("{} =l sub {}, {}", end, operandTemp(instruction.operands[1]), start));
        count = end;
      }
      const auto blob = fresh();
      line(std::format("{} =l call $ngrt_span_make(l {}, l {}, l {})", blob, array, start, count));
      bindResult(instruction, QType::Long, std::format("copy {}", blob));
      return;
    }
    case InstructionKind::CallTrait:
    {
      ngrt_.needsTraitDispatch = true;
      const auto view = operandTemp(instruction.operands[0]);
      const auto traitAddress = fresh();
      line(std::format("{} =l add {}, 8", traitAddress, view));
      const auto traitId = fresh();
      line(std::format("{} =l loadl {}", traitId, traitAddress));
      const auto concreteAddress = fresh();
      line(std::format("{} =l add {}, 16", concreteAddress, view));
      const auto concreteId = fresh();
      line(std::format("{} =l loadl {}", concreteId, concreteAddress));
      const auto shifted = fresh();
      line(std::format("{} =l shl {}, 32", shifted, traitId));
      const auto key = fresh();
      line(std::format("{} =l or {}, {}", key, shifted, concreteId));
      const auto table = fresh();
      line(std::format("{} =l call $ngrt_trait_vtable(l {})", table, key));
      const auto functionAddress = fresh();
      line(std::format("{} =l add {}, {}", functionAddress, table, instruction.payload * 8));
      const auto function = fresh();
      line(std::format("{} =l loadl {}", function, functionAddress));
      // The receiver is the view's ref object, passed as `Self ref`.
      const auto receiver = fresh();
      line(std::format("{} =l loadl {}", receiver, view));
      std::string arguments = std::format("l {}", receiver);
      for (size_t index = 1; index < instruction.operands.size(); ++index)
      {
        const auto typeId = function_.valueTypes.at(instruction.operands[index].value);
        arguments += std::format(", {} {}", qbeSuffixFor(typeId), operandTemp(instruction.operands[index]));
      }
      const auto resultTypeId = function_.valueTypes.at(instruction.result.value);
      const auto resultType = qtypeOf(resultTypeId);
      if (resultType == QType::None)
        line(std::format("call {}({})", function, arguments));
      else
        line(std::format("{} ={} call {}({})", valueTemps_.at(instruction.result.value),
                         resultType == QType::Aggregate ? "l" : qbeSuffixFor(resultTypeId), function, arguments));
      return;
    }
    case InstructionKind::LoadRef:
    {
      const auto reference = operandTemp(instruction.operands[0]);
      const auto loaded = fresh();
      line(std::format("{} =l call $ngrt_ref_load(l {})", loaded, reference));
      const auto resultType = qtypeOf(function_.valueTypes.at(instruction.result.value));
      const auto resultTypeId = function_.valueTypes.at(instruction.result.value);
      std::string value = loaded;
      // Copy-first semantics: `*r` yields a deep copy of aggregates
      // (the VM deep-copies on every reference load).
      if (needsClone(resultTypeId))
        value = emitClone(loaded, resultTypeId);
      bindResult(instruction, resultType, std::format("copy {}", castFromSlot(value, resultType)));
      return;
    }
    case InstructionKind::BoxClone:
    {
      // Box.clone(): shared-borrow the receiver box, deep-clone its cell
      // payload, and rebuild a fresh Box<T> value (no function call is
      // emitted; this is the derived `clone` method on Box).
      const auto reference = operandTemp(instruction.operands[0]);
      const auto box = fresh();
      line(std::format("{} =l call $ngrt_ref_load(l {})", box, reference));
      const auto resultType = qtypeOf(function_.valueTypes.at(instruction.result.value));
      const auto resultTypeId = function_.valueTypes.at(instruction.result.value);
      const auto clonedBox = emitBoxClone(box, resultTypeId);
      bindResult(instruction, resultType, std::format("copy {}", castFromSlot(clonedBox, resultType)));
      return;
    }
    case InstructionKind::WrapUnion:
    {
      // A member value coerced to a union: box it with its member's tag.
      const auto memberType = function_.valueTypes.at(instruction.source->value);
      const auto unionType = resultTypeId(instruction);
      bindUnionBox(instruction, unionType, unionMemberFor(unionType, memberType), operandTemp(*instruction.source),
                   qtypeOf(memberType));
      return;
    }
    case InstructionKind::AssignPlace:
    {
      const auto value = operandTemp(instruction.operands.back());
      const auto valueType = qtypeOf(function_.valueTypes.at(instruction.operands.back().value));
      if (instruction.placeRootRef)
      {
        const auto reference = operandTemp(*instruction.placeRootRef);
        if (instruction.placeSteps.empty())
        {
          const auto address = fresh();
          line(std::format("{} =l call $ngrt_ref_addr(l {})", address, reference));
          if (valueType == QType::Aggregate)
          {
            const auto clone = emitClone(value, function_.valueTypes.at(instruction.operands.back().value));
            line(std::format("storel {}, {}", clone, address));
          }
          else
          {
            line(std::format("store{} {}, {}", suffix(valueType), value, address));
          }
          return;
        }
        // Ref-rooted place with a path (`(*self).field := ...`): the
        // ref's prefix steps walk to the containing aggregate value,
        // then the static path steps walk to the target address.
        const auto current = fresh();
        line(std::format("{} =l call $ngrt_ref_load(l {})", current, reference));
        std::string address = current;
        const auto referent =
            function_.typeDescriptors[function_.valueTypes.at(instruction.placeRootRef->value).value].element;
        for (const auto &step : normalizeSteps(referent, instruction.placeSteps))
        {
          const auto next = fresh();
          if (step.kind == 1)
          {
            line(std::format("{} =l call $ngrt_arr_addr(l {}, l {})", next, address, step.payload));
          }
          else if (step.kind == 2)
          {
            line(std::format("{} =l add {}, {}", next, address, std::format("{}", 16 + 8 * std::stoll(step.payload))));
          }
          else
          {
            line(std::format("{} =l add {}, {}", next, address, step.payload));
          }
          address = next;
        }
        line(std::format("store{} {}, {}", suffix(valueType), value, address));
        return;
      }
      if (!instruction.placeRootLocal)
        throw LoweringError(std::format("native lowering (M2): malformed place in `{}`", function_.name));
      // Local-rooted place: walk the static steps from the slot value to
      // the target address.
      const auto rootLocal = instruction.placeRootLocal->value;
      const auto rootSlot = localSlots_.at(rootLocal);
      if (instruction.placeSteps.empty())
      {
        const auto localType = function_.localTypes.at(rootLocal);
        const auto valueTypeId = function_.valueTypes.at(instruction.operands.back().value);
        std::string wrappedBox;
        std::string storedValue = value;
        if (isUnionType(localType) && !isUnionType(valueTypeId))
        {
          wrappedBox = fresh();
          line(std::format("{} =l call $ngrt_alloc(l 16)", wrappedBox));
          line(std::format("storel {}, {}", unionMemberFor(localType, valueTypeId), wrappedBox));
          const auto address = fresh();
          line(std::format("{} =l add {}, 8", address, wrappedBox));
          line(std::format("store{} {}, {}", suffix(valueType), value, address));
          storedValue = wrappedBox;
        }
        if (isCellLocal(rootLocal))
        {
          // `:=` mutates the current cell (outstanding refs observe it).
          const auto cell = fresh();
          line(std::format("{} =l loadl {}", cell, rootSlot));
          if (valueType == QType::Aggregate)
          {
            // Copy-first semantics: aggregates are cloned onto the heap
            // so loop-rebuilt stack literals cannot alias into cells.
            const auto clone = emitClone(storedValue, valueTypeId);
            line(std::format("storel {}, {}", clone, cell));
          }
          else
          {
            line(std::format("storel {}, {}", storedValue, cell));
          }
        }
        else if (qtypeOf(localType) == QType::Aggregate)
        {
          // Copy-first semantics: the assigned aggregate is copied into
          // the local's stack slot.
          emitAggregateCopy(rootSlot, storedValue, localType);
        }
        else
        {
          line(std::format("store{} {}, {}", suffix(valueType), storedValue, rootSlot));
        }
        return;
      }
      std::string current;
      if (isCellLocal(rootLocal))
        current = loadLocal(rootLocal);
      else if (qtypeOf(function_.localTypes.at(rootLocal)) == QType::Aggregate)
        current = rootSlot; // the slot IS the aggregate object
      else
      {
        current = fresh();
        line(std::format("{} =l loadl {}", current, rootSlot));
      }
      std::string address = current;
      for (const auto &step : normalizeSteps(function_.localTypes.at(rootLocal), instruction.placeSteps))
      {
        const auto next = fresh();
        if (step.kind == 1)
        {
          line(std::format("{} =l call $ngrt_arr_addr(l {}, l {})", next, address, step.payload));
        }
        else if (step.kind == 2)
        {
          // Constant index into an array/tuple: header + element offset.
          line(std::format("{} =l add {}, {}", next, address, std::format("{}", 16 + 8 * std::stoll(step.payload))));
        }
        else
        {
          line(std::format("{} =l add {}, {}", next, address, step.payload));
        }
        address = next;
      }
      line(std::format("store{} {}, {}", suffix(valueType), value, address));
      return;
    }
    default:
      throw LoweringError(std::format("native lowering (M1): instruction kind {} is not supported yet in `{}`",
                                      static_cast<int>(instruction.kind), function_.name));
    }
  }
  void FunctionLowerer::lowerEvaluate(const Instruction &instruction)
  {
    if (instruction.callTarget)
    {
      lowerCall(instruction);
      return;
    }
    const auto kind = instruction.expressionKind;
    const auto payload = instruction.payload;
    switch (kind)
    {
    case ExpressionKind::IntegerLiteral:
    case ExpressionKind::BooleanLiteral:
    {
      if (isUnionType(resultTypeId(instruction)))
      {
        const auto temp = fresh();
        line(std::format("{} =l copy {}", temp, payload));
        bindUnionBox(instruction, resultTypeId(instruction),
                     unionMemberFor(resultTypeId(instruction), typecheck::builtin::I64), temp, QType::Long);
      }
      else if (typecheck::isFloatBuiltin(resultTypeId(instruction)))
      {
        // DEC-A: an unsuffixed integer literal adopted a float context
        // (`f == 1`); emit the value as the double it now is.
        bindResult(instruction, QType::Double, std::format("copy {}", doubleConstant(static_cast<double>(payload))));
      }
      else
      {
        bindResult(instruction, QType::Long, std::format("copy {}", payload));
      }
      return;
    }
    case ExpressionKind::FloatLiteral:
    {
      if (isUnionType(resultTypeId(instruction)))
      {
        const auto temp = fresh();
        line(std::format("{} =d copy {}", temp, doubleConstant(std::bit_cast<double>(payload))));
        bindUnionBox(instruction, resultTypeId(instruction),
                     unionMemberFor(resultTypeId(instruction), typecheck::builtin::F64), temp, QType::Double);
      }
      else
      {
        bindResult(instruction, QType::Double, std::format("copy {}", doubleConstant(std::bit_cast<double>(payload))));
      }
      return;
    }
    case ExpressionKind::StringLiteral:
    {
      const auto name = std::format("$ngstr_{}_{}", function_.source.value, dataItems_.size());
      dataItems_.push_back(std::format("data {} = {{ l {}, b \"{}\", b 0 }}", name, instruction.text.size(),
                                       escapeQbeString(instruction.text)));
      const auto text = fresh();
      line(std::format("{} =l copy {}", text, name));
      if (isUnionType(resultTypeId(instruction)))
        bindUnionBox(instruction, resultTypeId(instruction),
                     unionMemberFor(resultTypeId(instruction), typecheck::builtin::String), text, QType::Long);
      else
        bindResult(instruction, QType::Long, std::format("copy {}", text));
      return;
    }
    case ExpressionKind::ArrayLiteral:
    case ExpressionKind::TupleLiteral:
      lowerAggregateLiteral(instruction);
      return;
    case ExpressionKind::EnumLiteral:
    {
      // Enum object: { l tag, l payload-word } — multi-field variants
      // carry a tuple pointer in the payload word.
      const auto variant = static_cast<uint32_t>(static_cast<uint64_t>(payload) >> 32);
      const auto pointer = fresh();
      line(std::format("{} =l alloc8 16", pointer));
      line(std::format("storel {}, {}", variant, pointer));
      const auto wordAddress = fresh();
      line(std::format("{} =l add {}, 8", wordAddress, pointer));
      if (instruction.operands.empty())
      {
        line(std::format("storel 0, {}", wordAddress));
      }
      else
      {
        const auto elementType = qtypeOf(function_.valueTypes.at(instruction.operands[0].value));
        line(std::format("storel {}, {}", castForSlot(operandTemp(instruction.operands[0]), elementType), wordAddress));
      }
      bindResult(instruction, QType::Aggregate, std::format("copy {}", pointer));
      return;
    }
    case ExpressionKind::StructLiteral:
      lowerStructLiteral(instruction);
      return;
    case ExpressionKind::Member:
    {
      const auto receiver = operandTemp(instruction.operands[0]);
      const auto field = static_cast<size_t>(payload);
      const auto receiverType = function_.valueTypes.at(instruction.operands[0].value);
      const auto address = fresh();
      line(std::format("{} =l add {}, {}", address, receiver, fieldOffset(receiverType, field)));
      const auto fieldType = function_.typeDescriptors[receiverType.value].elements[field];
      const auto loaded = loadField(address, fieldType);
      bindResult(instruction, qtypeOf(fieldType), std::format("copy {}", loaded));
      return;
    }
    case ExpressionKind::Index:
    {
      const auto receiver = operandTemp(instruction.operands[0]);
      const auto index = operandTemp(instruction.operands[1]);
      const auto loaded = fresh();
      const TypeId receiverType = function_.valueTypes.at(instruction.operands[0].value);
      if (receiverType.value < function_.typeDescriptors.size() &&
          function_.typeDescriptors[receiverType.value].kind == TypeKind::Span)
        line(std::format("{} =l call $ngrt_span_get(l {}, l {})", loaded, receiver, index));
      else
        line(std::format("{} =l call $ngrt_arr_get(l {}, l {})", loaded, receiver, index));
      const auto resultType = qtypeOf(function_.valueTypes.at(instruction.result.value));
      bindResult(instruction, resultType, std::format("copy {}", castFromSlot(loaded, resultType)));
      return;
    }
    case ExpressionKind::ResolvedName:
    {
      const uint32_t local = static_cast<uint32_t>(payload);
      const auto type = localQType(local);
      if (ssaLocals_.contains(local))
      {
        const auto found = currentTemps_.find(local);
        if (found == currentTemps_.end())
          throw LoweringError(std::format("native lowering (M4): SSA local {} is read before it is bound in `{}`",
                                          local, function_.name));
        bindResult(instruction, type, std::format("copy {}", found->second));
      }
      else if (isCellLocal(local))
        bindResult(instruction, type, std::format("copy {}", castFromSlot(loadLocal(local), type)));
      else if (type == QType::Aggregate)
        bindResult(instruction, type, std::format("copy {}", localSlots_.at(local)));
      else
        bindResult(instruction, type, std::format("load{} {}", suffix(type), localSlots_.at(local)));
      if (const auto found = function_.localTypes.find(local);
          found != function_.localTypes.end() && isUnionType(found->second))
        unionBoxedValues_.emplace(instruction.result.value, found->second);
      return;
    }
    case ExpressionKind::Grouped:
    {
      const auto source = operandTemp(instruction.operands[0]);
      bindResult(instruction, qtypeOf(function_.valueTypes.at(instruction.result.value)),
                 std::format("copy {}", source));
      if (const auto found = unionBoxedValues_.find(instruction.operands[0].value); found != unionBoxedValues_.end())
        unionBoxedValues_.emplace(instruction.result.value, found->second);
      return;
    }
    case ExpressionKind::Prefix:
      lowerPrefix(instruction);
      return;
    case ExpressionKind::Binary:
      lowerBinary(instruction);
      return;
    case ExpressionKind::Call:
      // An unresolved trait-slot call inside a generic original body:
      // the VM throws on the same path (evaluateInstruction rejects
      // ExpressionKind::Call), so this code is dead for every reachable
      // program. Emit a defined-but-halting trap.
      bindResult(instruction, qtypeOf(function_.valueTypes.at(instruction.result.value)), "copy 0");
      {
        const auto unused = fresh();
        line(std::format("{} =l call $ngrt_panic()", unused));
      }
      return;
    default:
      throw LoweringError(std::format("native lowering (M1): expression kind {} is not supported yet in `{}`",
                                      static_cast<int>(kind), function_.name));
    }
  }
  void FunctionLowerer::lowerPrefix(const Instruction &instruction)
  {
    const auto payload = instruction.payload;
    const auto operand = operandTemp(instruction.operands[0]);
    const auto operandType = function_.valueTypes.at(instruction.operands[0].value);
    const auto resultType = qtypeOf(function_.valueTypes.at(instruction.result.value));
    // Unary +, move, and clone are copies for scalars. Cell/Box clone
    // allocates a fresh cell; other aggregates deep-copy on `clone`.
    if (payload == 3 || payload == 4 || payload == 5)
    {
      if (payload == 5 && (isBoxType(operandType) || isCellType(operandType)))
      {
        const auto cloned = emitBoxClone(operand, operandType);
        bindResult(instruction, resultType, std::format("copy {}", castFromSlot(cloned, resultType)));
        return;
      }
      if (payload == 5 && needsClone(operandType))
      {
        bindResult(instruction, resultType, std::format("copy {}", emitClone(operand, operandType)));
        return;
      }
      bindResult(instruction, resultType, std::format("copy {}", operand));
      if (payload == 3 && typecheck::isIntegerBuiltin(operandType))
        applyWidthCheck(instruction);
      if (const auto found = unionBoxedValues_.find(instruction.operands[0].value); found != unionBoxedValues_.end())
        unionBoxedValues_.emplace(instruction.result.value, found->second);
      return;
    }
    if (payload == 2)
    {
      if (isFloat(operandType))
        bindResult(instruction, resultType, std::format("neg {}", operand));
      else
        emitCheckedOp(instruction, "checked_neg", std::format("l {}", operand));
      return;
    }
    if (payload == 1)
    {
      if (isFloat(operandType))
        throw LoweringError(std::format("native lowering (M1): logical negation of a float in `{}`", function_.name));
      integerCompare(instruction, "ceql", operand, "0");
      return;
    }
    throw LoweringError(
        std::format("native lowering (M1): unsupported prefix payload {} in `{}`", payload, function_.name));
  }
  [[nodiscard]] auto FunctionLowerer::promoteToDouble(const std::string &temp, TypeId type) -> std::string
  {
    if (isFloat(type))
      return temp;
    const auto converted = fresh();
    line(std::format("{} =d sltof {}", converted, temp));
    return converted;
  }
  void FunctionLowerer::lowerBinary(const Instruction &instruction)
  {
    const auto payload = instruction.payload;
    const auto left = operandTemp(instruction.operands[0]);
    const auto right = operandTemp(instruction.operands[1]);
    const auto leftType = function_.valueTypes.at(instruction.operands[0].value);
    const auto rightType = function_.valueTypes.at(instruction.operands[1].value);

    // Unions compare against member values: (tag == member) && payload.
    // Union-ness is tracked by data flow (unionBoxedValues_), because the
    // checker records union operands as their member type at comparison
    // sites.
    const auto unionTypeOf = [&](ValueId value) -> std::optional<TypeId>
    {
      if (const auto found = unionBoxedValues_.find(value.value); found != unionBoxedValues_.end())
        return found->second;
      const auto type = function_.valueTypes.at(value.value);
      if (isUnionType(type))
        return type;
      return std::nullopt;
    };
    const auto leftUnion = unionTypeOf(instruction.operands[0]);
    const auto rightUnion = unionTypeOf(instruction.operands[1]);
    if (leftUnion || rightUnion)
    {
      lowerUnionCompare(instruction, leftUnion ? *leftUnion : *rightUnion, leftUnion.has_value());
      return;
    }

    // Strings: concatenation and content equality/inequality.
    if (leftType == typecheck::builtin::String)
    {
      if (payload == 1)
      {
        bindResult(instruction, QType::Long, std::format("call $ngrt_str_concat(l {}, l {})", left, right));
        return;
      }
      if (payload == 6 || payload == 7)
      {
        const auto compared = fresh();
        line(std::format("{} =w call $ngrt_str_eq(l {}, l {})", compared, left, right));
        const auto extended = fresh();
        line(std::format("{} =l extsw {}", extended, compared));
        if (payload == 6)
          bindResult(instruction, QType::Long, std::format("copy {}", extended));
        else
          bindResult(instruction, QType::Long, std::format("xor {}, 1", extended));
        return;
      }
      if (payload >= 8 && payload <= 11)
      {
        const auto order = fresh();
        line(std::format("{} =l call $ngrt_str_cmp(l {}, l {})", order, left, right));
        const char *qbeOp = payload == 8 ? "csltl" : payload == 9 ? "cslel" : payload == 10 ? "csgtl" : "csgel";
        integerCompare(instruction, qbeOp, order, "0");
        return;
      }
      throw LoweringError(
          std::format("native lowering (M2): unsupported string binary payload {} in `{}`", payload, function_.name));
    }
    // Range construction: { l start, l end }.
    if (payload == 19)
    {
      const auto pointer = fresh();
      line(std::format("{} =l call $ngrt_alloc(l 16)", pointer));
      line(std::format("storel {}, {}", left, pointer));
      const auto endAddress = fresh();
      line(std::format("{} =l add {}, 8", endAddress, pointer));
      line(std::format("storel {}, {}", right, endAddress));
      bindResult(instruction, QType::Long, std::format("copy {}", pointer));
      return;
    }

    if (isFloat(leftType) || isFloat(rightType))
    {
      const auto leftDouble = promoteToDouble(left, leftType);
      const auto rightDouble = promoteToDouble(right, rightType);
      switch (payload)
      {
      case 1:
        bindResult(instruction, QType::Double, std::format("add {}, {}", leftDouble, rightDouble));
        return;
      case 2:
        bindResult(instruction, QType::Double, std::format("sub {}, {}", leftDouble, rightDouble));
        return;
      case 3:
        bindResult(instruction, QType::Double, std::format("mul {}, {}", leftDouble, rightDouble));
        return;
      case 4:
        bindResult(instruction, QType::Double, std::format("div {}, {}", leftDouble, rightDouble));
        return;
      case 6:
      {
        const auto compared = fresh();
        line(std::format("{} =w ceqd {}, {}", compared, leftDouble, rightDouble));
        bindResult(instruction, QType::Long, std::format("extsw {}", compared));
        return;
      }
      case 7:
      {
        const auto compared = fresh();
        line(std::format("{} =w cned {}, {}", compared, leftDouble, rightDouble));
        bindResult(instruction, QType::Long, std::format("extsw {}", compared));
        return;
      }
      case 8:
      {
        const auto compared = fresh();
        line(std::format("{} =w cltd {}, {}", compared, leftDouble, rightDouble));
        bindResult(instruction, QType::Long, std::format("extsw {}", compared));
        return;
      }
      case 9:
      {
        const auto compared = fresh();
        line(std::format("{} =w cled {}, {}", compared, leftDouble, rightDouble));
        bindResult(instruction, QType::Long, std::format("extsw {}", compared));
        return;
      }
      case 10:
      {
        const auto compared = fresh();
        line(std::format("{} =w cgtd {}, {}", compared, leftDouble, rightDouble));
        bindResult(instruction, QType::Long, std::format("extsw {}", compared));
        return;
      }
      case 11:
      {
        const auto compared = fresh();
        line(std::format("{} =w cged {}, {}", compared, leftDouble, rightDouble));
        bindResult(instruction, QType::Long, std::format("extsw {}", compared));
        return;
      }
      default:
        throw LoweringError(
            std::format("native lowering (M1): unsupported float binary payload {} in `{}`", payload, function_.name));
      }
    }

    // INC-4 (F1①) / INC-5 (F1③): ordering comparisons and checked
    // arithmetic follow operand signedness. u64 values live in 64-bit
    // slots bit-pattern-wise; signed instructions would treat u64::MAX
    // as -1 and silently accept wrapping arithmetic.
    const auto usesUnsignedOps =
        typecheck::isUnsignedIntegerBuiltin(leftType) || typecheck::isUnsignedIntegerBuiltin(rightType);
    switch (payload)
    {
    case 1:
      emitCheckedOp(instruction, usesUnsignedOps ? "checked_uadd" : "checked_add",
                    std::format("l {}, l {}", left, right));
      return;
    case 2:
      emitCheckedOp(instruction, usesUnsignedOps ? "checked_usub" : "checked_sub",
                    std::format("l {}, l {}", left, right));
      return;
    case 3:
      emitCheckedOp(instruction, usesUnsignedOps ? "checked_umul" : "checked_mul",
                    std::format("l {}, l {}", left, right));
      return;
    case 4:
      emitCheckedOp(instruction, usesUnsignedOps ? "checked_udiv" : "checked_div",
                    std::format("l {}, l {}", left, right));
      return;
    case 5:
      emitCheckedOp(instruction, usesUnsignedOps ? "checked_urem" : "checked_rem",
                    std::format("l {}, l {}", left, right));
      return;
    case 6:
      integerCompare(instruction, "ceql", left, right);
      return;
    case 7:
      integerCompare(instruction, "cnel", left, right);
      return;
    case 8:
      integerCompare(instruction, usesUnsignedOps ? "cultl" : "csltl", left, right);
      return;
    case 9:
      integerCompare(instruction, usesUnsignedOps ? "culel" : "cslel", left, right);
      return;
    case 10:
      integerCompare(instruction, usesUnsignedOps ? "cugtl" : "csgtl", left, right);
      return;
    case 11:
      integerCompare(instruction, usesUnsignedOps ? "cugel" : "csgel", left, right);
      return;
    case 12:
    case 13:
    {
      // Short-circuit-free boolean semantics: both sides are compared to
      // zero and combined (VM evaluates both operands eagerly).
      const auto leftBool = fresh();
      const auto rightBool = fresh();
      const auto combined = fresh();
      line(std::format("{} =w cnel {}, 0", leftBool, left));
      line(std::format("{} =w cnel {}, 0", rightBool, right));
      line(std::format("{} =w {} {}, {}", combined, payload == 12 ? "and" : "or", leftBool, rightBool));
      bindResult(instruction, QType::Long, std::format("extsw {}", combined));
      return;
    }
    case 14:
      bindResult(instruction, QType::Long, std::format("and {}, {}", left, right));
      return;
    case 15:
      bindResult(instruction, QType::Long, std::format("or {}, {}", left, right));
      return;
    case 16:
      bindResult(instruction, QType::Long, std::format("xor {}, {}", left, right));
      return;
    case 17:
      emitCheckedOp(instruction, "checked_shl", std::format("l {}, l {}", left, right));
      return;
    case 18:
      emitCheckedOp(instruction, "checked_shr", std::format("l {}, l {}", left, right));
      return;
    default:
      throw LoweringError(
          std::format("native lowering (M1): unsupported integer binary payload {} in `{}`", payload, function_.name));
    }
  }
  void FunctionLowerer::lowerTerminator(const Terminator &terminator, size_t blockIndex)
  {
    switch (terminator.kind)
    {
    case TerminatorKind::Return:
      if (terminator.arguments.empty())
      {
        line(function_.name == "main" ? "ret 0" : "ret");
      }
      else if (qtypeOf(function_.valueTypes.at(terminator.arguments[0].value)) == QType::Aggregate &&
               function_.name != "main")
      {
        const auto clone =
            emitClone(operandTemp(terminator.arguments[0]), function_.valueTypes.at(terminator.arguments[0].value));
        line(std::format("ret {}", clone));
      }
      else if (function_.name == "main")
      {
        // String/float mains have no meaningful exit code: print the
        // value (matching the VM driver's "with value ...") and exit 0.
        const auto value = operandTemp(terminator.arguments[0]);
        const auto type = function_.valueTypes.at(terminator.arguments[0].value);
        if (type == typecheck::builtin::String)
        {
          const auto unused = fresh();
          line(std::format("{} =l call $ngrt_print_str(l {})", unused, value));
          line("ret 0");
        }
        else if (typecheck::isFloatBuiltin(type))
        {
          const auto unused = fresh();
          line(std::format("{} =l call $ngrt_print_f64(d {})", unused, value));
          line("ret 0");
        }
        else
        {
          line(std::format("ret {}", value));
        }
      }
      else
      {
        line(std::format("ret {}", operandTemp(terminator.arguments[0])));
      }
      return;
    case TerminatorKind::Jump:
    case TerminatorKind::LoopBackedge:
      line(std::format("jmp {}", labels_.at(terminator.targets[0].value)));
      return;
    case TerminatorKind::Branch:
      line(std::format("jnz {}, {}, {}", operandTemp(terminator.arguments[0]), labels_.at(terminator.targets[0].value),
                       labels_.at(terminator.targets[1].value)));
      return;
    case TerminatorKind::TailRecur:
      for (size_t index = 0; index < function_.parameterLocals.size(); ++index)
      {
        const auto local = function_.parameterLocals[index].value;
        if (isCellLocal(local))
          bindFreshCell(local, operandTemp(terminator.arguments[index]));
        else
          line(std::format("store{} {}, {}", suffix(localQTypes_.at(local)), operandTemp(terminator.arguments[index]),
                           localSlots_.at(local)));
      }
      line(std::format("jmp {}", labels_.front()));
      return;
    }
  }

} // namespace NG::native::detail
