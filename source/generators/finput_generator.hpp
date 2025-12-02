#pragma once

#include <memory>

#include "../core/generator_manager.hpp"
#include "../core/preprocessor.hpp"
#include "../core/types.hpp"
#include "../logger.hpp"

namespace galluz::generators {

    class FinputGenerator : public core::ICodeGenerator {
      private:
        core::GeneratorManager* m_GENERATOR_MANAGER;
        std::unique_ptr<core::Preprocessor> m_PREPROCESSOR;

        auto ensure_scanf_function(core::CompilationContext& context) -> llvm::Function* {
            auto* existing_func = context.m_MODULE.getFunction("scanf");
            if (existing_func) {
                return existing_func;
            }

            auto* byte_ptr_ty = llvm::PointerType::get(context.m_CTX, 0);
            return llvm::Function::Create(
                llvm::FunctionType::get(context.m_BUILDER.getInt32Ty(), byte_ptr_ty, true),
                llvm::Function::ExternalLinkage,
                "scanf",
                &context.m_MODULE);
        }

        auto ensure_fgets_function(core::CompilationContext& context) -> llvm::Function* {
            auto* existing_func = context.m_MODULE.getFunction("fgets");
            if (existing_func) {
                return existing_func;
            }

            auto* char_ptr_ty = llvm::PointerType::get(context.m_BUILDER.getInt8Ty(), 0);
            auto* size_ty = context.m_BUILDER.getInt64Ty();
            auto* file_ptr_ty = llvm::PointerType::get(context.m_CTX, 0);

            std::vector<llvm::Type*> params = {char_ptr_ty, size_ty, file_ptr_ty};
            return llvm::Function::Create(llvm::FunctionType::get(char_ptr_ty, params, false),
                                          llvm::Function::ExternalLinkage,
                                          "fgets",
                                          &context.m_MODULE);
        }

        auto ensure_stdin_global(core::CompilationContext& context) -> llvm::GlobalVariable* {
            auto* existing = context.m_MODULE.getNamedGlobal("stdin");
            if (existing) {
                return existing;
            }

            auto* file_ptr_ty = llvm::PointerType::get(llvm::PointerType::get(context.m_CTX, 0), 0);
            return new llvm::GlobalVariable(context.m_MODULE,
                                            file_ptr_ty,
                                            false,
                                            llvm::GlobalVariable::ExternalLinkage,
                                            nullptr,
                                            "stdin");
        }

      public:
        explicit FinputGenerator(core::GeneratorManager* manager)
            : m_GENERATOR_MANAGER(manager) {
            m_PREPROCESSOR = std::make_unique<core::Preprocessor>();
        }

        auto can_handle(const Exp& ast_node) const -> bool override {
            if (ast_node.type != ExpType::LIST) {
                return false;
            }
            if (ast_node.list.empty()) {
                return false;
            }

            const auto& first = ast_node.list[0];
            return (first.type == ExpType::SYMBOL && first.string == "finput");
        }

        auto generate(const Exp& ast_node, core::CompilationContext& context) -> llvm::Value* override {
            if (ast_node.list.size() < 2) {
                throw std::runtime_error("finput requires at least a format string");
            }

            const auto& format_exp = ast_node.list[1];

            if (format_exp.type != ExpType::STRING) {
                throw std::runtime_error("First argument to finput must be a format string");
            }

            std::string format_str = m_PREPROCESSOR->postprocess_string(format_exp.string);

            if (ast_node.list.size() == 2) {
                return read_line_input(ast_node, context, format_str);
            } else {
                return read_formatted_input(ast_node, context, format_str);
            }
        }

      private:
        auto read_line_input(const Exp& ast_node,
                             core::CompilationContext& context,
                             const std::string& prompt) -> llvm::Value* {
            auto* printf_func = ensure_printf_function(context);
            auto* fgets_func = ensure_fgets_function(context);
            auto* stdin_global = ensure_stdin_global(context);

            if (prompt.empty()) {
                throw std::runtime_error("Line input requires a prompt or empty string");
            }

            llvm::Value* prompt_str = context.m_BUILDER.CreateGlobalStringPtr(prompt);
            std::vector<llvm::Value*> printf_args = {prompt_str};
            context.m_BUILDER.CreateCall(printf_func, printf_args);

            const size_t buffer_size = 1024;
            auto* buffer_alloca = context.m_BUILDER.CreateAlloca(
                context.m_BUILDER.getInt8Ty(), context.m_BUILDER.getInt64(buffer_size), "input_buffer");

            auto* size_val = context.m_BUILDER.getInt64(buffer_size);
            auto* stdin_val =
                context.m_BUILDER.CreateLoad(stdin_global->getValueType(), stdin_global, "stdin_val");

            std::vector<llvm::Value*> fgets_args = {buffer_alloca, size_val, stdin_val};
            auto* fgets_result = context.m_BUILDER.CreateCall(fgets_func, fgets_args);

            auto* null_ptr = llvm::ConstantPointerNull::get(context.m_BUILDER.getInt8Ty()->getPointerTo());
            auto* is_null = context.m_BUILDER.CreateICmpEQ(fgets_result, null_ptr, "check_null");

            llvm::Function* current_func = context.m_CURRENT_FUNCTION;
            llvm::BasicBlock* error_block =
                llvm::BasicBlock::Create(context.m_CTX, "input_error", current_func);
            llvm::BasicBlock* success_block =
                llvm::BasicBlock::Create(context.m_CTX, "input_success", current_func);

            context.m_BUILDER.CreateCondBr(is_null, error_block, success_block);

            context.m_BUILDER.SetInsertPoint(error_block);
            auto* error_str = context.m_BUILDER.CreateGlobalStringPtr("Input error\n");
            std::vector<llvm::Value*> error_args = {error_str};
            context.m_BUILDER.CreateCall(printf_func, error_args);
            context.m_BUILDER.CreateRet(context.m_BUILDER.getInt32(1));

            context.m_BUILDER.SetInsertPoint(success_block);

            return buffer_alloca;
        }

