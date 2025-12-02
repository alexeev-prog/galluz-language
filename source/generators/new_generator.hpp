#pragma once

#include "../core/generator_manager.hpp"
#include "../core/types.hpp"
#include "../logger.hpp"

namespace galluz::generators {

    class NewGenerator : public core::ICodeGenerator {
      private:
        core::GeneratorManager* m_GENERATOR_MANAGER;

      public:
        explicit NewGenerator(core::GeneratorManager* manager)
            : m_GENERATOR_MANAGER(manager) {}

        auto can_handle(const Exp& ast_node) const -> bool override {
            if (ast_node.type != ExpType::LIST) {
                return false;
            }
            if (ast_node.list.empty()) {
                return false;
            }

            const auto& first = ast_node.list[0];
            return (first.type == ExpType::SYMBOL && first.string == "new");
        }

        auto generate(const Exp& ast_node, core::CompilationContext& context) -> llvm::Value* override {
            if (ast_node.list.size() < 2) {
                LOG_CRITICAL("new requires at least struct name: (new StructName ...)");
            }

            const auto& struct_name_exp = ast_node.list[1];
            if (struct_name_exp.type != ExpType::SYMBOL) {
                LOG_CRITICAL("Struct name must be a symbol");
            }

            std::string struct_name = struct_name_exp.string;
            auto* type_info = context.type_system->get_type(struct_name);

            if (!type_info || type_info->kind != core::TypeKind::STRUCT) {
                LOG_CRITICAL("Unknown struct type: %s", struct_name);
            }

            auto* struct_info = type_info->struct_info;
            if (!struct_info) {
                LOG_CRITICAL("Struct info not found for: %s", struct_name);
            }

            auto* alloca =
                context.m_BUILDER.CreateAlloca(type_info->llvm_type, nullptr, struct_name + "_inst");

            auto* zero_init = llvm::ConstantAggregateZero::get(type_info->llvm_type);
            context.m_BUILDER.CreateStore(zero_init, alloca);

            std::unordered_map<std::string, llvm::Value*> field_values;

            for (size_t i = 2; i < ast_node.list.size(); ++i) {
                const auto& field_assignment = ast_node.list[i];

                if (field_assignment.type != ExpType::LIST || field_assignment.list.size() != 2) {
                    LOG_CRITICAL("Field assignment must be (field-name value)");
                }

                const auto& field_name_exp = field_assignment.list[0];
                const auto& field_value_exp = field_assignment.list[1];

                if (field_name_exp.type != ExpType::SYMBOL) {
                    LOG_CRITICAL("Field name must be a symbol");
                }

                std::string field_name = field_name_exp.string;

                if (field_values.find(field_name) != field_values.end()) {
                    LOG_CRITICAL("Duplicate field assignment for: %s", field_name);
                }

                auto field_index_opt = context.type_system->get_struct_field_index(struct_name, field_name);
                if (!field_index_opt.has_value()) {
                    LOG_CRITICAL("Struct %s has no field named %s", struct_name, field_name);
                }

                llvm::Value* field_value = m_GENERATOR_MANAGER->generate_code(field_value_exp, context);
                field_values[field_name] = field_value;
            }

            for (const auto& [field_name, field_value] : field_values) {
                auto field_index_opt = context.type_system->get_struct_field_index(struct_name, field_name);
                size_t field_index = field_index_opt.value();

                auto* field_type_info = struct_info->fields[field_index].type;

                llvm::Value* casted_value = field_value;
                if (field_value->getType() != field_type_info->llvm_type) {
                    if (field_type_info->kind == core::TypeKind::INT && field_value->getType()->isIntegerTy())
                    {
                        casted_value =
                            context.m_BUILDER.CreateIntCast(field_value, field_type_info->llvm_type, true);
                    } else if (field_type_info->kind == core::TypeKind::DOUBLE
                               && field_value->getType()->isFloatingPointTy())
                    {
                        casted_value =
                            context.m_BUILDER.CreateFPCast(field_value, field_type_info->llvm_type);
                    } else if (field_type_info->kind == core::TypeKind::DOUBLE
                               && field_value->getType()->isIntegerTy())
                    {
                        casted_value =
                            context.m_BUILDER.CreateSIToFP(field_value, field_type_info->llvm_type);
                    } else if (field_type_info->kind == core::TypeKind::INT
                               && field_value->getType()->isFloatingPointTy())
                    {
                        casted_value =
                            context.m_BUILDER.CreateFPToSI(field_value, field_type_info->llvm_type);
                    } else if (field_type_info->kind == core::TypeKind::BOOL
                               && field_value->getType()->isIntegerTy())
                    {
                        casted_value = context.m_BUILDER.CreateIntCast(
                            field_value, context.m_BUILDER.getInt1Ty(), false);
                    } else if (field_type_info->kind == core::TypeKind::STRING
                               && field_value->getType()->isPointerTy())
                    {
                        casted_value = field_value;
                    } else {
                        LOG_CRITICAL("Type mismatch for field %s in struct %s", field_name, struct_name);
                    }
                }

                llvm::Value* gep =
                    context.m_BUILDER.CreateStructGEP(type_info->llvm_type, alloca, field_index, field_name);
                context.m_BUILDER.CreateStore(casted_value, gep);
            }

            return alloca;
        }

        auto get_priority() const -> int override { return 850; }
    };

}    // namespace galluz::generators
