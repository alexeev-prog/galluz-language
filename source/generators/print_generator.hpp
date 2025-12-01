#pragma once

#include <memory>

#include "../core/generator_manager.hpp"
#include "../core/preprocessor.hpp"
#include "../core/types.hpp"

namespace galluz::generators {

    class PrintGenerator : public core::ICodeGenerator {
      private:
        core::GeneratorManager* m_GENERATOR_MANAGER;
        std::unique_ptr<core::Preprocessor> m_PREPROCESSOR;

        auto ensure_printf_function(core::CompilationContext& context) -> llvm::Function* {
            auto* existing_func = context.m_MODULE.getFunction("printf");
            if (existing_func) {
                return existing_func;
            }

            auto* byte_ptr_ty = llvm::PointerType::get(context.m_CTX, 0);
            return llvm::Function::Create(
                llvm::FunctionType::get(context.m_BUILDER.getInt32Ty(), byte_ptr_ty, true),
                llvm::Function::ExternalLinkage,
                "printf",
                &context.m_MODULE);
        }

      public:
        explicit PrintGenerator(core::GeneratorManager* manager)
            : m_GENERATOR_MANAGER(manager) {
            m_PREPROCESSOR = std::make_unique<core::Preprocessor>();
        }

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
            auto* printf_func = ensure_printf_function(context);

            if (ast_node.list.size() < 2) {
                throw std::runtime_error("fprint requires at least a format string");
            }

            const auto& format_exp = ast_node.list[1];

            if (format_exp.type != ExpType::STRING) {
                throw std::runtime_error("First argument to fprint must be a format string");
            }

            std::string format_str = m_PREPROCESSOR->postprocess_string(format_exp.string);
            std::vector<llvm::Value*> printf_args;
            printf_args.push_back(context.m_BUILDER.CreateGlobalStringPtr(format_str));

            for (size_t i = 2; i < ast_node.list.size(); ++i) {
                llvm::Value* arg = m_GENERATOR_MANAGER->generate_code(ast_node.list[i], context);
                printf_args.push_back(arg);
            }

            llvm::Value* call_result = context.m_BUILDER.CreateCall(printf_func, printf_args);

            return context.m_BUILDER.CreateIntCast(call_result, context.m_BUILDER.getInt64Ty(), true);
        }

        auto get_priority() const -> int override { return 300; }
    };

}    // namespace galluz::generators
