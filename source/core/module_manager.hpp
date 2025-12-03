#pragma once

#include <filesystem>
#include <fstream>
#include <memory>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "generator_manager.hpp"
#include "preprocessor.hpp"
#include "types.hpp"

namespace galluz::core {

    struct ModuleInfo {
        std::string name;
        std::string file_path;
        std::unordered_map<std::string, FunctionInfo> functions;
        std::unordered_map<std::string, VariableInfo> variables;
        std::unordered_set<std::string> exported_symbols;
        bool is_used = false;
        bool is_loaded = false;
        std::string content;
    };

    class ModuleManager {
      private:
        std::unordered_map<std::string, std::shared_ptr<ModuleInfo>> modules;
        std::unordered_map<std::string, std::string> symbol_to_module;
        std::unordered_set<std::string> loaded_files;
        std::unordered_map<std::string, std::unordered_set<std::string>> file_dependencies;
        TypeSystem* type_system;
        std::string current_directory;

        auto find_matching_parenthesis(const std::string& str, size_t start) -> size_t {
            int depth = 0;
            for (size_t i = start; i < str.length(); i++) {
                if (str[i] == '(') {
                    depth++;
                } else if (str[i] == ')') {
                    depth--;
                    if (depth == 0) {
                        return i;
                    }
                }
            }
            return std::string::npos;
        }

        auto extract_module_name(const std::string& expr) -> std::string {
            size_t name_start = expr.find_first_not_of(' ', 10);
            if (name_start == std::string::npos) {
                return "";
            }

            size_t name_end = expr.find_first_of(" \t\n\r)", name_start);
            if (name_end == std::string::npos) {
                return "";
            }

            return expr.substr(name_start, name_end - name_start);
        }

        auto resolve_file_path(const std::string& file_path) -> std::string {
            std::filesystem::path path(file_path);

            if (path.is_absolute()) {
                return file_path;
            }

            std::filesystem::path base_path(current_directory);
            std::filesystem::path resolved_path = base_path / path;

            if (std::filesystem::exists(resolved_path)) {
                return resolved_path.string();
            }

            if (!path.has_extension() || path.extension() != ".glz") {
                std::filesystem::path with_glz = resolved_path;
                with_glz += ".glz";
                if (std::filesystem::exists(with_glz)) {
                    return with_glz.string();
                }
            }

            return resolved_path.string();
        }

        auto check_circular_helper(const std::string& current_file,
                                   const std::string& target_file,
                                   std::unordered_set<std::string>& visited) -> bool {
            if (current_file == target_file) {
                return true;
            }
            if (visited.count(current_file)) {
                return false;
            }

            visited.insert(current_file);

            if (!file_dependencies.count(current_file)) {
                return false;
            }

            for (const auto& dep : file_dependencies[current_file]) {
                if (check_circular_helper(dep, target_file, visited)) {
                    return true;
                }
            }

            return false;
        }

        auto extract_module_definitions(const std::string& content)
            -> std::unordered_map<std::string, std::string> {
            std::unordered_map<std::string, std::string> module_definitions;

            std::function<void(const std::string&, size_t)> find_and_extract_modules;
            find_and_extract_modules = [&](const std::string& str, size_t start_pos)
            {
                size_t pos = start_pos;
                while (pos < str.length()) {
                    if (str[pos] == '(') {
                        size_t end = find_matching_parenthesis(str, pos);
                        if (end == std::string::npos) {
                            break;
                        }

                        std::string expr = str.substr(pos, end - pos + 1);

                        if (expr.find("(defmodule") == 0) {
                            std::string module_name = extract_module_name(expr);
                            if (!module_name.empty()) {
                                module_definitions[module_name] = expr;
                            }
                        } else if (expr.find("(scope") == 0) {
                            find_and_extract_modules(expr, 6);
                        }

                        pos = end + 1;
                    } else {
                        pos++;
                    }
                }
            };

            find_and_extract_modules(content, 0);
            return module_definitions;
        }

      public:
        ModuleManager(TypeSystem* ts)
            : type_system(ts) {}

        auto set_current_directory(const std::string& dir) -> void {
            current_directory = dir;
            if (current_directory.empty()) {
                current_directory = ".";
            }
        }

