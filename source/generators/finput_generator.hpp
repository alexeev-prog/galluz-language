#pragma once

#include <memory>
#include <utility>
#include <vector>

#include "../core/generator_manager.hpp"
#include "../core/preprocessor.hpp"
#include "../core/types.hpp"
#include "../logger.hpp"

namespace galluz::generators {

    class FinputGenerator : public core::ICodeGenerator {
      private:
        core::GeneratorManager* m_GENERATOR_MANAGER;
        std::unique_ptr<core::Preprocessor> m_PREPROCESSOR;

        struct OutputValue {
            llvm::Value* storage_ptr;
            llvm::Value* string_buffer;

            OutputValue(llvm::Value* storage = nullptr, llvm::Value* buffer = nullptr)
                : storage_ptr(storage)
                , string_buffer(buffer) {}
        };

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

        auto ensure_fflush_function(core::CompilationContext& context) -> llvm::Function* {
            auto* existing_func = context.m_MODULE.getFunction("fflush");
            if (existing_func) {
                return existing_func;
            }

            auto* file_ptr_ty = llvm::PointerType::get(context.m_CTX, 0);
            return llvm::Function::Create(
                llvm::FunctionType::get(context.m_BUILDER.getInt32Ty(), file_ptr_ty, false),
                llvm::Function::ExternalLinkage,
                "fflush",
                &context.m_MODULE);
        }

        auto ensure_stdout_global(core::CompilationContext& context) -> llvm::GlobalVariable* {
            auto* existing = context.m_MODULE.getNamedGlobal("stdout");
            if (existing) {
                return existing;
            }

            auto* file_ptr_ty = llvm::PointerType::get(llvm::PointerType::get(context.m_CTX, 0), 0);
            return new llvm::GlobalVariable(context.m_MODULE,
                                            file_ptr_ty,
                                            false,
                                            llvm::GlobalVariable::ExternalLinkage,
                                            nullptr,
                                            "stdout");
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
                return read_line_input(context, format_str);
            } else {
                return read_formatted_input(ast_node, context, format_str);
            }
        }

      private:
        auto read_line_input(core::CompilationContext& context, const std::string& prompt) -> llvm::Value* {
            auto* printf_func = ensure_printf_function(context);
            auto* scanf_func = ensure_scanf_function(context);

            llvm::Value* prompt_str = context.m_BUILDER.CreateGlobalStringPtr(prompt);
            std::vector<llvm::Value*> printf_args = {prompt_str};
            context.m_BUILDER.CreateCall(printf_func, printf_args);

            const size_t buffer_size = 1024;
            auto* buffer_alloca = context.m_BUILDER.CreateAlloca(
                context.m_BUILDER.getInt8Ty(), context.m_BUILDER.getInt64(buffer_size), "input_buffer");

            llvm::Value* scan_format = context.m_BUILDER.CreateGlobalStringPtr("%1023[^\n]");
            std::vector<llvm::Value*> scanf_args = {scan_format, buffer_alloca};
            auto* scanf_result = context.m_BUILDER.CreateCall(scanf_func, scanf_args);

            auto* zero = context.m_BUILDER.getInt32(0);
            auto* is_error = context.m_BUILDER.CreateICmpSLT(scanf_result, zero, "scanf_error_check");

            llvm::Function* current_func = context.m_CURRENT_FUNCTION;
            llvm::BasicBlock* error_block =
                llvm::BasicBlock::Create(context.m_CTX, "input_error", current_func);
            llvm::BasicBlock* success_block =
                llvm::BasicBlock::Create(context.m_CTX, "input_success", current_func);

            context.m_BUILDER.CreateCondBr(is_error, error_block, success_block);

            context.m_BUILDER.SetInsertPoint(error_block);
            auto* error_str = context.m_BUILDER.CreateGlobalStringPtr("Input error\n");
            std::vector<llvm::Value*> error_args = {error_str};
            context.m_BUILDER.CreateCall(printf_func, error_args);
            context.m_BUILDER.CreateRet(context.m_BUILDER.getInt32(1));

            context.m_BUILDER.SetInsertPoint(success_block);

            auto* clear_format = context.m_BUILDER.CreateGlobalStringPtr("%*[^\n]");
            std::vector<llvm::Value*> clear_args = {clear_format};
            context.m_BUILDER.CreateCall(scanf_func, clear_args);

            auto* clear_newline = context.m_BUILDER.CreateGlobalStringPtr("%*c");
            std::vector<llvm::Value*> clear_newline_args = {clear_newline};
            context.m_BUILDER.CreateCall(scanf_func, clear_newline_args);

            return buffer_alloca;
        }

