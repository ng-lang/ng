
#include <intp/runtime.hpp>

namespace NG::runtime {
    Orders NGInteger::comparator(const NGObject *left, const NGObject *right) {
        auto leftInt = dynamic_cast<const NGInteger *>(left);
        auto rightInt = dynamic_cast<const NGInteger *>(right);

        if (leftInt == nullptr || rightInt == nullptr) {
            return Orders::UNORDERED;
        }

        long long int result = leftInt->value - rightInt->value;
        if (result > 0) {
            return Orders::GT;
        } else if (result < 0) {
            return Orders::LT;
        }
        return Orders::EQ;
    }

    NGObject *NGInteger::opPlus(NGObject *other) const {
        if (auto integer = dynamic_cast<NGInteger *>(other); integer != nullptr) {

            return new NGInteger{value + integer->value};
        }
        throw IllegalTypeException("Not a number");
    }

    NGObject *NGInteger::opMinus(NGObject *other) const {
        if (auto integer = dynamic_cast<NGInteger *>(other); integer != nullptr) {

            return new NGInteger{value - integer->value};
        }
        throw IllegalTypeException("Not a number");
    }

    NGObject *NGInteger::opTimes(NGObject *other) const {
        if (auto integer = dynamic_cast<NGInteger *>(other); integer != nullptr) {

            return new NGInteger{value * integer->value};
        }
        throw IllegalTypeException("Not a number");
    }

    NGObject *NGInteger::opDividedBy(NGObject *other) const {
        if (auto integer = dynamic_cast<NGInteger *>(other); integer != nullptr) {

            return new NGInteger{value / integer->value};
        }
        throw IllegalTypeException("Not a number");
    }

    NGObject *NGInteger::opModulus(NGObject *other) const {
        if (auto integer = dynamic_cast<NGInteger *>(other); integer != nullptr) {

            return new NGInteger{value % integer->value};
        }
        throw IllegalTypeException("Not a number");
    }

    bool NGInteger::boolValue() {
        return value != 0;
    }
}