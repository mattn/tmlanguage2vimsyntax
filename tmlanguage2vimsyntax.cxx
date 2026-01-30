#include "tmlanguage2vimsyntax.hxx"
#include <algorithm>
#include <fstream>
#include <iostream>
#include <sstream>

TmLanguage2VimSyntax::TmLanguage2VimSyntax() {
  // Initialize converter
  initializeOniguruma();
}

TmLanguage2VimSyntax::~TmLanguage2VimSyntax() {
  // Cleanup resources
}

bool TmLanguage2VimSyntax::parseJson(const std::string &jsonContent) {
  try {
    parseJsonValue(jsonContent);
    return true;
  } catch (const std::exception &e) {
    std::cerr << "Error parsing JSON: " << e.what() << std::endl;
    return false;
  }
}

void TmLanguage2VimSyntax::parseJsonValue(const std::string &jsonStr) {
  using json = nlohmann::json;

  try {
    auto data = json::parse(jsonStr);

    // Extract basic information
    if (data.contains("name")) {
      grammar_.name = data["name"];
    }
    if (data.contains("scopeName")) {
      grammar_.scopeName = data["scopeName"];
    }

    // Extract top-level patterns
    if (data.contains("patterns")) {
      for (const auto &patternJson : data["patterns"]) {
        Pattern pattern = parsePattern(patternJson);
        grammar_.patterns.push_back(pattern);
      }
    }

    // Extract repository rules
    if (data.contains("repository")) {
      for (const auto &[name, ruleJson] : data["repository"].items()) {
        Pattern rule = parsePattern(ruleJson);
        grammar_.repository.rules[name] = rule;
      }
    }

  } catch (const json::exception &e) {
    throw std::runtime_error("JSON parse error: " + std::string(e.what()));
  }
}

Pattern TmLanguage2VimSyntax::parsePattern(const nlohmann::json &patternJson) {
  Pattern pattern;

  // Extract pattern name
  if (patternJson.contains("name")) {
    pattern.name = patternJson["name"];
  }
  if (patternJson.contains("match")) {
    pattern.match = patternJson["match"];
  }
  if (patternJson.contains("begin")) {
    pattern.begin = patternJson["begin"];
  }
  if (patternJson.contains("end")) {
    pattern.end = patternJson["end"];
  }
  if (patternJson.contains("include")) {
    pattern.include = patternJson["include"];
  }

  // Extract captures for match patterns
  if (patternJson.contains("captures")) {
    for (const auto &[key, value] : patternJson["captures"].items()) {
      if (value.contains("name")) {
        pattern.captures[key] = value["name"];
      }
    }
  }

  // Extract beginCaptures for region patterns
  if (patternJson.contains("beginCaptures")) {
    for (const auto &[key, value] : patternJson["beginCaptures"].items()) {
      if (value.contains("name")) {
        pattern.beginCaptures[key] = value["name"];
      }
    }
  }

  // Extract endCaptures for region patterns
  if (patternJson.contains("endCaptures")) {
    for (const auto &[key, value] : patternJson["endCaptures"].items()) {
      if (value.contains("name")) {
        pattern.endCaptures[key] = value["name"];
      }
    }
  }

  // Extract nested patterns
  if (patternJson.contains("patterns")) {
    for (const auto &subPatternJson : patternJson["patterns"]) {
      Pattern subPattern = parsePattern(subPatternJson);
      pattern.patterns.push_back(subPattern);
    }
  }

  return pattern;
}

