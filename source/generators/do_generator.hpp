#pragma once

#include "../core/generator_manager.hpp"
#include "../core/types.hpp"

namespace galluz::generators {

    class DoGenerator : public core::ICodeGenerator {
      private:
        core::GeneratorManager* m_GENERATOR_MANAGER;

      public:
        explicit DoGenerator(core::GeneratorManager* manager)
            : m_GENERATOR_MANAGER(manager) {}

        auto can_handle(const Exp& ast_node) const -> bool override {
            if (ast_node.type != ExpType::LIST) {
                return false;
            }
            if (ast_node.list.empty()) {
                return false;
            }

            const auto& first = ast_node.list[0];
            return (first.type == ExpType::SYMBOL && first.string == "do");
        }

        auto generate(const Exp& ast_node, core::CompilationContext& context) -> llvm::Value* override {
            if (ast_node.list.size() < 2) {
                return context.m_BUILDER.getInt64(0);
            }

            context.push_scope();

            llvm::Value* last_result = nullptr;
            for (size_t i = 1; i < ast_node.list.size(); ++i) {
                last_result = m_GENERATOR_MANAGER->generate_code(ast_node.list[i], context);
            }

            context.pop_scope();

            if (last_result) {
                return last_result;
            }

            return context.m_BUILDER.getInt64(0);
        }

        auto get_priority() const -> int override { return 100; }
    };

}    // namespace galluz::generators