        auto read_formatted_input(const Exp& ast_node,
                                  core::CompilationContext& context,
                                  const std::string& format_str) -> llvm::Value* {
            auto* scanf_func = ensure_scanf_function(context);

            llvm::Value* format_val = context.m_BUILDER.CreateGlobalStringPtr(format_str);
            std::vector<llvm::Value*> scanf_args;
            scanf_args.push_back(format_val);

            std::vector<llvm::Value*> output_values;

            for (size_t i = 2; i < ast_node.list.size(); ++i) {
                const auto& arg_exp = ast_node.list[i];

                if (arg_exp.type == ExpType::SYMBOL) {
                    auto* var_info = context.find_variable(arg_exp.string);
                    if (!var_info) {
                        throw std::runtime_error("Variable not found for finput: " + arg_exp.string);
                    }

                    llvm::Value* storage_ptr = nullptr;

                    if (var_info->is_global) {
                        auto* global_var = context.m_MODULE.getNamedGlobal(arg_exp.string);
                        if (!global_var) {
                            throw std::runtime_error("Global variable not found: " + arg_exp.string);
                        }
                        storage_ptr = global_var;
                    } else {
                        if (var_info->type_info && var_info->type_info->kind == core::TypeKind::STRUCT) {
                            throw std::runtime_error("Cannot read directly into struct with finput");
                        }
                        storage_ptr = var_info->value;
                    }

                    scanf_args.push_back(storage_ptr);
                    output_values.push_back(storage_ptr);
                } else if (arg_exp.type == ExpType::LIST && arg_exp.list.size() == 2) {
                    const auto& type_exp = arg_exp.list[1];
                    if (type_exp.type != ExpType::SYMBOL || type_exp.string.empty()
                        || type_exp.string[0] != '!')
                    {
                        throw std::runtime_error("Invalid type specification in finput");
                    }

                    std::string type_str = type_exp.string.substr(1);
                    auto* type_info = context.type_system->get_type(type_str);
                    if (!type_info) {
                        throw std::runtime_error("Unknown type: " + type_str);
                    }

                    auto* alloca = context.m_BUILDER.CreateAlloca(type_info->llvm_type, nullptr, "input_tmp");
                    scanf_args.push_back(alloca);
                    output_values.push_back(alloca);
                } else {
                    throw std::runtime_error("Invalid argument to finput");
                }
            }

            auto* scanf_result = context.m_BUILDER.CreateCall(scanf_func, scanf_args);

            auto* expected_count = context.m_BUILDER.getInt32(ast_node.list.size() - 2);
            auto* is_error =
                context.m_BUILDER.CreateICmpSLT(scanf_result, expected_count, "scanf_error_check");

            llvm::Function* current_func = context.m_CURRENT_FUNCTION;
            llvm::BasicBlock* error_block =
                llvm::BasicBlock::Create(context.m_CTX, "scanf_error", current_func);
            llvm::BasicBlock* success_block =
                llvm::BasicBlock::Create(context.m_CTX, "scanf_success", current_func);

            context.m_BUILDER.CreateCondBr(is_error, error_block, success_block);

            context.m_BUILDER.SetInsertPoint(error_block);
            auto* error_str = context.m_BUILDER.CreateGlobalStringPtr("Input format error\n");
            auto* printf_func = ensure_printf_function(context);
            std::vector<llvm::Value*> error_args = {error_str};
            context.m_BUILDER.CreateCall(printf_func, error_args);
            context.m_BUILDER.CreateRet(context.m_BUILDER.getInt32(1));

            context.m_BUILDER.SetInsertPoint(success_block);

            if (output_values.size() == 1) {
                llvm::Type* element_type = nullptr;

                if (auto* alloca_inst = llvm::dyn_cast<llvm::AllocaInst>(output_values[0])) {
                    element_type = alloca_inst->getAllocatedType();
                } else if (auto* global_var = llvm::dyn_cast<llvm::GlobalVariable>(output_values[0])) {
                    element_type = global_var->getValueType();
                }

                if (element_type) {
                    return context.m_BUILDER.CreateLoad(element_type, output_values[0], "input_value");
                }
            }

            return scanf_result;
        }

        auto ensure_printf_function(core::CompilationContext& context) -> llvm::Function* {
            auto* existing_func = context.m_MODULE.getFunction("printf");
            if (existing_func) {
                return existing_func;
            }

            auto* byte_ptr_ty = llvm::PointerType::get(context.m_CTX, 0);
            return llvm::Function::Create(
                llvm::FunctionType::get(context.m_BUILDER.getInt32Ty(), byte_ptr_ty, true),
                llvm::Function::ExternalLinkage,
                "printf",
                &context.m_MODULE);
        }

        auto get_priority() const -> int override { return 300; }
    };

}    // namespace galluz::generators