std::string
TmLanguage2VimSyntax::convertRegexToVim(const std::string &regex) const {
  std::string preprocessed = regex;

  // Check for (?x) extended mode and remove whitespace/newlines if present
  if (regex.find("(?x)") != std::string::npos) {
    std::string result;
    bool inExtendedMode = false;
    bool inCharClass = false;
    bool escaped = false;

    for (size_t i = 0; i < regex.length(); ++i) {
      // Check for (?x)
      if (!escaped && i + 3 < regex.length() && regex.substr(i, 4) == "(?x)") {
        inExtendedMode = true;
        i += 3; // Skip (?x), will increment again in loop
        continue;
      }

      if (escaped) {
        result += regex[i];
        escaped = false;
        continue;
      }

      if (regex[i] == '\\') {
        result += regex[i];
        escaped = true;
        continue;
      }

      if (regex[i] == '[')
        inCharClass = true;
      if (regex[i] == ']')
        inCharClass = false;

      // In extended mode, skip whitespace and newlines outside character
      // classes
      if (inExtendedMode && !inCharClass &&
          (regex[i] == ' ' || regex[i] == '\t' || regex[i] == '\n' ||
           regex[i] == '\r')) {
        continue;
      }

      result += regex[i];
    }

    preprocessed = result;
  }

  std::string result;
  size_t i = 0;

  while (i < preprocessed.length()) {
    // Handle escaped characters from input
    if (preprocessed[i] == '\\' && i + 1 < preprocessed.length()) {
      char next = preprocessed[i + 1];

      // In Oniguruma:
      // \( \) \{ \} = literal characters
      // | + ? = operators (OR, one-or-more, zero-or-one)
      // \| \+ \? = literal characters
      //
      // In Vim:
      // ( ) { } = literal characters
      // \( \) \{ \} = operators (grouping, quantifier)
      // | + ? = literal characters
      // \| \+ \? = operators (OR, one-or-more, zero-or-one)

      if (next == '(' || next == ')' || next == '{' || next == '}') {
        // Oniguruma \( -> Vim (
        result += next;
        i += 2;
        continue;
      }

      if (next == '|' || next == '+' || next == '?' || next == '=') {
        // Oniguruma \| \+ \? \= -> Vim | + ? =
        // But for \|, use [|] to avoid confusion with Vim's \| operator
        if (next == '|') {
          result += "[|]";
        } else {
          result += next;
        }
        i += 2;
        continue;
      }

      if (next == '&') {
        // Oniguruma \& -> Vim & (literal in Vim)
        result += next;
        i += 2;
        continue;
      }

      // Keep other escaped sequences as-is (e.g., \b, \w, \d, \s, etc.)
      result += preprocessed[i];
      result += next;
      i += 2;
      continue;
    }

    // Handle special Oniguruma/PCRE constructs
    if (preprocessed[i] == '(' && i + 1 < preprocessed.length() &&
        preprocessed[i + 1] == '?') {
      // Special group construct
      if (i + 2 < preprocessed.length()) {
        char c = preprocessed[i + 2];

        // Handle modifiers like (?x), (?i), (?m) - just skip them as Vim
        // doesn't support them inline
        if (c == 'x' || c == 'i' || c == 'm' || c == 's') {
          if (i + 3 < preprocessed.length() && preprocessed[i + 3] == ')') {
            // Skip the modifier
            i += 4;
            continue;
          }
        }

        if (c == ':') {
          // Non-capturing group (?:...) -> \%(...\) in Vim
          result += "\\%(";
          i += 3;
          // Process content recursively
          int depth = 1;
          std::string inner;
          while (i < preprocessed.length() && depth > 0) {
            if (preprocessed[i] == '\\' && i + 1 < preprocessed.length()) {
              inner += preprocessed[i];
              inner += preprocessed[i + 1];
              i += 2;
              continue;
            }
            if (preprocessed[i] == '(')
              depth++;
            if (preprocessed[i] == ')') {
              depth--;
              if (depth == 0) {
                result += convertRegexToVim(inner);
                result += "\\)";
                i++;
                break;
              }
            }
            inner += preprocessed[i];
            i++;
          }
          continue;
        } else if (c == '=') {
          // Positive lookahead (?=...) -> (...)\@= in Vim
          i += 3;
          int depth = 1;
          std::string inner;
          while (i < preprocessed.length() && depth > 0) {
            if (preprocessed[i] == '\\' && i + 1 < preprocessed.length()) {
              inner += preprocessed[i];
              inner += preprocessed[i + 1];
              i += 2;
              continue;
            }
            if (preprocessed[i] == '(')
              depth++;
            if (preprocessed[i] == ')') {
              depth--;
              if (depth == 0) {
                // Convert inner pattern to Vim format
                std::string converted = convertRegexToVim(inner);
                result += "\\(" + converted + "\\)\\@=";
                i++;
                break;
              }
            }
            inner += preprocessed[i];
            i++;
          }
          continue;
        } else if (c == '!') {
          // Negative lookahead (?!...) -> (...)\@! in Vim
          i += 3;
          int depth = 1;
          std::string inner;
          while (i < preprocessed.length() && depth > 0) {
            if (preprocessed[i] == '\\' && i + 1 < preprocessed.length()) {
              inner += preprocessed[i];
              inner += preprocessed[i + 1];
              i += 2;
              continue;
            }
            if (preprocessed[i] == '(')
              depth++;
            if (preprocessed[i] == ')') {
              depth--;
              if (depth == 0) {
                std::string converted = convertRegexToVim(inner);
                result += "\\(" + converted + "\\)\\@!";
                i++;
                break;
              }
            }
            inner += preprocessed[i];
            i++;
          }
          continue;
        } else if (c == '<' && i + 3 < preprocessed.length()) {
          char d = preprocessed[i + 3];
          if (d == '=') {
            // Positive lookbehind (?<=...) -> (...)\@<= in Vim
            i += 4;
            int depth = 1;
            std::string inner;
            while (i < preprocessed.length() && depth > 0) {
              if (preprocessed[i] == '\\' && i + 1 < preprocessed.length()) {
                inner += preprocessed[i];
                inner += preprocessed[i + 1];
                i += 2;
                continue;
              }
              if (preprocessed[i] == '(')
                depth++;
              if (preprocessed[i] == ')') {
                depth--;
                if (depth == 0) {
                  std::string converted = convertRegexToVim(inner);
                  result += "\\(" + converted + "\\)\\@<=";
                  i++;
                  break;
                }
              }
              inner += preprocessed[i];
              i++;
            }
            continue;
          } else if (d == '!') {
            // Negative lookbehind (?<!...) -> (...)\@<! in Vim
            i += 4;
            int depth = 1;
            std::string inner;
            while (i < preprocessed.length() && depth > 0) {
              if (preprocessed[i] == '\\' && i + 1 < preprocessed.length()) {
                inner += preprocessed[i];
                inner += preprocessed[i + 1];
                i += 2;
                continue;
              }
              if (preprocessed[i] == '(')
                depth++;
              if (preprocessed[i] == ')') {
                depth--;
                if (depth == 0) {
                  std::string converted = convertRegexToVim(inner);
                  result += "\\(" + converted + "\\)\\@<!";
                  i++;
                  break;
                }
              }
              inner += preprocessed[i];
              i++;
            }
            continue;
          }
        }
      }
    }

    // Regular capturing group (Oniguruma)
    if (preprocessed[i] == '(') {
      result += "\\(";
      i++;
      continue;
    }

    // Handle closing parenthesis
    if (preprocessed[i] == ')') {
      result += "\\)";
      i++;
      continue;
    }

    // Handle special characters that are operators in Oniguruma
    if (preprocessed[i] == '|') {
      // Oniguruma | = OR, Vim \| = OR
      result += "\\|";
      i++;
      continue;
    }

    if (preprocessed[i] == '+') {
      // Oniguruma + = one or more, Vim \+ = one or more
      result += "\\+";
      i++;
      continue;
    }

    if (preprocessed[i] == '?') {
      // Oniguruma ? = zero or one, Vim \? = zero or one
      result += "\\?";
      i++;
      continue;
    }

    if (preprocessed[i] == '{') {
      // Oniguruma {n,m} = quantifier, Vim \{n,m\} = quantifier
      result += "\\{";
      i++;
      continue;
    }

    if (preprocessed[i] == '}') {
      result += "\\}";
      i++;
      continue;
    }

    // All other characters pass through as-is
    result += preprocessed[i];
    i++;
  }

  return result;
}

