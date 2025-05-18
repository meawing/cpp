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

// Helper functions for name checks

static bool containsDigits(const std::string &Name) {
    return std::any_of(Name.begin(), Name.end(), [](char c) {
        return std::isdigit(c);
    });
}

static bool isValidVariableName(const std::string &Name) {
    if (Name.empty() || Name == "_" || Name[0] == '_')
        return false;
    // Variables and local variables should be snake_case:
    // lowercase letters/digits/underscores, not ending with an underscore.
    std::regex pattern("^[a-z][a-z0-9_]*$");
    return std::regex_match(Name, pattern) &&
           Name.back() != '_' &&
           Name.find("__") == std::string::npos &&
           !containsDigits(Name);
}

static bool isValidNonPublicFieldName(const std::string &Name) {
    // Non‑public fields (in a class) must have a trailing underscore.
    std::regex pattern("^[a-z][a-z0-9_]*_$");
    return std::regex_match(Name, pattern) &&
           Name.find("__") == std::string::npos &&
           !containsDigits(Name);
}

static bool isValidPublicFieldName(const std::string &Name) {
    // Public fields (or declared in a struct/union) obey the same rules as variables.
    return isValidVariableName(Name);
}

static bool isValidTypeName(const std::string &Name) {
    if (Name.empty())
        return false;
    // Must start with an uppercase letter.
    if (!std::isupper(Name[0]))
        return false;
    // Names for types (and CamelCase free functions) must not contain underscores.
    if (Name.find('_') != std::string::npos)
        return false;
    // Must not be completely uppercase.
    bool allUpper = true;
    for (char c : Name) {
        if (std::islower(c)) { allUpper = false; break; }
    }
    if (allUpper)
        return false;
    // For every contiguous block of uppercase letters,
    // if its length is greater than 1 and less than 3, the name is rejected.
    size_t i = 0;
    while (i < Name.length()) {
        if (std::isupper(Name[i])) {
            size_t j = i + 1;
            while (j < Name.length() && std::isupper(Name[j]))
                j++;
            size_t seq = j - i;
            if (seq > 1 && seq < 3)
                return false;
            i = j;
        } else {
            i++;
        }
    }
    return !containsDigits(Name);
}

static bool isValidConstName(const std::string &Name) {
    if (Name.empty())
        return false;
    // Constants/constexpr variables: must follow kConstName style—
    // start with a lowercase "k" followed by CamelCase (no underscores).
    std::regex pattern("^k[A-Z][a-zA-Z0-9]*$");
    return std::regex_match(Name, pattern) &&
           Name.back() != '_' &&
           !containsDigits(Name);
}

// For free functions starting with lowercase: snake_case.
static bool isValidSnakeCaseFunctionName(const std::string &Name) {
    if (Name.empty() || !std::islower(Name[0]))
        return false;
    std::regex pattern("^[a-z][a-z0-9_]*$");
    return std::regex_match(Name, pattern) &&
           Name.find("__") == std::string::npos &&
           !containsDigits(Name);
}

// For free functions starting with uppercase: CamelCase (similar to type names).
static bool isValidCamelCaseFunctionName(const std::string &Name) {
    if (Name.size() < 2)
        return false;
    return isValidTypeName(Name);
}

// For non‑static member functions (methods), require CamelCase
// with at least two letters and no underscores.
static bool isValidMethodName(const std::string &Name) {
    if (Name.size() < 2)
        return false;
    std::regex pattern("^[A-Z][a-zA-Z]+$");
    return std::regex_match(Name, pattern) &&
           !containsDigits(Name);
}

// The AST visitor class
class NameChecker : public RecursiveASTVisitor<NameChecker> {
public:
    explicit NameChecker(ASTContext *Context, Statistics &Stats)
        : Context(Context), Stats(Stats), SM(Context->getSourceManager()) {}

    // Report a violation with file, name, entity code, and line.
    void addBadName(const std::string &Name, Entity EntityType, SourceLocation Loc) {
        if (Loc.isInvalid() || SM.isInSystemHeader(Loc))
            return;
        std::string FileName = SM.getFilename(Loc).str();
        if (FileName.empty())
            return;
        size_t LastSlash = FileName.find_last_of("/\\");
        if (LastSlash != std::string::npos)
            FileName = FileName.substr(LastSlash + 1);
        unsigned Line = SM.getSpellingLineNumber(Loc);
        Stats.bad_names.push_back({FileName, Name, EntityType, Line});
    }

    // Visit variable declarations.
    bool VisitVarDecl(VarDecl *Declaration) {
        if (Declaration->isImplicit())
            return true;

        std::string Name = Declaration->getNameAsString();
        if (Name.empty())
            return true;

        SourceLocation Loc = Declaration->getLocation();
        if (Loc.isInvalid() || SM.isInSystemHeader(Loc))
            return true;

        // Handle macro expansions correctly
        if (SM.isMacroBodyExpansion(Loc) || SM.isMacroArgExpansion(Loc)) {
            Loc = SM.getExpansionLoc(Loc);  // Get the usage location
        } else {
            Loc = SM.getSpellingLoc(Loc);   // Regular case
        }

        // Skip if in system header after location adjustment
        if (SM.isInSystemHeader(Loc))
            return true;

        // Get the actual filename
        std::string FileName = SM.getFilename(Loc).str();
        if (FileName.empty())
            return true;

        // Explicit check for single-letter uppercase variables
        if (Name.size() == 1 && std::isupper(Name[0])) {
            addBadName(Name, Entity::kVariable, Loc);
            return true;
        }

        // Handle static data members using getDeclContext().
        if (Declaration->isStaticDataMember()) {
            if (Declaration->getType().isConstQualified()) {
                if (!isValidConstName(Name))
                    addBadName(Name, Entity::kConst, Loc);
            } else {
                if (auto *RD = dyn_cast<CXXRecordDecl>(Declaration->getDeclContext())) {
                    // For classes (declared with 'class'), members must follow non‑public field style.
                    if (RD->isClass()) {
                        if (!isValidNonPublicFieldName(Name))
                            addBadName(Name, Entity::kVariable, Loc);
                    } else {
                        // For structs/unions, use public naming rules.
                        if (!isValidPublicFieldName(Name))
                            addBadName(Name, Entity::kVariable, Loc);
                    }
                } else {
                    if (!isValidVariableName(Name))
                        addBadName(Name, Entity::kVariable, Loc);
                }
            }
            return true;
        }
        
        // For constexpr or const globals, use constant naming.
        if (Declaration->isConstexpr() ||
            (Declaration->getType().isConstQualified() && Declaration->hasGlobalStorage())) {
            if (!isValidConstName(Name))
                addBadName(Name, Entity::kConst, Loc);
        } else {
            if (!isValidVariableName(Name))
                addBadName(Name, Entity::kVariable, Loc);
        }
        return true;
    } 

