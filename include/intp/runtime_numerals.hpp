#pragma once

#include <intp/runtime.hpp>

namespace NG::runtime
{

    struct NumeralBase
    {
        [[nodiscard]] virtual auto bytesize() const -> size_t = 0;
        [[nodiscard]] virtual auto signedness() const -> bool = 0;
        [[nodiscard]] virtual auto floating_point() const -> bool = 0;

        virtual auto opPlus(const NumeralBase *other) const -> RuntimeRef<NGObject> = 0;
        virtual auto opMinus(const NumeralBase *other) const -> RuntimeRef<NGObject> = 0;
        virtual auto opTimes(const NumeralBase *other) const -> RuntimeRef<NGObject> = 0;
        virtual auto opDividedBy(const NumeralBase *other) const -> RuntimeRef<NGObject> = 0;
        virtual auto opModulus(const NumeralBase *other) const -> RuntimeRef<NGObject> = 0;

        NumeralBase() = default;

        NumeralBase(const NumeralBase &) = delete;
        auto operator=(const NumeralBase &) -> NumeralBase & = delete;

        NumeralBase(NumeralBase &&) = delete;
        auto operator=(NumeralBase &&) -> NumeralBase & = delete;

        virtual ~NumeralBase() = 0;
    };

#pragma region Runtime Integrals

    template <std::integral T>
    struct NGIntegral final : ThreeWayComparable<NGIntegral<T>>, NumeralBase
    {
        T value = 0;
        using value_type = T;
        static auto comparator(const NGObject *left, const NGObject *right) -> Orders;

        constexpr static auto valueOf(const NumeralBase *numeralBase) -> T
        {
            switch (numeralBase->bytesize())
            {
            case sizeof(int8_t):
                if (numeralBase->signedness())
                {
                    return static_cast<T>(dynamic_cast<const NGIntegral<int8_t> *>(numeralBase)->value);
                }
                return static_cast<T>(dynamic_cast<const NGIntegral<uint8_t> *>(numeralBase)->value);
            case sizeof(int16_t):
                if (numeralBase->signedness())
                {
                    return static_cast<T>(dynamic_cast<const NGIntegral<int16_t> *>(numeralBase)->value);
                }
                return static_cast<T>(dynamic_cast<const NGIntegral<uint16_t> *>(numeralBase)->value);
            case sizeof(int32_t):
                if (numeralBase->signedness())
                {
                    return static_cast<T>(dynamic_cast<const NGIntegral<int32_t> *>(numeralBase)->value);
                }
                return static_cast<T>(dynamic_cast<const NGIntegral<uint32_t> *>(numeralBase)->value);
            case sizeof(int64_t):
                if (numeralBase->signedness())
                {
                    return static_cast<T>(dynamic_cast<const NGIntegral<int64_t> *>(numeralBase)->value);
                }
                return static_cast<T>(dynamic_cast<const NGIntegral<uint64_t> *>(numeralBase)->value);
            default:
                throw RuntimeException("Invalid value");
            }
        }

        explicit NGIntegral(T value = 0) : value{std::move(value)} {}

        template <std::integral U>
            requires(sizeof(T) >= sizeof(U))
        explicit NGIntegral(NGIntegral<U> other) : value{other.value}
        {
        }

        explicit NGIntegral(const NumeralBase *other)
        {
            if (other->bytesize() > this->bytesize())
            {
                throw std::overflow_error("Downcast a number");
            }
            value = valueOf(other);
        }

        [[nodiscard]] auto show() const -> Str override
        {
            return std::to_string(value);
        }

        [[nodiscard]] auto bytesize() const -> size_t override
        {
            return sizeof(T);
        }

        [[nodiscard]] auto signedness() const -> bool override
        {
            return std::is_signed_v<T>;
        }

        [[nodiscard]] auto floating_point() const -> bool override
        {
            return false;
        }

        [[nodiscard]] auto boolValue() const -> bool override
        {
            return value != 0;
        }

        [[nodiscard]] auto opPlus(RuntimeRef<NGObject> other) const -> RuntimeRef<NGObject> override;

        auto opPlus(const NumeralBase *other) const -> RuntimeRef<NGObject> override;

        [[nodiscard]] auto opMinus(RuntimeRef<NGObject> other) const -> RuntimeRef<NGObject> override;

        auto opMinus(const NumeralBase *other) const -> RuntimeRef<NGObject> override;

