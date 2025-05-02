#include "../check_names.h"
#include <clang/AST/ASTConsumer.h>
#include <clang/AST/RecursiveASTVisitor.h>
#include <clang/Frontend/CompilerInstance.h>
#include <clang/Frontend/FrontendAction.h>
#include <clang/Tooling/CommonOptionsParser.h>
#include <clang/Tooling/Tooling.h>
#include <clang/Basic/SourceManager.h>
#include <llvm/Support/CommandLine.h>
#include <fstream>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <iostream>

using namespace clang;
using namespace clang::tooling;
using namespace llvm;

static cl::OptionCategory CheckNamesCategory("check-names options");
static cl::opt<std::string> DictionaryPath("dict", cl::desc("Path to dictionary file"),
                                           cl::cat(CheckNamesCategory));

// Helper functions for style checking
bool IsValidVariableName(const std::string& name) {
    if (name.empty() || name[0] == '_') {
        return false;
    }
    
    // Check for snake_case
    for (size_t i = 0; i < name.size(); ++i) {
        if (!std::islower(name[i]) && name[i] != '_') {
            return false;
        }
        
        // Check for consecutive underscores
        if (name[i] == '_' && i > 0 && name[i-1] == '_') {
            return false;
        }
    }
    
    // Check for trailing underscore
    if (name.back() == '_') {
        return false;
    }
    
    // Check for digits
    for (char c : name) {
        if (std::isdigit(c)) {
            return false;
        }
    }
    
    return true;
}

bool IsValidConstName(const std::string& name) {
    // Should be kConstName
    if (name.size() < 2 || name[0] != 'k' || !std::isupper(name[1])) {
        return false;
    }
    
    // No underscores allowed
    if (name.find('_') != std::string::npos) {
        return false;
    }
    
    // Check for digits
    for (char c : name) {
        if (std::isdigit(c)) {
            return false;
        }
    }
    
    return true;
}

bool IsValidTypeName(const std::string& name) {
    // Should be UpperCamelCase
    if (name.empty() || !std::isupper(name[0])) {
        return false;
    }
    
    // No underscores allowed
    if (name.find('_') != std::string::npos) {
        return false;
    }
    
    // Check for all caps
    bool allCaps = true;
    for (char c : name) {
        if (std::islower(c)) {
            allCaps = false;
            break;
        }
    }
    if (allCaps) {
        return false;
    }
    
    // Check for capital letter sequences
    for (size_t i = 0; i < name.size(); ++i) {
        if (std::isupper(name[i])) {
            size_t start = i;
            while (i + 1 < name.size() && std::isupper(name[i + 1])) {
                i++;
            }
            size_t length = i - start + 1;
            if (length >= 2 && length < 3 && (i + 1 == name.size() || !std::isupper(name[i + 1]))) {
                return false;
            }
        }
    }
    
    // Check for digits
    for (char c : name) {
        if (std::isdigit(c)) {
            return false;
        }
    }
    
    return true;
}

bool IsValidFunctionName(const std::string& name) {
    // Should be lowerCamelCase or UpperCamelCase
    if (name.empty()) {
        return false;
    }
    
    // No underscores allowed
    if (name.find('_') != std::string::npos) {
        return false;
    }
    
    // Check for capital letter sequences
    for (size_t i = 0; i < name.size(); ++i) {
        if (std::isupper(name[i])) {
            size_t start = i;
            while (i + 1 < name.size() && std::isupper(name[i + 1])) {
                i++;
            }
            size_t length = i - start + 1;
            if (length >= 2 && length < 3 && (i + 1 == name.size() || !std::isupper(name[i + 1]))) {
                return false;
            }
        }
    }
    
    // Check for all caps
    bool allCaps = true;
    for (char c : name) {
        if (std::islower(c)) {
            allCaps = false;
            break;
        }
    }
    if (allCaps) {
        return false;
    }
    
    // Check for digits
    for (char c : name) {
        if (std::isdigit(c)) {
            return false;
        }
    }
    
    return true;
}

class NameChecker : public RecursiveASTVisitor<NameChecker> {
public:
    NameChecker(SourceManager& SM, std::vector<BadName>& badNames, const std::string& filename)
        : SM(SM), badNames(badNames), filename(filename) {}
    
    bool VisitVarDecl(VarDecl* D) {
        if (SM.isInSystemHeader(D->getLocation())) {
            return true;
        }
        
        if (!SM.isInMainFile(D->getLocation()) && !SM.isWrittenInMainFile(D->getLocation())) {
            return true;
        }
        
        std::string name = D->getNameAsString();
        if (name.empty()) {
            return true;
        }
        
        Entity entity;
        bool isValid = false;
        
        if (D->isConstexpr() || D->getType().isConstQualified()) {
            entity = Entity::kConst;
            isValid = IsValidConstName(name);
        } else if (D->isStaticDataMember()) {
            entity = Entity::kField;
            isValid = IsValidVariableName(name);
        } else {
            entity = Entity::kVariable;
            isValid = IsValidVariableName(name);
        }
        
        if (!isValid) {
            unsigned line = SM.getSpellingLineNumber(D->getLocation());
            badNames.push_back({filename, name, entity, line});
        }
        
        return true;
    }
    