        auto read_formatted_input(const Exp& ast_node,
                                  core::CompilationContext& context,
                                  const std::string& format_str) -> llvm::Value* {
            auto* printf_func = ensure_printf_function(context);
            auto* scanf_func = ensure_scanf_function(context);
            auto* fflush_func = ensure_fflush_function(context);
            auto* stdout_global = ensure_stdout_global(context);

            llvm::Value* prompt_str = context.m_BUILDER.CreateGlobalStringPtr(format_str);
            std::vector<llvm::Value*> printf_args = {prompt_str};
            context.m_BUILDER.CreateCall(printf_func, printf_args);

            auto* stdout_val =
                context.m_BUILDER.CreateLoad(stdout_global->getValueType(), stdout_global, "stdout_val");
            std::vector<llvm::Value*> fflush_args = {stdout_val};
            context.m_BUILDER.CreateCall(fflush_func, fflush_args);

            llvm::Value* scan_format =
                context.m_BUILDER.CreateGlobalStringPtr(create_scan_format(ast_node, context).c_str());
            std::vector<llvm::Value*> scanf_args;
            scanf_args.push_back(scan_format);

            std::vector<OutputValue> output_values;

            for (size_t i = 2; i < ast_node.list.size(); ++i) {
                const auto& arg_exp = ast_node.list[i];

                if (arg_exp.type == ExpType::SYMBOL && arg_exp.string[0] != '!') {
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

                    if (var_info->type_info && var_info->type_info->kind == core::TypeKind::STRING) {
                        auto* buffer_alloca = context.m_BUILDER.CreateAlloca(
                            context.m_BUILDER.getInt8Ty(), context.m_BUILDER.getInt64(256), "str_buffer");
                        scanf_args.push_back(buffer_alloca);
                        output_values.emplace_back(storage_ptr, buffer_alloca);
                    } else {
                        scanf_args.push_back(storage_ptr);
                        output_values.emplace_back(storage_ptr, nullptr);
                    }
                } else if (arg_exp.type == ExpType::SYMBOL && arg_exp.string[0] == '!') {
                    std::string type_str = arg_exp.string.substr(1);
                    auto* type_info = context.type_system->get_type(type_str);
                    if (!type_info) {
                        throw std::runtime_error("Unknown type: " + type_str);
                    }

                    if (type_info->kind == core::TypeKind::STRING) {
                        auto* buffer_alloca = context.m_BUILDER.CreateAlloca(
                            context.m_BUILDER.getInt8Ty(), context.m_BUILDER.getInt64(256), "str_buffer");
                        scanf_args.push_back(buffer_alloca);
                        auto* str_ptr_alloca =
                            context.m_BUILDER.CreateAlloca(type_info->llvm_type, nullptr, "input_str_ptr");
                        output_values.emplace_back(str_ptr_alloca, buffer_alloca);
                    } else {
                        auto* alloca =
                            context.m_BUILDER.CreateAlloca(type_info->llvm_type, nullptr, "input_tmp");
                        scanf_args.push_back(alloca);
                        output_values.emplace_back(alloca, nullptr);
                    }
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

                    if (type_info->kind == core::TypeKind::STRING) {
                        auto* buffer_alloca = context.m_BUILDER.CreateAlloca(
                            context.m_BUILDER.getInt8Ty(), context.m_BUILDER.getInt64(256), "str_buffer");
                        scanf_args.push_back(buffer_alloca);
                        auto* str_ptr_alloca =
                            context.m_BUILDER.CreateAlloca(type_info->llvm_type, nullptr, "input_str_ptr");
                        output_values.emplace_back(str_ptr_alloca, buffer_alloca);
                    } else {
                        auto* alloca =
                            context.m_BUILDER.CreateAlloca(type_info->llvm_type, nullptr, "input_tmp");
                        scanf_args.push_back(alloca);
                        output_values.emplace_back(alloca, nullptr);
                    }
                } else {
                    throw std::runtime_error("Invalid argument to finput");
                }
            }

            auto* scanf_result = context.m_BUILDER.CreateCall(scanf_func, scanf_args);

            auto* expected_count = context.m_BUILDER.getInt32(ast_node.list.size() - 2);
            auto* is_error =
                context.m_BUILDER.CreateICmpNE(scanf_result, expected_count, "scanf_error_check");

            llvm::Function* current_func = context.m_CURRENT_FUNCTION;
            llvm::BasicBlock* error_block =
                llvm::BasicBlock::Create(context.m_CTX, "scanf_error", current_func);
            llvm::BasicBlock* success_block =
                llvm::BasicBlock::Create(context.m_CTX, "scanf_success", current_func);
            llvm::BasicBlock* cleanup_block =
                llvm::BasicBlock::Create(context.m_CTX, "scanf_cleanup", current_func);

            context.m_BUILDER.CreateCondBr(is_error, error_block, success_block);

            context.m_BUILDER.SetInsertPoint(error_block);
            auto* error_str =
                context.m_BUILDER.CreateGlobalStringPtr("Input format error. Expected %d values, got %d\n");
            std::vector<llvm::Value*> error_args = {error_str, expected_count, scanf_result};
            context.m_BUILDER.CreateCall(printf_func, error_args);

            auto* clear_format = context.m_BUILDER.CreateGlobalStringPtr("%*[^\n]");
            std::vector<llvm::Value*> clear_args = {clear_format};
            context.m_BUILDER.CreateCall(scanf_func, clear_args);

            context.m_BUILDER.CreateBr(cleanup_block);

            context.m_BUILDER.SetInsertPoint(success_block);

            for (auto& output : output_values) {
                if (output.string_buffer) {
                    llvm::Value* str_copy = copy_string_to_heap(output.string_buffer, context);
                    context.m_BUILDER.CreateStore(str_copy, output.storage_ptr);
                }
            }

            auto* clear_rest = context.m_BUILDER.CreateGlobalStringPtr("%*[^\n]");
            std::vector<llvm::Value*> clear_rest_args = {clear_rest};
            context.m_BUILDER.CreateCall(scanf_func, clear_rest_args);

            context.m_BUILDER.CreateBr(cleanup_block);

            context.m_BUILDER.SetInsertPoint(cleanup_block);

            auto* clear_newline = context.m_BUILDER.CreateGlobalStringPtr("%*c");
            std::vector<llvm::Value*> clear_newline_args = {clear_newline};
            context.m_BUILDER.CreateCall(scanf_func, clear_newline_args);

            if (output_values.size() == 1) {
                auto& output = output_values[0];
                llvm::Type* element_type = nullptr;

                if (auto* alloca_inst = llvm::dyn_cast<llvm::AllocaInst>(output.storage_ptr)) {
                    element_type = alloca_inst->getAllocatedType();
                } else if (auto* global_var = llvm::dyn_cast<llvm::GlobalVariable>(output.storage_ptr)) {
                    element_type = global_var->getValueType();
                }

                if (element_type) {
                    return context.m_BUILDER.CreateLoad(element_type, output.storage_ptr, "input_value");
                }
            }

            return scanf_result;
        }