        [[nodiscard]] auto opTimes(RuntimeRef<NGObject> other) const -> RuntimeRef<NGObject> override;

        auto opTimes(const NumeralBase *other) const -> RuntimeRef<NGObject> override;

        [[nodiscard]] auto opDividedBy(RuntimeRef<NGObject> other) const -> RuntimeRef<NGObject> override;

        auto opDividedBy(const NumeralBase *other) const -> RuntimeRef<NGObject> override;

        [[nodiscard]] auto opModulus(RuntimeRef<NGObject> other) const -> RuntimeRef<NGObject> override;

        auto opModulus(const NumeralBase *other) const -> RuntimeRef<NGObject> override;

        [[nodiscard]] auto asSize() const -> size_t
        {
            return static_cast<size_t>(value);
        }
    };

    template <std::integral T>
    auto NGIntegral<T>::comparator(const NGObject *left, const NGObject *right) -> Orders
    {
        const auto *leftNum = dynamic_cast<const NGIntegral<T> *>(left);
        const auto *rightNum = dynamic_cast<const NumeralBase *>(right);
        if (leftNum == nullptr || rightNum == nullptr)
        {
            return Orders::UNORDERED;
        }
        if (leftNum->bytesize() >= rightNum->bytesize())
        {
            long long int result = leftNum->value - NGIntegral<T>(rightNum).value;
            if (result > 0)
            {
                return Orders::GT;
            }
            if (result < 0)
            {
                return Orders::LT;
            }
            return Orders::EQ;
        }
        return negate(right->compareTo(left));
    }

    template <std::integral T>
    auto NGIntegral<T>::opPlus(RuntimeRef<NGObject> other) const -> RuntimeRef<NGObject>
    {
        auto result = std::dynamic_pointer_cast<NumeralBase>(other);
        if (!result)
        {
            throw RuntimeException("Not a number");
        }
        if (result->bytesize() > bytesize())
        {
            return result->opPlus(dynamic_cast<const NumeralBase *>(this));
        }
        return this->opPlus(result.get());
    }

    template <std::integral T>
    auto NGIntegral<T>::opPlus(const NumeralBase *other) const -> RuntimeRef<NGObject>
    {
        return makert<NGIntegral<T>>(value + NGIntegral<T>(other).value);
    }

    template <std::integral T>
    auto NGIntegral<T>::opMinus(RuntimeRef<NGObject> other) const -> RuntimeRef<NGObject>
    {
        auto result = std::dynamic_pointer_cast<NumeralBase>(other);
        if (!result)
        {
            throw RuntimeException("Not a number");
        }
        if (result->bytesize() > bytesize())
        {
            return result->opMinus(dynamic_cast<const NumeralBase *>(this));
        }
        return this->opMinus(result.get());
    }

    template <std::integral T>
    auto NGIntegral<T>::opMinus(const NumeralBase *other) const -> RuntimeRef<NGObject>
    {
        return makert<NGIntegral<T>>(value - NGIntegral<T>(other).value);
    }

    template <std::integral T>
    auto NGIntegral<T>::opTimes(RuntimeRef<NGObject> other) const -> RuntimeRef<NGObject>
    {
        auto result = std::dynamic_pointer_cast<NumeralBase>(other);
        if (!result)
        {
            throw RuntimeException("Not a number");
        }
        if (result->bytesize() > bytesize())
        {
            return result->opTimes(dynamic_cast<const NumeralBase *>(this));
        }
        return this->opTimes(result.get());
    }

    template <std::integral T>
    auto NGIntegral<T>::opTimes(const NumeralBase *other) const -> RuntimeRef<NGObject>
    {
        return makert<NGIntegral<T>>(value * NGIntegral<T>(other).value);
    }

    template <std::integral T>
    auto NGIntegral<T>::opDividedBy(RuntimeRef<NGObject> other) const -> RuntimeRef<NGObject>
    {
        auto result = std::dynamic_pointer_cast<NumeralBase>(other);
        if (!result)
        {
            throw RuntimeException("Not a number");
        }
        if (result->bytesize() > bytesize())
        {
            return result->opDividedBy(dynamic_cast<const NumeralBase *>(this));
        }
        return this->opDividedBy(result.get());
    }

    template <std::integral T>
    auto NGIntegral<T>::opDividedBy(const NumeralBase *other) const -> RuntimeRef<NGObject>
    {
        return makert<NGIntegral<T>>(value / NGIntegral<T>(other).value);
    }

