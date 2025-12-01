#pragma once

#include <vector>

#include "../core/generator_manager.hpp"
#include "../core/types.hpp"

namespace galluz::generators {

    class PrintGenerator : public core::ICodeGenerator {
        core::GeneratorManager* m_GENERATOR_MANAGER;

      public:
        explicit PrintGenerator(core::GeneratorManager* manager)
            : m_GENERATOR_MANAGER(manager) {}

        auto can_handle(const Exp& ast_node) const -> bool override {
            if (ast_node.type != ExpType::LIST) {
                return false;
            }
            if (ast_node.list.empty()) {
                return false;
            }

            const auto& first = ast_node.list[0];
            return (first.type == ExpType::SYMBOL && first.string == "fprint");
        }

        auto generate(const Exp& ast_node, core::CompilationContext& context) -> llvm::Value* override {
            auto* printf_func = context.m_MODULE.getFunction("printf");
            if (printf_func == nullptr) {
                throw std::runtime_error("printf function not found");
            }

            std::vector<llvm::Value*> args;

            if (ast_node.list.size() < 2) {
                throw std::runtime_error("fprint requires at least a format string");
            }

            for (size_t i = 1; i < ast_node.list.size(); ++i) {
                llvm::Value* arg_value = m_GENERATOR_MANAGER->generate_code(ast_node.list[i], context);
                args.push_back(arg_value);
            }

            return context.m_BUILDER.CreateCall(printf_func, args);
        }

        auto get_priority() const -> int override { return 70; }
    };

}    // namespace galluz::generators
