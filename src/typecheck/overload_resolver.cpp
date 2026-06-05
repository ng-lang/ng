
#include <typecheck/overload_resolver.hpp>
#include <typecheck/pattern_matching.hpp>
#include <typecheck/typecheck.hpp>
#include <ast.hpp>

namespace NG::typecheck
{
    auto parseTypeInstanceArgs(const Str &name) -> Vec<Str>
    {
        auto lt = name.find('<');
        if (lt == Str::npos || !name.ends_with('>'))
        {
            return {};
        }
        auto inner = name.substr(lt + 1, name.size() - lt - 2);
        Vec<Str> args;
        int depth = 0;
        Str current;
        for (char c : inner)
        {
            if (c == '<') depth++;
            else if (c == '>') depth--;
            else if (c == ',' && depth == 0)
            {
                args.push_back(current);
                current.clear();
                continue;
            }
            current += c;
        }
        if (!current.empty()) args.push_back(current);
        return args;
    }

    auto functionPatternSpecificity(const ast::FunctionDef &candidate) -> size_t
    {
        auto genericNames = genericParamNameSet(candidate.genericParams);
        size_t score = 0;
        for (auto &param : candidate.params)
        {
            if (!param || !param->annotatedType) continue;
            auto anno = param->annotatedType.get();
            if (anno->genericArgs.empty() && anno->arguments.empty())
            {
                score += genericNames.contains(anno->name) ? 1 : 10;
            }
            else
            {
                score += 5;
            }
        }
        for (auto &genericParam : candidate.genericParams)
        {
            if (genericParam && genericParam->bound) ++score;
        }
        score += candidate.whereBounds.size();
        return score;
    }
} // namespace NG::typecheck
