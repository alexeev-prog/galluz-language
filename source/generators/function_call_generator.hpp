#pragma once

#include "../core/generator_manager.hpp"
#include "../core/module_manager.hpp"
#include "../core/types.hpp"
#include "../logger.hpp"

namespace galluz::generators {

    class FunctionCallGenerator : public core::ICodeGenerator {
      private:
        core::GeneratorManager* m_GENERATOR_MANAGER;
        core::ModuleManager* m_MODULE_MANAGER;

        auto is_module_call(const Exp& ast_node) -> bool {
            if (ast_node.type != ExpType::LIST || ast_node.list.empty()) {
                return false;
            }

            const auto& first = ast_node.list[0];
            if (first.type != ExpType::SYMBOL) {
                return false;
            }

            std::string first_symbol = first.string;

            if (ast_node.list.size() < 2) {
                return false;
            }

            const auto& second = ast_node.list[1];
            if (second.type != ExpType::SYMBOL) {
                return false;
            }

            auto module_info = m_MODULE_MANAGER->get_module(first_symbol);
            if (!module_info) {
                return false;
            }

            return module_info->exported_symbols.find(second.string) != module_info->exported_symbols.end();
        }

        auto generate_module_call(const Exp& ast_node, core::CompilationContext& context) -> llvm::Value* {
            if (ast_node.list.size() < 3) {
                LOG_CRITICAL("Module call requires at least module name, function name and arguments");
            }

            const auto& module_name_exp = ast_node.list[0];
            const auto& func_name_exp = ast_node.list[1];

            if (module_name_exp.type != ExpType::SYMBOL || func_name_exp.type != ExpType::SYMBOL) {
                LOG_CRITICAL("Module and function names must be symbols");
            }

            std::string module_name = module_name_exp.string;
            std::string func_name = func_name_exp.string;
            std::string full_name = module_name + "." + func_name;

            auto* func_info = context.find_function(full_name);
            if (!func_info) {
                func_info = context.find_function(func_name);
                if (!func_info) {
                    LOG_CRITICAL(
                        "Function %s not found in module %s", func_name.c_str(), module_name.c_str());
                }
            }

            size_t expected_args = func_info->parameters.size();
            size_t actual_args = ast_node.list.size() - 2;

            if (actual_args != expected_args) {
                LOG_CRITICAL("Function %s.%s expects % arguments, got %s",
                             module_name.c_str(),
                             func_name.c_str(),
                             std::to_string(expected_args),
                             std::to_string(actual_args));
            }

            std::vector<llvm::Value*> args;
            for (size_t i = 2; i < ast_node.list.size(); ++i) {
                llvm::Value* arg_value = m_GENERATOR_MANAGER->generate_code(ast_node.list[i], context);

                auto& param = func_info->parameters[i - 2];

                if (arg_value->getType() != param.type) {
                    if (param.type_info->kind == core::TypeKind::STRUCT) {
                        if (!arg_value->getType()->isPointerTy()) {
                            LOG_CRITICAL("Struct argument must be a pointer for function: %s.%s",
                                         module_name.c_str(),
                                         func_name.c_str());
                        }
                    } else if (param.type_info->kind == core::TypeKind::INT
                               && arg_value->getType()->isIntegerTy())
                    {
                        arg_value = context.m_BUILDER.CreateIntCast(arg_value, param.type, true);
                    } else if (param.type_info->kind == core::TypeKind::DOUBLE
                               && arg_value->getType()->isFloatingPointTy())
                    {
                        arg_value = context.m_BUILDER.CreateFPCast(arg_value, param.type);
                    } else if (param.type_info->kind == core::TypeKind::DOUBLE
                               && arg_value->getType()->isIntegerTy())
                    {
                        arg_value = context.m_BUILDER.CreateSIToFP(arg_value, param.type);
                    } else if (param.type_info->kind == core::TypeKind::INT
                               && arg_value->getType()->isFloatingPointTy())
                    {
                        arg_value = context.m_BUILDER.CreateFPToSI(arg_value, param.type);
                    } else if (param.type_info->kind == core::TypeKind::BOOL
                               && arg_value->getType()->isIntegerTy())
                    {
                        arg_value = context.m_BUILDER.CreateIntCast(arg_value, param.type, false);
                    } else {
                        LOG_CRITICAL("Argument type mismatch for function: %s.%s",
                                     module_name.c_str(),
                                     func_name.c_str());
                    }
                }

                args.push_back(arg_value);
            }

            return context.m_BUILDER.CreateCall(func_info->function, args);
        }

      public:
        explicit FunctionCallGenerator(core::GeneratorManager* manager, core::ModuleManager* module_manager)
            : m_GENERATOR_MANAGER(manager)
            , m_MODULE_MANAGER(module_manager) {}

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

            std::string name = first.string;

            std::unordered_set<std::string> reserved_keywords = {
                "defn",    "var",     "global",    "set",      "scope",     "do",    "fprint",
                "if",      "while",   "break",     "continue", "struct",    "new",   "getprop",
                "setprop", "hasprop", "defmodule", "import",   "moduleuse", "finput"};

            if (reserved_keywords.count(name)) {
                return false;
            }

            std::unordered_set<std::string> operators = {
                "+", "-", "*", "/", "%", ">", "<", ">=", "<=", "==", "!="};

            if (operators.count(name)) {
                return false;
            }

            return true;
        }

