#pragma once

#include <llvm/IR/GlobalVariable.h>

#include "../core/generator_manager.hpp"
#include "../core/types.hpp"

namespace galluz::generators {

    class VariableGenerator : public core::ICodeGenerator {
      private:
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
            return (first.type == ExpType::SYMBOL && (first.string == "var" || first.string == "global"));
        }

        auto generate(const Exp& ast_node, core::CompilationContext& context) -> llvm::Value* override {
            if (ast_node.list.size() < 3) {
                throw std::runtime_error("Invalid variable declaration");
            }

            const auto& first = ast_node.list[0];
            const auto& name_exp = ast_node.list[1];
            const auto& value_exp = ast_node.list[2];

            if (name_exp.type != ExpType::SYMBOL) {
                throw std::runtime_error("Variable name must be a symbol");
            }

            std::string var_name = name_exp.string;
            bool is_global = (first.string == "global");

            llvm::Value* init_value = m_GENERATOR_MANAGER->generate_code(value_exp, context);
            llvm::Type* value_type = init_value->getType();

            if (is_global) {
                llvm::Constant* const_init = llvm::dyn_cast<llvm::Constant>(init_value);
                if (!const_init) {
                    throw std::runtime_error("Global variable initializer must be constant");
                }

                context.m_MODULE.getOrInsertGlobal(var_name, value_type);
                auto* variable = context.m_MODULE.getNamedGlobal(var_name);

                variable->setAlignment(llvm::MaybeAlign(8));
                variable->setConstant(false);
                variable->setInitializer(const_init);

                context.add_variable(var_name, variable, value_type, true);

                return init_value;
            } else {
                auto* alloca = context.m_BUILDER.CreateAlloca(value_type, nullptr, var_name);
                context.m_BUILDER.CreateStore(init_value, alloca);

                context.add_variable(var_name, alloca, value_type, false);

                return init_value;
            }
        }

        auto get_priority() const -> int override { return 800; }
    };

}    // namespace galluz::generators