    template <std::integral T>
    auto NGIntegral<T>::opModulus(RuntimeRef<NGObject> other) const -> RuntimeRef<NGObject>
    {
        auto result = std::dynamic_pointer_cast<NumeralBase>(other);
        if (!result)
        {
            throw RuntimeException("Not a number");
        }
        if (result->bytesize() > bytesize())
        {
            return result->opModulus(dynamic_cast<const NumeralBase *>(this));
        }
        return this->opModulus(result.get());
    }

    template <std::integral T>
    auto NGIntegral<T>::opModulus(const NumeralBase *other) const -> RuntimeRef<NGObject>
    {
        return makert<NGIntegral<T>>(value % NGIntegral<T>(other).value);
    }

#pragma endregion

#pragma region Runtime FloatingPoints

    template <std::floating_point T>
    struct NGFloatingPoint final : ThreeWayComparable<NGFloatingPoint<T>>, NumeralBase
    {
        T value = 0;
        using value_type = T;
        static auto comparator(const NGObject *left, const NGObject *right) -> Orders;

        constexpr static auto valueOf(const NumeralBase *numeralBase) -> T
        {
            if (!numeralBase->floating_point())
            {
                if (numeralBase->signedness())
                {
                    return static_cast<T>(NGIntegral<int64_t>::valueOf(numeralBase));
                }
                return static_cast<T>(NGIntegral<uint64_t>::valueOf(numeralBase));
            }
            switch (numeralBase->bytesize())
            {
            // case sizeof(float16_t):
            //     return static_cast<T>(dynamic_cast<const NGFloatingPoint<float16_t>*>(numeralBase)->value);
            case sizeof(float /* float32_t */):
                return static_cast<T>(dynamic_cast<const NGFloatingPoint<float> *>(numeralBase)->value);
            case sizeof(double /* float64_t */):
                return static_cast<T>(dynamic_cast<const NGFloatingPoint<double> *>(numeralBase)->value);
            // case sizeof(float128_t /* float128_t */):
            //     return static_cast<T>(dynamic_cast<const NGFloatingPoint<float128_t>*>(numeralBase)->value);
            default:
                throw RuntimeException("Invalid value");
            }
        }

        explicit NGFloatingPoint(T value = 0) : value{std::move(value)} {}

        template <std::integral U>
            requires(sizeof(T) >= sizeof(U))
        explicit NGFloatingPoint(NGFloatingPoint<U> other) : value{other.value}
        {
        }

        explicit NGFloatingPoint(const NumeralBase *other)
        {
            if (other->bytesize() > this->bytesize())
            {
                throw std::overflow_error("Downcast a number");
            }
            value = valueOf(other);
        }

        [[nodiscard]] auto show() const -> Str override
        {
            return std::to_string(value);
        }

        [[nodiscard]] auto bytesize() const -> size_t override
        {
            return sizeof(T);
        }

        [[nodiscard]] auto signedness() const -> bool override
        {
            return value < 0;
        }

        [[nodiscard]] auto floating_point() const -> bool override
        {
            return true;
        }

        [[nodiscard]] auto boolValue() const -> bool override
        {
            return value != 0;
        }

        [[nodiscard]] auto opPlus(RuntimeRef<NGObject> other) const -> RuntimeRef<NGObject> override;

        auto opPlus(const NumeralBase *other) const -> RuntimeRef<NGObject> override;

        [[nodiscard]] auto opMinus(RuntimeRef<NGObject> other) const -> RuntimeRef<NGObject> override;

        auto opMinus(const NumeralBase *other) const -> RuntimeRef<NGObject> override;

        [[nodiscard]] auto opTimes(RuntimeRef<NGObject> other) const -> RuntimeRef<NGObject> override;

        auto opTimes(const NumeralBase *other) const -> RuntimeRef<NGObject> override;

        [[nodiscard]] auto opDividedBy(RuntimeRef<NGObject> other) const -> RuntimeRef<NGObject> override;

        auto opDividedBy(const NumeralBase *other) const -> RuntimeRef<NGObject> override;

        [[nodiscard]] auto opModulus(RuntimeRef<NGObject> other) const -> RuntimeRef<NGObject> override;

        auto opModulus(const NumeralBase *other) const -> RuntimeRef<NGObject> override;

        [[nodiscard]] auto asSize() const -> size_t
        {
            return static_cast<size_t>(value);
        }
    };

