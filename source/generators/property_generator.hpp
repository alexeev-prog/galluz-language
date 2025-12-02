#pragma once

#include "../core/generator_manager.hpp"
#include "../core/types.hpp"

namespace galluz::generators {

    class PropertyGenerator : public core::ICodeGenerator {
      private:
        core::GeneratorManager* m_GENERATOR_MANAGER;

      public:
        explicit PropertyGenerator(core::GeneratorManager* manager)
            : m_GENERATOR_MANAGER(manager) {}

        auto can_handle(const Exp& ast_node) const -> bool override {
            if (ast_node.type != ExpType::LIST) {
                return false;
            }
            if (ast_node.list.empty()) {
                return false;
            }

            const auto& first = ast_node.list[0];
            if (first.type != ExpType::SYMBOL) {
                return false;
            }

            const std::string& op = first.string;
            return op == "getprop" || op == "setprop" || op == "hasprop";
        }

        auto generate(const Exp& ast_node, core::CompilationContext& context) -> llvm::Value* override {
            const auto& op = ast_node.list[0].string;

            if (op == "getprop") {
                return generate_getprop(ast_node, context);
            } else if (op == "setprop") {
                return generate_setprop(ast_node, context);
            } else if (op == "hasprop") {
                return generate_hasprop(ast_node, context);
            }

            LOG_CRITICAL("Unknown property operation: %s", op);
        }

      private:
        auto generate_getprop(const Exp& ast_node, core::CompilationContext& context) -> llvm::Value* {
            if (ast_node.list.size() != 3) {
                LOG_CRITICAL("getprop requires exactly 2 arguments: (getprop struct-instance field-name)");
            }

            llvm::Value* struct_value = m_GENERATOR_MANAGER->generate_code(ast_node.list[1], context);
            const auto& field_name_exp = ast_node.list[2];

            if (field_name_exp.type != ExpType::SYMBOL) {
                LOG_CRITICAL("Field name must be a symbol");
            }

            std::string field_name = field_name_exp.string;

            auto* var_info = context.find_variable_from_value(struct_value);
            if (!var_info) {
                auto* global_var = llvm::dyn_cast<llvm::GlobalVariable>(struct_value);
                if (global_var) {
                    std::string var_name = global_var->getName().str();
                    var_info = context.find_variable(var_name);
                }
            }

            if (!var_info) {
                LOG_CRITICAL("Cannot find variable info for struct");
            }

            if (!var_info->type_info || var_info->type_info->kind != core::TypeKind::STRUCT) {
                LOG_CRITICAL("Variable is not a struct");
            }

            auto* struct_info = var_info->type_info->struct_info;
            if (!struct_info) {
                LOG_CRITICAL("Struct info not found");
            }

            auto field_index_opt = context.type_system->get_struct_field_index(struct_info->name, field_name);
            if (!field_index_opt.has_value()) {
                LOG_CRITICAL("Struct %s has no field named %s", struct_info->name, field_name);
            }

            size_t field_index = field_index_opt.value();
            auto* field_type_info = struct_info->fields[field_index].type;
            if (!field_type_info) {
                LOG_CRITICAL("Field type info not found for: %s", field_name);
            }

            llvm::Value* gep = context.m_BUILDER.CreateStructGEP(
                struct_info->llvm_type, struct_value, field_index, field_name);

            return context.m_BUILDER.CreateLoad(field_type_info->llvm_type, gep, field_name);
        }

