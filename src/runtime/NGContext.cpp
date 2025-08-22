#include <intp/intp.hpp>
#include <intp/runtime.hpp>

namespace NG::runtime
{

    const static NGInvocationHandler DUMMY = [](RuntimeRef<NGObject>, RuntimeRef<NGContext>, RuntimeRef<NGInvocationContext>) {};
    void NGContext::appendModulePath(const Str &path)
    {
        if (std::ranges::find(modulePaths, path) == std::end(modulePaths))
        {
            modulePaths.push_back(path);
        }
    }

    auto NGContext::fork() -> RuntimeRef<NGContext>
    {
        auto ctx = makert<NGContext>(Vec<Str>{}, intp::predefs());
        ctx->parent = this;
        ctx->modulePaths = this->modulePaths;

        return ctx;
    }

    auto NGContext::get(Str name) -> RuntimeRef<NGObject>
    {
        if (objects.contains(name))
        {
            return objects.at(name);
        }
        if (parent != nullptr)
        {
            return parent->get(name);
        }
        return nullptr;
    }

    void NGContext::set(Str name, RuntimeRef<NGObject> value)
    {
        if (locals.contains(name))
        {
            objects[name] = value;
        }
        else if (parent != nullptr)
        {
            parent->set(name, value);
        }
        else
        {
            throw RuntimeException("Invalid assignment to " + name);
        }
    }

    void NGContext::define(Str name, RuntimeRef<NGObject> value)
    {
        if (locals.contains(name))
        {
            // todo: redefine, consider as error?
            throw RuntimeException("Redefine " + name);
        }
        locals.insert(name);
        objects[name] = value;
    }

    void NGContext::define_function(Str name, NGInvocationHandler value)
    {
        if (locals.contains(name))
        {
            throw RuntimeException("Redefine " + name);
        }
        locals.insert(name);
        functions[name] = value;
    }

    void NGContext::define_type(Str name, RuntimeRef<NGType> type)
    {
        if (locals.contains(name))
        {
            throw RuntimeException("Redefine " + name);
        }
        locals.insert(name);
        types[name] = type;
    }

    void NGContext::define_module(Str name, RuntimeRef<NGModule> module)
    {
        if (locals.contains(name))
        {
            throw RuntimeException("Redefine " + name);
        }
        locals.insert(name);
        modules[name] = module;
    }

    auto NGContext::has_object(Str name, bool global) -> bool
    {
        return objects.contains(name) || (global && parent != nullptr && parent->has_object(name, global));
    }
    auto NGContext::has_function(Str name, bool global) -> bool
    {
        return functions.contains(name) || (global && parent != nullptr && parent->has_function(name, global));
    }
    auto NGContext::has_module(Str name, bool global) -> bool
    {
        return modules.contains(name) || (global && parent != nullptr && parent->has_module(name, global));
    }
    auto NGContext::has_type(Str name, bool global) -> bool
    {
        return objects.contains(name) || (global && parent != nullptr && parent->has_type(name, global));
    }

    auto NGContext::get_function(Str name) -> NGInvocationHandler
    {
        if (functions.contains(name))
        {
            return functions.at(name);
        }
        if (parent != nullptr)
        {
            return parent->get_function(name);
        }
        return DUMMY;
    }
    auto NGContext::get_module(Str name) -> RuntimeRef<NGModule>
    {
        if (modules.contains(name))
        {
            return modules.at(name);
        }
        if (parent != nullptr)
        {
            return parent->get_module(name);
        }
        return nullptr;
    }
    auto NGContext::get_type(Str name) -> RuntimeRef<NGType>
    {
        if (types.contains(name))
        {
            return types.at(name);
        }
        if (parent != nullptr)
        {
            return parent->get_type(name);
        }
        return nullptr;
    }

    void NGContext::try_save_module()
    {
        if (currentModule != nullptr)
        {
            // copy
            this->currentModule->objects = objects;
            this->currentModule->types = types;
            this->currentModule->functions = functions;

            // save
            this->modules.insert_or_assign(this->currentModuleName, this->currentModule);

            // clear
            this->objects = {};
            this->types = {};
            this->functions = intp::predefs();
            this->locals = {};
        }
    }

    void NGContext::new_current(ast::Module *mod)
    {
        this->currentModuleName = mod->name;
        this->currentModule = makert<NGModule>();

        this->currentModule->exports = mod->exports;
    }

    auto NGContext::current_module() -> RuntimeRef<NGModule>
    {
        return this->currentModule;
    }

    void NGContext::summary()
    {
        auto context = this;
        debug_log("Context objects size", context->objects.size());

        for (const auto &pair : context->objects)
        {
            debug_log("Context object", "key:", pair.first, "value:", pair.second->show());
        }

        debug_log("Context modules size", context->modules.size());

        for (const auto &pair : context->modules)
        {
            debug_log("Context module", "name:", pair.first, "value:", code(pair.second->size()));
        }

        for (const auto &type : context->types)
        {
            debug_log("Context types", "name:", type.first, "members:",
                      type.second->properties.size() + type.second->memberFunctions.size());
        }
    }
}