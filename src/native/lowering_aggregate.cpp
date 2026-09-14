// AI-generated code; reviewed for this repository's vNext rewrite.
// ARCH-2: Aggregate lowering: structs, tuples, unions, places, deep clones.
#include "function_lowerer.hpp"

namespace NG::native::detail
{

  [[nodiscard]] auto FunctionLowerer::unionMemberFor(TypeId unionType, TypeId memberType) -> int64_t
  {
    const auto &descriptor = function_.typeDescriptors[unionType.value];
    const auto matches = [&](TypeId candidate)
    {
      if (memberType == typecheck::builtin::String)
        return candidate == typecheck::builtin::String;
      if (typecheck::isFloatBuiltin(memberType))
        return typecheck::isFloatBuiltin(candidate);
      if (memberType == typecheck::builtin::Bool)
        return candidate == typecheck::builtin::Bool;
      return typecheck::isIntegerBuiltin(candidate);
    };
    for (size_t index = 0; index < descriptor.elements.size(); ++index)
      if (matches(descriptor.elements[index]))
        return static_cast<int64_t>(index);
    throw LoweringError(
        std::format("native lowering (M2): value does not match any member of union `{}`", descriptor.name));
  }
  void FunctionLowerer::bindUnionBox(const Instruction &instruction, TypeId unionType, int64_t member,
                                     const std::string &payloadTemp, QType payloadType)
  {
    const auto pointer = fresh();
    line(std::format("{} =l call $ngrt_alloc(l 16)", pointer));
    line(std::format("storel {}, {}", member, pointer));
    const auto address = fresh();
    line(std::format("{} =l add {}, 8", address, pointer));
    line(std::format("store{} {}, {}", suffix(payloadType), payloadTemp, address));
    bindResult(instruction, QType::Long, std::format("copy {}", pointer));
    unionBoxedValues_.emplace(instruction.result.value, unionType);
  }
  [[nodiscard]] auto FunctionLowerer::memberSuffix(TypeId type) -> std::string
  {
    if (type == typecheck::builtin::I8 || type == typecheck::builtin::U8)
      return "b";
    if (type == typecheck::builtin::I16 || type == typecheck::builtin::U16)
      return "h";
    if (type == typecheck::builtin::I32 || type == typecheck::builtin::U32)
      return "w";
    if (type == typecheck::builtin::F32)
      return "s";
    if (type == typecheck::builtin::F64)
      return "d";
    return "l";
  }
  [[nodiscard]] auto FunctionLowerer::memberLayout(TypeId type) -> std::pair<size_t, size_t>
  {
    if (type == typecheck::builtin::I8 || type == typecheck::builtin::U8)
      return {1, 1};
    if (type == typecheck::builtin::I16 || type == typecheck::builtin::U16)
      return {2, 2};
    if (type == typecheck::builtin::I32 || type == typecheck::builtin::U32 || type == typecheck::builtin::F32)
      return {4, 4};
    return {8, 8};
  }
  [[nodiscard]] auto FunctionLowerer::fieldOffset(TypeId structType, size_t field) -> size_t
  {
    const auto &descriptor = function_.typeDescriptors[structType.value];
    size_t offset = 0;
    for (size_t index = 0; index < field; ++index)
    {
      const auto [memberSize, memberAlign] = memberLayout(descriptor.elements[index]);
      offset = (offset + memberAlign - 1) / memberAlign * memberAlign + memberSize;
    }
    const auto [memberSize, memberAlign] = memberLayout(descriptor.elements[field]);
    return (offset + memberAlign - 1) / memberAlign * memberAlign;
  }
  [[nodiscard]] auto FunctionLowerer::aggregateSize(TypeId type) -> size_t
  {
    const auto &descriptor = function_.typeDescriptors[type.value];
    if (descriptor.kind == TypeKind::Enum)
      return 16;
    size_t size = 0;
    size_t align = 1;
    for (const auto field : descriptor.elements)
    {
      const auto [memberSize, memberAlign] = memberLayout(field);
      size = (size + memberAlign - 1) / memberAlign * memberAlign + memberSize;
      align = std::max(align, memberAlign);
    }
    return (size + align - 1) / align * align;
  }
  void FunctionLowerer::storeField(const std::string &address, const std::string &value, TypeId fieldType)
  {
    if (fieldType == typecheck::builtin::F32)
    {
      const auto single = fresh();
      line(std::format("{} =s truncd {}", single, value));
      line(std::format("stores {}, {}", single, address));
    }
    else if (typecheck::isFloatBuiltin(fieldType))
    {
      line(std::format("stored {}, {}", value, address));
    }
    else if (fieldType == typecheck::builtin::I8 || fieldType == typecheck::builtin::U8)
    {
      line(std::format("storeb {}, {}", value, address));
    }
    else if (fieldType == typecheck::builtin::I16 || fieldType == typecheck::builtin::U16)
    {
      line(std::format("storeh {}, {}", value, address));
    }
    else if (fieldType == typecheck::builtin::I32 || fieldType == typecheck::builtin::U32)
    {
      line(std::format("storew {}, {}", value, address));
    }
    else
    {
      line(std::format("storel {}, {}", castForSlot(value, qtypeOf(fieldType)), address));
    }
  }
  [[nodiscard]] auto FunctionLowerer::loadField(const std::string &address, TypeId fieldType) -> std::string
  {
    if (fieldType == typecheck::builtin::F32)
    {
      const auto single = fresh();
      line(std::format("{} =s loads {}", single, address));
      const auto extended = fresh();
      line(std::format("{} =d exts {}", extended, single));
      return extended;
    }
    const auto loaded = fresh();
    const char *loadOp = "loadl";
    if (fieldType == typecheck::builtin::I8)
      loadOp = "loadsb";
    else if (fieldType == typecheck::builtin::U8)
      loadOp = "loadub";
    else if (fieldType == typecheck::builtin::I16)
      loadOp = "loadsh";
    else if (fieldType == typecheck::builtin::U16)
      loadOp = "loaduh";
    else if (fieldType == typecheck::builtin::I32)
      loadOp = "loadsw";
    else if (fieldType == typecheck::builtin::U32)
      loadOp = "loaduw";
    line(std::format("{} =l {} {}", loaded, loadOp, address));
    if (typecheck::isFloatBuiltin(fieldType))
    {
      const auto casted = fresh();
      line(std::format("{} =d cast {}", casted, loaded));
      return casted;
    }
    return loaded;
  }
  void FunctionLowerer::emitAggregateCopy(const std::string &destination, const std::string &source, TypeId type)
  {
    const auto &descriptor = function_.typeDescriptors[type.value];
    if (descriptor.kind == TypeKind::Enum)
    {
      for (size_t index = 0; index < 2; ++index)
      {
        const auto sourceAddress = fresh();
        line(std::format("{} =l add {}, {}", sourceAddress, source, index * 8));
        const auto value = fresh();
        line(std::format("{} =l loadl {}", value, sourceAddress));
        const auto target = fresh();
        line(std::format("{} =l add {}, {}", target, destination, index * 8));
        line(std::format("storel {}, {}", value, target));
      }
      return;
    }
    for (size_t index = 0; index < descriptor.fieldNames.size(); ++index)
    {
      const auto offset = fieldOffset(type, index);
      const auto sourceAddress = fresh();
      line(std::format("{} =l add {}, {}", sourceAddress, source, offset));
      const auto value = loadField(sourceAddress, descriptor.elements[index]);
      const auto target = fresh();
      line(std::format("{} =l add {}, {}", target, destination, offset));
      storeField(target, value, descriptor.elements[index]);
    }
  }
  void FunctionLowerer::lowerStructLiteral(const Instruction &instruction)
  {
    const auto typeId = function_.valueTypes.at(instruction.result.value);
    const auto &descriptor = function_.typeDescriptors[typeId.value];
    const size_t count = descriptor.fieldNames.size();
    if (count != instruction.operands.size())
      throw LoweringError(std::format("native lowering (M2): struct literal arity mismatch in `{}`", function_.name));
    const auto pointer = fresh();
    line(std::format("{} =l alloc8 {}", pointer, aggregateSize(typeId)));
    for (size_t index = 0; index < count; ++index)
    {
      const auto address = fresh();
      line(std::format("{} =l add {}, {}", address, pointer, fieldOffset(typeId, index)));
      storeField(address, operandTemp(instruction.operands[index]), descriptor.elements[index]);
    }
    bindResult(instruction, QType::Aggregate, std::format("copy {}", pointer));
  }
  void FunctionLowerer::lowerTupleSplice(const Instruction &instruction)
  {
    size_t plainCount = 0;
    for (const auto operand : instruction.operands)
    {
      const auto type = function_.valueTypes.at(operand.value);
      if (type.value >= function_.typeDescriptors.size() ||
          function_.typeDescriptors[type.value].kind != TypeKind::Tuple)
        ++plainCount;
    }
    const auto total = fresh();
    line(std::format("{} =l copy {}", total, plainCount));
    for (const auto operand : instruction.operands)
    {
      const auto type = function_.valueTypes.at(operand.value);
      if (type.value < function_.typeDescriptors.size() &&
          function_.typeDescriptors[type.value].kind == TypeKind::Tuple)
      {
        const auto length = fresh();
        line(std::format("{} =l loadl {}", length, operandTemp(operand)));
        line(std::format("{} =l add {}, {}", total, total, length));
      }
    }
    const auto bytes = fresh();
    line(std::format("{} =l mul {}, 8", bytes, total));
    const auto allocation = fresh();
    line(std::format("{} =l add {}, 16", allocation, bytes));
    const auto pointer = fresh();
    line(std::format("{} =l call $ngrt_alloc(l {})", pointer, allocation));
    line(std::format("storel {}, {}", total, pointer));
    const auto capAddress = fresh();
    line(std::format("{} =l add {}, 8", capAddress, pointer));
    line(std::format("storel {}, {}", total, capAddress));
    auto destination = fresh();
    line(std::format("{} =l add {}, 16", destination, pointer));
    for (const auto operand : instruction.operands)
    {
      const auto type = function_.valueTypes.at(operand.value);
      if (type.value < function_.typeDescriptors.size() &&
          function_.typeDescriptors[type.value].kind == TypeKind::Tuple)
      {
        const auto length = fresh();
        line(std::format("{} =l loadl {}", length, operandTemp(operand)));
        const auto unused = fresh();
        line(std::format("{} =l call $ngrt_copy_elements(l {}, l {}, l {})", unused, destination, operandTemp(operand),
                         length));
        const auto lengthBytes = fresh();
        line(std::format("{} =l mul {}, 8", lengthBytes, length));
        const auto next = fresh();
        line(std::format("{} =l add {}, {}", next, destination, lengthBytes));
        destination = next;
      }
      else
      {
        const auto elementType = qtypeOf(type);
        line(std::format("storel {}, {}", castForSlot(operandTemp(operand), elementType), destination));
        const auto next = fresh();
        line(std::format("{} =l add {}, 8", next, destination));
        destination = next;
      }
    }
    bindResult(instruction, QType::Long, std::format("copy {}", pointer));
  }
  [[nodiscard]] auto FunctionLowerer::emitRefObject(uint32_t rootLocal, const std::vector<flowir::PlaceStep> &steps)
      -> std::string
  {
    // The ref's root is the local's current cell (captured now); cell
    // locals keep the snapshot semantics of the VM's shared cells.
    std::string root;
    if (isCellLocal(rootLocal))
    {
      const auto cell = fresh();
      line(std::format("{} =l loadl {}", cell, localSlots_.at(rootLocal)));
      root = cell;
    }
    else
    {
      root = localSlots_.at(rootLocal);
    }
    const auto rootType = function_.localTypes.at(rootLocal);
    const auto normalized = normalizeSteps(rootType, steps);
    const size_t count = normalized.size();
    const auto pointer = fresh();
    line(std::format("{} =l call $ngrt_alloc(l {})", pointer, 16 + count * 16));
    line(std::format("storel {}, {}", root, pointer));
    const auto countAddress = fresh();
    line(std::format("{} =l add {}, 8", countAddress, pointer));
    line(std::format("storel {}, {}", count, countAddress));
    for (size_t index = 0; index < count; ++index)
    {
      const auto stepAddress = fresh();
      line(std::format("{} =l add {}, {}", stepAddress, pointer, 16 + index * 16));
      const auto payloadAddress = fresh();
      line(std::format("{} =l add {}, 8", payloadAddress, stepAddress));
      line(std::format("storel {}, {}", normalized[index].kind, stepAddress));
      line(std::format("storel {}, {}", normalized[index].payload, payloadAddress));
    }
    return pointer;
  }
  [[nodiscard]] auto FunctionLowerer::isBoxType(TypeId type) -> bool
  {
    if (type.value >= function_.typeDescriptors.size())
      return false;
    const auto &descriptor = function_.typeDescriptors[type.value];
    return descriptor.kind == TypeKind::Struct && descriptor.name == "Box" && descriptor.fieldNames.size() == 1 &&
           descriptor.fieldNames.front() == "handle" && descriptor.typeArguments.size() == 1;
  }
  [[nodiscard]] auto FunctionLowerer::needsClone(TypeId type) -> bool
  {
    if (type.value >= function_.typeDescriptors.size())
      return false;
    const auto kind = function_.typeDescriptors[type.value].kind;
    return kind == TypeKind::Struct || kind == TypeKind::Enum || kind == TypeKind::Tuple ||
           kind == TypeKind::DynamicArray || kind == TypeKind::FixedArray ||
           (kind == TypeKind::Builtin && type == typecheck::builtin::String);
  }
  [[nodiscard]] auto FunctionLowerer::emitClone(const std::string &source, TypeId type) -> std::string
  {
    // Copy-first D-015. A Box<T> value is a handle word. A bare Box copy
    // shares the cell (deref/`(*b).handle` keeps the real handle) but still
    // materializes a heap envelope: the source may be a stack temporary
    // (e.g. a box literal's return), and only the heap envelope survives the
    // originating frame. Envelopes are arena-owned and never released, so
    // re-enveloping is harmless; the cell is released exactly once through
    // the shared handle. Every other aggregate copy is deep so the copy
    // OWNS its tree: box fields inside composites become fresh cells and
    // drop-exactly-once holds (memory.ng: "no payload is ever aliased by
    // the cell").
    depthGuard_ = 0;
    if (isBoxType(type))
      return emitBoxEnvelopeCopy(source);
    return emitCloneDeep(source, type);
  }