        auto copy_string_to_heap(llvm::Value* buffer, core::CompilationContext& context) -> llvm::Value* {
            auto* strlen_func = ensure_strlen_function(context);
            auto* malloc_func = ensure_malloc_function(context);
            auto* strcpy_func = ensure_strcpy_function(context);

            llvm::Value* len = context.m_BUILDER.CreateCall(strlen_func, {buffer}, "strlen");
            llvm::Value* len_plus_one =
                context.m_BUILDER.CreateAdd(len, context.m_BUILDER.getInt64(1), "len_plus_one");

            llvm::Value* heap_ptr =
                context.m_BUILDER.CreateCall(malloc_func, {len_plus_one}, "malloc_result");

            context.m_BUILDER.CreateCall(strcpy_func, {heap_ptr, buffer});

            return heap_ptr;
        }

        auto ensure_strlen_function(core::CompilationContext& context) -> llvm::Function* {
            auto* existing_func = context.m_MODULE.getFunction("strlen");
            if (existing_func) {
                return existing_func;
            }

            auto* byte_ptr_ty = llvm::PointerType::get(context.m_CTX, 0);
            return llvm::Function::Create(
                llvm::FunctionType::get(context.m_BUILDER.getInt64Ty(), byte_ptr_ty, false),
                llvm::Function::ExternalLinkage,
                "strlen",
                &context.m_MODULE);
        }

