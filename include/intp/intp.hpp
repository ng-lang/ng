
#pragma once

#include <fwd.hpp>
#include <visitor.hpp>
#include <intp/runtime.hpp>

namespace NG::intp
{
    using namespace NG::runtime;

    /**
     * @brief Interface for objects that can be summarized.
     */
    struct ISummarizable
    {
        /**
         * @brief Prints a summary of the object.
         */
        virtual void summary() = 0;

        ISummarizable() = default;

        ISummarizable(const ISummarizable &) = delete;
        ISummarizable(ISummarizable &&) = delete;

        auto operator=(const ISummarizable &) -> ISummarizable & = delete;
        auto operator=(ISummarizable &&) -> ISummarizable & = delete;

        virtual ~ISummarizable() = 0;
    };

    /**
     * @brief Interface for the interpreter.
     */
    struct Interpreter : public virtual ISummarizable, public virtual NG::ast::AstVisitor
    {
        virtual ~Interpreter() override = default;
    };

    /**
     * @brief Creates a new instance of the stupid interpreter.
     *
     * @return A pointer to the new interpreter instance.
     */
    auto stupid() -> Interpreter *;
}
