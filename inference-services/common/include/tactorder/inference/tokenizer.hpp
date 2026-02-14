#ifndef TACTORDER_INFERENCE_TOKENIZER_HPP
#define TACTORDER_INFERENCE_TOKENIZER_HPP

#include <algorithm>
#include <fstream>
#include <iostream>
#include <memory>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace tactorder {
namespace inference {

// Trie structure for efficient token matching
class Trie {
public:
  struct TrieNode {
    std::unordered_map<char, int> children;
    int id = -1;
  };

private:
  std::vector<TrieNode> list;
  int size = 1;
  int getFree();
  void insert(int nid, int token_id, std::string::const_iterator it,
              std::string::const_iterator end);
  int find(int nid, int current_matched, std::string::const_iterator current_it,
           std::string::const_iterator &it,
           const std::string::const_iterator &end);

public:
  Trie(int initial_size = 10000);
  void insert(std::pair<const std::string &, int> entry);
  int find(std::string::const_iterator &it,
           const std::string::const_iterator &end);
};

class Tokenizer {
public:
  static constexpr int MAGIC_NUMBER = 430;
  enum TokenizerType {
    SENTENCEPIECE = 0,
    TIKTOIKEN = 1,
    BERT = 2,
    HUGGINGFACE = 3
  };
  Tokenizer() = default;
  virtual ~Tokenizer() = default;
  static std::unique_ptr<Tokenizer>
  createTokenizer(const std::string &filename);
  bool is_stop(int token);
  bool is_special(int token);
  std::vector<int> encode(const std::string &str);
  virtual std::string decode(int id) = 0;

protected:
  void cache_special_tokens();
  virtual void load_special(std::ifstream &file);
  virtual bool load_vocab(std::ifstream &file) = 0;
  virtual void encode(const std::string &str, std::vector<int> &ids) = 0;
  std::vector<int> special_tokens_;
  std::vector<int> stop_tokens_;
  std::vector<int> prefix_tokens_;
  std::vector<std::pair<std::string, int>> special_tokens_cache_;

private:
  std::string mTemplate;
};

class Sentencepiece : public Tokenizer {
public:
  Sentencepiece() = default;
  virtual std::string decode(int id) override;

protected:
  virtual bool load_vocab(std::ifstream &file) override;
  virtual void encode(const std::string &str, std::vector<int> &ids) override;

private:
  enum ModelType { UNIGRAM = 1, BPE = 2, WORD = 3, CHAR = 4 };
  enum PieceType {
    NORMAL = 1,
    UNKNOWN = 2,
    CONTROL = 3,
    USER_DEFINED = 4,
    UNUSED = 5,
    BYTE = 6
  };
  struct SentencePiece {
    std::string piece;
    float score;
    PieceType type = PieceType::NORMAL;
    SentencePiece() {}
    SentencePiece(const std::string &p, float s, PieceType t)
        : piece(p), score(s), type(t) {}
  };
  using EncodeResult = std::vector<std::pair<std::string_view, int>>;

private:
  // model train type
  ModelType type_ = BPE;
  // byte fall back enable
  bool byte_fall_back_ = true;
  // unknown id.
  int unk_id_ = 0;
  // pieces from model
  std::vector<SentencePiece> sentence_pieces_;
  // piece -> id map for normal pieces
  std::unordered_map<std::string_view, int> pieces_;
  // piece -> id map for control, unknown, and byte pieces
  std::unordered_map<std::string_view, int> reserved_id_map_;

private:
  float get_score(int id) const;
  bool is_unused(int id) const;
  bool is_control(int id) const;
  int piece_to_id(std::string_view w) const;
  std::string byte_to_piece(unsigned char c) const;
  EncodeResult bpe_encode(std::string_view str, float alpha = 0.f);
};

class Tiktoken : public Tokenizer {
public:
  Tiktoken() = default;
  virtual std::string decode(int id) override;

protected:
  virtual bool load_vocab(std::ifstream &file) override;
  virtual void encode(const std::string &str, std::vector<int> &ids) override;
  Trie encoder_;
  std::vector<std::string> decoder_;
  std::unordered_map<std::string, int> encoder_map_;
};

class BertTokenizer : public Tokenizer {
public:
  BertTokenizer() = default;
  virtual std::string decode(int id) override;

protected:
  virtual bool load_vocab(std::ifstream &file) override;
  virtual void encode(const std::string &str, std::vector<int> &ids) override;
  std::unordered_map<std::string, int> encoder_;
  std::vector<std::string> decoder_;

private:
  std::vector<int> word_piece(const std::string &token);
};

class HuggingfaceTokenizer : public Tokenizer {
  struct hash_pair_wstring {
    size_t operator()(const std::pair<std::wstring, std::wstring> &p) const {
      auto hash1 = std::hash<std::wstring>{}(p.first);
      auto hash2 = std::hash<std::wstring>{}(p.second);
      return (hash1 != hash2) ? hash1 ^ hash2 : hash1;
    }
  };
  using BPERanks = std::unordered_map<std::pair<std::wstring, std::wstring>,
                                      int, hash_pair_wstring>;

public:
  HuggingfaceTokenizer() = default;
  virtual std::string decode(int id) override;

protected:
  virtual bool load_vocab(std::ifstream &file) override;
  virtual void encode(const std::string &str, std::vector<int> &ids) override;

private:
  void bpe(const std::wstring &token, const BPERanks &bpe_ranks,
           std::vector<std::wstring> *result);
  BPERanks bpe_ranks_;
  std::unordered_map<uint8_t, wchar_t> b2u_;
  std::unordered_map<wchar_t, uint8_t> u2b_;
  std::unordered_map<std::string, int> encoder_;
  std::vector<std::string> decoder_;
};

} // namespace inference
} // namespace tactorder

#endif // TACTORDER_INFERENCE_TOKENIZER_HPP
