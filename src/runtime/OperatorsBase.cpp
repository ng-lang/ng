
#include <intp/runtime.hpp>

namespace NG::runtime
{
  // NOLINTNEXTLINE(performance-unnecessary-value-param)
  auto OperatorsBase::opIndex(RuntimeRef<NGObject> index) const -> RuntimeRef<NGObject>
  {
    return nullptr;
  }

  // NOLINTNEXTLINE(bugprone-easily-swappable-parameters, performance-unnecessary-value-param)
  auto OperatorsBase::opIndex(RuntimeRef<NGObject> index, RuntimeRef<NGObject> newValue) -> RuntimeRef<NGObject>
  {
    return nullptr;
  }

  // NOLINTNEXTLINE(performance-unnecessary-value-param)
  auto OperatorsBase::opGreaterThan(RuntimeRef<NGObject> other) const -> bool
  {
    return false;
  }

  // NOLINTNEXTLINE(performance-unnecessary-value-param)
  auto OperatorsBase::opGreaterEqual(RuntimeRef<NGObject> other) const -> bool
  {
    return false;
  }

  // NOLINTNEXTLINE(performance-unnecessary-value-param)
  auto OperatorsBase::opLessThan(RuntimeRef<NGObject> other) const -> bool
  {
    return false;
  }

  // NOLINTNEXTLINE(performance-unnecessary-value-param)
  auto OperatorsBase::opLessEqual(RuntimeRef<NGObject> other) const -> bool
  {
    return false;
  }

  // NOLINTNEXTLINE(performance-unnecessary-value-param)
  auto OperatorsBase::opEquals(RuntimeRef<NGObject> other) const -> bool
  {
    return false;
  }

  // NOLINTNEXTLINE(performance-unnecessary-value-param)
  auto OperatorsBase::opNotEqual(RuntimeRef<NGObject> other) const -> bool
  {
    return false;
  }

  // NOLINTNEXTLINE(performance-unnecessary-value-param)
  auto OperatorsBase::opPlus(RuntimeRef<NGObject> other) const -> RuntimeRef<NGObject>
  {
    return nullptr;
  }

  // NOLINTNEXTLINE(performance-unnecessary-value-param)
  auto OperatorsBase::opMinus(RuntimeRef<NGObject> other) const -> RuntimeRef<NGObject>
  {
    return nullptr;
  }

  // NOLINTNEXTLINE(performance-unnecessary-value-param)
  auto OperatorsBase::opTimes(RuntimeRef<NGObject> other) const -> RuntimeRef<NGObject>
  {
    return nullptr;
  }

  // NOLINTNEXTLINE(performance-unnecessary-value-param)
  auto OperatorsBase::opModulus(RuntimeRef<NGObject> other) const -> RuntimeRef<NGObject>
  {
    return nullptr;
  }

  // NOLINTNEXTLINE(performance-unnecessary-value-param)
  auto OperatorsBase::opDividedBy(RuntimeRef<NGObject> other) const -> RuntimeRef<NGObject>
  {
    return nullptr;
  }

  // NOLINT(performance-unnecessary-value-param)
  auto OperatorsBase::respond(const Str &member,
                              RuntimeRef<NGContext> context, // NOLINT(performance-unnecessary-value-param)
                              NGInvCtx invocationContext)    // NOLINT(performance-unnecessary-value-param)

      -> RuntimeRef<NGObject>
  {
    return nullptr;
  }

  // NOLINTNEXTLINE(performance-unnecessary-value-param)
  auto OperatorsBase::opLShift(RuntimeRef<NGObject> other) -> RuntimeRef<NGObject>
  {
    return nullptr;
  }

  // NOLINTNEXTLINE(performance-unnecessary-value-param)
  auto OperatorsBase::opRShift(RuntimeRef<NGObject> other) -> RuntimeRef<NGObject>
  {
    return nullptr;
  }

  OperatorsBase::~OperatorsBase() noexcept = default;
} // namespace NG::runtime