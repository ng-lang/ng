
#ifndef __NG_INTP_RUNTIME_NUMERALS_HPP
#define __NG_INTP_RUNTIME_NUMERALS_HPP

#include <intp/runtime.hpp>

namespace NG::runtime {

    struct NumeralBase
    {
        virtual size_t bytesize() const = 0;
        virtual bool signedness() const = 0;
        virtual bool floating_point() const = 0;

        virtual RuntimeRef<NGObject> opPlus(const NumeralBase* other) const = 0;
        virtual RuntimeRef<NGObject> opMinus(const NumeralBase* other) const = 0;
        virtual RuntimeRef<NGObject> opTimes(const NumeralBase* other) const = 0;
        virtual RuntimeRef<NGObject> opDividedBy(const NumeralBase* other) const = 0;
        virtual RuntimeRef<NGObject> opModulus(const NumeralBase* other) const = 0;
    };
    
    #pragma region Runtime Integrals

    template<std::integral T>
    struct NGIntegral final : ThreeWayComparable<NGIntegral<T>>, NumeralBase {
        T value = 0;
        using value_type = T;
        static Orders comparator(const NGObject *left, const NGObject* right);

        constexpr static inline T valueOf(const NumeralBase* numeralBase) {
            switch (numeralBase->bytesize())
            {
            case sizeof(int8_t):
                if (numeralBase->signedness()) {
                    return static_cast<T>(dynamic_cast<const NGIntegral<int8_t>*>(numeralBase)->value);
                }
                return static_cast<T>(dynamic_cast<const NGIntegral<uint8_t>*>(numeralBase)->value);
            case sizeof(int16_t):
                if (numeralBase->signedness()) {
                    return static_cast<T>(dynamic_cast<const NGIntegral<int16_t>*>(numeralBase)->value);
                }
                return static_cast<T>(dynamic_cast<const NGIntegral<uint16_t>*>(numeralBase)->value);
            case sizeof(int32_t):
                if (numeralBase->signedness()) {
                    return static_cast<T>(dynamic_cast<const NGIntegral<int32_t>*>(numeralBase)->value);
                }
                return static_cast<T>(dynamic_cast<const NGIntegral<uint32_t>*>(numeralBase)->value);
            case sizeof(int64_t):
                if (numeralBase->signedness()) {
                    return static_cast<T>(dynamic_cast<const NGIntegral<int64_t>*>(numeralBase)->value);
                }
                return static_cast<T>(dynamic_cast<const NGIntegral<uint64_t>*>(numeralBase)->value);
            default:
                throw RuntimeException("Invalid value");
            }
        }

        explicit NGIntegral(T value = 0) : value { std::move(value) } {}

        template<std::integral U> requires (sizeof(T) >= sizeof(U))
        explicit NGIntegral(NGIntegral<U> other): value {other.value} {}

        explicit NGIntegral(const NumeralBase* other) {
            if (other->bytesize() > this->bytesize()) {
                throw std::overflow_error("Downcast a number");
            }
            value = valueOf(other);
        }

        Str show() const override {
            return std::to_string(value);
        }

        size_t bytesize() const override {
            return sizeof(T);
        }

        bool signedness() const override {
            return std::is_signed_v<T>;
        }

        bool floating_point() const override {
            return false;
        }

        bool boolValue() const override {
            return value != 0;
        }

        RuntimeRef<NGObject> opPlus(RuntimeRef<NGObject> other) const override;

        RuntimeRef<NGObject> opPlus(const NumeralBase* other) const override;

        RuntimeRef<NGObject> opMinus(RuntimeRef<NGObject> other) const override;

        RuntimeRef<NGObject> opMinus(const NumeralBase* other) const override;

        RuntimeRef<NGObject> opTimes(RuntimeRef<NGObject> other) const override;

        RuntimeRef<NGObject> opTimes(const NumeralBase* other) const override;

        RuntimeRef<NGObject> opDividedBy(RuntimeRef<NGObject> other) const override;

