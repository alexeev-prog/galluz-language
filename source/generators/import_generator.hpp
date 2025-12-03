#pragma once

#include <filesystem>

#include "../core/generator_manager.hpp"
#include "../core/module_manager.hpp"
#include "../core/types.hpp"
#include "../logger.hpp"

namespace galluz::generators {

    class ImportGenerator : public core::ICodeGenerator {
      private:
        core::GeneratorManager* m_GENERATOR_MANAGER;
        core::ModuleManager* m_MODULE_MANAGER;

      public:
        explicit ImportGenerator(core::GeneratorManager* manager, core::ModuleManager* module_manager)
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
            return (first.type == ExpType::SYMBOL && first.string == "import");
        }

        auto generate(const Exp& ast_node, core::CompilationContext& context) -> llvm::Value* override {
            if (ast_node.list.size() < 2) {
                LOG_CRITICAL("import requires at least a file path");
            }

            const auto& file_path_exp = ast_node.list[1];
            if (file_path_exp.type != ExpType::STRING) {
                LOG_CRITICAL("File path must be a string");
            }

            std::string file_path = file_path_exp.string;

            std::vector<std::string> modules_to_import;

            for (size_t i = 2; i < ast_node.list.size(); ++i) {
                const auto& module_exp = ast_node.list[i];
                if (module_exp.type != ExpType::LIST || module_exp.list.empty()) {
                    LOG_CRITICAL("Invalid module specification");
                }

                const auto& module_keyword = module_exp.list[0];
                if (module_keyword.type != ExpType::SYMBOL || module_keyword.string != "module") {
                    LOG_CRITICAL("Module specification must start with 'module'");
                }

                if (module_exp.list.size() < 2) {
                    LOG_CRITICAL("Module name missing");
                }

                const auto& module_name_exp = module_exp.list[1];
                if (module_name_exp.type != ExpType::SYMBOL) {
                    LOG_CRITICAL("Module name must be a symbol");
                }

                std::string module_name = module_name_exp.string;
                modules_to_import.push_back(module_name);
            }

            try {
                m_MODULE_MANAGER->import_modules(file_path, modules_to_import, context, m_GENERATOR_MANAGER);
            } catch (const std::exception& e) {
                LOG_CRITICAL("Import failed: %s", e.what());
            }

            return context.m_BUILDER.getInt32(0);
        }

        auto get_priority() const -> int override { return 950; }
    };

}    // namespace galluz::generators
