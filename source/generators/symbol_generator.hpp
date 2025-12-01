#pragma once

#include "../core/types.hpp"

namespace galluz::generators {

    class SymbolGenerator : public core::ICodeGenerator {
      public:
        auto can_handle(const Exp& ast_node) const -> bool override {
            return ast_node.type == ExpType::SYMBOL;
        }

        auto generate(const Exp& ast_node, core::CompilationContext& context) -> llvm::Value* override {
            const std::string& symbol = ast_node.string;

            if (symbol == "true" || symbol == "false") {
                return context.m_BUILDER.getInt64(static_cast<uint64_t>(symbol == "true"));
            }

            auto* global_variable = context.m_MODULE.getNamedGlobal(symbol);
            if (global_variable == nullptr) {
                throw std::runtime_error("Undefined symbol: " + symbol);
            }

            return global_variable->getInitializer();
        }

        auto get_priority() const -> int override { return 90; }
    };

}    // namespace galluz::generators