std::string
TmLanguage2VimSyntax::convertScopeToVim(const std::string &scope) const {
  // Convert TextMate scope to Vim syntax group name
  std::string vimGroup = scope;

  // If scope is empty, return empty string.
  if (vimGroup.empty()) {
    return "";
  }

  // Replace dots with underscores
  std::replace(vimGroup.begin(), vimGroup.end(), '.', '_');

  // Replace hyphens with underscores
  std::replace(vimGroup.begin(), vimGroup.end(), '-', '_');

  // Add prefix
  return "Go_" + vimGroup;
}

std::string
TmLanguage2VimSyntax::escapeVimString(const std::string &str) const {
  std::string escaped = str;
  size_t pos = 0;
  while ((pos = escaped.find("'", pos)) != std::string::npos) {
    escaped.replace(pos, 1, "''");
    pos += 2;
  }
  return escaped;
}

std::string
TmLanguage2VimSyntax::mapScopeToHighlightGroup(const std::string &scope) const {
  // Map TextMate scopes to standard Vim highlight groups
  // Based on official Vim syntax files
  if (scope.empty())
    return "";

  // Comments
  if (scope.find("comment.line") != std::string::npos)
    return "Comment";
  if (scope.find("comment.block") != std::string::npos)
    return "Comment";
  if (scope.find("comment") != std::string::npos)
    return "Comment";

  // Keywords - more specific mappings
  if (scope.find("keyword.package") != std::string::npos)
    return "Statement";
  if (scope.find("keyword.control.import") != std::string::npos)
    return "Statement";
  if (scope.find("keyword.control.go") != std::string::npos)
    return "Conditional";
  if (scope.find("keyword.control") != std::string::npos)
    return "Conditional";
  if (scope.find("keyword.function") != std::string::npos)
    return "Keyword";
  if (scope.find("keyword.var") != std::string::npos)
    return "Keyword";
  if (scope.find("keyword.const") != std::string::npos)
    return "Keyword";
  if (scope.find("keyword.type") != std::string::npos)
    return "Keyword";
  if (scope.find("keyword.interface") != std::string::npos)
    return "Keyword";
  if (scope.find("keyword.struct") != std::string::npos)
    return "Keyword";
  if (scope.find("keyword.map") != std::string::npos)
    return "Keyword";
  if (scope.find("keyword.channel") != std::string::npos)
    return "Keyword";
  if (scope.find("keyword.operator") != std::string::npos)
    return "Operator";
  if (scope.find("keyword") != std::string::npos)
    return "Keyword";

  // Storage types - Go built-in types
  if (scope.find("storage.type.boolean") != std::string::npos)
    return "Boolean";
  if (scope.find("storage.type.numeric") != std::string::npos)
    return "Type";
  if (scope.find("storage.type.string") != std::string::npos)
    return "Type";
  if (scope.find("storage.type.byte") != std::string::npos)
    return "Type";
  if (scope.find("storage.type.rune") != std::string::npos)
    return "Type";
  if (scope.find("storage.type.uintptr") != std::string::npos)
    return "Type";
  if (scope.find("storage.type.error") != std::string::npos)
    return "Type";
  if (scope.find("storage.type") != std::string::npos)
    return "Type";
  if (scope.find("storage") != std::string::npos)
    return "StorageClass";

  // Strings
  if (scope.find("string.quoted.double") != std::string::npos)
    return "String";
  if (scope.find("string.quoted.raw") != std::string::npos)
    return "String";
  if (scope.find("string.quoted.rune") != std::string::npos)
    return "Character";
  if (scope.find("string") != std::string::npos)
    return "String";

  // Constants
  if (scope.find("constant.numeric") != std::string::npos)
    return "Number";
  if (scope.find("constant.character.escape") != std::string::npos)
    return "SpecialChar";
  if (scope.find("constant.other.placeholder") != std::string::npos)
    return "SpecialChar";
  if (scope.find("constant.other.rune") != std::string::npos)
    return "Character";
  if (scope.find("constant.language") != std::string::npos)
    return "Boolean";
  if (scope.find("constant") != std::string::npos)
    return "Constant";

  // Functions
  if (scope.find("entity.name.function.support.builtin") != std::string::npos)
    return "Function";
  if (scope.find("entity.name.function") != std::string::npos)
    return "Function";
  if (scope.find("support.function.builtin") != std::string::npos)
    return "Function";
  if (scope.find("support.function") != std::string::npos)
    return "Function";

  // Types and entities
  if (scope.find("entity.name.type.package") != std::string::npos)
    return "Identifier";
  if (scope.find("entity.name.type.any") != std::string::npos)
    return "Type";
  if (scope.find("entity.name.type.comparable") != std::string::npos)
    return "Type";
  if (scope.find("entity.name.type") != std::string::npos)
    return "Type";

  // Variables
  if (scope.find("variable.parameter") != std::string::npos)
    return "Identifier";
  if (scope.find("variable.other.assignment") != std::string::npos)
    return "Identifier";
  if (scope.find("variable.other") != std::string::npos)
    return "Identifier";
  if (scope.find("variable") != std::string::npos)
    return "Identifier";

  // Punctuation - more specific
  if (scope.find("punctuation.terminator") != std::string::npos)
    return "Delimiter";
  if (scope.find("punctuation.separator") != std::string::npos)
    return "Delimiter";
  if (scope.find("punctuation.definition.begin") != std::string::npos)
    return "Delimiter";
  if (scope.find("punctuation.definition.end") != std::string::npos)
    return "Delimiter";
  if (scope.find("punctuation.other") != std::string::npos)
    return "Delimiter";
  if (scope.find("punctuation") != std::string::npos)
    return "Delimiter";

  // Invalid/Error
  if (scope.find("invalid.illegal") != std::string::npos)
    return "Error";
  if (scope.find("invalid") != std::string::npos)
    return "Error";

  // Support
  if (scope.find("support.type") != std::string::npos)
    return "Type";
  if (scope.find("support") != std::string::npos)
    return "Special";

  // Meta
  if (scope.find("meta.function") != std::string::npos)
    return "Function";
  if (scope.find("meta.type") != std::string::npos)
    return "Type";

  // Default - don't map everything to Normal
  return "";
}