        RuntimeRef<NGObject> opDividedBy(const NumeralBase* other) const override;

        RuntimeRef<NGObject> opModulus(RuntimeRef<NGObject> other) const override;

        RuntimeRef<NGObject> opModulus(const NumeralBase* other) const override;

        [[nodiscard]] inline size_t asSize() const {
            return static_cast<size_t>(value);
        }
    };

    template<std::integral T>
    Orders NGIntegral<T>::comparator(const NGObject *left, const NGObject* right) {
        const NGIntegral<T>* leftNum = dynamic_cast<const NGIntegral<T>*>(left);
        const NumeralBase* rightNum = dynamic_cast<const NumeralBase*>(right);
        if (leftNum == nullptr || rightNum == nullptr) {
            return Orders::UNORDERED;
        }
        if (leftNum->bytesize() >= rightNum->bytesize()) {    
            long long int result = leftNum->value - NGIntegral<T>(rightNum).value;
            if (result > 0) {
                return Orders::GT;
            } else if (result < 0) {
                return Orders::LT;
            }
            return Orders::EQ;                
        }
        return negate(right->compareTo(left));
    }


    template<std::integral T>
    RuntimeRef<NGObject> NGIntegral<T>::opPlus(RuntimeRef<NGObject> other) const {
        auto result = std::dynamic_pointer_cast<NumeralBase>(other);
        if (!result) {
            throw RuntimeException("Not a number");
        }
        if (result->bytesize() > bytesize()) {
            return result->opPlus(dynamic_cast<const NumeralBase*>(this));
        }
        return this->opPlus(result.get());
    }

    template<std::integral T>
    RuntimeRef<NGObject> NGIntegral<T>::opPlus(const NumeralBase* other) const {
        return makert<NGIntegral<T>>(value + NGIntegral<T>(other).value);
    }

    template<std::integral T>
    RuntimeRef<NGObject> NGIntegral<T>::opMinus(RuntimeRef<NGObject> other) const {
        auto result = std::dynamic_pointer_cast<NumeralBase>(other);
        if (!result) {
            throw RuntimeException("Not a number");
        }
        if (result->bytesize() > bytesize()) {
            return result->opMinus(dynamic_cast<const NumeralBase*>(this));
        }
        return this->opMinus(result.get());
    }

    template<std::integral T>
    RuntimeRef<NGObject> NGIntegral<T>::opMinus(const NumeralBase* other) const {
        return makert<NGIntegral<T>>(value - NGIntegral<T>(other).value);
    }

    template<std::integral T>
    RuntimeRef<NGObject> NGIntegral<T>::opTimes(RuntimeRef<NGObject> other) const {
        auto result = std::dynamic_pointer_cast<NumeralBase>(other);
        if (!result) {
            throw RuntimeException("Not a number");
        }
        if (result->bytesize() > bytesize()) {
            return result->opTimes(dynamic_cast<const NumeralBase*>(this));
        }
        return this->opTimes(result.get());
    }

    template<std::integral T>
    RuntimeRef<NGObject> NGIntegral<T>::opTimes(const NumeralBase* other) const {
        return makert<NGIntegral<T>>(value * NGIntegral<T>(other).value);
    }

    template<std::integral T>
    RuntimeRef<NGObject> NGIntegral<T>::opDividedBy(RuntimeRef<NGObject> other) const {
        auto result = std::dynamic_pointer_cast<NumeralBase>(other);
        if (!result) {
            throw RuntimeException("Not a number");
        }
        if (result->bytesize() > bytesize()) {
            return result->opDividedBy(dynamic_cast<const NumeralBase*>(this));
        }
        return this->opDividedBy(result.get());
    }

    template<std::integral T>
    RuntimeRef<NGObject> NGIntegral<T>::opDividedBy(const NumeralBase* other) const {
        return makert<NGIntegral<T>>(value / NGIntegral<T>(other).value);
    }

