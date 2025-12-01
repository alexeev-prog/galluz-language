#pragma once

#include "../core/generator_manager.hpp"
#include "../core/types.hpp"

namespace galluz::generators {

    class VariableGenerator : public core::ICodeGenerator {
        core::GeneratorManager* m_GENERATOR_MANAGER;

      public:
        explicit VariableGenerator(core::GeneratorManager* manager)
            : m_GENERATOR_MANAGER(manager) {}

        auto can_handle(const Exp& ast_node) const -> bool override {
            if (ast_node.type != ExpType::LIST) {
                return false;
            }
            if (ast_node.list.empty()) {
                return false;
            }

            const auto& first = ast_node.list[0];
            return (first.type == ExpType::SYMBOL && first.string == "var");
        }

        auto generate(const Exp& ast_node, core::CompilationContext& context) -> llvm::Value* override {
            // Проверка структуры: (var name value)
            if (ast_node.list.size() != 3) {
                throw std::runtime_error("Invalid var syntax");
            }

            const auto& name_exp = ast_node.list[1];
            const auto& value_exp = ast_node.list[2];

            if (name_exp.type != ExpType::SYMBOL) {
                throw std::runtime_error("Variable name must be a symbol");
            }

            llvm::Value* init_value = m_GENERATOR_MANAGER->generate_code(value_exp, context);

            llvm::Constant* const_init = nullptr;
            if (auto* constant = llvm::dyn_cast<llvm::Constant>(init_value)) {
                const_init = constant;
            } else {
                throw std::runtime_error("Global variable initializer must be constant");
            }

            context.m_MODULE.getOrInsertGlobal(name_exp.string, const_init->getType());
            auto* variable = context.m_MODULE.getNamedGlobal(name_exp.string);

            variable->setAlignment(llvm::MaybeAlign(4));
            variable->setConstant(false);
            variable->setInitializer(const_init);

            context.globals[name_exp.string] = variable;

            return variable->getInitializer();
        }

        auto get_priority() const -> int override { return 80; }
    };

}    // namespace galluz::generators
