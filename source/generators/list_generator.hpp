#pragma once

#include "../core/generator_manager.hpp"
#include "../core/types.hpp"

namespace galluz::generators {

    class ListGenerator : public core::ICodeGenerator {
        core::GeneratorManager* m_GENERATOR_MANAGER;

      public:
        explicit ListGenerator(core::GeneratorManager* manager)
            : m_GENERATOR_MANAGER(manager) {}

        auto can_handle(const Exp& ast_node) const -> bool override { return ast_node.type == ExpType::LIST; }

        auto generate(const Exp& ast_node, core::CompilationContext& context) -> llvm::Value* override {
            if (ast_node.list.empty()) {
                return context.m_BUILDER.getInt64(0);
            }

            const auto& first = ast_node.list[0];
            if (first.type == ExpType::SYMBOL) {
                return context.m_BUILDER.getInt64(0);
            }

            return context.m_BUILDER.getInt64(0);
        }

        auto get_priority() const -> int override { return 10; }
    };

}    // namespace galluz::generators
