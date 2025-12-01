#pragma once

#include "../core/types.hpp"

namespace galluz::generators {

    class NumberGenerator : public core::ICodeGenerator {
      public:
        auto can_handle(const Exp& ast_node) const -> bool override {
            return ast_node.type == ExpType::NUMBER;
        }

        auto generate(const Exp& ast_node, core::CompilationContext& context) -> llvm::Value* override {
            return context.m_BUILDER.getInt64(ast_node.number);
        }

        auto get_priority() const -> int override { return 1000; }
    };

}    // namespace galluz::generators
