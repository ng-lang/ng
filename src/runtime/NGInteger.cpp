
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

    RuntimeRef<NGObject> NGInteger::opPlus(RuntimeRef<NGObject> other) const {
        if (auto integer = std::dynamic_pointer_cast<NGInteger>(other); integer != nullptr) {

            return makert<NGInteger>(value + integer->value);
        }
        throw IllegalTypeException("+ Not a number" + other->show());
    }

    RuntimeRef<NGObject> NGInteger::opMinus(RuntimeRef<NGObject> other) const {
        if (auto integer = std::dynamic_pointer_cast<NGInteger>(other); integer != nullptr) {

            return makert<NGInteger>(value - integer->value);
        }
        throw IllegalTypeException("- Not a number" + other->show());
    }

    RuntimeRef<NGObject> NGInteger::opTimes(RuntimeRef<NGObject> other) const {
        if (auto integer = std::dynamic_pointer_cast<NGInteger>(other); integer != nullptr) {

            return makert<NGInteger>(value * integer->value);
        }
        throw IllegalTypeException("* Not a number" + other->show());
    }

    RuntimeRef<NGObject> NGInteger::opDividedBy(RuntimeRef<NGObject> other) const {
        if (auto integer = std::dynamic_pointer_cast<NGInteger>(other); integer != nullptr) {

            return makert<NGInteger>(value / integer->value);
        }
        throw IllegalTypeException("/ Not a number" + other->show());
    }

    RuntimeRef<NGObject> NGInteger::opModulus(RuntimeRef<NGObject> other) const {
        if (auto integer = std::dynamic_pointer_cast<NGInteger>(other); integer != nullptr) {

            return makert<NGInteger>(value % integer->value);
        }
        throw IllegalTypeException("% Not a number" + other->show());
    }

    bool NGInteger::boolValue() {
        return value != 0;
    }
}