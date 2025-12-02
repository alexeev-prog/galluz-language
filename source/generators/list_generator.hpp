#pragma once

#include "../core/generator_manager.hpp"
#include "../core/types.hpp"

namespace galluz::generators {

    class ListGenerator : public core::ICodeGenerator {
      private:
        core::GeneratorManager* m_GENERATOR_MANAGER;

      public:
        explicit ListGenerator(core::GeneratorManager* manager)
            : m_GENERATOR_MANAGER(manager) {}

        auto can_handle(const Exp& ast_node) const -> bool override { return ast_node.type == ExpType::LIST; }

        auto generate(const Exp& ast_node, core::CompilationContext& context) -> llvm::Value* override {
            if (ast_node.list.empty()) {
                return context.m_BUILDER.getInt32(0);
            }

            const auto& first = ast_node.list[0];

            if (ast_node.list.size() == 2 && first.type == ExpType::SYMBOL && first.string == "-") {
                llvm::Value* operand = m_GENERATOR_MANAGER->generate_code(ast_node.list[1], context);

                if (operand->getType()->isIntegerTy()) {
                    return context.m_BUILDER.CreateNeg(operand);
                } else if (operand->getType()->isFloatingPointTy()) {
                    llvm::Value* zero = llvm::ConstantFP::get(operand->getType(), 0.0);
                    return context.m_BUILDER.CreateFSub(zero, operand);
                }
            }

            llvm::Value* last_result = context.m_BUILDER.getInt32(0);

            for (const auto& item : ast_node.list) {
                last_result = m_GENERATOR_MANAGER->generate_code(item, context);
            }

            return last_result;
        }

        auto get_priority() const -> int override { return 100; }
    };

}    // namespace galluz::generators