void TmLanguage2VimSyntax::collectSyntaxGroups(
    const std::vector<Pattern> &patterns, std::set<std::string> &groups) const {
  for (const auto &pattern : patterns) {
    if (!pattern.name.empty()) {
      std::string groupName = convertScopeToVim(pattern.name);
      if (!groupName.empty()) {
        groups.insert(pattern.name); // Store original scope name
      }
    }
    // Collect beginCaptures
    if (!pattern.beginCaptures.empty()) {
      for (const auto &[key, scopeName] : pattern.beginCaptures) {
        if (!scopeName.empty()) {
          groups.insert(scopeName);
        }
      }
    }
    // Collect endCaptures
    if (!pattern.endCaptures.empty()) {
      for (const auto &[key, scopeName] : pattern.endCaptures) {
        if (!scopeName.empty()) {
          groups.insert(scopeName);
        }
      }
    }
    // Recursively collect from nested patterns
    if (!pattern.patterns.empty()) {
      collectSyntaxGroups(pattern.patterns, groups);
    }
  }
}

// Choose a delimiter that doesn't appear in the pattern
std::string chooseDelimiter(const std::string &pattern) {
  // Try delimiters in order of preference
  std::vector<char> delimiters = {'@', '#', '|', '~', '!', '%', '^', '&', '*'};
  for (char delim : delimiters) {
    if (pattern.find(delim) == std::string::npos) {
      return std::string(1, delim);
    }
  }
  // Fallback to @ if all else fails
  return "@";
}