    template <std::floating_point T>
    auto NGFloatingPoint<T>::comparator(const NGObject *left, const NGObject *right) -> Orders
    {
        const auto *leftNum = dynamic_cast<const NGFloatingPoint<T> *>(left);
        const auto *rightNum = dynamic_cast<const NumeralBase *>(right);
        if (leftNum == nullptr || rightNum == nullptr)
        {
            return Orders::UNORDERED;
        }
        if (leftNum->bytesize() >= rightNum->bytesize())
        {
            long long int result = leftNum->value - NGFloatingPoint<T>(rightNum).value;
            if (result > 0)
            {
                return Orders::GT;
            }
            if (result < 0)
            {
                return Orders::LT;
            }
            return Orders::EQ;
        }
        return negate(right->compareTo(left));
    }

    template <std::floating_point T>
    auto NGFloatingPoint<T>::opPlus(RuntimeRef<NGObject> other) const -> RuntimeRef<NGObject>
    {
        auto result = std::dynamic_pointer_cast<NumeralBase>(other);
        if (!result)
        {
            throw RuntimeException("Not a number");
        }
        if (result->bytesize() > bytesize())
        {
            return result->opPlus(dynamic_cast<const NumeralBase *>(this));
        }
        return this->opPlus(result.get());
    }

    template <std::floating_point T>
    auto NGFloatingPoint<T>::opPlus(const NumeralBase *other) const -> RuntimeRef<NGObject>
    {
        return makert<NGFloatingPoint<T>>(value + NGFloatingPoint<T>(other).value);
    }

    template <std::floating_point T>
    auto NGFloatingPoint<T>::opMinus(RuntimeRef<NGObject> other) const -> RuntimeRef<NGObject>
    {
        auto result = std::dynamic_pointer_cast<NumeralBase>(other);
        if (!result)
        {
            throw RuntimeException("Not a number");
        }
        if (result->bytesize() > bytesize())
        {
            return result->opMinus(dynamic_cast<const NumeralBase *>(this));
        }
        return this->opMinus(result.get());
    }

    template <std::floating_point T>
    auto NGFloatingPoint<T>::opMinus(const NumeralBase *other) const -> RuntimeRef<NGObject>
    {
        return makert<NGFloatingPoint<T>>(value - NGFloatingPoint<T>(other).value);
    }

    template <std::floating_point T>
    auto NGFloatingPoint<T>::opTimes(RuntimeRef<NGObject> other) const -> RuntimeRef<NGObject>
    {
        auto result = std::dynamic_pointer_cast<NumeralBase>(other);
        if (!result)
        {
            throw RuntimeException("Not a number");
        }
        if (result->bytesize() > bytesize())
        {
            return result->opTimes(dynamic_cast<const NumeralBase *>(this));
        }
        return this->opTimes(result.get());
    }

    template <std::floating_point T>
    auto NGFloatingPoint<T>::opTimes(const NumeralBase *other) const -> RuntimeRef<NGObject>
    {
        return makert<NGFloatingPoint<T>>(value * NGFloatingPoint<T>(other).value);
    }

    template <std::floating_point T>
    auto NGFloatingPoint<T>::opDividedBy(RuntimeRef<NGObject> other) const -> RuntimeRef<NGObject>
    {
        auto result = std::dynamic_pointer_cast<NumeralBase>(other);
        if (!result)
        {
            throw RuntimeException("Not a number");
        }
        if (result->bytesize() > bytesize())
        {
            return result->opDividedBy(dynamic_cast<const NumeralBase *>(this));
        }
        return this->opDividedBy(result.get());
    }

    template <std::floating_point T>
    auto NGFloatingPoint<T>::opDividedBy(const NumeralBase *other) const -> RuntimeRef<NGObject>
    {
        return makert<NGFloatingPoint<T>>(value / NGFloatingPoint<T>(other).value);
    }

    template <std::floating_point T>
    auto NGFloatingPoint<T>::opModulus(RuntimeRef<NGObject> other) const -> RuntimeRef<NGObject>
    {
        throw std::logic_error("floating point not support modulus operation");
    }

    template <std::floating_point T>
    auto NGFloatingPoint<T>::opModulus(const NumeralBase *other) const -> RuntimeRef<NGObject>
    {
        throw std::logic_error("floating point not support modulus operation");
    }

#pragma endregion

}