    // Visit field declarations.
    bool VisitFieldDecl(FieldDecl *Declaration) {
        std::string Name = Declaration->getNameAsString();
        if (Name.empty())
            return true;
        SourceLocation Loc = Declaration->getLocation();
        if (Loc.isInvalid() || SM.isInSystemHeader(Loc))
            return true;
        
        if (Declaration->getType().isConstQualified()) {
            if (!isValidConstName(Name))
                addBadName(Name, Entity::kConst, Loc);
        } else {
            bool valid = false;
            // Use the DeclContext of the field.
            if (auto *RD = dyn_cast<CXXRecordDecl>(Declaration->getParent())) {
                // If declared in a C++ class (keyword "class"), use field style.
                if (RD->isClass()) {
                    valid = isValidNonPublicFieldName(Name);
                    if (!valid)
                        addBadName(Name, Entity::kField, Loc);
                    return true;
                }
                else {
                    // For a struct/union, use variable naming.
                    valid = isValidPublicFieldName(Name);
                }
            } else {
                valid = isValidVariableName(Name);
            }
            if (!valid)
                // Report as a variable (entity 0).
                addBadName(Name, Entity::kVariable, Loc);
        }
        return true;
    }

    // Visit type declarations.
    bool VisitTagDecl(TagDecl *Declaration) {
        std::string Name = Declaration->getNameAsString();
        if (Name.empty())
            return true;
        SourceLocation Loc = Declaration->getLocation();
        if (Loc.isInvalid() || SM.isInSystemHeader(Loc))
            return true;
        if (!isValidTypeName(Name))
            addBadName(Name, Entity::kType, Loc);
        return true;
    }

    bool VisitTypedefNameDecl(TypedefNameDecl *Declaration) {
        std::string Name = Declaration->getNameAsString();
        if (Name.empty())
            return true;
        SourceLocation Loc = Declaration->getLocation();
        if (Loc.isInvalid() || SM.isInSystemHeader(Loc))
            return true;
        if (!isValidTypeName(Name))
            addBadName(Name, Entity::kType, Loc);
        return true;
    }

    // Visit function declarations.
    bool VisitFunctionDecl(FunctionDecl *Declaration) {
        if (Declaration->isImplicit())
            return true;
        // Exclude constructors and destructors.
        if (isa<CXXConstructorDecl>(Declaration) || isa<CXXDestructorDecl>(Declaration))
            return true;
        std::string Name = Declaration->getNameAsString();
        if (Name.empty())
            return true;
        SourceLocation Loc = Declaration->getLocation();
        if (Loc.isInvalid() || SM.isInSystemHeader(Loc))
            return true;
        
        // For constexpr functions, enforce constant naming.
        if (Declaration->isConstexpr()) {
            if (!isValidConstName(Name))
                addBadName(Name, Entity::kFunction, Loc);
            return true;
        }
        
        // For non‑static member functions (methods)
        if (Declaration->isCXXClassMember() && !Declaration->isStatic()) {
            if (!isValidMethodName(Name))
                addBadName(Name, Entity::kFunction, Loc);
        }
        else {
            // For free functions and static member functions.
            if (std::islower(Name[0])) {
                if (!isValidSnakeCaseFunctionName(Name))
                    addBadName(Name, Entity::kFunction, Loc);
            }
            else {
                if (!isValidCamelCaseFunctionName(Name))
                    addBadName(Name, Entity::kFunction, Loc);
            }
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
        : Visitor(Context, Stats) { }
    void HandleTranslationUnit(ASTContext &Context) override {
        Visitor.TraverseDecl(Context.getTranslationUnitDecl());
    }
private:
    NameChecker Visitor;
};

class NameAction : public ASTFrontendAction {
public:
    NameAction(std::unordered_map<std::string, Statistics> &StatsMap)
        : StatsMap(StatsMap) { }
    std::unique_ptr<ASTConsumer> CreateASTConsumer(CompilerInstance &Compiler,
                                                   StringRef File) override {
        std::string FileName = File.str();
        size_t LastSlash = FileName.find_last_of("/\\");
        if (LastSlash != std::string::npos)
            FileName = FileName.substr(LastSlash + 1);
        Statistics &Stats = StatsMap[FileName];
        return std::make_unique<NameConsumer>(&Compiler.getASTContext(), Stats);
    }
private:
    std::unordered_map<std::string, Statistics> &StatsMap;
};

class NameActionFactory : public FrontendActionFactory {
public:
    NameActionFactory(std::unordered_map<std::string, Statistics> &StatsMap)
        : StatsMap(StatsMap) { }
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
