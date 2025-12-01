#pragma once

#include <llvm/IR/GlobalVariable.h>

#include "../core/generator_manager.hpp"
#include "../core/types.hpp"

namespace galluz::generators {

    class VariableGenerator : public core::ICodeGenerator {
      private:
        core::GeneratorManager* m_GENERATOR_MANAGER;

      public:
        explicit VariableGenerator(core::GeneratorManager* manager)
            : m_GENERATOR_MANAGER(manager) {}

        auto can_handle(const Exp& ast_node) const -> bool override {
            if (ast_node.type != ExpType::LIST) {
                return false;
            }
            if (ast_node.list.empty()) {
                return false;
            }

            const auto& first = ast_node.list[0];
            return (first.type == ExpType::SYMBOL && (first.string == "var" || first.string == "global"));
        }

        auto generate(const Exp& ast_node, core::CompilationContext& context) -> llvm::Value* override {
            if (ast_node.list.size() < 3) {
                throw std::runtime_error("Invalid variable declaration");
            }

            const auto& first = ast_node.list[0];
            const auto& name_exp = ast_node.list[1];
            const auto& value_exp = ast_node.list[2];

            std::string var_name;
            core::TypeInfo* type_info = nullptr;
            bool is_global = (first.string == "global");

            if (name_exp.type == ExpType::LIST) {
                if (name_exp.list.size() != 2) {
                    throw std::runtime_error("Invalid type annotation");
                }
                if (name_exp.list[0].type != ExpType::SYMBOL) {
                    throw std::runtime_error("Variable name must be a symbol");
                }
                if (name_exp.list[1].type != ExpType::SYMBOL || name_exp.list[1].string.empty()
                    || name_exp.list[1].string[0] != '!')
                {
                    throw std::runtime_error("Type annotation must start with !");
                }

                var_name = name_exp.list[0].string;
                std::string type_str = name_exp.list[1].string.substr(1);
                type_info = context.type_system->get_type(type_str);
                if (!type_info) {
                    throw std::runtime_error("Unknown type: " + type_str);
                }
            } else if (name_exp.type == ExpType::SYMBOL) {
                var_name = name_exp.string;
            } else {
                throw std::runtime_error("Variable name must be a symbol or typed specification");
            }

            llvm::Value* init_value = m_GENERATOR_MANAGER->generate_code(value_exp, context);

            if (type_info) {
                if (init_value->getType() != type_info->llvm_type) {
                    if (type_info->kind == core::TypeKind::INT && init_value->getType()->isIntegerTy()) {
                        init_value = context.m_BUILDER.CreateIntCast(init_value, type_info->llvm_type, true);
                    } else if (type_info->kind == core::TypeKind::DOUBLE
                               && init_value->getType()->isFloatingPointTy())
                    {
                        init_value = context.m_BUILDER.CreateFPCast(init_value, type_info->llvm_type);
                    } else if (type_info->kind == core::TypeKind::DOUBLE
                               && init_value->getType()->isIntegerTy())
                    {
                        init_value = context.m_BUILDER.CreateSIToFP(init_value, type_info->llvm_type);
                    } else if (type_info->kind == core::TypeKind::INT
                               && init_value->getType()->isFloatingPointTy())
                    {
                        init_value = context.m_BUILDER.CreateFPToSI(init_value, type_info->llvm_type);
                    } else {
                        throw std::runtime_error("Type mismatch for variable " + var_name);
                    }
                }
            }

            llvm::Type* value_type = init_value->getType();

            if (is_global) {
                llvm::Constant* const_init = llvm::dyn_cast<llvm::Constant>(init_value);
                if (!const_init) {
                    throw std::runtime_error("Global variable initializer must be constant");
                }

                context.m_MODULE.getOrInsertGlobal(var_name, value_type);
                auto* variable = context.m_MODULE.getNamedGlobal(var_name);

                variable->setAlignment(llvm::MaybeAlign(8));
                variable->setConstant(false);
                variable->setInitializer(const_init);

                context.add_variable(var_name, variable, value_type, type_info, true);

                return init_value;
            } else {
                auto* alloca = context.m_BUILDER.CreateAlloca(value_type, nullptr, var_name);
                context.m_BUILDER.CreateStore(init_value, alloca);

                context.add_variable(var_name, alloca, value_type, type_info, false);

                return init_value;
            }
        }

        auto get_priority() const -> int override { return 800; }
    };

}    // namespace galluz::generators
