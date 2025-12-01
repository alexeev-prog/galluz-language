#pragma once

#include <memory>
#include <string>

#include <llvm/IR/Verifier.h>

#include "generator_factory.hpp"
#include "generator_manager.hpp"
#include "preprocessor.hpp"
#include "types.hpp"

namespace galluz {

    class Compiler {
      private:
        std::unique_ptr<llvm::LLVMContext> m_CTX;
        std::unique_ptr<llvm::Module> m_MODULE;
        std::unique_ptr<llvm::IRBuilder<>> m_BUILDER;
        std::unique_ptr<syntax::GalluzGrammar> m_PARSER;
        core::GeneratorManager m_GENERATOR_MANAGER;
        std::unique_ptr<core::CompilationContext> m_COMPILATION_CONTEXT;
        core::Preprocessor m_PREPROCESSOR;
        std::unique_ptr<core::TypeSystem> m_TYPE_SYSTEM;

      public:
        Compiler()
            : m_PARSER(std::make_unique<syntax::GalluzGrammar>()) {
            initialize_llvm();
            setup_external_functions();
            core::GeneratorFactory::register_default_generators(m_GENERATOR_MANAGER);
        }

        auto execute(const std::string& program, const std::string& output_base) -> int {
            std::string processed_program = m_PREPROCESSOR.preprocess(program);

            auto ast = m_PARSER->parse(processed_program);

            generate_ir(ast);

            llvm::verifyModule(*m_MODULE, &llvm::errs());

            save_module_to_file(output_base + ".ll");

            return 0;
        }

      private:
        void initialize_llvm() {
            m_CTX = std::make_unique<llvm::LLVMContext>();
            m_MODULE = std::make_unique<llvm::Module>("GalluzLangCompilationUnit", *m_CTX);
            m_BUILDER = std::make_unique<llvm::IRBuilder<>>(*m_CTX);

            m_TYPE_SYSTEM = std::make_unique<core::TypeSystem>(*m_CTX);

            m_TYPE_SYSTEM->register_type("int", core::TypeKind::INT, m_BUILDER->getInt64Ty());
            m_TYPE_SYSTEM->register_type("double", core::TypeKind::DOUBLE, m_BUILDER->getDoubleTy());
            m_TYPE_SYSTEM->register_type(
                "str", core::TypeKind::STRING, m_BUILDER->getInt8Ty()->getPointerTo());
            m_TYPE_SYSTEM->register_type("bool", core::TypeKind::BOOL, m_BUILDER->getInt1Ty());
            m_TYPE_SYSTEM->register_type("void", core::TypeKind::VOID, m_BUILDER->getVoidTy());
            m_TYPE_SYSTEM->register_type("auto", core::TypeKind::UNKNOWN, nullptr);

            m_COMPILATION_CONTEXT = std::make_unique<core::CompilationContext>(
                *m_CTX, *m_MODULE, *m_BUILDER, nullptr, m_TYPE_SYSTEM.get());
        }

        void setup_external_functions() {
            auto* byte_ptr_ty = m_BUILDER->getInt8Ty()->getPointerTo();
            m_MODULE->getOrInsertFunction(
                "printf", llvm::FunctionType::get(m_BUILDER->getInt64Ty(), byte_ptr_ty, true));
        }

        void generate_ir(const Exp& ast) {
            auto* main_type = llvm::FunctionType::get(m_BUILDER->getInt64Ty(), {}, false);

            auto* main_func = create_function("main", main_type);
            m_COMPILATION_CONTEXT->m_CURRENT_FUNCTION = main_func;

            create_global_variable("_GALLUZ_LLVM_VERSION", m_BUILDER->getInt64(19), false);

            llvm::Value* result = m_GENERATOR_MANAGER.generate_code(ast, *m_COMPILATION_CONTEXT);

            m_BUILDER->CreateRet(m_BUILDER->getInt64(0));
        }

        auto create_function(const std::string& name, llvm::FunctionType* type) -> llvm::Function* {
            if (auto* existing = m_MODULE->getFunction(name)) {
                return existing;
            }

            auto* func = llvm::Function::Create(type, llvm::Function::ExternalLinkage, name, m_MODULE.get());

            llvm::verifyFunction(*func);
            setup_function_body(func);
            return func;
        }

        void setup_function_body(llvm::Function* func) {
            auto* entry_block = llvm::BasicBlock::Create(*m_CTX, "entry", func);
            m_BUILDER->SetInsertPoint(entry_block);
        }

        auto create_global_variable(const std::string& name,
                                    llvm::Constant* init_value,
                                    bool is_mutable = false) -> llvm::GlobalVariable* {
            m_MODULE->getOrInsertGlobal(name, init_value->getType());
            auto* variable = m_MODULE->getNamedGlobal(name);

            variable->setAlignment(llvm::MaybeAlign(4));
            variable->setConstant(!is_mutable);
            variable->setInitializer(init_value);

            m_COMPILATION_CONTEXT->globals[name] = variable;

            return variable;
        }

        void save_module_to_file(const std::string& filename) {
            std::error_code err_code;
            llvm::raw_fd_ostream out_file(filename, err_code);
            m_MODULE->print(out_file, nullptr);
        }
    };

}    // namespace galluz
