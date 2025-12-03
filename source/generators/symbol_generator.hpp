// generators/symbol_generator.hpp
#pragma once

#include <llvm/IR/Instructions.h>

#include "../core/generator_manager.hpp"
#include "../core/module_manager.hpp"
#include "../core/types.hpp"
#include "../logger.hpp"
#include "../parser/utils.hpp"

namespace galluz::generators {

    class SymbolGenerator : public core::ICodeGenerator {
      private:
        core::GeneratorManager* m_GENERATOR_MANAGER;
        core::ModuleManager* m_MODULE_MANAGER;

      public:
        SymbolGenerator()
            : m_GENERATOR_MANAGER(nullptr)
            , m_MODULE_MANAGER(nullptr) {}

        void initialize(core::GeneratorManager* manager, core::ModuleManager* module_manager) {
            m_GENERATOR_MANAGER = manager;
            m_MODULE_MANAGER = module_manager;
        }

        auto can_handle(const Exp& ast_node) const -> bool override {
            return ast_node.type == ExpType::SYMBOL;
        }

        auto generate(const Exp& ast_node, core::CompilationContext& context) -> llvm::Value* override {
            const std::string& symbol = ast_node.string;

            if (symbol == "true") {
                return context.m_BUILDER.getInt1(true);
            }

            if (symbol == "false") {
                return context.m_BUILDER.getInt1(false);
            }

            std::unordered_set<std::string> keywords = {
                "import", "moduleuse", "defmodule", "defn",    "var",     "global", "set",
                "scope",  "do",        "fprint",    "if",      "while",   "break",  "continue",
                "struct", "new",       "getprop",   "setprop", "hasprop", "finput"};

            if (keywords.count(symbol)) {
                LOG_CRITICAL("Undefined symbol: %s (this is a keyword)", symbol);
            }

            if (symbol.find('.') != std::string::npos && m_MODULE_MANAGER) {
                auto [module_value, member_name] = m_MODULE_MANAGER->resolve_symbol(symbol);
                if (module_value) {
                    return module_value;
                }
            }

            auto* var_info = context.find_variable(symbol);
            if (var_info) {
                if (var_info->is_global) {
                    auto* global_var = context.m_MODULE.getNamedGlobal(symbol);
                    if (!global_var) {
                        LOG_CRITICAL("Global variable not found: %s", symbol);
                    }
                    if (var_info->type_info && var_info->type_info->kind == core::TypeKind::STRUCT) {
                        return global_var;
                    }
                    return context.m_BUILDER.CreateLoad(var_info->type, global_var, symbol);
                } else {
                    if (var_info->type_info && var_info->type_info->kind == core::TypeKind::STRUCT) {
                        return var_info->value;
                    }

                    if (llvm::isa<llvm::Argument>(var_info->value)) {
                        return var_info->value;
                    }

                    return context.m_BUILDER.CreateLoad(var_info->type, var_info->value, symbol);
                }
            }

            auto* func_info = context.find_function(symbol);
            if (func_info) {
                return func_info->function;
            }

            auto* global_var = context.m_MODULE.getNamedGlobal(symbol);
            if (global_var) {
                llvm::Type* value_type = global_var->getValueType();
                if (value_type->isStructTy()) {
                    return global_var;
                }
                return context.m_BUILDER.CreateLoad(value_type, global_var, symbol);
            }

            LOG_CRITICAL("Undefined symbol: %s", symbol);
        }

        auto get_priority() const -> int override { return 800; }
    };

}    // namespace galluz::generators
