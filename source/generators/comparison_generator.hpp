#pragma once

#include "../core/generator_manager.hpp"
#include "../core/types.hpp"

namespace galluz::generators {

    class ComparisonGenerator : public core::ICodeGenerator {
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
        explicit ComparisonGenerator(core::GeneratorManager* manager)
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
            return op == ">" || op == "<" || op == ">=" || op == "<=" || op == "==" || op == "!=";
        }

        auto generate(const Exp& ast_node, core::CompilationContext& context) -> llvm::Value* override {
            if (ast_node.list.size() != 3) {
                throw std::runtime_error("Comparison operation requires exactly two operands");
            }

            const auto& op = ast_node.list[0].string;

            llvm::Value* left = m_GENERATOR_MANAGER->generate_code(ast_node.list[1], context);
            llvm::Value* right = m_GENERATOR_MANAGER->generate_code(ast_node.list[2], context);

            bool left_int = is_integer_type(left);
            bool right_int = is_integer_type(right);

            llvm::Value* comparison_result = nullptr;

            if (left_int && right_int) {
                if (op == ">") {
                    comparison_result = context.m_BUILDER.CreateICmpSGT(left, right);
                } else if (op == "<") {
                    comparison_result = context.m_BUILDER.CreateICmpSLT(left, right);
                } else if (op == ">=") {
                    comparison_result = context.m_BUILDER.CreateICmpSGE(left, right);
                } else if (op == "<=") {
                    comparison_result = context.m_BUILDER.CreateICmpSLE(left, right);
                } else if (op == "==") {
                    comparison_result = context.m_BUILDER.CreateICmpEQ(left, right);
                } else if (op == "!=") {
                    comparison_result = context.m_BUILDER.CreateICmpNE(left, right);
                }
            } else {
                llvm::Value* left_fp = promote_to_double(left, context);
                llvm::Value* right_fp = promote_to_double(right, context);

                if (op == ">") {
                    comparison_result = context.m_BUILDER.CreateFCmpOGT(left_fp, right_fp);
                } else if (op == "<") {
                    comparison_result = context.m_BUILDER.CreateFCmpOLT(left_fp, right_fp);
                } else if (op == ">=") {
                    comparison_result = context.m_BUILDER.CreateFCmpOGE(left_fp, right_fp);
                } else if (op == "<=") {
                    comparison_result = context.m_BUILDER.CreateFCmpOLE(left_fp, right_fp);
                } else if (op == "==") {
                    comparison_result = context.m_BUILDER.CreateFCmpOEQ(left_fp, right_fp);
                } else if (op == "!=") {
                    comparison_result = context.m_BUILDER.CreateFCmpONE(left_fp, right_fp);
                }
            }

            if (!comparison_result) {
                throw std::runtime_error("Unknown comparison operator: " + op);
            }

            return context.m_BUILDER.CreateZExt(comparison_result, context.m_BUILDER.getInt64Ty());
        }

        auto get_priority() const -> int override { return 400; }
    };

}    // namespace galluz::generators
