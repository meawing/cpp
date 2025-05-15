#include "../check_names.h"
#include <clang/AST/ASTConsumer.h>
#include <clang/AST/RecursiveASTVisitor.h>
#include <clang/Frontend/CompilerInstance.h>
#include <clang/Frontend/FrontendAction.h>
#include <clang/Tooling/CommonOptionsParser.h>
#include <clang/Tooling/Tooling.h>
#include <clang/Basic/SourceManager.h>
#include <llvm/Support/CommandLine.h>
#include <regex>
#include <cctype>
#include <string>
#include <algorithm>

using namespace clang;
using namespace clang::tooling;
using namespace llvm;

// Command line options
static cl::OptionCategory CheckNamesCategory("Check Names options");

class NameChecker : public RecursiveASTVisitor<NameChecker> {
public:
    explicit NameChecker(ASTContext *Context, Statistics &Stats)
        : Context(Context), Stats(Stats), SM(Context->getSourceManager()) {}

    // Check if a name is valid according to the style guide
    bool isValidVariableName(const std::string &Name) {
        if (Name.empty() || Name == "_" || Name[0] == '_') return false;
        
        // Variable names should be snake_case
        std::regex pattern("^[a-z][a-z0-9_]*$");
        return std::regex_match(Name, pattern) && 
               Name.back() != '_' && 
               Name.find("__") == std::string::npos &&
               !containsDigits(Name);
    }

    bool isValidFieldName(const std::string &Name) {
        if (Name.empty()) return false;
        
        // Field names should be snake_case ending with underscore
        std::regex pattern("^[a-z][a-z0-9_]*_$");
        return std::regex_match(Name, pattern) && 
               Name.find("__") == std::string::npos &&
               !containsDigits(Name);
    }

    bool isValidTypeName(const std::string &Name) {
        if (Name.empty()) return false;
        
        // Type names should start with capital letter and use CamelCase
        if (!std::isupper(Name[0])) return false;
        
        // Check for all uppercase (not allowed)
        bool allUpper = true;
        for (char c : Name) {
            if (std::islower(c)) {
                allUpper = false;
                break;
            }
        }
        if (allUpper) return false;
        
        // Check for uppercase letter sequences (must be at least 3 letters long)
        size_t i = 0;
        while (i < Name.length()) {
            if (std::isupper(Name[i])) {
                size_t j = i + 1;
                while (j < Name.length() && std::isupper(Name[j])) {
                    j++;
                }
                
                size_t sequence_length = j - i;
                if (sequence_length >= 2 && sequence_length < 3 && 
                    j < Name.length() && std::isupper(Name[j])) {
                    return false;
                }
                i = j;
            } else {
                i++;
            }
        }
        
        return !containsDigits(Name);
    }

    bool isValidConstName(const std::string &Name) {
        if (Name.empty()) return false;
        
        // Const names should start with k followed by CamelCase
        std::regex pattern("^k[A-Z][a-zA-Z0-9]*$");
        return std::regex_match(Name, pattern) && 
               Name.back() != '_' &&
               !containsDigits(Name);
    }

    bool isValidFunctionName(const std::string &Name) {
        if (Name.empty()) return false;
        
        // Function names should start with lowercase
        if (!std::islower(Name[0])) return false;
        
        // Check for snake_case or camelCase
        std::regex snake_pattern("^[a-z][a-z0-9_]*$");
        bool is_snake = std::regex_match(Name, snake_pattern) && 
                        Name.find("__") == std::string::npos;
        
        // For camelCase, check uppercase letter sequences
        if (!is_snake) {
            size_t i = 0;
            while (i < Name.length()) {
                if (std::isupper(Name[i])) {
                    size_t j = i + 1;
                    while (j < Name.length() && std::isupper(Name[j])) {
                        j++;
                    }
                    
                    size_t sequence_length = j - i;
                    if (sequence_length >= 2 && sequence_length < 3 && 
                        j < Name.length() && std::isupper(Name[j])) {
                        return false;
                    }
                    i = j;
                } else {
                    i++;
                }
            }
        }
        
        return !containsDigits(Name);
    }

    // Check if a name contains digits (not allowed)
    bool containsDigits(const std::string &Name) {
        return std::any_of(Name.begin(), Name.end(), [](char c) { return std::isdigit(c); });
    }

    // Add a bad name to the statistics
    void addBadName(const std::string &Name, Entity EntityType, SourceLocation Loc) {
        if (Loc.isInvalid() || SM.isInSystemHeader(Loc)) return;
        
        // Get the file and line information
        std::string FileName = SM.getFilename(Loc).str();
        if (FileName.empty()) return;
        
        // Get just the filename without the path
        size_t LastSlash = FileName.find_last_of("/\\");
        if (LastSlash != std::string::npos) {
            FileName = FileName.substr(LastSlash + 1);
        }
        
        unsigned Line = SM.getSpellingLineNumber(Loc);
        
        // Add to statistics
        Stats.bad_names.push_back({FileName, Name, EntityType, Line});
    }