    template<std::integral T>
    RuntimeRef<NGObject> NGIntegral<T>::opModulus(RuntimeRef<NGObject> other) const {
        auto result = std::dynamic_pointer_cast<NumeralBase>(other);
        if (!result) {
            throw RuntimeException("Not a number");
        }
        if (result->bytesize() > bytesize()) {
            return result->opModulus(dynamic_cast<const NumeralBase*>(this));
        }
        return this->opModulus(result.get());
    }

    template<std::integral T>
    RuntimeRef<NGObject> NGIntegral<T>::opModulus(const NumeralBase* other) const {
        return makert<NGIntegral<T>>(value % NGIntegral<T>(other).value);
    }

    #pragma endregion

    #pragma region Runtime FloatingPoints
    
    template<std::floating_point T>
    struct NGFloatingPoint final : ThreeWayComparable<NGFloatingPoint<T>>, NumeralBase {
        T value = 0;
        using value_type = T;
        static Orders comparator(const NGObject *left, const NGObject* right);

        constexpr static inline T valueOf(const NumeralBase* numeralBase) {
            if (!numeralBase->floating_point()) {
                if (numeralBase->signedness()) {
                    return static_cast<T>(NGIntegral<int64_t>::valueOf(numeralBase));
                }
                return static_cast<T>(NGIntegral<uint64_t>::valueOf(numeralBase));
            }
            switch (numeralBase->bytesize())
            {
            // case sizeof(float16_t):
            //     return static_cast<T>(dynamic_cast<const NGFloatingPoint<float16_t>*>(numeralBase)->value);
            case sizeof(float /* float32_t */):
                return static_cast<T>(dynamic_cast<const NGFloatingPoint<float>*>(numeralBase)->value);
            case sizeof(double /* float64_t */):
                return static_cast<T>(dynamic_cast<const NGFloatingPoint<double>*>(numeralBase)->value);
            // case sizeof(float128_t /* float128_t */):
            //     return static_cast<T>(dynamic_cast<const NGFloatingPoint<float128_t>*>(numeralBase)->value);
            default:
                throw RuntimeException("Invalid value");
            }
        }

        explicit NGFloatingPoint(T value = 0) : value { std::move(value) } {}

        template<std::integral U> requires (sizeof(T) >= sizeof(U))
        explicit NGFloatingPoint(NGFloatingPoint<U> other): value {other.value} {}

        explicit NGFloatingPoint(const NumeralBase* other) {
            if (other->bytesize() > this->bytesize()) {
                throw std::overflow_error("Downcast a number");
            }
            value = valueOf(other);
        }

        Str show() const override {
            return std::to_string(value);
        }

        size_t bytesize() const override {
            return sizeof(T);
        }

        bool signedness() const override {
            return value < 0;
        }

        bool floating_point() const override {
            return true;
        }

        bool boolValue() const override {
            return value != 0;
        }

        RuntimeRef<NGObject> opPlus(RuntimeRef<NGObject> other) const override;

        RuntimeRef<NGObject> opPlus(const NumeralBase* other) const override;

        RuntimeRef<NGObject> opMinus(RuntimeRef<NGObject> other) const override;

        RuntimeRef<NGObject> opMinus(const NumeralBase* other) const override;

        RuntimeRef<NGObject> opTimes(RuntimeRef<NGObject> other) const override;

        RuntimeRef<NGObject> opTimes(const NumeralBase* other) const override;

        RuntimeRef<NGObject> opDividedBy(RuntimeRef<NGObject> other) const override;

        RuntimeRef<NGObject> opDividedBy(const NumeralBase* other) const override;

        RuntimeRef<NGObject> opModulus(RuntimeRef<NGObject> other) const override;

        RuntimeRef<NGObject> opModulus(const NumeralBase* other) const override;

        [[nodiscard]] inline size_t asSize() const {
            return static_cast<size_t>(value);
        }
    };

