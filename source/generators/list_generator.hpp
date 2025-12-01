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
                return context.m_BUILDER.getInt64(0);
            }

            llvm::Value* last_result = context.m_BUILDER.getInt64(0);

            for (const auto& item : ast_node.list) {
                last_result = m_GENERATOR_MANAGER->generate_code(item, context);
            }

            return last_result;
        }

        auto get_priority() const -> int override { return 100; }
    };

}    // namespace galluz::generators