    // Visit variable declarations
    bool VisitVarDecl(VarDecl *Declaration) {
        std::string Name = Declaration->getNameAsString();
        if (Name.empty()) return true;
        
        SourceLocation Loc = Declaration->getLocation();
        if (Loc.isInvalid() || SM.isInSystemHeader(Loc)) return true;
        
        if (Declaration->isConstexpr() || 
            (Declaration->getType().isConstQualified() && Declaration->hasGlobalStorage())) {
            // Const or constexpr variables should follow kConstName
            if (!isValidConstName(Name)) {
                addBadName(Name, Entity::kConst, Loc);
            }
        } else {
            // Regular variables
            if (!isValidVariableName(Name)) {
                addBadName(Name, Entity::kVariable, Loc);
            }
        }
        
        return true;
    }

    // Visit field declarations
    bool VisitFieldDecl(FieldDecl *Declaration) {
        std::string Name = Declaration->getNameAsString();
        if (Name.empty()) return true;
        
        SourceLocation Loc = Declaration->getLocation();
        if (Loc.isInvalid() || SM.isInSystemHeader(Loc)) return true;
        
        if (Declaration->getType().isConstQualified()) {
            // Const fields should follow kConstName
            if (!isValidConstName(Name)) {
                addBadName(Name, Entity::kConst, Loc);
            }
        } else {
            // Regular fields should end with underscore
            if (!isValidFieldName(Name)) {
                addBadName(Name, Entity::kField, Loc);
            }
        }
        
        return true;
    }

    // Visit type declarations (classes, structs, unions, enums)
    bool VisitTagDecl(TagDecl *Declaration) {
        std::string Name = Declaration->getNameAsString();
        if (Name.empty()) return true;
        
        SourceLocation Loc = Declaration->getLocation();
        if (Loc.isInvalid() || SM.isInSystemHeader(Loc)) return true;
        
        if (!isValidTypeName(Name)) {
            addBadName(Name, Entity::kType, Loc);
        }
        
        return true;
    }

    // Visit typedef declarations
    bool VisitTypedefNameDecl(TypedefNameDecl *Declaration) {
        std::string Name = Declaration->getNameAsString();
        if (Name.empty()) return true;
        
        SourceLocation Loc = Declaration->getLocation();
        if (Loc.isInvalid() || SM.isInSystemHeader(Loc)) return true;
        
        if (!isValidTypeName(Name)) {
            addBadName(Name, Entity::kType, Loc);
        }
        
        return true;
    }

    // Visit function declarations
    bool VisitFunctionDecl(FunctionDecl *Declaration) {
        std::string Name = Declaration->getNameAsString();
        if (Name.empty()) return true;
        
        SourceLocation Loc = Declaration->getLocation();
        if (Loc.isInvalid() || SM.isInSystemHeader(Loc)) return true;
        
        if (!isValidFunctionName(Name)) {
            addBadName(Name, Entity::kFunction, Loc);
        }
        
        return true;
    }

private:
    ASTContext *Context;
    Statistics &Stats;
    SourceManager &SM;
};

class NameConsumer : public ASTConsumer {
public:
    explicit NameConsumer(ASTContext *Context, Statistics &Stats)
        : Visitor(Context, Stats) {}

    void HandleTranslationUnit(ASTContext &Context) override {
        Visitor.TraverseDecl(Context.getTranslationUnitDecl());
    }

private:
    NameChecker Visitor;
};

class NameAction : public ASTFrontendAction {
public:
    NameAction(std::unordered_map<std::string, Statistics> &StatsMap)
        : StatsMap(StatsMap) {}

    std::unique_ptr<ASTConsumer> CreateASTConsumer(CompilerInstance &Compiler, StringRef File) override {
        // Get or create statistics for this file
        std::string FileName = File.str();
        size_t LastSlash = FileName.find_last_of("/\\");
        if (LastSlash != std::string::npos) {
            FileName = FileName.substr(LastSlash + 1);
        }
        
        Statistics &Stats = StatsMap[FileName];
        return std::make_unique<NameConsumer>(&Compiler.getASTContext(), Stats);
    }

private:
    std::unordered_map<std::string, Statistics> &StatsMap;
};

class NameActionFactory : public FrontendActionFactory {
public:
    NameActionFactory(std::unordered_map<std::string, Statistics> &StatsMap)
        : StatsMap(StatsMap) {}

    std::unique_ptr<FrontendAction> create() override {
        return std::make_unique<NameAction>(StatsMap);
    }

private:
    std::unordered_map<std::string, Statistics> &StatsMap;
};

std::unordered_map<std::string, Statistics> CheckNames(int argc, const char* argv[]) {
    auto ExpectedParser = CommonOptionsParser::create(argc, argv, CheckNamesCategory);
    if (!ExpectedParser) {
        llvm::errs() << ExpectedParser.takeError();
        return {};
    }
    
    CommonOptionsParser &OptionsParser = ExpectedParser.get();
    ClangTool Tool(OptionsParser.getCompilations(), OptionsParser.getSourcePathList());
    
    std::unordered_map<std::string, Statistics> StatsMap;
    NameActionFactory Factory(StatsMap);
    
    Tool.run(&Factory);
    
    return StatsMap;
}