    template<std::floating_point T>
    Orders NGFloatingPoint<T>::comparator(const NGObject *left, const NGObject* right) {
        const NGFloatingPoint<T>* leftNum = dynamic_cast<const NGFloatingPoint<T>*>(left);
        const NumeralBase* rightNum = dynamic_cast<const NumeralBase*>(right);
        if (leftNum == nullptr || rightNum == nullptr) {
            return Orders::UNORDERED;
        }
        if (leftNum->bytesize() >= rightNum->bytesize()) {    
            long long int result = leftNum->value - NGFloatingPoint<T>(rightNum).value;
            if (result > 0) {
                return Orders::GT;
            } else if (result < 0) {
                return Orders::LT;
            }
            return Orders::EQ;                
        }
        return negate(right->compareTo(left));
    }


    template<std::floating_point T>
    RuntimeRef<NGObject> NGFloatingPoint<T>::opPlus(RuntimeRef<NGObject> other) const {
        auto result = std::dynamic_pointer_cast<NumeralBase>(other);
        if (!result) {
            throw RuntimeException("Not a number");
        }
        if (result->bytesize() > bytesize()) {
            return result->opPlus(dynamic_cast<const NumeralBase*>(this));
        }
        return this->opPlus(result.get());
    }

    template<std::floating_point T>
    RuntimeRef<NGObject> NGFloatingPoint<T>::opPlus(const NumeralBase* other) const {
        return makert<NGFloatingPoint<T>>(value + NGFloatingPoint<T>(other).value);
    }

    template<std::floating_point T>
    RuntimeRef<NGObject> NGFloatingPoint<T>::opMinus(RuntimeRef<NGObject> other) const {
        auto result = std::dynamic_pointer_cast<NumeralBase>(other);
        if (!result) {
            throw RuntimeException("Not a number");
        }
        if (result->bytesize() > bytesize()) {
            return result->opMinus(dynamic_cast<const NumeralBase*>(this));
        }
        return this->opMinus(result.get());
    }

    template<std::floating_point T>
    RuntimeRef<NGObject> NGFloatingPoint<T>::opMinus(const NumeralBase* other) const {
        return makert<NGFloatingPoint<T>>(value - NGFloatingPoint<T>(other).value);
    }

    template<std::floating_point T>
    RuntimeRef<NGObject> NGFloatingPoint<T>::opTimes(RuntimeRef<NGObject> other) const {
        auto result = std::dynamic_pointer_cast<NumeralBase>(other);
        if (!result) {
            throw RuntimeException("Not a number");
        }
        if (result->bytesize() > bytesize()) {
            return result->opTimes(dynamic_cast<const NumeralBase*>(this));
        }
        return this->opTimes(result.get());
    }

    template<std::floating_point T>
    RuntimeRef<NGObject> NGFloatingPoint<T>::opTimes(const NumeralBase* other) const {
        return makert<NGFloatingPoint<T>>(value * NGFloatingPoint<T>(other).value);
    }

    template<std::floating_point T>
    RuntimeRef<NGObject> NGFloatingPoint<T>::opDividedBy(RuntimeRef<NGObject> other) const {
        auto result = std::dynamic_pointer_cast<NumeralBase>(other);
        if (!result) {
            throw RuntimeException("Not a number");
        }
        if (result->bytesize() > bytesize()) {
            return result->opDividedBy(dynamic_cast<const NumeralBase*>(this));
        }
        return this->opDividedBy(result.get());
    }

    template<std::floating_point T>
    RuntimeRef<NGObject> NGFloatingPoint<T>::opDividedBy(const NumeralBase* other) const {
        return makert<NGFloatingPoint<T>>(value / NGFloatingPoint<T>(other).value);
    }

    template<std::floating_point T>
    RuntimeRef<NGObject> NGFloatingPoint<T>::opModulus(RuntimeRef<NGObject> other) const {
        throw std::logic_error("floating point not support modulus operation");
    }

    template<std::floating_point T>
    RuntimeRef<NGObject> NGFloatingPoint<T>::opModulus(const NumeralBase* other) const {
        throw std::logic_error("floating point not support modulus operation");
    }

    #pragma endregion

}

#endif // __NG_INTP_RUNTIME_NUMERALS_HPP