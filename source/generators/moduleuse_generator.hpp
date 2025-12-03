#pragma once

#include "../core/generator_manager.hpp"
#include "../core/module_manager.hpp"
#include "../core/types.hpp"
#include "../logger.hpp"

namespace galluz::generators {

    class ModuleUseGenerator : public core::ICodeGenerator {
      private:
        core::GeneratorManager* m_GENERATOR_MANAGER;
        core::ModuleManager* m_MODULE_MANAGER;

      public:
        explicit ModuleUseGenerator(core::GeneratorManager* manager, core::ModuleManager* module_manager)
            : m_GENERATOR_MANAGER(manager)
            , m_MODULE_MANAGER(module_manager) {}

        auto can_handle(const Exp& ast_node) const -> bool override {
            if (ast_node.type != ExpType::LIST) {
                return false;
            }
            if (ast_node.list.empty()) {
                return false;
            }

            const auto& first = ast_node.list[0];
            return (first.type == ExpType::SYMBOL && first.string == "moduleuse");
        }

        auto generate(const Exp& ast_node, core::CompilationContext& context) -> llvm::Value* override {
            if (ast_node.list.size() != 2) {
                LOG_CRITICAL("moduleuse requires exactly one argument: (moduleuse module.name)");
            }

            const auto& module_name_exp = ast_node.list[1];
            if (module_name_exp.type != ExpType::SYMBOL) {
                LOG_CRITICAL("Module name must be a symbol");
            }

            std::string module_name = module_name_exp.string;

            try {
                m_MODULE_MANAGER->use_module(module_name, context);
            } catch (const std::exception& e) {
                LOG_CRITICAL("Module use failed: %s", e.what());
            }

            return context.m_BUILDER.getInt32(0);
        }

        auto get_priority() const -> int override { return 950; }
    };

}    // namespace galluz::generators
