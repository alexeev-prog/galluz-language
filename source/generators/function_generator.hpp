#pragma once

#include <llvm/IR/Function.h>
#include <llvm/IR/Verifier.h>

#include "../core/generator_manager.hpp"
#include "../core/types.hpp"

namespace galluz::generators {

    class FunctionGenerator : public core::ICodeGenerator {
      private:
        core::GeneratorManager* m_GENERATOR_MANAGER;

        struct FunctionParam {
            std::string name;
            core::TypeInfo* type;
        };

        auto parse_typed_name(const Exp& name_exp, core::CompilationContext& context)
            -> std::pair<std::string, core::TypeInfo*> {
            if (name_exp.type != ExpType::LIST || name_exp.list.size() != 2) {
                throw std::runtime_error("Invalid function name format");
            }

            const auto& func_name_exp = name_exp.list[0];
            const auto& return_type_exp = name_exp.list[1];

            if (func_name_exp.type != ExpType::SYMBOL) {
                throw std::runtime_error("Function name must be a symbol");
            }

            std::string func_name = func_name_exp.string;

            core::TypeInfo* return_type = nullptr;
            if (return_type_exp.type == ExpType::SYMBOL && return_type_exp.string[0] == '!') {
                return_type = context.type_system->get_type(return_type_exp.string.substr(1));
            }

            if (!return_type) {
                throw std::runtime_error("Invalid return type specification: " + return_type_exp.string);
            }

            return {func_name, return_type};
        }

        auto parse_params(const Exp& params_list, core::CompilationContext& context)
            -> std::vector<FunctionParam> {
            std::vector<FunctionParam> params;

            if (params_list.type != ExpType::LIST) {
                throw std::runtime_error("Function parameters must be a list");
            }

            for (const auto& param_item : params_list.list) {
                if (param_item.type == ExpType::LIST && param_item.list.size() == 2) {
                    const auto& param_name_exp = param_item.list[0];
                    const auto& param_type_exp = param_item.list[1];

                    if (param_name_exp.type != ExpType::SYMBOL) {
                        throw std::runtime_error("Parameter name must be a symbol");
                    }

                    FunctionParam param;
                    param.name = param_name_exp.string;

                    if (param_type_exp.type == ExpType::SYMBOL && param_type_exp.string[0] == '!') {
                        param.type = context.type_system->get_type(param_type_exp.string.substr(1));
                    } else {
                        throw std::runtime_error("Parameter type must start with !");
                    }

                    if (!param.type) {
                        throw std::runtime_error("Unknown parameter type: " + param_type_exp.string);
                    }

                    params.push_back(param);
                } else {
                    throw std::runtime_error("Invalid parameter syntax");
                }
            }

            return params;
        }

        auto create_function_ir(const std::string& func_name,
                                const std::vector<FunctionParam>& params,
                                core::TypeInfo* return_type,
                                const Exp& body,
                                core::CompilationContext& context) -> llvm::Function* {
            std::vector<llvm::Type*> param_types;
            for (const auto& param : params) {
                param_types.push_back(param.type->llvm_type);
            }

            llvm::FunctionType* func_type =
                llvm::FunctionType::get(return_type->llvm_type, param_types, false);

            llvm::Function* func = llvm::Function::Create(
                func_type, llvm::Function::ExternalLinkage, func_name, &context.m_MODULE);

            auto* old_insert_block = context.m_BUILDER.GetInsertBlock();

            llvm::BasicBlock* entry_block = llvm::BasicBlock::Create(context.m_CTX, "entry", func);
            context.m_BUILDER.SetInsertPoint(entry_block);

            context.push_scope();

            size_t idx = 0;
            std::vector<core::VariableInfo> param_infos;
            for (auto& arg : func->args()) {
                const auto& param = params[idx];
                arg.setName(param.name);

                llvm::AllocaInst* alloca =
                    context.m_BUILDER.CreateAlloca(param.type->llvm_type, nullptr, param.name);

                context.m_BUILDER.CreateStore(&arg, alloca);

                core::VariableInfo var_info = {alloca, param.type->llvm_type, param.type, false, param.name};
                context.add_variable(param.name, alloca, param.type->llvm_type, param.type, false);
                param_infos.push_back(var_info);

                idx++;
            }

            context.add_function(func_name, func, return_type, param_infos, false);

            llvm::Value* result = m_GENERATOR_MANAGER->generate_code(body, context);

            if (return_type->kind == core::TypeKind::VOID) {
                context.m_BUILDER.CreateRetVoid();
            } else if (result) {
                if (result->getType() != return_type->llvm_type) {
                    if (return_type->kind == core::TypeKind::INT && result->getType()->isIntegerTy()) {
                        result = context.m_BUILDER.CreateIntCast(result, return_type->llvm_type, true);
                    } else if (return_type->kind == core::TypeKind::DOUBLE
                               && result->getType()->isFloatingPointTy())
                    {
                        result = context.m_BUILDER.CreateFPCast(result, return_type->llvm_type);
                    } else if (return_type->kind == core::TypeKind::DOUBLE
                               && result->getType()->isIntegerTy())
                    {
                        result = context.m_BUILDER.CreateSIToFP(result, return_type->llvm_type);
                    } else if (return_type->kind == core::TypeKind::INT
                               && result->getType()->isFloatingPointTy())
                    {
                        result = context.m_BUILDER.CreateFPToSI(result, return_type->llvm_type);
                    } else if (return_type->kind == core::TypeKind::BOOL && result->getType()->isIntegerTy())
                    {
                        result =
                            context.m_BUILDER.CreateIntCast(result, context.m_BUILDER.getInt1Ty(), false);
                    }
                }
                context.m_BUILDER.CreateRet(result);
            } else {
                context.m_BUILDER.CreateRet(llvm::Constant::getNullValue(return_type->llvm_type));
            }

            context.pop_scope();

            if (old_insert_block) {
                context.m_BUILDER.SetInsertPoint(old_insert_block);
            }

            llvm::verifyFunction(*func);

            return func;
        }

      public:
        explicit FunctionGenerator(core::GeneratorManager* manager)
            : m_GENERATOR_MANAGER(manager) {}

        auto can_handle(const Exp& ast_node) const -> bool override {
            if (ast_node.type != ExpType::LIST) {
                return false;
            }
            if (ast_node.list.empty()) {
                return false;
            }

            const auto& first = ast_node.list[0];
            return (first.type == ExpType::SYMBOL && first.string == "defn");
        }

        auto generate(const Exp& ast_node, core::CompilationContext& context) -> llvm::Value* override {
            if (ast_node.list.size() < 4) {
                throw std::runtime_error("Invalid function definition syntax");
            }

            const auto& name_exp = ast_node.list[1];
            const auto& params_exp = ast_node.list[2];
            const auto& body_exp = ast_node.list[3];

            auto [func_name, return_type] = parse_typed_name(name_exp, context);
            auto params = parse_params(params_exp, context);

            auto* old_func = context.m_CURRENT_FUNCTION;

            llvm::Function* func = create_function_ir(func_name, params, return_type, body_exp, context);

            context.m_CURRENT_FUNCTION = old_func;

            std::vector<core::VariableInfo> param_infos;
            for (const auto& param : params) {
                core::VariableInfo var_info = {nullptr, param.type->llvm_type, param.type, false, param.name};
                param_infos.push_back(var_info);
            }

            context.add_function(func_name, func, return_type, param_infos, false);

            return func;
        }

        auto get_priority() const -> int override { return 200; }
    };

}    // namespace galluz::generators
