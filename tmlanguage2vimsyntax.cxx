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
  std::string vimRegex = regex;

  // Convert TextMate regex to Vim format
  // Escape backslashes
  size_t pos = 0;
  while ((pos = vimRegex.find("\\\\", pos)) != std::string::npos) {
    vimRegex.replace(pos, 2, "\\\\\\\\");
    pos += 4;
  }

  // Additional conversion rules can be added here

  return vimRegex;
}

std::string
TmLanguage2VimSyntax::convertScopeToVim(const std::string &scope) const {
  // Convert TextMate scope to Vim syntax group name
  std::string vimGroup = scope;

  // Replace dots with underscores
  std::replace(vimGroup.begin(), vimGroup.end(), '.', '_');

  // Replace hyphens with underscores
  std::replace(vimGroup.begin(), vimGroup.end(), '-', '_');

  // Add prefix
  return "tm" + vimGroup;
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
      std::string vimRegex = convertRegexToVim(pattern.match);

      os << "syntax " << groupName;
      if (!parentGroup.empty()) {
        os << " contained";
      }
      os << " /" << vimRegex << "/\n";
    }

    if (!pattern.begin.empty() && !pattern.end.empty()) {
      std::string groupName = convertScopeToVim(pattern.name);
      std::string beginRegex = convertRegexToVim(pattern.begin);
      std::string endRegex = convertRegexToVim(pattern.end);

      os << "syntax region " << groupName;
      if (!parentGroup.empty()) {
        os << " contained";
      }
      os << " start=/" << beginRegex << "/ end=/" << endRegex << "/\n";
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
