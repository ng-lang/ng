
#pragma once

#include <fwd.hpp>
#include <visitor.hpp>
#include <intp/runtime.hpp>

namespace NG::intp
{
    using namespace NG::runtime;

    struct ISummarizable
    {
        virtual void summary() = 0;

        ISummarizable() = default;

        ISummarizable(const ISummarizable &) = delete;
        ISummarizable(ISummarizable &&) = delete;

        auto operator=(const ISummarizable &) -> ISummarizable & = delete;
        auto operator=(ISummarizable &&) -> ISummarizable & = delete;

        virtual ~ISummarizable() = 0;
    };

    struct Interpreter : public virtual ISummarizable, public virtual NG::ast::AstVisitor
    {
        virtual auto intpContext() -> NG::runtime::NGContext * = 0;
    };

    auto stupid() -> Interpreter *;

    auto predefs() -> Map<Str, NGInvocable>;
}
