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
#include <unordered_map>
#include <unordered_set>
#include <fstream>

using namespace clang;
using namespace clang::tooling;
using namespace llvm;

// Command line options
static cl::OptionCategory CheckNamesCategory("Check Names options");
static cl::opt<std::string> DictionaryPath("dict", cl::desc("Path to dictionary file"), cl::cat(CheckNamesCategory));

// Dictionary for typo detection
class Dictionary {
public:
    Dictionary() = default;

    void loadFromFile(const std::string& path) {
        std::ifstream file(path);
        if (!file.is_open()) return;
        
        std::string word;
        while (file >> word) {
            // Convert to lowercase for case-insensitive comparison
            std::transform(word.begin(), word.end(), word.begin(), 
                          [](unsigned char c){ return std::tolower(c); });
            words.insert(word);
        }
    }

    bool contains(const std::string& word) const {
        std::string lowerWord = word;
        std::transform(lowerWord.begin(), lowerWord.end(), lowerWord.begin(), 
                      [](unsigned char c){ return std::tolower(c); });
        return words.find(lowerWord) != words.end();
    }

    // Find closest word in the dictionary using Levenshtein distance
    std::string findClosestWord(const std::string& word, int maxDistance = 2) const {
        std::string lowerWord = word;
        std::transform(lowerWord.begin(), lowerWord.end(), lowerWord.begin(), 
                      [](unsigned char c){ return std::tolower(c); });
        
        int minDistance = maxDistance + 1;
        std::string closestWord;
        
        for (const auto& dictWord : words) {
            int distance = levenshteinDistance(lowerWord, dictWord);
            if (distance < minDistance && distance > 0) {
                minDistance = distance;
                closestWord = dictWord;
            }
        }
        
        return closestWord;
    }

private:
    std::unordered_set<std::string> words;

    // Levenshtein distance algorithm
    int levenshteinDistance(const std::string& s1, const std::string& s2) const {
        // Early exit if the string lengths differ significantly
        if (std::abs(static_cast<int>(s1.length() - s2.length())) > 2) {
            return 3; // Beyond our threshold
        }

        const size_t m = s1.size();
        const size_t n = s2.size();
        
        // Create a matrix with (m+1) rows and (n+1) columns
        std::vector<std::vector<int>> dp(m + 1, std::vector<int>(n + 1, 0));
        
        // Initialize the first row and column
        for (size_t i = 0; i <= m; i++) dp[i][0] = i;
        for (size_t j = 0; j <= n; j++) dp[0][j] = j;
        
        // Fill the matrix
        for (size_t i = 1; i <= m; i++) {
            for (size_t j = 1; j <= n; j++) {
                int cost = (s1[i - 1] == s2[j - 1]) ? 0 : 1;
                dp[i][j] = std::min({
                    dp[i - 1][j] + 1,        // deletion
                    dp[i][j - 1] + 1,        // insertion
                    dp[i - 1][j - 1] + cost  // substitution
                });
            }
        }
        
        return dp[m][n];
    }
};

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

// Helper function to extract words from identifiers
static std::vector<std::string> extractWords(const std::string& name) {
    std::vector<std::string> words;
    std::string currentWord;
    
    for (char c : name) {
        if (c == '_' || c == 'k') {
            if (!currentWord.empty()) {
                words.push_back(currentWord);
                currentWord.clear();
            }
            continue;
        }
        
        if (std::isupper(c) && !currentWord.empty() && std::islower(currentWord.back())) {
            words.push_back(currentWord);
            currentWord.clear();
        }
        
        currentWord += c;
    }
    
    if (!currentWord.empty()) {
        words.push_back(currentWord);
    }
    
    return words;
}

// The AST visitor class
class NameChecker : public RecursiveASTVisitor<NameChecker> {
public:
    explicit NameChecker(ASTContext *Context, Statistics &Stats, const Dictionary &Dict)
        : Context(Context), Stats(Stats), SM(Context->getSourceManager()), Dict(Dict) {}

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
        
        // Check for potential typos
        checkTypos(Name, Loc);
    }
    
    // Check for typos in identifier names
    void checkTypos(const std::string &Name, SourceLocation Loc) {
        // If no dictionary was loaded or no dictionary file was provided, skip typo check
        if (DictionaryPath.empty()) {
            return;
        }
        
        std::string FileName = SM.getFilename(Loc).str();
        if (FileName.empty())
            return;
        size_t LastSlash = FileName.find_last_of("/\\");
        if (LastSlash != std::string::npos)
            FileName = FileName.substr(LastSlash + 1);
        unsigned Line = SM.getSpellingLineNumber(Loc);
        
        // Break the identifier into words
        auto words = extractWords(Name);
        
        for (const auto& word : words) {
            // Skip very short words (likely not typos or not meaningful)
            if (word.size() <= 2)
                continue;
                
            // Skip if word is all uppercase (likely an acronym)
            bool allUpper = true;
            for (char c : word) {
                if (!std::isupper(c)) {
                    allUpper = false;
                    break;
                }
            }
            if (allUpper)
                continue;
                
            // Convert to lowercase for dictionary check
            std::string lowerWord = word;
            std::transform(lowerWord.begin(), lowerWord.end(), lowerWord.begin(), 
                          [](unsigned char c){ return std::tolower(c); });
                
            // Skip if word is in dictionary
            if (Dict.contains(lowerWord))
                continue;
                
            // Find closest match in dictionary with max distance of 2
            std::string suggestion = Dict.findClosestWord(lowerWord, 2);
            if (!suggestion.empty()) {
                Stats.mistakes.push_back({FileName, Name, lowerWord, suggestion, Line});
            }
        }
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
                            addBadName(Name, Entity::kField, Loc);
                    } else {
                        // For structs/unions, use public naming rules and report as kVariable
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
            
        // Use point of declaration for location, not point of definition
        SourceLocation Loc = Declaration->getLocation();
        if (Loc.isInvalid() || SM.isInSystemHeader(Loc))
            return true;
            
        // Ensure we use the actual declaration location, not the definition
        if (Declaration->isThisDeclarationADefinition() && Declaration->getPointOfInstantiation().isValid()) {
            // For templated functions, use the point of declaration
            const FunctionDecl *Template = Declaration->getTemplateInstantiationPattern();
            if (Template)
                Loc = Template->getLocation();
        }
        
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
    const Dictionary &Dict;
};

class NameConsumer : public ASTConsumer {
public:
    explicit NameConsumer(ASTContext *Context, Statistics &Stats, const Dictionary &Dict)
        : Visitor(Context, Stats, Dict) { }
    void HandleTranslationUnit(ASTContext &Context) override {
        Visitor.TraverseDecl(Context.getTranslationUnitDecl());
    }
private:
    NameChecker Visitor;
};

class NameAction : public ASTFrontendAction {
public:
    NameAction(std::unordered_map<std::string, Statistics> &StatsMap)
        : StatsMap(StatsMap) {
        if (!DictionaryPath.empty()) {
            Dict.loadFromFile(DictionaryPath);
        }
    }
    std::unique_ptr<ASTConsumer> CreateASTConsumer(CompilerInstance &Compiler,
                                                   StringRef File) override {
        std::string FileName = File.str();
        size_t LastSlash = FileName.find_last_of("/\\");
        if (LastSlash != std::string::npos)
            FileName = FileName.substr(LastSlash + 1);
        Statistics &Stats = StatsMap[FileName];
        return std::make_unique<NameConsumer>(&Compiler.getASTContext(), Stats, Dict);
    }
private:
    std::unordered_map<std::string, Statistics> &StatsMap;
    Dictionary Dict;
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
