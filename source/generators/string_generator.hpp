#pragma once

#include <regex>

#include "../core/types.hpp"

namespace galluz::generators {

    class StringGenerator : public core::ICodeGenerator {
        auto replace_escapes(const std::string& str) -> std::string {
            auto regex_new_line = std::regex(R"(\\n)");
            auto regex_tab = std::regex(R"(\\t)");

            auto newlined = std::regex_replace(str, regex_new_line, "\n");
            return std::regex_replace(newlined, regex_tab, "\t");
        }

      public:
        auto can_handle(const Exp& ast_node) const -> bool override {
            return ast_node.type == ExpType::STRING;
        }

        auto generate(const Exp& ast_node, core::CompilationContext& context) -> llvm::Value* override {
            auto processed_string = replace_escapes(ast_node.string);
            return context.m_BUILDER.CreateGlobalStringPtr(processed_string);
        }

        auto get_priority() const -> int override { return 100; }
    };

}    // namespace galluz::generators
