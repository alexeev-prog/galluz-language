#pragma once

#include "../core/generator_manager.hpp"
#include "../core/types.hpp"

namespace galluz::generators {

    class ScopeGenerator : public core::ICodeGenerator {
      private:
        core::GeneratorManager* m_GENERATOR_MANAGER;

      public:
        explicit ScopeGenerator(core::GeneratorManager* manager)
            : m_GENERATOR_MANAGER(manager) {}

        auto can_handle(const Exp& ast_node) const -> bool override {
            if (ast_node.type != ExpType::LIST) {
                return false;
            }
            if (ast_node.list.empty()) {
                return false;
            }

            const auto& first = ast_node.list[0];
            return (first.type == ExpType::SYMBOL && first.string == "scope");
        }

        auto generate(const Exp& ast_node, core::CompilationContext& context) -> llvm::Value* override {
            context.push_scope();

            llvm::Value* last_result = context.m_BUILDER.getInt32(0);

            for (size_t i = 1; i < ast_node.list.size(); ++i) {
                last_result = m_GENERATOR_MANAGER->generate_code(ast_node.list[i], context);
            }

            context.pop_scope();

            return last_result;
        }

        auto get_priority() const -> int override { return 600; }
    };

}    // namespace galluz::generators