void TmLanguage2VimSyntax::generateSyntaxRules(
    std::ostream &os, const std::vector<Pattern> &patterns,
    const std::string &parentGroup) const {
  for (size_t i = 0; i < patterns.size(); ++i) {
    const auto &pattern = patterns[i];
    // Only nested patterns (with parentGroup) are contained
    bool shouldBeContained = !parentGroup.empty();

    if (!pattern.match.empty()) {
      std::string groupName = convertScopeToVim(pattern.name);
      if (!groupName.empty()) {
        std::string vimRegex = convertRegexToVim(pattern.match);
        std::string delim = chooseDelimiter(vimRegex);

        os << "syntax match " << groupName;
        if (shouldBeContained) {
          os << " contained";
        }
        os << " " << delim << vimRegex << delim << "\n";
      }
    }
    if (!pattern.begin.empty() && !pattern.end.empty()) {
      std::string groupName = convertScopeToVim(pattern.name);
      std::string beginRegex = convertRegexToVim(pattern.begin);
      std::string endRegex = convertRegexToVim(pattern.end);

      // Handle beginCaptures - use matchgroup for first capture
      std::string matchGroup;
      if (!pattern.beginCaptures.empty()) {
        auto it = pattern.beginCaptures.find("1");
        if (it != pattern.beginCaptures.end() && !it->second.empty()) {
          matchGroup = convertScopeToVim(it->second);
        }
      }

      if (!groupName.empty() || !matchGroup.empty()) {
        // Choose delimiter that works for both begin and end
        std::string combinedPattern = beginRegex + endRegex;
        std::string delim = chooseDelimiter(combinedPattern);

        os << "syntax region ";
        if (!groupName.empty()) {
          os << groupName;
        } else {
          os << matchGroup << "_region";
        }
        if (shouldBeContained) {
          os << " contained";
        }
        if (!matchGroup.empty()) {
          os << " matchgroup=" << matchGroup;
        }
        os << " start=" << delim << beginRegex << delim << " end=" << delim
           << endRegex << delim;

        // Add contains for nested patterns - only if there are named patterns
        if (!pattern.patterns.empty()) {
          std::vector<std::string> containsList;
          for (const auto &subPattern : pattern.patterns) {
            if (!subPattern.name.empty()) {
              std::string subGroupName = convertScopeToVim(subPattern.name);
              if (!subGroupName.empty()) {
                containsList.push_back(subGroupName);
              }
            }
          }
          if (!containsList.empty()) {
            os << " contains=";
            for (size_t i = 0; i < containsList.size(); ++i) {
              if (i > 0)
                os << ",";
              os << containsList[i];
            }
          }
        }
        os << "\n";
      }
    }
    // Process nested patterns
    if (!pattern.patterns.empty()) {
      generateSyntaxRules(os, pattern.patterns,
                          convertScopeToVim(pattern.name));
    }
  }
}

