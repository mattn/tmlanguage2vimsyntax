#include "tmlanguage2vimsyntax.hxx"
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
  std::string result;
  size_t i = 0;

  while (i < regex.length()) {
    // Handle escaped characters from input
    if (regex[i] == '\\' && i + 1 < regex.length()) {
      char next = regex[i + 1];

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

      if (next == '|' || next == '+' || next == '?') {
        // Oniguruma \| \+ \? -> Vim | + ?
        result += next;
        i += 2;
        continue;
      }

      // Keep other escaped sequences as-is (e.g., \b, \w, \d, \s, etc.)
      result += regex[i];
      result += next;
      i += 2;
      continue;
    }

    // Handle special Oniguruma/PCRE constructs
    if (regex[i] == '(' && i + 1 < regex.length() && regex[i + 1] == '?') {
      // Special group construct
      if (i + 2 < regex.length()) {
        char c = regex[i + 2];

        if (c == ':') {
          // Non-capturing group (?:...) -> \%(...\) in Vim
          result += "\\%(";
          i += 3;
          // Process content recursively
          int depth = 1;
          std::string inner;
          while (i < regex.length() && depth > 0) {
            if (regex[i] == '\\' && i + 1 < regex.length()) {
              inner += regex[i];
              inner += regex[i + 1];
              i += 2;
              continue;
            }
            if (regex[i] == '(')
              depth++;
            if (regex[i] == ')') {
              depth--;
              if (depth == 0) {
                result += convertRegexToVim(inner);
                result += "\\)";
                i++;
                break;
              }
            }
            inner += regex[i];
            i++;
          }
          continue;
        } else if (c == '=') {
          // Positive lookahead (?=...) -> (...)\@= in Vim
          i += 3;
          int depth = 1;
          std::string inner;
          while (i < regex.length() && depth > 0) {
            if (regex[i] == '\\' && i + 1 < regex.length()) {
              inner += regex[i];
              inner += regex[i + 1];
              i += 2;
              continue;
            }
            if (regex[i] == '(')
              depth++;
            if (regex[i] == ')') {
              depth--;
              if (depth == 0) {
                // Convert inner pattern to Vim format
                std::string converted = convertRegexToVim(inner);
                result += "\\(" + converted + "\\)\\@=";
                i++;
                break;
              }
            }
            inner += regex[i];
            i++;
          }
          continue;
        } else if (c == '!') {
          // Negative lookahead (?!...) -> (...)\@! in Vim
          i += 3;
          int depth = 1;
          std::string inner;
          while (i < regex.length() && depth > 0) {
            if (regex[i] == '\\' && i + 1 < regex.length()) {
              inner += regex[i];
              inner += regex[i + 1];
              i += 2;
              continue;
            }
            if (regex[i] == '(')
              depth++;
            if (regex[i] == ')') {
              depth--;
              if (depth == 0) {
                std::string converted = convertRegexToVim(inner);
                result += "\\(" + converted + "\\)\\@!";
                i++;
                break;
              }
            }
            inner += regex[i];
            i++;
          }
          continue;
        } else if (c == '<' && i + 3 < regex.length()) {
          char d = regex[i + 3];
          if (d == '=') {
            // Positive lookbehind (?<=...) -> (...)\@<= in Vim
            i += 4;
            int depth = 1;
            std::string inner;
            while (i < regex.length() && depth > 0) {
              if (regex[i] == '\\' && i + 1 < regex.length()) {
                inner += regex[i];
                inner += regex[i + 1];
                i += 2;
                continue;
              }
              if (regex[i] == '(')
                depth++;
              if (regex[i] == ')') {
                depth--;
                if (depth == 0) {
                  std::string converted = convertRegexToVim(inner);
                  result += "\\(" + converted + "\\)\\@<=";
                  i++;
                  break;
                }
              }
              inner += regex[i];
              i++;
            }
            continue;
          } else if (d == '!') {
            // Negative lookbehind (?<!...) -> (...)\@<! in Vim
            i += 4;
            int depth = 1;
            std::string inner;
            while (i < regex.length() && depth > 0) {
              if (regex[i] == '\\' && i + 1 < regex.length()) {
                inner += regex[i];
                inner += regex[i + 1];
                i += 2;
                continue;
              }
              if (regex[i] == '(')
                depth++;
              if (regex[i] == ')') {
                depth--;
                if (depth == 0) {
                  std::string converted = convertRegexToVim(inner);
                  result += "\\(" + converted + "\\)\\@<!";
                  i++;
                  break;
                }
              }
              inner += regex[i];
              i++;
            }
            continue;
          }
        }
      }
    }

    // Regular capturing group (Oniguruma)
    if (regex[i] == '(') {
      result += "\\(";
      i++;
      continue;
    }

    // Handle closing parenthesis
    if (regex[i] == ')') {
      result += "\\)";
      i++;
      continue;
    }

    // Handle special characters that are operators in Oniguruma
    if (regex[i] == '|') {
      // Oniguruma | = OR, Vim \| = OR
      result += "\\|";
      i++;
      continue;
    }

    if (regex[i] == '+') {
      // Oniguruma + = one or more, Vim \+ = one or more
      result += "\\+";
      i++;
      continue;
    }

    if (regex[i] == '?') {
      // Oniguruma ? = zero or one, Vim \? = zero or one
      result += "\\?";
      i++;
      continue;
    }

    if (regex[i] == '{') {
      // Oniguruma {n,m} = quantifier, Vim \{n,m\} = quantifier
      result += "\\{";
      i++;
      continue;
    }

    if (regex[i] == '}') {
      result += "\\}";
      i++;
      continue;
    }

    // All other characters pass through as-is
    result += regex[i];
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

void TmLanguage2VimSyntax::generateSyntaxRules(
    std::ostream &os, const std::vector<Pattern> &patterns,
    const std::string &parentGroup) const {
  for (const auto &pattern : patterns) {
    if (!pattern.match.empty()) {
      std::string groupName = convertScopeToVim(pattern.name);
      if (!groupName.empty()) {
        std::string vimRegex = convertRegexToVim(pattern.match);

        os << "syntax match " << groupName;
        if (!parentGroup.empty()) {
          os << " contained";
        }
        // Use @ as delimiter to avoid conflicts with / in patterns
        os << " @" << vimRegex << "@\n";
      }
    }
    if (!pattern.begin.empty() && !pattern.end.empty()) {
      std::string groupName = convertScopeToVim(pattern.name);
      if (!groupName.empty()) {
        std::string beginRegex = convertRegexToVim(pattern.begin);
        std::string endRegex = convertRegexToVim(pattern.end);

        os << "syntax region " << groupName;
        if (!parentGroup.empty()) {
          os << " contained";
        }
        // Use @ as delimiter
        os << " start=@" << beginRegex << "@ end=@" << endRegex << "@\n";
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
  for (const auto &[name, rule] : grammar_.repository.rules) {
    os << "\" Repository rule: " << name << "\n";
    generateSyntaxRules(os, {rule});
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

  // Footer
  os << "\nlet b:current_syntax = \"" << grammar_.scopeName << "\"\n";

  return os.str();
}

void TmLanguage2VimSyntax::initializeOniguruma() {
  // Initialize Oniguruma regex engine (if needed for advanced features)
  // onig_init(); // Uncomment when actually using Oniguruma features
}
