#include "tmlanguage2vimsyntax.hxx"
#include <fstream>
#include <iostream>
#include <string>

int main(int argc, char *argv[]) {
  if (argc != 3) {
    std::cerr << "Usage: " << argv[0] << " <input.tmLanguage> <output.vim>"
              << std::endl;
    return 1;
  }

  std::string inputFile = argv[1];
  std::string outputFile = argv[2];

  // Read input file
  std::ifstream file(inputFile);
  if (!file.is_open()) {
    std::cerr << "Error: Cannot open input file: " << inputFile << std::endl;
    return 1;
  }

  std::string jsonContent((std::istreambuf_iterator<char>(file)),
                          std::istreambuf_iterator<char>());
  file.close();

  // Parse TextMate grammar
  TmLanguage2VimSyntax parser;
  if (!parser.parseJson(jsonContent)) {
    std::cerr << "Error: Failed to parse TextMate grammar" << std::endl;
    return 1;
  }

  // Generate Vim syntax
  std::string vimSyntax = parser.generateVimSyntax();

  // Write output file
  std::ofstream outFile(outputFile);
  if (!outFile.is_open()) {
    std::cerr << "Error: Cannot open output file: " << outputFile << std::endl;
    return 1;
  }

  outFile << vimSyntax;
  outFile.close();

  std::cout << "Successfully generated Vim syntax file: " << outputFile
            << std::endl;
  return 0;
}