void TmLanguage2VimSyntax::generateRepositoryRules(std::ostream &os) const {
  // Define priority order - specific patterns first, generic patterns last
  // Note: Later definitions have higher priority in Vim
  std::vector<std::string> priorityOrder = {"keywords",
                                            "package_name",
                                            "import",
                                            "imports",
                                            "string_literals",
                                            "raw_string_literals",
                                            "runes",
                                            "numeric_literals",
                                            "storage_types",
                                            "built_in_functions",
                                            "operators",
                                            "delimiters",
                                            "language_constants",
                                            "comments"};

  std::vector<std::string> lowPriorityOrder = {
      "other_variables", "variable_assignment",
      "other_struct_interface_expressions"};

  std::set<std::string> processed;

  // Output high priority rules first
  for (const auto &name : priorityOrder) {
    auto it = grammar_.repository.rules.find(name);
    if (it != grammar_.repository.rules.end()) {
      os << "\" Repository rule: " << name << "\n";

      // Special handling for keywords - convert simple \b word \b patterns to
      // syntax keyword
      if (name == "keywords") {
        // Extract keywords and use syntax keyword which has highest priority
        std::set<std::string> handledPatterns;
        for (const auto &pattern : it->second.patterns) {
          if (!pattern.match.empty() && !pattern.name.empty()) {
            std::string groupName = convertScopeToVim(pattern.name);
            if (!groupName.empty()) {
              bool handled = false;
              // Check if it's a simple keyword pattern like
              // \b(word1|word2|...)\b
              std::string match = pattern.match;
              if (match.find("\\b(") != std::string::npos &&
                  match.find(")\\b") != std::string::npos &&
                  match.find("|") != std::string::npos) {
                // Extract keywords from pattern
                size_t start = match.find("\\b(") + 3;
                size_t end = match.find(")\\b");
                if (start < end) {
                  std::string keywords = match.substr(start, end - start);
                  // Replace | with space for syntax keyword
                  std::replace(keywords.begin(), keywords.end(), '|', ' ');
                  os << "syntax keyword " << groupName << " " << keywords
                     << "\n";
                  handledPatterns.insert(pattern.name);
                  handled = true;
                }
              }
              // For single keyword patterns like \bfunc\b
              if (!handled && match.find("\\b") == 0 &&
                  match.rfind("\\b") == match.length() - 2) {
                std::string keyword = match.substr(2, match.length() - 4);
                if (keyword.find('\\') == std::string::npos &&
                    keyword.find('(') == std::string::npos) {
                  os << "syntax keyword " << groupName << " " << keyword
                     << "\n";
                  handledPatterns.insert(pattern.name);
                  handled = true;
                }
              }
            }
          }
        }
        // Don't generate syntax match for patterns we've handled with syntax
        // keyword Skip generateSyntaxRules for keywords
        processed.insert(name);
        // Note: syntax keyword doesn't count for isFirstRule since keywords are
        // always top-level
        continue;
      }

      // Special handling for package_name - add package keyword first
      if (name == "package_name") {
        os << "syntax keyword Go_keyword_package_go package\n";
        // Note: syntax keyword doesn't count for isFirstRule
      }

      generateSyntaxRules(os, it->second.patterns);
      processed.insert(name);
    }
  }

  // Output medium priority rules (everything else except low priority)
  for (const auto &[name, rule] : grammar_.repository.rules) {
    if (processed.find(name) == processed.end() &&
        std::find(lowPriorityOrder.begin(), lowPriorityOrder.end(), name) ==
            lowPriorityOrder.end()) {
      os << "\" Repository rule: " << name << "\n";
      generateSyntaxRules(os, rule.patterns);
      processed.insert(name);
    }
  }

  // Output low priority rules last
  for (const auto &name : lowPriorityOrder) {
    auto it = grammar_.repository.rules.find(name);
    if (it != grammar_.repository.rules.end()) {
      os << "\" Repository rule: " << name << "\n";
      generateSyntaxRules(os, it->second.patterns);
      processed.insert(name);
    }
  }
}

