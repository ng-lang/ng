
#include <intp/runtime.hpp>
#include <intp/runtime_numerals.hpp>
namespace NG::runtime
{

    auto NGUnit::show() const -> Str
    {
        return "unit";
    }

    auto NGUnit::type() const -> RuntimeRef<NGType>
    {
        static RuntimeRef<NGType> tupleType = makert<NGType>(NGType{
            .name = "unit",
            .memberFunctions = {}});
        return tupleType;
    }

}
