#pragma once

#include <memory>
#include <string>
#include <unordered_map>

#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/Value.h>

#include "../parser/GalluzGrammar.h"

namespace galluz::core {

    struct CompilationContext {
        llvm::LLVMContext& m_CTX;
        llvm::Module& m_MODULE;
        llvm::IRBuilder<>& m_BUILDER;
        llvm::Function* m_CURRENT_FUNCTION;
        std::unordered_map<std::string, llvm::GlobalVariable*> globals;

        CompilationContext(llvm::LLVMContext& ctx,
                           llvm::Module& module,
                           llvm::IRBuilder<>& builder,
                           llvm::Function* current_function)
            : m_CTX(ctx)
            , m_MODULE(module)
            , m_BUILDER(builder)
            , m_CURRENT_FUNCTION(current_function) {}
    };

    class ICodeGenerator {
      public:
        virtual ~ICodeGenerator() = default;

        virtual auto can_handle(const Exp& ast_node) const -> bool = 0;
        virtual auto generate(const Exp& ast_node, CompilationContext& context) -> llvm::Value* = 0;
        virtual auto get_priority() const -> int = 0;
    };

}    // namespace galluz::core