        auto generate(const Exp& ast_node, core::CompilationContext& context) -> llvm::Value* override {
            if (is_module_call(ast_node)) {
                return generate_module_call(ast_node, context);
            }

            const auto& first = ast_node.list[0];
            std::string func_name = first.string;

            if (func_name.find('.') != std::string::npos) {
                return generate_dot_notation_call(func_name, ast_node, context);
            }

            auto* func_info = context.find_function(func_name);
            if (!func_info) {
                LOG_CRITICAL("Undefined function: %s", func_name);
            }

            size_t expected_args = func_info->parameters.size();
            size_t actual_args = ast_node.list.size() - 1;

            if (actual_args != expected_args) {
                LOG_CRITICAL("Function %s expects % arguments, got %s",
                             func_name,
                             std::to_string(expected_args),
                             std::to_string(actual_args));
            }

            std::vector<llvm::Value*> args;
            for (size_t i = 1; i < ast_node.list.size(); ++i) {
                llvm::Value* arg_value = m_GENERATOR_MANAGER->generate_code(ast_node.list[i], context);

                auto& param = func_info->parameters[i - 1];

                if (arg_value->getType() != param.type) {
                    if (param.type_info->kind == core::TypeKind::STRUCT) {
                        if (!arg_value->getType()->isPointerTy()) {
                            LOG_CRITICAL("Struct argument must be a pointer for function: %s", func_name);
                        }
                    } else if (param.type_info->kind == core::TypeKind::INT
                               && arg_value->getType()->isIntegerTy())
                    {
                        arg_value = context.m_BUILDER.CreateIntCast(arg_value, param.type, true);
                    } else if (param.type_info->kind == core::TypeKind::DOUBLE
                               && arg_value->getType()->isFloatingPointTy())
                    {
                        arg_value = context.m_BUILDER.CreateFPCast(arg_value, param.type);
                    } else if (param.type_info->kind == core::TypeKind::DOUBLE
                               && arg_value->getType()->isIntegerTy())
                    {
                        arg_value = context.m_BUILDER.CreateSIToFP(arg_value, param.type);
                    } else if (param.type_info->kind == core::TypeKind::INT
                               && arg_value->getType()->isFloatingPointTy())
                    {
                        arg_value = context.m_BUILDER.CreateFPToSI(arg_value, param.type);
                    } else if (param.type_info->kind == core::TypeKind::BOOL
                               && arg_value->getType()->isIntegerTy())
                    {
                        arg_value = context.m_BUILDER.CreateIntCast(arg_value, param.type, false);
                    } else {
                        LOG_CRITICAL("Argument type mismatch for function: %s", func_name);
                    }
                }

                args.push_back(arg_value);
            }

            return context.m_BUILDER.CreateCall(func_info->function, args);
        }

      private:
        auto generate_dot_notation_call(const std::string& full_name,
                                        const Exp& ast_node,
                                        core::CompilationContext& context) -> llvm::Value* {
            size_t dot_pos = full_name.find('.');
            std::string module_name = full_name.substr(0, dot_pos);
            std::string func_name = full_name.substr(dot_pos + 1);

            auto full_func_name = module_name + "." + func_name;
            auto* func_info = context.find_function(full_func_name);
            if (!func_info) {
                func_info = context.find_function(func_name);
                if (!func_info) {
                    LOG_CRITICAL("Function not found: %s", full_name);
                }
            }

            size_t expected_args = func_info->parameters.size();
            size_t actual_args = ast_node.list.size() - 1;

            if (actual_args != expected_args) {
                LOG_CRITICAL("Function %s expects % arguments, got %s",
                             full_name,
                             std::to_string(expected_args),
                             std::to_string(actual_args));
            }

            std::vector<llvm::Value*> args;
            for (size_t i = 1; i < ast_node.list.size(); ++i) {
                llvm::Value* arg_value = m_GENERATOR_MANAGER->generate_code(ast_node.list[i], context);

                auto& param = func_info->parameters[i - 1];

                if (arg_value->getType() != param.type) {
                    if (param.type_info->kind == core::TypeKind::STRUCT) {
                        if (!arg_value->getType()->isPointerTy()) {
                            LOG_CRITICAL("Struct argument must be a pointer for function: %s", full_name);
                        }
                    } else if (param.type_info->kind == core::TypeKind::INT
                               && arg_value->getType()->isIntegerTy())
                    {
                        arg_value = context.m_BUILDER.CreateIntCast(arg_value, param.type, true);
                    } else if (param.type_info->kind == core::TypeKind::DOUBLE
                               && arg_value->getType()->isFloatingPointTy())
                    {
                        arg_value = context.m_BUILDER.CreateFPCast(arg_value, param.type);
                    } else if (param.type_info->kind == core::TypeKind::DOUBLE
                               && arg_value->getType()->isIntegerTy())
                    {
                        arg_value = context.m_BUILDER.CreateSIToFP(arg_value, param.type);
                    } else if (param.type_info->kind == core::TypeKind::INT
                               && arg_value->getType()->isFloatingPointTy())
                    {
                        arg_value = context.m_BUILDER.CreateFPToSI(arg_value, param.type);
                    } else if (param.type_info->kind == core::TypeKind::BOOL
                               && arg_value->getType()->isIntegerTy())
                    {
                        arg_value = context.m_BUILDER.CreateIntCast(arg_value, param.type, false);
                    } else {
                        LOG_CRITICAL("Argument type mismatch for function: %s", full_name);
                    }
                }

                args.push_back(arg_value);
            }

            return context.m_BUILDER.CreateCall(func_info->function, args);
        }

        auto get_priority() const -> int override { return 250; }
    };

}    // namespace galluz::generators
