#pragma once

#include "../core/generator_manager.hpp"
#include "../core/types.hpp"
#include "../logger.hpp"

namespace galluz::generators {

    class FunctionCallGenerator : public core::ICodeGenerator {
      private:
        core::GeneratorManager* m_GENERATOR_MANAGER;

      public:
        explicit FunctionCallGenerator(core::GeneratorManager* manager)
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

            std::string name = first.string;
            if (name == "defn" || name == "var" || name == "global" || name == "set" || name == "scope"
                || name == "do" || name == "fprint" || name == "if" || name == "while" || name == "break"
                || name == "continue" || name == "struct" || name == "new" || name == "getprop"
                || name == "setprop" || name == "hasprop")
            {
                return false;
            }

            if (name == "+" || name == "-" || name == "*" || name == "/" || name == "%" || name == ">"
                || name == "<" || name == ">=" || name == "<=" || name == "==" || name == "!=")
            {
                return false;
            }

            return true;
        }

        auto generate(const Exp& ast_node, core::CompilationContext& context) -> llvm::Value* override {
            const auto& first = ast_node.list[0];
            std::string func_name = first.string;

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
                        LOG_DEBUG("Casting int: from %s to %s",
                                  llvm_type_to_string(arg_value->getType()).c_str(),
                                  llvm_type_to_string(param.type).c_str());
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

        auto get_priority() const -> int override { return 250; }
    };

}    // namespace galluz::generators
