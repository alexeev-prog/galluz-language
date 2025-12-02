#pragma once

#include "../core/generator_manager.hpp"
#include "../core/types.hpp"

namespace galluz::generators {

    class StructAllocGenerator : public core::ICodeGenerator {
      private:
        core::GeneratorManager* m_GENERATOR_MANAGER;

      public:
        explicit StructAllocGenerator(core::GeneratorManager* manager)
            : m_GENERATOR_MANAGER(manager) {}

        auto can_handle(const Exp& ast_node) const -> bool override {
            if (ast_node.type != ExpType::LIST) {
                return false;
            }
            if (ast_node.list.empty()) {
                return false;
            }

            const auto& first = ast_node.list[0];
            return (first.type == ExpType::SYMBOL && first.string == "struct-alloc");
        }

        auto generate(const Exp& ast_node, core::CompilationContext& context) -> llvm::Value* override {
            if (ast_node.list.size() != 2) {
                LOG_CRITICAL("struct-alloc requires exactly 1 argument: (struct-alloc StructName)");
            }

            const auto& struct_name_exp = ast_node.list[1];
            if (struct_name_exp.type != ExpType::SYMBOL) {
                LOG_CRITICAL("Struct name must be a symbol");
            }

            std::string struct_name = struct_name_exp.string;
            auto* type_info = context.type_system->get_type(struct_name);

            if (!type_info || type_info->kind != core::TypeKind::STRUCT) {
                LOG_CRITICAL("Unknown struct type: %s", struct_name);
            }

            auto* alloca =
                context.m_BUILDER.CreateAlloca(type_info->llvm_type, nullptr, struct_name + "_inst");

            auto* zero_init = llvm::ConstantAggregateZero::get(type_info->llvm_type);
            context.m_BUILDER.CreateStore(zero_init, alloca);

            return alloca;
        }

        auto get_priority() const -> int override { return 850; }
    };

}    // namespace galluz::generators
