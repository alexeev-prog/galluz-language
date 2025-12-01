#pragma once

#include <memory>

#include "../core/preprocessor.hpp"
#include "../core/types.hpp"

namespace galluz::generators {

    class StringGenerator : public core::ICodeGenerator {
      private:
        std::unique_ptr<core::Preprocessor> m_PREPROCESSOR;

      public:
        StringGenerator() { m_PREPROCESSOR = std::make_unique<core::Preprocessor>(); }

        auto can_handle(const Exp& ast_node) const -> bool override {
            return ast_node.type == ExpType::STRING;
        }

        auto generate(const Exp& ast_node, core::CompilationContext& context) -> llvm::Value* override {
            std::string processed_str = m_PREPROCESSOR->postprocess_string(ast_node.string);
            return context.m_BUILDER.CreateGlobalStringPtr(processed_str);
        }

        auto get_priority() const -> int override { return 1000; }
    };

}    // namespace galluz::generators
