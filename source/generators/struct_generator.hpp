#pragma once

#include "../core/generator_manager.hpp"
#include "../core/types.hpp"

namespace galluz::generators {

    class StructGenerator : public core::ICodeGenerator {
      private:
        core::GeneratorManager* m_GENERATOR_MANAGER;

      public:
        explicit StructGenerator(core::GeneratorManager* manager)
            : m_GENERATOR_MANAGER(manager) {}

        auto can_handle(const Exp& ast_node) const -> bool override {
            if (ast_node.type != ExpType::LIST) {
                return false;
            }
            if (ast_node.list.empty()) {
                return false;
            }

            const auto& first = ast_node.list[0];
            return (first.type == ExpType::SYMBOL && first.string == "struct");
        }

        auto generate(const Exp& ast_node, core::CompilationContext& context) -> llvm::Value* override {
            if (ast_node.list.size() < 3) {
                LOG_CRITICAL("Invalid struct definition: (struct name ((field1 !type) (field2 !type) ...))");
            }

            const auto& name_exp = ast_node.list[1];
            const auto& fields_exp = ast_node.list[2];

            if (name_exp.type != ExpType::SYMBOL) {
                LOG_CRITICAL("Struct name must be a symbol");
            }

            std::string struct_name = name_exp.string;

            if (fields_exp.type != ExpType::LIST) {
                LOG_CRITICAL("Struct fields must be a list");
            }

            std::vector<std::pair<std::string, core::TypeInfo*>> fields;

            for (const auto& field_exp : fields_exp.list) {
                if (field_exp.type != ExpType::LIST || field_exp.list.size() != 2) {
                    LOG_CRITICAL("Field definition must be (name !type)");
                }

                const auto& field_name_exp = field_exp.list[0];
                const auto& field_type_exp = field_exp.list[1];

                if (field_name_exp.type != ExpType::SYMBOL) {
                    LOG_CRITICAL("Field name must be a symbol");
                }

                std::string field_name = field_name_exp.string;

                if (field_type_exp.type != ExpType::SYMBOL || field_type_exp.string.empty()
                    || field_type_exp.string[0] != '!')
                {
                    LOG_CRITICAL("Field type must start with !");
                }

                std::string type_str = field_type_exp.string.substr(1);
                auto* type_info = context.type_system->get_type(type_str);
                if (!type_info) {
                    LOG_CRITICAL("Unknown type: %s", type_str);
                }

                fields.emplace_back(field_name, type_info);
            }

            context.type_system->define_struct(struct_name, fields);

            return context.m_BUILDER.getInt64(0);
        }

        auto get_priority() const -> int override { return 950; }
    };

}    // namespace galluz::generators
