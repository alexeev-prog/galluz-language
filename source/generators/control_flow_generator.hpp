#pragma once

#include "../core/generator_manager.hpp"
#include "../core/types.hpp"

namespace galluz::generators {

    class ControlFlowGenerator : public core::ICodeGenerator {
      private:
        core::GeneratorManager* m_GENERATOR_MANAGER;

      public:
        explicit ControlFlowGenerator(core::GeneratorManager* manager)
            : m_GENERATOR_MANAGER(manager) {}

        auto can_handle(const Exp& ast_node) const -> bool override {
            if (ast_node.type != ExpType::LIST) {
                return false;
            }
            if (ast_node.list.empty()) {
                return false;
            }

            const auto& first = ast_node.list[0];
            if (first.type != ExpType::SYMBOL) {
                return false;
            }

            const std::string& keyword = first.string;
            return keyword == "if" || keyword == "while" || keyword == "break" || keyword == "continue";
        }

        auto generate(const Exp& ast_node, core::CompilationContext& context) -> llvm::Value* override {
            const auto& keyword = ast_node.list[0].string;

            if (keyword == "if") {
                return generate_if(ast_node, context);
            } else if (keyword == "while") {
                return generate_while(ast_node, context);
            } else if (keyword == "break") {
                return generate_break(context);
            } else if (keyword == "continue") {
                return generate_continue(context);
            }

            return nullptr;
        }

      private:
        auto generate_if(const Exp& ast_node, core::CompilationContext& context) -> llvm::Value* {
            if (ast_node.list.size() < 3) {
                LOG_CRITICAL("if statement requires condition and then-branch");
            }

            llvm::Function* current_func = context.m_CURRENT_FUNCTION;

            llvm::Value* cond_value = m_GENERATOR_MANAGER->generate_code(ast_node.list[1], context);

            if (!cond_value->getType()->isIntegerTy(1)) {
                cond_value = context.m_BUILDER.CreateICmpNE(cond_value,
                                                            llvm::ConstantInt::get(cond_value->getType(), 0));
            }

            llvm::BasicBlock* then_block = llvm::BasicBlock::Create(context.m_CTX, "if.then", current_func);
            llvm::BasicBlock* else_block = nullptr;
            llvm::BasicBlock* merge_block = llvm::BasicBlock::Create(context.m_CTX, "if.end", current_func);

            bool has_else = (ast_node.list.size() >= 4);

            if (has_else) {
                else_block = llvm::BasicBlock::Create(context.m_CTX, "if.else", current_func);
                context.m_BUILDER.CreateCondBr(cond_value, then_block, else_block);
            } else {
                context.m_BUILDER.CreateCondBr(cond_value, then_block, merge_block);
            }

            context.m_BUILDER.SetInsertPoint(then_block);
            context.push_scope();
            llvm::Value* then_result = m_GENERATOR_MANAGER->generate_code(ast_node.list[2], context);
            context.pop_scope();

            if (!context.m_BUILDER.GetInsertBlock()->getTerminator()) {
                context.m_BUILDER.CreateBr(merge_block);
            }

            llvm::BasicBlock* then_end = context.m_BUILDER.GetInsertBlock();

            llvm::Value* else_result = nullptr;
            llvm::BasicBlock* else_end = nullptr;

            if (has_else) {
                context.m_BUILDER.SetInsertPoint(else_block);
                context.push_scope();
                else_result = m_GENERATOR_MANAGER->generate_code(ast_node.list[3], context);
                context.pop_scope();

                if (!context.m_BUILDER.GetInsertBlock()->getTerminator()) {
                    context.m_BUILDER.CreateBr(merge_block);
                }

                else_end = context.m_BUILDER.GetInsertBlock();
            }

            context.m_BUILDER.SetInsertPoint(merge_block);

            if (then_result) {
                llvm::Type* result_type = then_result->getType();
                llvm::PHINode* phi = context.m_BUILDER.CreatePHI(result_type, has_else ? 2 : 2, "if.result");

                phi->addIncoming(then_result, then_end);

                if (has_else && else_result) {
                    phi->addIncoming(else_result, else_end);
                } else if (has_else) {
                    phi->addIncoming(llvm::Constant::getNullValue(result_type), else_end);
                } else {
                    phi->addIncoming(llvm::Constant::getNullValue(result_type), then_end);
                }

                return phi;
            }

            return context.m_BUILDER.getInt32(0);
        }

        auto generate_while(const Exp& ast_node, core::CompilationContext& context) -> llvm::Value* {
            if (ast_node.list.size() != 3) {
                LOG_CRITICAL("while statement requires condition and body");
            }

            llvm::Function* current_func = context.m_CURRENT_FUNCTION;

            llvm::BasicBlock* cond_block =
                llvm::BasicBlock::Create(context.m_CTX, "while.cond", current_func);
            llvm::BasicBlock* body_block =
                llvm::BasicBlock::Create(context.m_CTX, "while.body", current_func);
            llvm::BasicBlock* exit_block = llvm::BasicBlock::Create(context.m_CTX, "while.end", current_func);

            context.m_BUILDER.CreateBr(cond_block);

            context.m_BUILDER.SetInsertPoint(cond_block);
            llvm::Value* cond_value = m_GENERATOR_MANAGER->generate_code(ast_node.list[1], context);

            if (!cond_value->getType()->isIntegerTy(1)) {
                cond_value = context.m_BUILDER.CreateICmpNE(cond_value,
                                                            llvm::ConstantInt::get(cond_value->getType(), 0));
            }

            context.m_BUILDER.CreateCondBr(cond_value, body_block, exit_block);

            context.m_BUILDER.SetInsertPoint(body_block);

            core::LoopContext loop_ctx = {cond_block, body_block, cond_block, exit_block};
            context.push_loop(loop_ctx);
            context.push_scope();

            m_GENERATOR_MANAGER->generate_code(ast_node.list[2], context);

            context.pop_scope();
            context.pop_loop();

            if (!context.m_BUILDER.GetInsertBlock()->getTerminator()) {
                context.m_BUILDER.CreateBr(cond_block);
            }

            context.m_BUILDER.SetInsertPoint(exit_block);

            return context.m_BUILDER.getInt32(0);
        }

        auto generate_break(core::CompilationContext& context) -> llvm::Value* {
            auto* loop = context.get_current_loop();
            if (!loop) {
                LOG_CRITICAL("break statement outside loop");
            }

            context.m_BUILDER.CreateBr(loop->exit_block);

            return context.m_BUILDER.getInt32(0);
        }

        auto generate_continue(core::CompilationContext& context) -> llvm::Value* {
            auto* loop = context.get_current_loop();
            if (!loop) {
                LOG_CRITICAL("continue statement outside loop");
            }

            context.m_BUILDER.CreateBr(loop->continue_block);

            return context.m_BUILDER.getInt32(0);
        }

        auto get_priority() const -> int override { return 150; }
    };

}    // namespace galluz::generators
