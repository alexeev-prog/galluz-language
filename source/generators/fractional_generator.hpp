#pragma once

#include <llvm/IR/Constants.h>

#include "../core/types.hpp"

namespace galluz::generators {

    class FractionalGenerator : public core::ICodeGenerator {
      public:
        auto can_handle(const Exp& ast_node) const -> bool override {
            return ast_node.type == ExpType::FRACTIONAL;
        }

        auto generate(const Exp& ast_node, core::CompilationContext& context) -> llvm::Value* override {
            return llvm::ConstantFP::get(context.m_BUILDER.getDoubleTy(), ast_node.fractional);
        }

        auto get_priority() const -> int override { return 1000; }
    };

}    // namespace galluz::generators