        auto load_module_file(const std::string& file_path)
            -> std::unordered_map<std::string, std::shared_ptr<ModuleInfo>> {
            std::string resolved_path = resolve_file_path(file_path);

            if (loaded_files.count(resolved_path)) {
                std::unordered_map<std::string, std::shared_ptr<ModuleInfo>> existing_modules;
                for (const auto& [name, info] : modules) {
                    if (info->file_path == resolved_path) {
                        existing_modules[name] = info;
                    }
                }
                return existing_modules;
            }

            std::ifstream file(resolved_path);
            if (!file.is_open()) {
                throw std::runtime_error("Cannot open module file: " + resolved_path);
            }

            std::string content((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
            file.close();

            loaded_files.insert(resolved_path);
            file_dependencies[resolved_path] = {};

            Preprocessor preprocessor;
            std::string processed = preprocessor.preprocess(content);

            auto module_definitions = extract_module_definitions(processed);

            std::unordered_map<std::string, std::shared_ptr<ModuleInfo>> loaded_modules;

            for (const auto& [module_name, module_content] : module_definitions) {
                auto module_info = std::make_shared<ModuleInfo>();
                module_info->name = module_name;
                module_info->file_path = resolved_path;
                module_info->is_loaded = true;
                module_info->content = module_content;

                modules[module_name] = module_info;
                loaded_modules[module_name] = module_info;
            }

            return loaded_modules;
        }

        auto import_modules(const std::string& file_path,
                            const std::vector<std::string>& module_names,
                            CompilationContext& context,
                            GeneratorManager* generator_manager) -> void {
            auto loaded_modules = load_module_file(file_path);

            if (loaded_modules.empty()) {
                throw std::runtime_error("No modules found in file: " + file_path);
            }

            if (module_names.empty()) {
                for (const auto& [module_name, module_info] : loaded_modules) {
                    if (!module_info->is_loaded) {
                        throw std::runtime_error("Module not loaded: " + module_name);
                    }
                    register_module(module_name, context, generator_manager);
                }
            } else {
                for (const auto& requested_module : module_names) {
                    auto it = loaded_modules.find(requested_module);
                    if (it == loaded_modules.end()) {
                        throw std::runtime_error("Module not found: " + requested_module);
                    }
                    if (!it->second->is_loaded) {
                        throw std::runtime_error("Module not loaded: " + requested_module);
                    }
                    register_module(requested_module, context, generator_manager);
                }
            }
        }

        auto register_module(const std::string& module_name,
                             CompilationContext& context,
                             GeneratorManager* generator_manager) -> void {
            auto module_it = modules.find(module_name);
            if (module_it == modules.end()) {
                throw std::runtime_error("Module not found in registry: " + module_name);
            }

            auto& module = module_it->second;

            if (module->is_used) {
                return;
            }

            module->is_used = true;

            std::unique_ptr<syntax::GalluzGrammar> parser = std::make_unique<syntax::GalluzGrammar>();
            Exp module_ast = parser->parse(module->content);

            if (module_ast.type == ExpType::LIST && module_ast.list.size() >= 2) {
                const auto& name_exp = module_ast.list[1];
                if (name_exp.type == ExpType::SYMBOL && name_exp.string == module_name) {
                    for (size_t i = 2; i < module_ast.list.size(); ++i) {
                        const auto& item = module_ast.list[i];
                        if (item.type == ExpType::LIST && !item.list.empty()) {
                            const auto& first_item = item.list[0];
                            if (first_item.type == ExpType::SYMBOL) {
                                if (first_item.string == "defn") {
                                    if (item.list.size() >= 4) {
                                        const auto& name_exp = item.list[1];
                                        if (name_exp.type == ExpType::LIST && name_exp.list.size() == 2) {
                                            const auto& func_name_exp = name_exp.list[0];
                                            if (func_name_exp.type == ExpType::SYMBOL) {
                                                std::string func_name = func_name_exp.string;
                                                std::string full_func_name = module_name + "." + func_name;
                                                module->exported_symbols.insert(func_name);
                                                module->exported_symbols.insert(full_func_name);

                                                auto* func_info = context.find_function(func_name);
                                                if (func_info) {
                                                    context.add_function(full_func_name,
                                                                         func_info->function,
                                                                         func_info->return_type,
                                                                         func_info->parameters,
                                                                         func_info->is_external);
                                                }
                                            }
                                        }
                                    }
                                }
                            }
                        }
                        generator_manager->generate_code(item, context);
                    }
                }
            }

            for (const auto& symbol : module->exported_symbols) {
                if (symbol_to_module.count(symbol)) {
                    throw std::runtime_error("Symbol conflict: " + symbol + " already exported from module "
                                             + symbol_to_module[symbol]);
                }
                symbol_to_module[symbol] = module_name;
            }
        }

        auto use_module(const std::string& module_name, CompilationContext& context) -> void {
            auto module_it = modules.find(module_name);
            if (module_it == modules.end() || !module_it->second->is_loaded) {
                throw std::runtime_error("Module not found: " + module_name);
            }

            auto& module = module_it->second;

            for (const auto& symbol : module->exported_symbols) {
                auto* func_info = context.find_function(symbol);
                if (func_info) {
                    module->functions[symbol] = *func_info;
                }
            }
        }

        auto resolve_symbol(const std::string& symbol) -> std::pair<llvm::Value*, std::string> {
            if (symbol.find('.') != std::string::npos) {
                size_t dot_pos = symbol.find('.');
                std::string module_name = symbol.substr(0, dot_pos);
                std::string member_name = symbol.substr(dot_pos + 1);

                auto module_it = modules.find(module_name);
                if (module_it == modules.end() || !module_it->second->is_loaded) {
                    throw std::runtime_error("Module not found: " + module_name);
                }

                auto& module = module_it->second;

                auto func_it = module->functions.find(member_name);
                if (func_it != module->functions.end()) {
                    return {func_it->second.function, member_name};
                }

                auto var_it = module->variables.find(member_name);
                if (var_it != module->variables.end()) {
                    return {var_it->second.value, member_name};
                }

                throw std::runtime_error("Symbol not found in module: " + member_name);
            }

            return {nullptr, ""};
        }

        auto get_module(const std::string& name) -> std::shared_ptr<ModuleInfo> {
            auto it = modules.find(name);
            if (it != modules.end()) {
                return it->second;
            }
            return nullptr;
        }

        auto has_module(const std::string& name) -> bool {
            auto it = modules.find(name);
            return it != modules.end() && it->second->is_loaded;
        }

        auto check_circular_dependency(const std::string& file_path, const std::string& importing_file)
            -> bool {
            std::unordered_set<std::string> visited;
            return check_circular_helper(file_path, importing_file, visited);
        }
    };

}    // namespace galluz::core