std::string TmLanguage2VimSyntax::generateVimSyntax() const {
  std::ostringstream os;

  // Header
  os << "\" Vim syntax file generated from TextMate grammar\n";
  os << "\" Language: " << grammar_.name << "\n";
  os << "\" Maintainer: Generated by tmlanguage2vimsyntax\n\n";

  // Clear existing syntax
  os << "if exists(\"b:current_syntax\")\n";
  os << "  finish\n";
  os << "endif\n\n";

  // Clear syntax
  os << "syntax clear\n\n";

  // Generate top-level patterns
  generateSyntaxRules(os, grammar_.patterns);

  // Generate repository rules
  if (!grammar_.repository.rules.empty()) {
    os << "\n\" Repository rules\n";
    generateRepositoryRules(os);
  }

  // Collect all syntax groups
  std::set<std::string> scopeNames;
  collectSyntaxGroups(grammar_.patterns, scopeNames);
  for (const auto &[name, rule] : grammar_.repository.rules) {
    collectSyntaxGroups({rule}, scopeNames);
  }

  // Generate highlight links
  os << "\n\" Highlight links\n";
  for (const auto &scopeName : scopeNames) {
    std::string groupName = convertScopeToVim(scopeName);
    std::string hlGroup = mapScopeToHighlightGroup(scopeName);
    if (!groupName.empty() && !hlGroup.empty()) {
      os << "highlight default link " << groupName << " " << hlGroup << "\n";
    }
  }

  // Footer
  os << "\nlet b:current_syntax = \"" << grammar_.scopeName << "\"\n";

  return os.str();
}

void TmLanguage2VimSyntax::initializeOniguruma() {
  // Initialize Oniguruma regex engine (if needed for advanced features)
  // onig_init(); // Uncomment when actually using Oniguruma features
}