    bool VisitFieldDecl(FieldDecl* D) {
        if (SM.isInSystemHeader(D->getLocation())) {
            return true;
        }
        
        if (!SM.isInMainFile(D->getLocation()) && !SM.isWrittenInMainFile(D->getLocation())) {
            return true;
        }
        
        std::string name = D->getNameAsString();
        if (name.empty()) {
            return true;
        }
        
        Entity entity;
        bool isValid = false;
        
        if (D->getType().isConstQualified()) {
            entity = Entity::kConst;
            isValid = IsValidConstName(name);
        } else {
            entity = Entity::kField;
            isValid = IsValidVariableName(name);
        }
        
        if (!isValid) {
            unsigned line = SM.getSpellingLineNumber(D->getLocation());
            badNames.push_back({filename, name, entity, line});
        }
        
        return true;
    }
    
    bool VisitFunctionDecl(FunctionDecl* D) {
        if (SM.isInSystemHeader(D->getLocation())) {
            return true;
        }
        
        if (!SM.isInMainFile(D->getLocation()) && !SM.isWrittenInMainFile(D->getLocation())) {
            return true;
        }
        
        std::string name = D->getNameAsString();
        if (name.empty()) {
            return true;
        }
        
        if (!IsValidFunctionName(name)) {
            unsigned line = SM.getSpellingLineNumber(D->getLocation());
            badNames.push_back({filename, name, Entity::kFunction, line});
        }
        
        return true;
    }
    
    bool VisitCXXRecordDecl(CXXRecordDecl* D) {
        if (SM.isInSystemHeader(D->getLocation())) {
            return true;
        }
        
        if (!SM.isInMainFile(D->getLocation()) && !SM.isWrittenInMainFile(D->getLocation())) {
            return true;
        }
        
        if (D->isAnonymousStructOrUnion()) {
            return true;
        }
        
        std::string name = D->getNameAsString();
        if (name.empty()) {
            return true;
        }
        
        if (!IsValidTypeName(name)) {
            unsigned line = SM.getSpellingLineNumber(D->getLocation());
            badNames.push_back({filename, name, Entity::kType, line});
        }
        
        return true;
    }
    
    bool VisitEnumDecl(EnumDecl* D) {
        if (SM.isInSystemHeader(D->getLocation())) {
            return true;
        }
        
        if (!SM.isInMainFile(D->getLocation()) && !SM.isWrittenInMainFile(D->getLocation())) {
            return true;
        }
        
        std::string name = D->getNameAsString();
        if (name.empty()) {
            return true;
        }
        
        if (!IsValidTypeName(name)) {
            unsigned line = SM.getSpellingLineNumber(D->getLocation());
            badNames.push_back({filename, name, Entity::kType, line});
        }
        
        return true;
    }
    
    bool VisitTypedefNameDecl(TypedefNameDecl* D) {
        if (SM.isInSystemHeader(D->getLocation())) {
            return true;
        }
        
        if (!SM.isInMainFile(D->getLocation()) && !SM.isWrittenInMainFile(D->getLocation())) {
            return true;
        }
        
        std::string name = D->getNameAsString();
        if (name.empty()) {
            return true;
        }
        
        if (!IsValidTypeName(name)) {
            unsigned line = SM.getSpellingLineNumber(D->getLocation());
            badNames.push_back({filename, name, Entity::kType, line});
        }
        
        return true;
    }
    
private:
    SourceManager& SM;
    std::vector<BadName>& badNames;
    std::string filename;
};

class StyleConsumer : public ASTConsumer {
public:
    StyleConsumer(SourceManager& SM, std::vector<BadName>& badNames, const std::string& filename)
        : visitor(SM, badNames, filename) {}
    
    void HandleTranslationUnit(ASTContext& Context) override {
        visitor.TraverseDecl(Context.getTranslationUnitDecl());
    }
    
private:
    NameChecker visitor;
};

class StyleAction : public ASTFrontendAction {
public:
    StyleAction(std::unordered_map<std::string, Statistics>& results)
        : results(results) {}
    
    std::unique_ptr<ASTConsumer> CreateASTConsumer(CompilerInstance& CI, StringRef file) override {
        std::string filename = file.str();
        // Extract just the filename without the path
        size_t pos = filename.find_last_of("/\\");
        if (pos != std::string::npos) {
            filename = filename.substr(pos + 1);
        }
        
        return std::make_unique<StyleConsumer>(
            CI.getSourceManager(), results[filename].bad_names, filename);
    }
    
private:
    std::unordered_map<std::string, Statistics>& results;
};

class StyleActionFactory : public FrontendActionFactory {
public:
    StyleActionFactory(std::unordered_map<std::string, Statistics>& results)
        : results(results) {}
    
    std::unique_ptr<FrontendAction> create() override {
        return std::make_unique<StyleAction>(results);
    }
    
private:
    std::unordered_map<std::string, Statistics>& results;
};

std::unordered_map<std::string, Statistics> CheckNames(int argc, const char* argv[]) {
    auto ExpectedParser = CommonOptionsParser::create(argc, argv, CheckNamesCategory);
    if (!ExpectedParser) {
        llvm::errs() << ExpectedParser.takeError();
        return {};
    }
    
    CommonOptionsParser& OptionsParser = ExpectedParser.get();
    ClangTool Tool(OptionsParser.getCompilations(), OptionsParser.getSourcePathList());
    
    std::unordered_map<std::string, Statistics> results;
    
    // Set up results for each file
    for (const auto& file : OptionsParser.getSourcePathList()) {
        std::string filename = file;
        size_t pos = filename.find_last_of("/\\");
        if (pos != std::string::npos) {
            filename = filename.substr(pos + 1);
        }
        results[filename] = Statistics{};
    }
    
    auto Factory = std::make_unique<StyleActionFactory>(results);
    Tool.run(Factory.get());
    
    // Uncomment this line for debugging
    // PrintCheckNamesResults(results);
    
    return results;
}