        auto ensure_malloc_function(core::CompilationContext& context) -> llvm::Function* {
            auto* existing_func = context.m_MODULE.getFunction("malloc");
            if (existing_func) {
                return existing_func;
            }

            return llvm::Function::Create(
                llvm::FunctionType::get(
                    llvm::PointerType::get(context.m_CTX, 0), context.m_BUILDER.getInt64Ty(), false),
                llvm::Function::ExternalLinkage,
                "malloc",
                &context.m_MODULE);
        }

        auto ensure_strcpy_function(core::CompilationContext& context) -> llvm::Function* {
            auto* existing_func = context.m_MODULE.getFunction("strcpy");
            if (existing_func) {
                return existing_func;
            }

            auto* byte_ptr_ty = llvm::PointerType::get(context.m_CTX, 0);
            return llvm::Function::Create(
                llvm::FunctionType::get(byte_ptr_ty, {byte_ptr_ty, byte_ptr_ty}, false),
                llvm::Function::ExternalLinkage,
                "strcpy",
                &context.m_MODULE);
        }

        auto create_scan_format(const Exp& ast_node, core::CompilationContext& context) -> std::string {
            std::string format;

            for (size_t i = 2; i < ast_node.list.size(); ++i) {
                const auto& arg_exp = ast_node.list[i];

                if (arg_exp.type == ExpType::SYMBOL && arg_exp.string[0] != '!') {
                    auto* var_info = context.find_variable(arg_exp.string);
                    if (!var_info) {
                        throw std::runtime_error("Variable not found: " + arg_exp.string);
                    }

                    if (var_info->type_info) {
                        if (var_info->type_info->kind == core::TypeKind::STRING) {
                            format += "%255s";
                        } else {
                            format += get_format_specifier(var_info->type_info);
                        }
                    } else {
                        format += "%d";
                    }

                    if (i < ast_node.list.size() - 1) {
                        format += " ";
                    }
                } else if (arg_exp.type == ExpType::SYMBOL && arg_exp.string[0] == '!') {
                    std::string type_str = arg_exp.string.substr(1);
                    auto* type_info = context.type_system->get_type(type_str);

                    if (type_info) {
                        if (type_info->kind == core::TypeKind::STRING) {
                            format += "%255s";
                        } else {
                            format += get_format_specifier(type_info);
                        }
                    } else {
                        format += "%d";
                    }

                    if (i < ast_node.list.size() - 1) {
                        format += " ";
                    }
                } else if (arg_exp.type == ExpType::LIST && arg_exp.list.size() == 2) {
                    const auto& type_exp = arg_exp.list[1];
                    std::string type_str = type_exp.string.substr(1);
                    auto* type_info = context.type_system->get_type(type_str);

                    if (type_info) {
                        if (type_info->kind == core::TypeKind::STRING) {
                            format += "%255s";
                        } else {
                            format += get_format_specifier(type_info);
                        }
                    } else {
                        format += "%d";
                    }

                    if (i < ast_node.list.size() - 1) {
                        format += " ";
                    }
                }
            }

            return format;
        }

        auto get_format_specifier(core::TypeInfo* type_info) -> std::string {
            if (!type_info) {
                return "%d";
            }

            switch (type_info->kind) {
                case core::TypeKind::INT:
                    return "%d";
                case core::TypeKind::DOUBLE:
                    return "%lf";
                case core::TypeKind::BOOL:
                    return "%d";
                case core::TypeKind::STRING:
                    return "%s";
                default:
                    return "%d";
            }
        }

        auto get_priority() const -> int override { return 300; }
    };

}    // namespace galluz::generators
