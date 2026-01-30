#ifndef TMLANGUAGE2VIMSYNTAX_H
#define TMLANGUAGE2VIMSYNTAX_H

#include <iostream>
#include <map>
#include <memory>
#include <set>
#include <string>
#include <vector>

extern "C" {
#include <oniguruma.h>
}

#include <nlohmann/json.hpp>

// Structure representing a TextMate grammar pattern
struct Pattern {
  std::string name;              // Name of the pattern
  std::string match;             // Regular expression for simple match
  std::string begin;             // Begin pattern for regions
  std::string end;               // End pattern for regions
  std::vector<Pattern> patterns; // Nested patterns
  std::map<std::string, std::string>
      captures; // Capture groups for match patterns
  std::map<std::string, std::string>
      beginCaptures; // Capture groups for begin pattern
  std::map<std::string, std::string>
      endCaptures;     // Capture groups for end pattern
  std::string include; // Include reference to other patterns
};

// Repository containing named pattern rules
struct Repository {
  std::map<std::string, Pattern> rules;
};

// Complete TextMate grammar definition
struct TextMateGrammar {
  std::string name;              // Language name
  std::string scopeName;         // Scope name (e.g., "source.go")
  std::vector<Pattern> patterns; // Top-level patterns
  Repository repository;         // Named pattern repository
};

// Main converter class from TextMate grammar to Vim syntax
class TmLanguage2VimSyntax {
public:
  TmLanguage2VimSyntax();
  ~TmLanguage2VimSyntax();

  // Parse TextMate grammar from JSON content
  bool parseJson(const std::string &jsonContent);

  // Generate Vim syntax file content
  std::string generateVimSyntax() const;

private:
  TextMateGrammar grammar_;

  // Parse JSON value into grammar structure
  void parseJsonValue(const std::string &json);

  // Parse individual pattern from JSON
  Pattern parsePattern(const nlohmann::json &patternJson);

  // Convert TextMate regex to Vim regex format
  std::string convertRegexToVim(const std::string &regex) const;

  // Convert TextMate scope to Vim syntax group name
  std::string convertScopeToVim(const std::string &scope) const;

  // Generate syntax rules for patterns
  void generateSyntaxRules(std::ostream &os,
                           const std::vector<Pattern> &patterns,
                           const std::string &parentGroup = "") const;

  // Generate repository rules
  void generateRepositoryRules(std::ostream &os) const;

  // Escape string for Vim syntax
  std::string escapeVimString(const std::string &str) const;

  // Map TextMate scope to Vim highlight group
  std::string mapScopeToHighlightGroup(const std::string &scope) const;

  // Collect all syntax groups from patterns
  void collectSyntaxGroups(const std::vector<Pattern> &patterns,
                           std::set<std::string> &groups) const;

  // Initialize Oniguruma (if needed)
  void initializeOniguruma();
};

#endif
