
#include "intp/runtime.hpp"
#include <debug.hpp>
#include <runtime/value_access.hpp>
#include <typeinfo>

namespace NG::runtime
{

  auto NGObject::boolean(bool boolean) -> RuntimeRef<NGObject>
  {
    return makert<NGBoolean>(boolean);
  }

  auto NGObject::show() const -> Str
  {
    return runtime_value_show(runtime_self_alias(const_cast<NGObject *>(this)));
  }

  auto NGObject::opEquals(RuntimeRef<NGObject> other) const -> bool
  {
    return false;
  }

  auto NGObject::boolValue() const -> bool
  {
    return runtime_value_bool(runtime_self_alias(const_cast<NGObject *>(this)));
  }

  auto NGObject::opIndex(RuntimeRef<NGObject> index) const -> RuntimeRef<NGObject>
  {
    throw IllegalTypeException("Not index-accessible");
  }

  // NOLINTNEXTLINE(bugprone-easily-swappable-parameters)
  auto NGObject::opIndex(RuntimeRef<NGObject> index, RuntimeRef<NGObject> newValue) -> RuntimeRef<NGObject>
  {
    throw IllegalTypeException("Not index-accessible");
  }

  auto NGObject::opPlus(RuntimeRef<NGObject> other) const -> RuntimeRef<NGObject>
  {
    throw NotImplementedException("Plus not implemented for " + type()->name);
  }

  auto NGObject::opMinus(RuntimeRef<NGObject> other) const -> RuntimeRef<NGObject>
  {
    throw NotImplementedException("Minus not implemented for " + type()->name);
  }

  auto NGObject::opTimes(RuntimeRef<NGObject> other) const -> RuntimeRef<NGObject>
  {
    throw NotImplementedException("Times not implemented for " + type()->name);
  }

  auto NGObject::opDividedBy(RuntimeRef<NGObject> other) const -> RuntimeRef<NGObject>
  {
    throw NotImplementedException("Divide not implemented for " + type()->name);
  }

  auto NGObject::opModulus(RuntimeRef<NGObject> other) const -> RuntimeRef<NGObject>
  {
    throw NotImplementedException("Modulus not implemented for " + type()->name);
  }

  auto NGObject::opGreaterThan(RuntimeRef<NGObject> other) const -> bool
  {
    throw NotImplementedException();
  }

  auto NGObject::opLessThan(RuntimeRef<NGObject> other) const -> bool
  {
    throw NotImplementedException();
  }

  auto NGObject::opGreaterEqual(RuntimeRef<NGObject> other) const -> bool
  {
    throw NotImplementedException();
  }

  auto NGObject::opLessEqual(RuntimeRef<NGObject> other) const -> bool
  {
    throw NotImplementedException();
  }

  auto NGObject::respond(const RuntimeRef<NGObject> &self, const Str &member, NGCtx context,
                         const NGArgs &args) -> RuntimeRef<NGObject>
  {
    auto dispatchSelf = self && self.get() == this ? self : runtime_self_alias(this);
    return runtime_value_respond(dispatchSelf, member, context, args);
  }

  auto NGObject::opNotEqual(RuntimeRef<NGObject> other) const -> bool
  {
    return !opEquals(other);
  }

  auto NGObject::opLShift(RuntimeRef<NGObject> other) -> RuntimeRef<NGObject>
  {
    throw NotImplementedException();
  }

  auto NGObject::opRShift(RuntimeRef<NGObject> other) -> RuntimeRef<NGObject>
  {
    throw NotImplementedException();
  }

  auto NGObject::objectType() -> RuntimeRef<NGType>
  {
    static RuntimeRef<NGType> OBJECT_TYPE = makert<NGType>(NGType{
      .name = "Object",
      .layout = TypeLayout{.name = "Object", .kind = LayoutKind::DYNAMIC},
      .showHandler = [](const NGSelf &) { return Str{"[NGObject]"}; },
      .boolHandler = [](const NGSelf &) { return true; },
    });
    return OBJECT_TYPE;
  }

  auto NGObject::type() const -> RuntimeRef<NGType>
  {
    return runtime_value_type(runtime_self_alias(const_cast<NGObject *>(this)));
  }

  NGObject::~NGObject() = default;
} // namespace NG::runtime
