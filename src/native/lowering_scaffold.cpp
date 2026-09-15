// AI-generated code; reviewed for this repository's vNext rewrite.
// ARCH-2: Scaffolding: run/emit, block/local collection, temp assignment.
#include "function_lowerer.hpp"

namespace NG::native::detail
{

  [[nodiscard]] auto FunctionLowerer::run() -> std::string
  {
    if (function_.nativeFunction)
      throw LoweringError(std::format("native lowering (M1): native placeholder `{}` has no body", function_.name));
    collectBlocks();
    collectLocals();
    preassignValueTemps();
    return emit();
  }
  [[nodiscard]] auto FunctionLowerer::symbolFor(hir::DefId id) -> std::string
  {
    std::string name;
    if (id == function_.source)
      name = function_.name;
    else if (const auto found = names_.find(id.value); found != names_.end())
      name = found->second;
    else
      name = std::format("fn{}", id.value);
    return qbeSymbol(name, id.value);
  }
  void FunctionLowerer::ensureSlot(uint32_t local)
  {
    if (localSlots_.contains(local))
      return;
    QType type = QType::Long;
    if (const auto found = function_.localTypes.find(local); found != function_.localTypes.end())
      type = qtypeOf(found->second);
    localSlots_.emplace(local, fresh());
    localQTypes_.emplace(local, type);
    size_t words = 1;
    if (type == QType::Aggregate)
      words = (aggregateSize(function_.localTypes.at(local)) + 7) / 8;
    slotOrder_.push_back(SlotSpec{.local = local, .heapCell = cellLocals_.contains(local), .words = words});
  }
  [[nodiscard]] auto FunctionLowerer::localQType(uint32_t local) -> QType
  {
    if (const auto found = localQTypes_.find(local); found != localQTypes_.end())
      return found->second;
    if (const auto typed = function_.localTypes.find(local); typed != function_.localTypes.end())
      return qtypeOf(typed->second);
    return QType::Long;
  }
  [[nodiscard]] auto FunctionLowerer::loadLocal(uint32_t local) -> std::string
  {
    const auto &slot = localSlots_.at(local);
    if (!isCellLocal(local))
    {
      const auto value = fresh();
      line(std::format("{} =l loadl {}", value, slot));
      return value;
    }
    const auto cell = fresh();
    line(std::format("{} =l loadl {}", cell, slot));
    const auto value = fresh();
    line(std::format("{} =l loadl {}", value, cell));
    return value;
  }
  void FunctionLowerer::bindFreshCell(uint32_t local, const std::string &value)
  {
    const auto &slot = localSlots_.at(local);
    const auto cell = fresh();
    line(std::format("{} =l call $ngrt_alloc(l 8)", cell));
    line(std::format("storel {}, {}", cell, slot));
    line(std::format("storel {}, {}", value, cell));
  }
  void FunctionLowerer::collectLocals()
  {
    // Pass 1a: locals rooted by MakeRef/MakeTraitView need heap cells.
    for (const auto &block : function_.blocks)
      for (const auto &instruction : block.instructions)
        if ((instruction.kind == InstructionKind::MakeRef || instruction.kind == InstructionKind::MakeTraitView) &&
            instruction.placeRootLocal)
          cellLocals_.insert(instruction.placeRootLocal->value);
    // Box locals always live in heap cells: a box VALUE is an address to an
    // 8-byte cell holding the handle, and that address can be embedded in an
    // aggregate/enum payload (moved into a constructor). A stack slot would
    // be reused once the binding dies, dangling every payload token that
    // pointed at it.
    for (const auto &[local, type] : function_.localTypes)
      if (isBoxType(type))
        cellLocals_.insert(local);
    // Pass 1b: SSA locals — never borrowed, never assigned, never a
    // block parameter — live as QBE temps (no slot traffic; QBE's
    // non-SSA fixup handles rebinding and cross-block uses). Parameters
    // qualify unless the function rebinds them through TailRecur.
    for (const auto &block : function_.blocks)
    {
      for (const auto &instruction : block.instructions)
      {
        if (instruction.local)
          ssaCandidates_.insert(instruction.local->value);
        if (instruction.kind == InstructionKind::AssignPlace && instruction.placeRootLocal)
          ssaExcluded_.insert(instruction.placeRootLocal->value);
      }
    }
    for (const auto local : function_.parameterLocals)
    {
      ssaCandidates_.insert(local.value);
      if (hasTailRecur_)
        ssaExcluded_.insert(local.value);
    }
    for (const auto local : ssaCandidates_)
      if (!ssaExcluded_.contains(local) && !cellLocals_.contains(local) &&
          !(function_.localTypes.contains(local) && qtypeOf(function_.localTypes.at(local)) == QType::Aggregate))
        ssaLocals_.insert(local);
    paramTemps_.reserve(function_.parameterLocals.size());
    for (size_t index = 0; index < function_.parameterLocals.size(); ++index)
    {
      const auto local = function_.parameterLocals[index].value;
      paramTemps_.push_back(std::format("%p{}", index));
      if (ssaLocals_.contains(local))
        currentTemps_.emplace(local, paramTemps_.back());
      else
        ensureSlot(local);
    }
    for (const auto &block : function_.blocks)
      for (const auto local : block.parameterLocals)
        ensureSlot(local.value);
    for (const auto &block : function_.blocks)
    {
      for (const auto &instruction : block.instructions)
      {
        if (instruction.local && !ssaLocals_.contains(instruction.local->value))
          ensureSlot(instruction.local->value);
        if (instruction.kind == InstructionKind::Evaluate && instruction.expressionKind == ExpressionKind::ResolvedName)
        {
          const auto local = static_cast<uint32_t>(instruction.payload);
          if (!ssaLocals_.contains(local))
            ensureSlot(local);
        }
        if (instruction.kind == InstructionKind::AssignPlace && instruction.placeRootLocal)
          ensureSlot(instruction.placeRootLocal->value);
      }
    }
  }
  void FunctionLowerer::preassignValueTemps()
  {
    const auto touch = [&](ValueId value)
    {
      if (!valueTemps_.contains(value.value))
        valueTemps_.emplace(value.value, fresh());
    };
    for (const auto &block : function_.blocks)
    {
      for (const auto &instruction : block.instructions)
      {
        touch(instruction.result);
        if (instruction.source)
          touch(*instruction.source);
        for (const auto operand : instruction.operands)
          touch(operand);
      }
      if (block.terminator)
        for (const auto argument : block.terminator->arguments)
          touch(argument);
    }
  }
  [[nodiscard]] auto FunctionLowerer::findReturnType() -> std::optional<QType>
  {
    for (const auto &block : function_.blocks)
    {
      const auto &terminator = *block.terminator;
      if (terminator.kind == TerminatorKind::Return && !terminator.arguments.empty())
      {
        returnTypeId_ = function_.valueTypes.at(terminator.arguments[0].value);
        return qtypeOf(*returnTypeId_);
      }
    }
    return std::nullopt;
  }
  [[nodiscard]] auto FunctionLowerer::emit() -> std::string
  {
    returnType_ = findReturnType();
    // `main` is the C entry point: a unit return becomes an i64 exit code.
    if (function_.name == "main" && (!returnType_ || *returnType_ == QType::None))
    {
      returnType_ = QType::Long;
      returnTypeId_ = typecheck::builtin::I64;
    }
    out_ << (function_.name == "main" ? "export function" : "function");
    if (returnType_ && *returnType_ != QType::None)
      // Aggregate returns are heap-cloned pointers (`l`): callee stack
      // slots must not outlive the frame, and QBE's aggregate return
      // areas interact poorly with recursive aggregate traffic.
      out_ << ' ' << (*returnType_ == QType::Aggregate ? "l" : suffix(*returnType_));
    out_ << ' ' << symbolFor(function_.source) << '(';
    for (size_t index = 0; index < paramTemps_.size(); ++index)
    {
      if (index != 0)
        out_ << ", ";
      out_ << qbeSuffixFor(function_.localTypes.at(function_.parameterLocals[index].value)) << ' '
           << paramTemps_[index];
    }
    out_ << ") {\n";
    // One-shot entry logic: frame slots and parameter stores live under
    // `@start` so tail recursion never re-runs them.
    out_ << "@start\n";
    for (const auto &slot : slotOrder_)
    {
      if (slot.heapCell)
      {
        // Cell locals: the slot stores a pointer to the current cell.
        line(std::format("{} =l call $ngrt_alloc(l 8)", localSlots_.at(slot.local)));
        const auto cell = fresh();
        line(std::format("{} =l call $ngrt_alloc(l 8)", cell));
        line(std::format("storel {}, {}", cell, localSlots_.at(slot.local)));
        line(std::format("storel 0, {}", cell));
      }
      else
      {
        // QBE `alloc8` sizes are byte counts (the trailing number is the
        // alignment), so a slot of `words` 8-byte words needs `words * 8`.
        line(std::format("{} =l alloc8 {}", localSlots_.at(slot.local), slot.words * 8));
      }
    }
    for (size_t param = 0; param < paramTemps_.size(); ++param)
    {
      const auto local = function_.parameterLocals[param].value;
      if (ssaLocals_.contains(local))
        continue; // parameter temps are used directly
      const auto localTypeId = function_.localTypes.at(local);
      if (isCellLocal(local))
      {
        const auto cell = fresh();
        line(std::format("{} =l loadl {}", cell, localSlots_.at(local)));
        line(std::format("storel {}, {}", paramTemps_[param], cell));
      }
      else if (qtypeOf(localTypeId) == QType::Aggregate)
      {
        // Copy-first semantics: aggregates are copied into the frame.
        emitAggregateCopy(localSlots_.at(local), paramTemps_[param], localTypeId);
      }
      else
      {
        line(std::format("store{} {}, {}", suffix(localQTypes_.at(local)), paramTemps_[param], localSlots_.at(local)));
      }
    }
    if (hasTailRecur_)
      line(std::format("jmp {}", labels_.front()));
    for (size_t index = 0; index < function_.blocks.size(); ++index)
      emitBlock(index);
    out_ << "}\n";
    // Recursive deep-copy helpers requested while emitting the body are
    // generated as additional QBE functions, module-wide and exactly once per
    // type (later functions reuse the already-emitted helper). Generating a
    // helper expands its whole structure, which can itself discover further
    // recursive types, so loop until the registry stabilizes.
    const size_t scanLimit = 128;
    for (size_t scan = 0; !ngrt_.cloneHelperTypes.empty() && scan < scanLimit; ++scan)
    {
      std::vector<uint32_t> pendingHelpers;
      for (const uint32_t id : ngrt_.cloneHelperTypes)
        if (!ngrt_.cloneHelpersEmitted.contains(id))
          pendingHelpers.push_back(id);
      ngrt_.cloneHelperTypes.clear();
      std::sort(pendingHelpers.begin(), pendingHelpers.end());
      for (const uint32_t id : pendingHelpers)
      {
        ngrt_.cloneHelpersEmitted.insert(id);
        TypeId type;
        type.value = id;
        emitCloneHelper(type);
      }
    }
    out_ << cloneHelperOut_.str();
    // String-literal data items are collected while emitting blocks, so
    // they are written after the body — data declarations are top-level
    // and order-independent.
    for (const auto &item : dataItems_)
      out_ << item << '\n';
    return out_.str();
  }
  void FunctionLowerer::emitBlock(size_t index)
  {
    const auto &block = function_.blocks[index];
    // Block 0 without tail recursion continues `@start` directly; every
    // other block (and block 0 behind the tail-recursion trampoline)
    // carries its own label.
    if (index != 0 || hasTailRecur_)
      out_ << labels_[index] << '\n';
    // Block parameters become phi temporaries stored into their slots.
    // QBE requires every phi of a block to be consecutive at the top, so
    // all phis are emitted before any store.
    const auto &predecessors = predecessors_[index];
    std::vector<std::string> phiTemps;
    phiTemps.reserve(block.parameterLocals.size());
    for (size_t param = 0; param < block.parameterLocals.size(); ++param)
    {
      const auto local = block.parameterLocals[param].value;
      const auto type = localQTypes_.at(local);
      std::string arguments;
      for (size_t edge = 0; edge < predecessors.size(); ++edge)
      {
        const auto &[predIndex, values] = predecessors[edge];
        if (values.size() != block.parameterLocals.size())
          throw LoweringError(
              std::format("native lowering (M1): malformed block-parameter edge in `{}`", function_.name));
        if (edge != 0)
          arguments += ", ";
        arguments += std::format("{} {}", labels_[predIndex], valueTemps_.at(values[param].value));
      }
      const auto phi = fresh();
      line(std::format("{} ={} phi {}", phi, suffix(type), arguments));
      phiTemps.push_back(phi);
    }
    for (size_t param = 0; param < block.parameterLocals.size(); ++param)
    {
      const auto local = block.parameterLocals[param].value;
      if (isCellLocal(local))
      {
        // The VM rebinds block parameters into fresh cells on every
        // entry (jumpToBlock), so loop accumulators observed through
        // refs keep their per-iteration snapshot.
        bindFreshCell(local, phiTemps[param]);
      }
      else
      {
        line(std::format("store{} {}, {}", suffix(localQTypes_.at(local)), phiTemps[param], localSlots_.at(local)));
      }
    }
    for (const auto &instruction : block.instructions)
      lowerInstruction(instruction);
    lowerTerminator(*block.terminator, index);
  }

} // namespace NG::native::detail