        auto generate_setprop(const Exp& ast_node, core::CompilationContext& context) -> llvm::Value* {
            if (ast_node.list.size() != 4) {
                LOG_CRITICAL(
                    "setprop requires exactly 3 arguments: (setprop struct-instance field-name value)");
            }

            llvm::Value* struct_value = m_GENERATOR_MANAGER->generate_code(ast_node.list[1], context);
            const auto& field_name_exp = ast_node.list[2];
            llvm::Value* new_value = m_GENERATOR_MANAGER->generate_code(ast_node.list[3], context);

            if (field_name_exp.type != ExpType::SYMBOL) {
                LOG_CRITICAL("Field name must be a symbol");
            }

            std::string field_name = field_name_exp.string;

            auto* var_info = context.find_variable_from_value(struct_value);
            if (!var_info) {
                auto* global_var = llvm::dyn_cast<llvm::GlobalVariable>(struct_value);
                if (global_var) {
                    std::string var_name = global_var->getName().str();
                    var_info = context.find_variable(var_name);
                }
            }

            if (!var_info) {
                LOG_CRITICAL("Cannot find variable info for struct");
            }

            if (!var_info->type_info || var_info->type_info->kind != core::TypeKind::STRUCT) {
                LOG_CRITICAL("Variable is not a struct");
            }

            auto* struct_info = var_info->type_info->struct_info;
            if (!struct_info) {
                LOG_CRITICAL("Struct info not found");
            }

            auto field_index_opt = context.type_system->get_struct_field_index(struct_info->name, field_name);
            if (!field_index_opt.has_value()) {
                LOG_CRITICAL("Struct %s has no field named %s", struct_info->name, field_name);
            }

            size_t field_index = field_index_opt.value();
            auto* field_type_info = struct_info->fields[field_index].type;
            if (!field_type_info) {
                LOG_CRITICAL("Field type info not found for: %s", field_name);
            }

            if (new_value->getType() != field_type_info->llvm_type) {
                if (field_type_info->kind == core::TypeKind::INT && new_value->getType()->isIntegerTy()) {
                    new_value = context.m_BUILDER.CreateIntCast(new_value, field_type_info->llvm_type, true);
                } else if (field_type_info->kind == core::TypeKind::DOUBLE
                           && new_value->getType()->isFloatingPointTy())
                {
                    new_value = context.m_BUILDER.CreateFPCast(new_value, field_type_info->llvm_type);
                } else if (field_type_info->kind == core::TypeKind::DOUBLE
                           && new_value->getType()->isIntegerTy())
                {
                    new_value = context.m_BUILDER.CreateSIToFP(new_value, field_type_info->llvm_type);
                } else if (field_type_info->kind == core::TypeKind::INT
                           && new_value->getType()->isFloatingPointTy())
                {
                    new_value = context.m_BUILDER.CreateFPToSI(new_value, field_type_info->llvm_type);
                } else if (field_type_info->kind == core::TypeKind::BOOL
                           && new_value->getType()->isIntegerTy())
                {
                    new_value =
                        context.m_BUILDER.CreateIntCast(new_value, context.m_BUILDER.getInt1Ty(), false);
                } else {
                    LOG_CRITICAL("Type mismatch in setprop for field: %s", field_name);
                }
            }

            llvm::Value* gep = context.m_BUILDER.CreateStructGEP(
                struct_info->llvm_type, struct_value, field_index, field_name);

            context.m_BUILDER.CreateStore(new_value, gep);
            return new_value;
        }

        auto generate_hasprop(const Exp& ast_node, core::CompilationContext& context) -> llvm::Value* {
            if (ast_node.list.size() != 3) {
                LOG_CRITICAL("hasprop requires exactly 2 arguments: (hasprop struct-instance field-name)");
            }

            llvm::Value* struct_value = m_GENERATOR_MANAGER->generate_code(ast_node.list[1], context);
            const auto& field_name_exp = ast_node.list[2];

            if (field_name_exp.type != ExpType::SYMBOL) {
                LOG_CRITICAL("Field name must be a symbol");
            }

            std::string field_name = field_name_exp.string;

            auto* var_info = context.find_variable_from_value(struct_value);
            if (!var_info) {
                auto* global_var = llvm::dyn_cast<llvm::GlobalVariable>(struct_value);
                if (global_var) {
                    std::string var_name = global_var->getName().str();
                    var_info = context.find_variable(var_name);
                }
            }

            if (!var_info || !var_info->type_info || var_info->type_info->kind != core::TypeKind::STRUCT) {
                return context.m_BUILDER.getInt1(false);
            }

            auto* struct_info = var_info->type_info->struct_info;
            if (!struct_info) {
                return context.m_BUILDER.getInt1(false);
            }

            auto field_index_opt = context.type_system->get_struct_field_index(struct_info->name, field_name);
            return context.m_BUILDER.getInt1(field_index_opt.has_value());
        }

        auto get_priority() const -> int override { return 850; }
    };

}    // namespace galluz::generators
