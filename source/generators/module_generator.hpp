#pragma once

#include "../core/generator_manager.hpp"
#include "../core/module_manager.hpp"
#include "../core/types.hpp"
#include "../logger.hpp"

namespace galluz::generators {

    class ModuleGenerator : public core::ICodeGenerator {
      private:
        core::GeneratorManager* m_GENERATOR_MANAGER;
        core::ModuleManager* m_MODULE_MANAGER;

      public:
        explicit ModuleGenerator(core::GeneratorManager* manager, core::ModuleManager* module_manager)
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
            return (first.type == ExpType::SYMBOL && first.string == "defmodule");
        }

        auto generate(const Exp& ast_node, core::CompilationContext& context) -> llvm::Value* override {
            if (ast_node.list.size() < 2) {
                LOG_CRITICAL("Invalid module definition: (defmodule name ...)");
            }

            const auto& name_exp = ast_node.list[1];
            if (name_exp.type != ExpType::SYMBOL) {
                LOG_CRITICAL("Module name must be a symbol");
            }

            std::string module_name = name_exp.string;

            for (size_t i = 2; i < ast_node.list.size(); ++i) {
                const auto& item = ast_node.list[i];

                if (item.type == ExpType::LIST && !item.list.empty()) {
                    const auto& first_item = item.list[0];
                    if (first_item.type == ExpType::SYMBOL) {
                        if (first_item.string == "defn") {
                            parse_function_in_module(item, module_name, context);
                        }
                    }
                }
            }

            llvm::Value* result = m_GENERATOR_MANAGER->generate_code(ast_node, context);

            return result;
        }

      private:
        auto parse_function_in_module(const Exp& func_exp,
                                      const std::string& module_name,
                                      core::CompilationContext& context) -> void {
            if (func_exp.list.size() < 4) {
                return;
            }

            const auto& name_exp = func_exp.list[1];
            if (name_exp.type != ExpType::LIST || name_exp.list.size() != 2) {
                return;
            }

            const auto& func_name_exp = name_exp.list[0];
            if (func_name_exp.type != ExpType::SYMBOL) {
                return;
            }

            std::string func_name = func_name_exp.string;

            auto module_info = m_MODULE_MANAGER->get_module(module_name);
            if (module_info) {
                module_info->exported_symbols.insert(func_name);
            }
        }

        auto get_priority() const -> int override { return 960; }
    };

}    // namespace galluz::generators
