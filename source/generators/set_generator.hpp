#pragma once

#include "../core/generator_manager.hpp"
#include "../core/types.hpp"

namespace galluz::generators {

    class SetGenerator : public core::ICodeGenerator {
      private:
        core::GeneratorManager* m_GENERATOR_MANAGER;

      public:
        explicit SetGenerator(core::GeneratorManager* manager)
            : m_GENERATOR_MANAGER(manager) {}

        auto can_handle(const Exp& ast_node) const -> bool override {
            if (ast_node.type != ExpType::LIST) {
                return false;
            }
            if (ast_node.list.empty()) {
                return false;
            }

            const auto& first = ast_node.list[0];
            return (first.type == ExpType::SYMBOL && first.string == "set");
        }

        auto generate(const Exp& ast_node, core::CompilationContext& context) -> llvm::Value* override {
            if (ast_node.list.size() != 3) {
                throw std::runtime_error("Invalid set syntax: (set variable value)");
            }

            const auto& name_exp = ast_node.list[1];
            const auto& value_exp = ast_node.list[2];

            if (name_exp.type != ExpType::SYMBOL) {
                throw std::runtime_error("Variable name must be a symbol");
            }

            std::string var_name = name_exp.string;

            llvm::Value* new_value = m_GENERATOR_MANAGER->generate_code(value_exp, context);
            llvm::Type* value_type = new_value->getType();

            auto* var_info = context.find_variable(var_name);
            if (var_info) {
                if (var_info->is_global) {
                    auto* global_var = context.m_MODULE.getNamedGlobal(var_name);
                    if (global_var->getValueType() != value_type) {
                        throw std::runtime_error("Type mismatch in set operation for variable: " + var_name);
                    }
                    context.m_BUILDER.CreateStore(new_value, global_var);
                    return new_value;
                } else {
                    if (var_info->type != value_type) {
                        throw std::runtime_error("Type mismatch in set operation for variable: " + var_name);
                    }
                    context.m_BUILDER.CreateStore(new_value, var_info->value);
                    return new_value;
                }
            }

            auto* global_var = context.m_MODULE.getNamedGlobal(var_name);
            if (global_var) {
                if (global_var->getValueType() != value_type) {
                    throw std::runtime_error("Type mismatch in set operation for variable: " + var_name);
                }
                context.m_BUILDER.CreateStore(new_value, global_var);
                return new_value;
            }

            throw std::runtime_error("Cannot set undefined variable: " + var_name);
        }

        auto get_priority() const -> int override { return 700; }
    };

}    // namespace galluz::generators
