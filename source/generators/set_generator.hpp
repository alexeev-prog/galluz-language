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
                LOG_CRITICAL("Invalid set syntax: (set variable value)");
            }

            const auto& name_exp = ast_node.list[1];
            const auto& value_exp = ast_node.list[2];

            std::string var_name;

            if (name_exp.type == ExpType::SYMBOL) {
                var_name = name_exp.string;
            } else if (name_exp.type == ExpType::LIST && name_exp.list.size() == 2) {
                if (name_exp.list[0].type != ExpType::SYMBOL) {
                    LOG_CRITICAL("Variable name must be a symbol");
                }
                var_name = name_exp.list[0].string;
            } else {
                LOG_CRITICAL("Invalid variable name in set operation");
            }

            llvm::Value* new_value = m_GENERATOR_MANAGER->generate_code(value_exp, context);
            llvm::Type* value_type = new_value->getType();

            auto* var_info = context.find_variable(var_name);
            if (var_info) {
                if (var_info->type_info && var_info->type_info->llvm_type != value_type) {
                    if (var_info->type_info->kind == core::TypeKind::STRUCT) {
                        if (!value_type->isStructTy() && !value_type->isPointerTy()) {
                            LOG_CRITICAL("Type mismatch in set operation for struct variable: %s", var_name);
                        }
                    } else if (var_info->type_info->kind == core::TypeKind::INT && value_type->isIntegerTy())
                    {
                        new_value =
                            context.m_BUILDER.CreateIntCast(new_value, var_info->type_info->llvm_type, true);
                    } else if (var_info->type_info->kind == core::TypeKind::DOUBLE
                               && value_type->isFloatingPointTy())
                    {
                        new_value = context.m_BUILDER.CreateFPCast(new_value, var_info->type_info->llvm_type);
                    } else if (var_info->type_info->kind == core::TypeKind::DOUBLE
                               && value_type->isIntegerTy())
                    {
                        new_value = context.m_BUILDER.CreateSIToFP(new_value, var_info->type_info->llvm_type);
                    } else if (var_info->type_info->kind == core::TypeKind::INT
                               && value_type->isFloatingPointTy())
                    {
                        new_value = context.m_BUILDER.CreateFPToSI(new_value, var_info->type_info->llvm_type);
                    } else if (var_info->type_info->kind == core::TypeKind::BOOL && value_type->isIntegerTy())
                    {
                        new_value =
                            context.m_BUILDER.CreateIntCast(new_value, context.m_BUILDER.getInt1Ty(), false);
                    } else {
                        LOG_CRITICAL("Type mismatch in set operation for variable: %s", var_name);
                    }
                    value_type = new_value->getType();
                }

                if (var_info->is_global) {
                    auto* global_var = context.m_MODULE.getNamedGlobal(var_name);
                    if (global_var->getValueType() != value_type) {
                        LOG_CRITICAL("Type mismatch in set operation for variable: %s", var_name);
                    }
                    context.m_BUILDER.CreateStore(new_value, global_var);
                    return new_value;
                } else {
                    if (var_info->type != value_type) {
                        LOG_CRITICAL("Type mismatch in set operation for variable: %s", var_name);
                    }
                    context.m_BUILDER.CreateStore(new_value, var_info->value);
                    return new_value;
                }
            }

            auto* global_var = context.m_MODULE.getNamedGlobal(var_name);
            if (global_var) {
                if (global_var->getValueType() != value_type) {
                    LOG_CRITICAL("Type mismatch in set operation for variable: %s", var_name);
                }
                context.m_BUILDER.CreateStore(new_value, global_var);
                return new_value;
            }

            LOG_CRITICAL("Cannot set undefined variable: %s", var_name);
        }

        auto get_priority() const -> int override { return 700; }
    };

}    // namespace galluz::generators
