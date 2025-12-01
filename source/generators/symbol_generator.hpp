#pragma once

#include <llvm/IR/Instructions.h>

#include "../core/types.hpp"

namespace galluz::generators {

    class SymbolGenerator : public core::ICodeGenerator {
      public:
        auto can_handle(const Exp& ast_node) const -> bool override {
            return ast_node.type == ExpType::SYMBOL;
        }

        auto generate(const Exp& ast_node, core::CompilationContext& context) -> llvm::Value* override {
            const std::string& symbol = ast_node.string;

            if (symbol == "true") {
                return context.m_BUILDER.getInt64(1);
            }

            if (symbol == "false") {
                return context.m_BUILDER.getInt64(0);
            }

            auto* var_info = context.find_variable(symbol);
            if (var_info) {
                if (var_info->is_global) {
                    auto* global_var = context.m_MODULE.getNamedGlobal(symbol);
                    if (!global_var) {
                        throw std::runtime_error("Global variable not found: " + symbol);
                    }
                    return context.m_BUILDER.CreateLoad(global_var->getValueType(), global_var, symbol);
                } else {
                    if (llvm::isa<llvm::AllocaInst>(var_info->value)) {
                        return context.m_BUILDER.CreateLoad(var_info->type, var_info->value, symbol);
                    } else {
                        return var_info->value;
                    }
                }
            }

            auto* global_var = context.m_MODULE.getNamedGlobal(symbol);
            if (global_var) {
                return context.m_BUILDER.CreateLoad(global_var->getValueType(), global_var, symbol);
            }

            throw std::runtime_error("Undefined symbol: " + symbol);
        }

        auto get_priority() const -> int override { return 900; }
    };

}    // namespace galluz::generators
