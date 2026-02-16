
#include "tactorder/inference/tokenizer.hpp"
#include <chrono>
#include <iostream>
#include <string>
#include <vector>

using namespace tactorder::inference;

int main(int argc, char **argv) {
  if (argc < 2) {
    std::cerr << "Usage: " << argv[0] << " <tokenizer_path>" << std::endl;
    return 1;
  }

  std::string tokenizer_path = argv[1];
  std::cout << "Loading tokenizer from: " << tokenizer_path << std::endl;

  auto start = std::chrono::high_resolution_clock::now();
  auto tokenizer = Tokenizer::createTokenizer(tokenizer_path);
  auto end = std::chrono::high_resolution_clock::now();
  std::chrono::duration<double> elapsed = end - start;

  if (!tokenizer) {
    std::cerr << "Failed to load tokenizer!" << std::endl;
    return 1;
  }

  std::cout << "Tokenizer loaded in " << elapsed.count() << " seconds."
            << std::endl;

  std::string text = "Hello world! This is a test.";
  if (argc > 2) {
    text = argv[2];
  }
  std::cout << "Encoding text: \"" << text << "\"" << std::endl;
  std::vector<int> ids = tokenizer->encode(text);

  std::cout << "IDs: ";
  for (int id : ids) {
    std::cout << id << " ";
  }
  std::cout << std::endl;

  std::cout << "Decoded: ";
  for (int id : ids) {
    std::cout << tokenizer->decode(id);
  } // No space, decode should reconstruct nicely
  std::cout << std::endl;

  return 0;
}
