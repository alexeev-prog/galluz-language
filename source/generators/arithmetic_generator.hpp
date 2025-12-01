#pragma once

#include <llvm/IR/Instructions.h>

#include "../core/generator_manager.hpp"
#include "../core/types.hpp"

namespace galluz::generators {

    class ArithmeticGenerator : public core::ICodeGenerator {
      private:
        core::GeneratorManager* m_GENERATOR_MANAGER;

        auto is_integer_type(llvm::Value* value) -> bool { return value->getType()->isIntegerTy(); }

        auto is_floating_type(llvm::Value* value) -> bool { return value->getType()->isFloatingPointTy(); }

        auto promote_to_double(llvm::Value* value, core::CompilationContext& context) -> llvm::Value* {
            if (is_integer_type(value)) {
                return context.m_BUILDER.CreateSIToFP(value, context.m_BUILDER.getDoubleTy());
            }
            return value;
        }

      public:
        explicit ArithmeticGenerator(core::GeneratorManager* manager)
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
            return op == "+" || op == "-" || op == "*" || op == "/" || op == "%";
        }

        auto generate(const Exp& ast_node, core::CompilationContext& context) -> llvm::Value* override {
            if (ast_node.list.size() < 2) {
                throw std::runtime_error("Arithmetic operation requires at least one operand");
            }

            const auto& op = ast_node.list[0].string;

            std::vector<llvm::Value*> operands;
            for (size_t i = 1; i < ast_node.list.size(); ++i) {
                operands.push_back(m_GENERATOR_MANAGER->generate_code(ast_node.list[i], context));
            }

            if (operands.empty()) {
                return context.m_BUILDER.getInt64(0);
            }

            if (operands.size() == 1) {
                if (op == "+") {
                    return operands[0];
                }
                if (op == "-") {
                    if (is_integer_type(operands[0])) {
                        return context.m_BUILDER.CreateNeg(operands[0]);
                    } else {
                        llvm::Value* zero = llvm::ConstantFP::get(operands[0]->getType(), 0.0);
                        return context.m_BUILDER.CreateFSub(zero, operands[0]);
                    }
                }
                return operands[0];
            }

            llvm::Value* result = operands[0];

            for (size_t i = 1; i < operands.size(); ++i) {
                llvm::Value* left = result;
                llvm::Value* right = operands[i];

                bool left_int = is_integer_type(left);
                bool right_int = is_integer_type(right);

                if (left_int && right_int) {
                    if (op == "+") {
                        result = context.m_BUILDER.CreateAdd(left, right);
                    } else if (op == "-") {
                        result = context.m_BUILDER.CreateSub(left, right);
                    } else if (op == "*") {
                        result = context.m_BUILDER.CreateMul(left, right);
                    } else if (op == "/") {
                        result = context.m_BUILDER.CreateSDiv(left, right);
                    } else if (op == "%") {
                        result = context.m_BUILDER.CreateSRem(left, right);
                    }
                } else {
                    llvm::Value* left_fp = promote_to_double(left, context);
                    llvm::Value* right_fp = promote_to_double(right, context);

                    if (op == "+") {
                        result = context.m_BUILDER.CreateFAdd(left_fp, right_fp);
                    } else if (op == "-") {
                        result = context.m_BUILDER.CreateFSub(left_fp, right_fp);
                    } else if (op == "*") {
                        result = context.m_BUILDER.CreateFMul(left_fp, right_fp);
                    } else if (op == "/") {
                        result = context.m_BUILDER.CreateFDiv(left_fp, right_fp);
                    } else if (op == "%") {
                        throw std::runtime_error("Modulo operation not supported for floating point");
                    }
                }
            }

            return result;
        }

        auto get_priority() const -> int override { return 500; }
    };

}    // namespace galluz::generators
