#pragma once

#include <orgasm/module.hpp>
#include <orgasm/native_bridge.hpp>
#include <intp/runtime.hpp>
#include <functional>

namespace NG::orgasm
{
    using namespace NG::runtime;

    using NativeFunction = std::function<RuntimeRef<StorageCell>(const Vec<RuntimeRef<StorageCell>> &)>;

    /**
     * @brief A simple virtual machine for executing ORGASM bytecode.
     */
    class VM
    {
      public:
        explicit VM(Vec<Str> modulePaths = {}) : modulePaths(std::move(modulePaths)) {}

        /**
         * @brief Executes the entry point of a bytecode module.
         *
         * @param module The bytecode module to execute.
         * @return The return value of the entry point.
         */
        auto run(const BytecodeModule &module) -> RuntimeRef<StorageCell>;

        /**
         * @brief Registers a native function with auto-marshaling.
         *
         * Accepts regular C++ functions/lambdas. Types are automatically
         * converted between NG runtime and C++ types.
         *
         * Supported C++ types: int8_t..int64_t, uint8_t..uint64_t,
         * float, double, bool, std::string, RuntimeRef<StorageCell>
         *
         * @param name The name of the native function (used by NATIVE_CALL opcode).
         * @param func A C++ function or lambda with supported parameter/return types.
         */
        template <typename Func>
        void register_native(const Str &name, Func func)
        {
            native_functions[name] = wrap_native(std::move(func));
        }

        /**
         * @brief Registers a raw native function (no auto-marshaling).
         *
         * @param name The name of the native function.
         * @param func The raw native function.
         */
        void register_native_raw(const Str &name, NativeFunction func);

      private:
        struct Frame
        {
            const BytecodeModule *module = nullptr;
            const Function *function = nullptr;
            size_t ip;
            Vec<RuntimeRef<StorageCell>> locals;
        };

        Vec<RuntimeRef<StorageCell>> stack;
        const BytecodeModule *current_module = nullptr;
        Vec<RuntimeRef<StorageCell>> globals;
        NGSymbols root_symbols;
        Map<Str, RuntimeRef<NGType>> root_types;
        Vec<Frame> call_stack;
        Vec<Str> modulePaths;
        Map<Str, NativeFunction> native_functions;

        void push_frame(const BytecodeModule &module, const Function &fun,
                        const Vec<RuntimeRef<StorageCell>> &argSlots);
        auto execute_slots(const BytecodeModule &module, const Function &fun,
                           const Vec<RuntimeRef<StorageCell>> &argSlots) -> RuntimeRef<StorageCell>;
    };
} // namespace NG::orgasm