  // Allocate a fresh 8-byte envelope on the heap and copy `handle` into it.
  // Used when a bare Box value is copied/returned: the handle is shared with
  // the source box so the underlying cell stays single-owner, but the
  // envelope gets a heap address instead of a stack-allocated one.
  [[nodiscard]] auto FunctionLowerer::emitBoxEnvelopeCopy(const std::string &source) -> std::string
  {
    const auto envelope = fresh();
    line(std::format("{} =l call $ngrt_alloc(l 8)", envelope));
    const auto sourceHandle = fresh();
    line(std::format("{} =l add {}, 0", sourceHandle, source));
    const auto handle = fresh();
    line(std::format("{} =l loadl {}", handle, sourceHandle));
    const auto target = fresh();
    line(std::format("{} =l add {}, 0", target, envelope));
    line(std::format("storel {}, {}", handle, target));
    return envelope;
  }
  [[nodiscard]] auto FunctionLowerer::emitCloneDeep(const std::string &source, TypeId type) -> std::string
  {
    // Full deep copy (used by Box.clone): Box edges become fresh cells and
    // enum payloads are cloned per variant. Recursive types re-enter through
    // generated runtime helpers instead of re-expanding forever.
    if (cloneSkipNextGuard_)
      cloneSkipNextGuard_ = false;
    else if (cloneInProgress_.contains(type.value))
    {
      if (!ngrt_.cloneHelpersEmitted.contains(type.value))
        ngrt_.cloneHelperTypes.insert(type.value);
      const auto helper = fresh();
      line(std::format("{} =l call $ng_clone{}_helper(l {})", helper, type.value, source));
      return helper;
    }
    else
    {
      ++depthGuard_;
      if (depthGuard_ > 128)
        throw LoweringError(
            std::format("clone recursion guard tripped in `{}` at type id {}", function_.name, type.value));
    }
    const auto &descriptor = function_.typeDescriptors[type.value];
    if (isBoxType(type))
    {
      const TypeId element = descriptor.typeArguments[0];
      const auto handleAddress = fresh();
      line(std::format("{} =l add {}, 0", handleAddress, source));
      const auto handle = fresh();
      line(std::format("{} =l loadl {}", handle, handleAddress));
      const auto cellValue = fresh();
      line(std::format("{} =l call $ngrt_load(l {})", cellValue, handle));
      const auto clonedCellValue = emitCloneDeep(cellValue, element);
      const auto freshCell = fresh();
      line(std::format("{} =l call $ngrt_allocate(l {})", freshCell, clonedCellValue));
      const auto box = fresh();
      line(std::format("{} =l call $ngrt_alloc(l 8)", box));
      line(std::format("storel {}, {}", freshCell, box));
      return box;
    }
    if (descriptor.kind == TypeKind::Builtin)
    {
      if (type != typecheck::builtin::String)
        return source;
      const auto clone = fresh();
      line(std::format("{} =l call $ngrt_str_clone(l {})", clone, source));
      return clone;
    }
    if (descriptor.kind == TypeKind::Enum)
    {
      // Runtime tag dispatch needs block labels; inlining the ladder into a
      // function body would split that function's QBE blocks and desync the
      // phi predecessor sets of the surrounding control flow. Ladders are
      // only ever emitted inside the per-type clone helper functions (the
      // helper stream is active there), so when expanding into a function
      // body, delegate to the (generated, module-wide) helper instead.
      if (cloneHelperStream_ == nullptr)
      {
        if (!ngrt_.cloneHelpersEmitted.contains(type.value))
          ngrt_.cloneHelperTypes.insert(type.value);
        const auto helper = fresh();
        line(std::format("{} =l call $ng_clone{}_helper(l {})", helper, type.value, source));
        return helper;
      }
      // Deep clone with runtime tag dispatch so payload words inside
      // variant blocks (e.g. Box handles) are recursively cloned.
      cloneInProgress_.insert(type.value);
      const auto clone = fresh();
      line(std::format("{} =l call $ngrt_alloc(l 16)", clone));
      const auto tag = fresh();
      line(std::format("{} =l loadl {}", tag, source));
      const auto payloadAddr = fresh();
      line(std::format("{} =l add {}, 8", payloadAddr, source));
      const auto payload = fresh();
      line(std::format("{} =l loadl {}", payload, payloadAddr));
      line(std::format("storel {}, {}", tag, clone));
      const auto payloadTarget = fresh();
      line(std::format("{} =l add {}, 8", payloadTarget, clone));
      if (descriptor.elements.size() <= 1)
      {
        const TypeId payloadType = descriptor.elements.empty() ? typecheck::builtin::Unit : descriptor.elements[0];
        const auto clonedPayload = emitCloneDeep(payload, payloadType);
        line(std::format("storel {}, {}", clonedPayload, payloadTarget));
      }
      else
      {
        const size_t startLabel = ngrt_.cloneLabelCounter++;
        const std::string endLabel = std::format("@ng_cl{}", startLabel + descriptor.elements.size());
        for (size_t vi = 0; vi < descriptor.elements.size(); ++vi)
        {
          const std::string bodyLabel = std::format("@ng_cl{}", startLabel + vi);
          const std::string nextLabel =
              (vi + 1 == descriptor.elements.size()) ? endLabel : std::format("@ng_cl{}", startLabel + vi + 1);
          const auto match = fresh();
          line(std::format("{} =w ceql {}, {}", match, tag, vi));
          line(std::format("jnz {}, {}, {}", match, bodyLabel, nextLabel));
          line(bodyLabel);
          const auto clonedPayload = emitCloneDeep(payload, descriptor.elements[vi]);
          line(std::format("storel {}, {}", clonedPayload, payloadTarget));
        }
        line(endLabel);
        ngrt_.cloneLabelCounter = startLabel + descriptor.elements.size() + 1;
      }
      cloneInProgress_.erase(type.value);
      return clone;
    }
    if (descriptor.kind == TypeKind::Struct || descriptor.kind == TypeKind::Tuple)
    {
      cloneInProgress_.insert(type.value);
      const size_t count =
          descriptor.kind == TypeKind::Struct ? descriptor.fieldNames.size() : descriptor.elements.size();
      const size_t header = descriptor.kind == TypeKind::Struct ? 0 : 16;
      const auto clone = fresh();
      if (descriptor.kind == TypeKind::Struct)
        line(std::format("{} =l call $ngrt_alloc(l {})", clone, count * 8));
      else
        line(std::format("{} =l call $ngrt_alloc(l {})", clone, count * 8 + header));
      if (descriptor.kind == TypeKind::Tuple)
      {
        line(std::format("storel {}, {}", count, clone));
        const auto capAddress = fresh();
        line(std::format("{} =l add {}, 8", capAddress, clone));
        line(std::format("storel {}, {}", count, capAddress));
      }
      for (size_t index = 0; index < count; ++index)
      {
        const auto fieldAddress = fresh();
        line(std::format("{} =l add {}, {}", fieldAddress, source, header + index * 8));
        const auto fieldValue = fresh();
        line(std::format("{} =l loadl {}", fieldValue, fieldAddress));
        const auto fieldClone = emitCloneDeep(fieldValue, descriptor.elements[index]);
        const auto target = fresh();
        line(std::format("{} =l add {}, {}", target, clone, header + index * 8));
        line(std::format("storel {}, {}", fieldClone, target));
      }
      cloneInProgress_.erase(type.value);
      return clone;
    }
    if (descriptor.kind == TypeKind::DynamicArray || descriptor.kind == TypeKind::FixedArray)
    {
      const auto elementKind = function_.typeDescriptors[descriptor.element.value].kind;
      if (elementKind == TypeKind::Builtin && descriptor.element == typecheck::builtin::String)
      {
        const auto clone = fresh();
        line(std::format("{} =l call $ngrt_arr_clone_strings(l {})", clone, source));
        return clone;
      }
      if (elementKind == TypeKind::Struct || elementKind == TypeKind::Tuple || elementKind == TypeKind::DynamicArray ||
          elementKind == TypeKind::FixedArray)
        throw LoweringError(std::format("native lowering (M2): deep copy of nested aggregate arrays is not "
                                        "supported yet in `{}`",
                                        function_.name));
      const auto clone = fresh();
      line(std::format("{} =l call $ngrt_arr_clone_words(l {})", clone, source));
      return clone;
    }
    return source;
  }
  /// Box.clone(): `source` points at the 8-byte Box<T> struct block; returns a
  /// rebuilt Box whose fresh cell holds a full deep copy of the payload.
  [[nodiscard]] auto FunctionLowerer::emitBoxClone(const std::string &source, TypeId type) -> std::string
  {
    const auto &descriptor = function_.typeDescriptors[type.value];
    const TypeId element = descriptor.typeArguments[0];
    const auto handleAddress = fresh();
    line(std::format("{} =l add {}, 0", handleAddress, source));
    const auto handle = fresh();
    line(std::format("{} =l loadl {}", handle, handleAddress));
    const auto cellValue = fresh();
    line(std::format("{} =l call $ngrt_load(l {})", cellValue, handle));
    const auto clonedCellValue = emitCloneDeep(cellValue, element);
    const auto freshCell = fresh();
    line(std::format("{} =l call $ngrt_allocate(l {})", freshCell, clonedCellValue));
    const auto box = fresh();
    line(std::format("{} =l call $ngrt_alloc(l 8)", box));
    line(std::format("storel {}, {}", freshCell, box));
    return box;
  }
  void FunctionLowerer::emitCloneHelper(TypeId type)
  {
    cloneInProgress_.clear();
    cloneInProgress_.insert(type.value);
    cloneSkipNextGuard_ = true;
    const auto previousStream = cloneHelperStream_;
    cloneHelperStream_ = &cloneHelperOut_;
    cloneHelperOut_ << "function l $ng_clone" << type.value << "_helper(l %p) {" << '\n'
                    << "@ng_clone" << type.value << "_start" << '\n';
    const auto result = emitCloneDeep("%p", type);
    line(std::format("ret {}", result));
    cloneHelperOut_ << "}\n";
    cloneHelperStream_ = previousStream;
    cloneInProgress_.clear();
  }
  [[nodiscard]] auto FunctionLowerer::normalizeSteps(TypeId rootType, const std::vector<flowir::PlaceStep> &steps)
      -> std::vector<NormalizedStep>
  {
    std::vector<NormalizedStep> result;
    result.reserve(steps.size());
    TypeId current = rootType;
    for (const auto &step : steps)
    {
      if (step.kind == flowir::PlaceStep::Kind::Member)
      {
        const auto &descriptor = function_.typeDescriptors[current.value];
        const bool elementLike = descriptor.kind == TypeKind::DynamicArray || descriptor.kind == TypeKind::FixedArray ||
                                 descriptor.kind == TypeKind::Tuple;
        const int64_t payload =
            elementLike ? static_cast<int64_t>(step.field) : static_cast<int64_t>(fieldOffset(current, step.field));
        result.push_back(NormalizedStep{.kind = elementLike ? 2 : 0, .payload = std::format("{}", payload)});
        current = elementLike
                      ? (descriptor.kind == TypeKind::Tuple ? descriptor.elements[step.field] : descriptor.element)
                      : descriptor.elements[step.field];
      }
      else
      {
        result.push_back(NormalizedStep{.kind = 1, .payload = operandTemp(step.indexValue)});
        current = function_.typeDescriptors[current.value].element;
      }
    }
    return result;
  }
  void FunctionLowerer::lowerAggregateLiteral(const Instruction &instruction)
  {
    const size_t count = instruction.operands.size();
    const auto pointer = fresh();
    line(std::format("{} =l call $ngrt_alloc(l {})", pointer, count * 8 + 16));
    line(std::format("storel {}, {}", count, pointer));
    const auto cap = fresh();
    line(std::format("{} =l add {}, 8", cap, pointer));
    line(std::format("storel {}, {}", count, cap));
    for (size_t index = 0; index < count; ++index)
    {
      const auto elementType = qtypeOf(function_.valueTypes.at(instruction.operands[index].value));
      const auto element = castForSlot(operandTemp(instruction.operands[index]), elementType);
      const auto address = fresh();
      line(std::format("{} =l add {}, {}", address, pointer, 16 + index * 8));
      line(std::format("storel {}, {}", element, address));
    }
    bindResult(instruction, QType::Long, std::format("copy {}", pointer));
  }
  void FunctionLowerer::lowerUnionCompare(const Instruction &instruction, TypeId unionType, bool leftIsUnion)
  {
    const auto payload = instruction.payload;
    const auto memberType =
        function_.valueTypes.at((leftIsUnion ? instruction.operands[1] : instruction.operands[0]).value);
    const auto unionTemp = leftIsUnion ? operandTemp(instruction.operands[0]) : operandTemp(instruction.operands[1]);
    const auto memberTemp = leftIsUnion ? operandTemp(instruction.operands[1]) : operandTemp(instruction.operands[0]);
    const auto member = unionMemberFor(unionType, memberType);
    const auto payloadAddress = fresh();
    line(std::format("{} =l add {}, 8", payloadAddress, unionTemp));
    const auto payloadValue = fresh();
    line(std::format("{} =l loadl {}", payloadValue, payloadAddress));
    if (payload == 8 || payload == 9 || payload == 10 || payload == 11)
    {
      const char *qbeOp = payload == 8 ? "csltl" : payload == 9 ? "cslel" : payload == 10 ? "csgtl" : "csgel";
      const auto compared = fresh();
      line(std::format("{} =w {} {}, {}", compared, qbeOp, payloadValue, memberTemp));
      const auto extended = fresh();
      line(std::format("{} =l extsw {}", extended, compared));
      bindResult(instruction, QType::Long, std::format("copy {}", extended));
      return;
    }
    if (payload != 6 && payload != 7)
      throw LoweringError(
          std::format("native lowering (M2): unsupported union binary payload {} in `{}`", payload, function_.name));
    const auto tag = fresh();
    line(std::format("{} =l loadl {}", tag, unionTemp));
    const auto tagOk = fresh();
    line(std::format("{} =w ceql {}, {}", tagOk, tag, member));
    const auto valueOk = fresh();
    if (memberType == typecheck::builtin::String)
    {
      line(std::format("{} =w call $ngrt_str_eq(l {}, l {})", valueOk, payloadValue, memberTemp));
    }
    else if (typecheck::isFloatBuiltin(memberType))
    {
      const auto payloadDouble = fresh();
      line(std::format("{} =d cast {}", payloadDouble, payloadValue));
      line(std::format("{} =w ceqd {}, {}", valueOk, payloadDouble, memberTemp));
    }
    else
    {
      line(std::format("{} =w ceql {}, {}", valueOk, payloadValue, memberTemp));
    }
    const auto combined = fresh();
    line(std::format("{} =w and {}, {}", combined, tagOk, valueOk));
    const auto extended = fresh();
    line(std::format("{} =l extsw {}", extended, combined));
    if (payload == 6)
      bindResult(instruction, QType::Long, std::format("copy {}", extended));
    else
      bindResult(instruction, QType::Long, std::format("xor {}, 1", extended));
  }

} // namespace NG::native::detail
