
#include <intp/runtime_numerals.hpp>

namespace NG::runtime
{
  auto numeral_runtime_show(const NumeralBase &numeral) -> Str
  {
    if (auto value = dynamic_cast<const NGIntegral<int8_t> *>(&numeral)) return std::to_string(value->value);
    if (auto value = dynamic_cast<const NGIntegral<uint8_t> *>(&numeral)) return std::to_string(value->value);
    if (auto value = dynamic_cast<const NGIntegral<int16_t> *>(&numeral)) return std::to_string(value->value);
    if (auto value = dynamic_cast<const NGIntegral<uint16_t> *>(&numeral)) return std::to_string(value->value);
    if (auto value = dynamic_cast<const NGIntegral<int32_t> *>(&numeral)) return std::to_string(value->value);
    if (auto value = dynamic_cast<const NGIntegral<uint32_t> *>(&numeral)) return std::to_string(value->value);
    if (auto value = dynamic_cast<const NGIntegral<int64_t> *>(&numeral)) return std::to_string(value->value);
    if (auto value = dynamic_cast<const NGIntegral<uint64_t> *>(&numeral)) return std::to_string(value->value);
    if (auto value = dynamic_cast<const NGFloatingPoint<float> *>(&numeral)) return std::to_string(value->value);
    if (auto value = dynamic_cast<const NGFloatingPoint<double> *>(&numeral)) return std::to_string(value->value);
    return "0";
  }

  auto numeral_runtime_bool(const NumeralBase &numeral) -> bool
  {
    if (auto value = dynamic_cast<const NGIntegral<int8_t> *>(&numeral)) return value->value != 0;
    if (auto value = dynamic_cast<const NGIntegral<uint8_t> *>(&numeral)) return value->value != 0;
    if (auto value = dynamic_cast<const NGIntegral<int16_t> *>(&numeral)) return value->value != 0;
    if (auto value = dynamic_cast<const NGIntegral<uint16_t> *>(&numeral)) return value->value != 0;
    if (auto value = dynamic_cast<const NGIntegral<int32_t> *>(&numeral)) return value->value != 0;
    if (auto value = dynamic_cast<const NGIntegral<uint32_t> *>(&numeral)) return value->value != 0;
    if (auto value = dynamic_cast<const NGIntegral<int64_t> *>(&numeral)) return value->value != 0;
    if (auto value = dynamic_cast<const NGIntegral<uint64_t> *>(&numeral)) return value->value != 0;
    if (auto value = dynamic_cast<const NGFloatingPoint<float> *>(&numeral)) return value->value != 0;
    if (auto value = dynamic_cast<const NGFloatingPoint<double> *>(&numeral)) return value->value != 0;
    return false;
  }

  auto NumeralBase::bytesize() const -> size_t
  {
    return 0;
  }

  auto NumeralBase::signedness() const -> bool
  {
    return false;
  }

  auto NumeralBase::floating_point() const -> bool
  {
    return false;
  }

  auto NumeralBase::opPlus(const NumeralBase * /*unused*/) const -> RuntimeRef<NGObject>
  {
    return nullptr;
  }
  auto NumeralBase::opMinus(const NumeralBase * /*unused*/) const -> RuntimeRef<NGObject>
  {
    return nullptr;
  }
  auto NumeralBase::opTimes(const NumeralBase * /*unused*/) const -> RuntimeRef<NGObject>
  {
    return nullptr;
  }
  auto NumeralBase::opDividedBy(const NumeralBase * /*unused*/) const -> RuntimeRef<NGObject>
  {
    return nullptr;
  }
  auto NumeralBase::opModulus(const NumeralBase * /*unused*/) const -> RuntimeRef<NGObject>
  {
    return nullptr;
  }

  NumeralBase::~NumeralBase() = default;
} // namespace NG::runtime
