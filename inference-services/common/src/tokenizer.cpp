#include "tactorder/inference/tokenizer.hpp"
#include <algorithm>
#include <cctype>
#include <climits>
#include <codecvt>
#include <cstring>
#include <fstream>
#include <functional>
#include <queue>
#include <random>
#include <regex>
#include <set>
#include <sstream>

namespace tactorder {
namespace inference {

// base64
static const int kBase64DecodeTable[] = {
    -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1, // 0-15
    -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1, // 16-31
    -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, 62, -1, -1, -1, 63, // 32-47 (+, /)
    52, 53, 54, 55, 56, 57, 58, 59,
    60, 61, -1, -1, -1, -1, -1, -1, // 48-63 (0-9)
    -1, 0,  1,  2,  3,  4,  5,  6,
    7,  8,  9,  10, 11, 12, 13, 14, // 64-79 (A-O)
    15, 16, 17, 18, 19, 20, 21, 22,
    23, 24, 25, -1, -1, -1, -1, -1, // 80-95 (P-Z)
    -1, 26, 27, 28, 29, 30, 31, 32,
    33, 34, 35, 36, 37, 38, 39, 40, // 96-111 (a-o)
    41, 42, 43, 44, 45, 46, 47, 48,
    49, 50, 51, -1, -1, -1, -1, -1 // 112-127 (p-z)
};

static std::string base64_decode(const std::string &str) {
  if (str.empty())
    return "";
  size_t in_len = str.size();
  std::string ret;
  ret.reserve(in_len * 3 / 4 + 2);

  int val = 0, valb = -8;
  for (unsigned char c : str) {
    if (c > 127)
      continue;
    int d = kBase64DecodeTable[c];
    if (d == -1)
      continue;
    val = (val << 6) + d;
    valb += 6;
    if (valb >= 0) {
      ret.push_back(char((val >> valb) & 0xFF));
      valb -= 8;
    }
  }
  return ret;
}

static inline size_t one_char_len(const char *src) {
  return "\1\1\1\1\1\1\1\1\1\1\1\1\2\2\3\4"[(*src & 0xFF) >> 4];
}

static inline void to_lower_case(std::string &str) {
  for (auto &c : str) {
    if (c >= 'A' && c <= 'Z') {
      c = tolower(static_cast<unsigned char>(c));
    }
  }
}

// Trie implementations
int Trie::getFree() {
  if (size < list.size()) {
    return size++;
  } else {
    list.resize(list.size() * 2);
    return size++;
  }
}

void Trie::insert(int nid, int token_id, std::string::const_iterator it,
                  std::string::const_iterator end) {
  auto &node = list[nid];
  if (it == end) {
    if (node.id == -1) {
      node.id = token_id;
    }
    return;
  }
  auto cid = node.children.find(*it);
  if (cid == node.children.end()) {
    int new_id = getFree();
    list[nid].children.insert({*it, new_id});
    insert(new_id, token_id, it + 1, end);
  } else {
    insert(cid->second, token_id, it + 1, end);
  }
}

int Trie::find(int nid, int current_matched,
               std::string::const_iterator current_it,
               std::string::const_iterator &it,
               const std::string::const_iterator &end) {
  const auto &node = list[nid];
  if (node.id != -1) {
    current_matched = node.id;
    current_it = it;
  }
  if (it == end) {
    return current_matched;
  }
  auto cid = node.children.find(*it);
  if (cid != node.children.end()) {
    return find(cid->second, current_matched, current_it, ++it, end);
  } else {
    if (node.id != -1) {
      return node.id;
    } else {
      it = current_it;
      return current_matched;
    }
  }
}

Trie::Trie(int initial_size) {
  list.resize(initial_size); // init the allocate size
  size = 1;                  // root
}
void Trie::insert(std::pair<const std::string &, int> entry) {
  insert(0, entry.second, entry.first.begin(), entry.first.end());
}
// Fix: update 'it' only if match found. passing 'it' as start position
int Trie::find(std::string::const_iterator &it,
               const std::string::const_iterator &end) {
  if (it == end) {
    return -1;
  }
  std::string::const_iterator next_it = it;
  int id = find(0, -1, it, next_it, end);
  if (id != -1) {
    it = next_it;
  }
  return id;
}

std::unique_ptr<Tokenizer>
Tokenizer::createTokenizer(const std::string &filename) {
  Tokenizer *tokenizer = nullptr;
  // check file
  std::ifstream tok_file(filename);
  if (!tok_file.good()) {
    std::cerr << "Failed: can't load tokenzier from: " << filename << std::endl;
    return nullptr;
  }
  // check tokenizer info
  std::string line;
  std::getline(tok_file, line);
  std::istringstream line_str(line);
  int magic_number, tokenizer_type;
  line_str >> magic_number;
  if (magic_number != MAGIC_NUMBER) {
    std::cerr << "Failed: magic number is wrong from: " << filename
              << std::endl;
    return nullptr;
  }
  line_str >> tokenizer_type;
  // create tokenizer
  switch (tokenizer_type) {
  case SENTENCEPIECE:
    tokenizer = new Sentencepiece();
    break;
  case TIKTOIKEN:
    tokenizer = new Tiktoken();
    break;
  case BERT:
    tokenizer = new BertTokenizer();
    break;
  case HUGGINGFACE:
    tokenizer = new HuggingfaceTokenizer();
    break;
  default:
    return nullptr;
  }
  tokenizer->load_special(tok_file);
  tokenizer->load_vocab(tok_file);
  tok_file.close();
  tokenizer->cache_special_tokens();
  return std::unique_ptr<Tokenizer>(tokenizer);
}

bool Tokenizer::is_stop(int token) {
  return std::find(stop_tokens_.begin(), stop_tokens_.end(), token) !=
         stop_tokens_.end();
}

bool Tokenizer::is_special(int token) {
  return std::find(special_tokens_.begin(), special_tokens_.end(), token) !=
         special_tokens_.end();
}

void Tokenizer::load_special(std::ifstream &tok_file) {
  std::string line;
  std::getline(tok_file, line);
  std::istringstream line_str(line);
  int special_num, stop_num, prefix_num;
  line_str >> special_num >> stop_num >> prefix_num;
  std::getline(tok_file, line);
  std::istringstream specail_line(line);
  if (special_num) {
    // load special tokens
    special_tokens_.resize(special_num);
    for (int i = 0; i < special_num; i++) {
      specail_line >> special_tokens_[i];
    }
  }
  if (stop_num) {
    // load stop tokens
    stop_tokens_.resize(stop_num);
    for (int i = 0; i < stop_num; i++) {
      specail_line >> stop_tokens_[i];
    }
  }
  if (prefix_num) {
    // load prefix tokens
    prefix_tokens_.resize(prefix_num);
    for (int i = 0; i < prefix_num; i++) {
      specail_line >> prefix_tokens_[i];
    }
  }
}

void Tokenizer::cache_special_tokens() {
  special_tokens_cache_.clear();
  for (int id : special_tokens_) {
    std::string token_str = decode(id);
    if (!token_str.empty()) {
      special_tokens_cache_.emplace_back(token_str, id);
    }
  }
}

std::vector<int> Tokenizer::encode(const std::string &str) {
  std::vector<int> ids = prefix_tokens_;
  if (special_tokens_cache_.empty() && !special_tokens_.empty()) {
    cache_special_tokens();
  }

  if (!special_tokens_cache_.empty()) {
    std::string text = str;
    size_t start = 0;
    for (size_t i = 0; i < text.length(); ++i) {
      for (const auto &pair : special_tokens_cache_) {
        const std::string &token = pair.first;
        int special_id = pair.second;
        if (i + token.length() <= text.length() &&
            strncmp(text.c_str() + i, token.c_str(), token.length()) == 0) {
          if (i > start) {
            encode(text.substr(start, i - start), ids);
          }
          ids.push_back(special_id);
          start = i + token.length();
          i = start - 1;
          break;
        }
      }
    }
    if (start < text.length()) {
      encode(text.substr(start), ids);
    }
  } else {
    encode(str, ids);
  }
  return ids;
}

// Sentencepiece Implementation

bool Sentencepiece::load_vocab(std::ifstream &tok_file) {
  std::string line;
  if (!std::getline(tok_file, line))
    return false;
  int vocab_len = std::stoi(line);
  sentence_pieces_.resize(vocab_len);
  pieces_.reserve(vocab_len);
  reserved_id_map_.reserve(vocab_len);

  for (int index = 0; index < vocab_len; index++) {
    std::getline(tok_file, line);

    size_t first_space = line.find(' ');
    if (first_space == std::string::npos)
      continue;
    size_t second_space = line.find(' ', first_space + 1);
    if (second_space == std::string::npos)
      continue;

    std::string token = base64_decode(line.substr(0, first_space));
    float score = std::strtof(line.c_str() + first_space + 1, nullptr);
    int type = std::atoi(line.c_str() + second_space + 1);

    auto piece_type = static_cast<PieceType>(type);
    sentence_pieces_[index] = {token, score, piece_type};
    // Use std::string_view constructor
    std::string_view token_sv(sentence_pieces_[index].piece);
    if (piece_type == PieceType::NORMAL) {
      pieces_.insert({token_sv, index});
    } else {
      reserved_id_map_.insert({token_sv, index});
      if (piece_type == PieceType::UNKNOWN) {
        unk_id_ = index;
      }
    }
  }
  return true;
}

int Sentencepiece::piece_to_id(std::string_view piece) const {
  auto it = reserved_id_map_.find(piece);
  if (it != reserved_id_map_.end()) {
    return it->second;
  }
  auto it2 = pieces_.find(piece);
  if (it2 != pieces_.end()) {
    return it2->second;
  }
  return unk_id_;
}

std::string Sentencepiece::byte_to_piece(unsigned char c) const {
  char buf[16];
  std::snprintf(buf, sizeof(buf), "<0x%02X>", c);
  return std::string(buf);
}

Sentencepiece::EncodeResult
Sentencepiece::bpe_encode(std::string_view normalized, float alpha) {
  struct SymbolPair {
    int left;
    int right;
    float score;
    size_t size;
  };

  struct SymbolPairComparator {
    const bool operator()(SymbolPair *h1, SymbolPair *h2) {
      return (h1->score < h2->score ||
              (h1->score == h2->score && h1->left > h2->left));
    }
  };

  struct Symbol {
    int prev;
    int next;
    bool freeze = false;
    std::string_view piece;
  };

  using Agenda = std::priority_queue<SymbolPair *, std::vector<SymbolPair *>,
                                     SymbolPairComparator>;
  Agenda agenda;
  std::vector<Symbol> symbols;
  symbols.reserve(normalized.size());
  std::unordered_map<std::string_view,
                     std::pair<std::string_view, std::string_view>>
      rev_merge;
  std::vector<std::unique_ptr<SymbolPair>> symbol_pair_holder;

  auto MaybeAddNewSymbolPair = [this, &symbol_pair_holder, &symbols, &agenda,
                                &rev_merge](int left, int right) {
    if (left == -1 || right == -1 || symbols[left].freeze ||
        symbols[right].freeze) {
      return;
    }
    const std::string_view piece(symbols[left].piece.data(),
                                 symbols[left].piece.size() +
                                     symbols[right].piece.size());
    const auto it = pieces_.find(piece);
    if (it == pieces_.end()) {
      return;
    }
    symbol_pair_holder.emplace_back(new SymbolPair);
    auto *h = symbol_pair_holder.back().get();
    h->left = left;
    h->right = right;
    h->score = get_score(it->second);
    h->size = piece.size();
    agenda.push(h);

    if (is_unused(it->second)) {
      rev_merge[piece] =
          std::make_pair(symbols[left].piece, symbols[right].piece);
    }
  };

  int index = 0;
  while (!normalized.empty()) {
    Symbol s;
    int mblen =
        std::min<int>(normalized.size(), one_char_len(normalized.data()));
    s.piece = std::string_view(normalized.data(), mblen);
    s.prev = index == 0 ? -1 : index - 1;
    normalized.remove_prefix(mblen);
    s.next = normalized.empty() ? -1 : index + 1;
    ++index;
    symbols.emplace_back(s);
  }

  if (symbols.empty()) {
    return {};
  }

  for (size_t i = 1; i < symbols.size(); ++i) {
    MaybeAddNewSymbolPair(i - 1, i);
  }

  std::mt19937 rand_gen;
  auto skip_merge = [&]() {
    if (alpha <= 0.0)
      return false;
    if (alpha >= 1.0)
      return true;
    std::uniform_real_distribution<> gen(0.0, 1.0);
    return gen(rand_gen) < alpha;
  };

  while (!agenda.empty()) {
    SymbolPair *top = agenda.top();
    agenda.pop();

    if (symbols[top->left].piece.empty() || symbols[top->right].piece.empty() ||
        symbols[top->left].piece.size() + symbols[top->right].piece.size() !=
            top->size) {
      continue;
    }

    if (skip_merge())
      continue;

    symbols[top->left].piece = std::string_view(
        symbols[top->left].piece.data(),
        symbols[top->left].piece.size() + symbols[top->right].piece.size());

    symbols[top->left].next = symbols[top->right].next;
    if (symbols[top->right].next >= 0) {
      symbols[symbols[top->right].next].prev = top->left;
    }
    symbols[top->right].piece = std::string_view("");

    MaybeAddNewSymbolPair(symbols[top->left].prev, top->left);
    MaybeAddNewSymbolPair(top->left, symbols[top->left].next);
  }

  std::function<void(std::string_view, EncodeResult *)> resegment;
  resegment = [this, &resegment, &rev_merge](std::string_view w,
                                             EncodeResult *output) -> void {
    const int id = piece_to_id(w);
    if (id == -1 || !is_unused(id)) {
      output->emplace_back(w, id);
      return;
    }
    const auto p = rev_merge.find(w);
    if (p == rev_merge.end()) {
      output->emplace_back(w, id);
      return;
    }
    resegment(p->second.first, output);
    resegment(p->second.second, output);
  };
  EncodeResult output;
  for (int index = 0; index != -1; index = symbols[index].next) {
    resegment(symbols[index].piece, &output);
  }
  return output;
}

void Sentencepiece::encode(const std::string &str, std::vector<int> &ids) {
  std::string normalized_str = "▁"; // Special character used in SP
  for (char c : str) {
    if (c == ' ') {
      normalized_str += "▁";
    } else {
      normalized_str += c;
    }
  }
  auto result = bpe_encode(normalized_str);
  for (const auto &p : result) {
    const std::string_view w = p.first;
    const int id = p.second;
    const bool is_unk = (id == unk_id_);
    if (is_unk && byte_fall_back_) {
      for (int i = 0; i < w.size(); ++i) {
        const char b = w[i];
        const auto piece = byte_to_piece(b);
        auto sp_id = piece_to_id(piece);
        ids.push_back(sp_id);
      }
    } else {
      ids.push_back(id);
    }
  }
}

std::string Sentencepiece::decode(int id) {
  if (id < 0 || id >= static_cast<int>(sentence_pieces_.size())) {
    return "";
  }
  auto piece = sentence_pieces_[id].piece;
  int pos = piece.find("▁");
  if (pos != -1) {
    piece.replace(pos, pos + 3, " ");
  }
  return piece;
}

float Sentencepiece::get_score(int id) const {
  return sentence_pieces_[id].score;
}

bool Sentencepiece::is_unused(int id) const {
  return sentence_pieces_[id].type == PieceType::UNUSED;
}

bool Sentencepiece::is_control(int id) const {
  return sentence_pieces_[id].type == PieceType::CONTROL;
}

// Tiktoken Implementation

bool Tiktoken::load_vocab(std::ifstream &tok_file) {
  std::string line;
  std::getline(tok_file, line);
  int vocab_len = std::stoi(line);
  // load vocab
  decoder_.resize(vocab_len);
  for (int i = 0; i < vocab_len; i++) {
    std::getline(tok_file, line);
    auto token = base64_decode(line);
    // encoder_.insert({token, i}); // Original used Trie
    // Using Trie for fast prefix match:
    encoder_.insert(std::pair<const std::string &, int>(token, i));

    decoder_[i] = token;
    // Optim: also keep a map for exact match if needed, but Trie handles it.
    encoder_map_[token] = i;
  }
  return true;
}

void Tiktoken::encode(const std::string &str, std::vector<int> &ids) {
  if (str.empty()) {
    return;
  }
  auto it = str.begin();
  while (it != str.end()) {
    auto last_it = it;
    int token_id = encoder_.find(it, str.end());
    if (token_id >= 0) {
      ids.push_back(token_id);
    } else {
      // Fallback or error. MNN printed error.
      std::cerr << "Error: No encoding found for sequence starting at "
                << std::distance(str.begin(), it) << std::endl;
      ++it; // Skip one char to avoid infinite loop
    }
  }
}

std::string Tiktoken::decode(int id) {
  if (id < 0 || id >= static_cast<int>(decoder_.size())) {
    return "";
  }
  return decoder_[id];
}

// BertTokenizer Implementation

bool BertTokenizer::load_vocab(std::ifstream &tok_file) {
  std::string line;
  std::getline(tok_file, line);
  int vocab_len = std::stoi(line);
  // load vocab
  decoder_.resize(vocab_len);
  for (int i = 0; i < vocab_len; i++) {
    std::getline(tok_file, line);
    auto token = base64_decode(line);
    encoder_.insert({token, i});
    decoder_[i] = token;
  }
  return true;
}

std::string BertTokenizer::decode(int id) {
  if (id < 0 || id >= static_cast<int>(decoder_.size())) {
    return "";
  }
  return decoder_[id];
}

std::vector<int> BertTokenizer::word_piece(const std::string &token) {
  auto it = encoder_.find(token);
  if (it != encoder_.end()) {
    return {it->second};
  }

  std::vector<int> ids;
  std::string current = token;
  bool is_first_piece = true;

  std::string candidate;
  candidate.reserve(token.size() + 2);

  while (!current.empty()) {
    int match_id = -1;
    size_t match_pos = 0;

    for (size_t len = current.size(); len > 0; --len) {
      candidate.clear();
      if (!is_first_piece) {
        candidate.append("##");
      }
      candidate.append(current.data(), len);

      auto vocab_it = encoder_.find(candidate);
      if (vocab_it != encoder_.end()) {
        match_id = vocab_it->second;
        match_pos = len;
        break;
      }
    }

    if (match_id == -1) {
      auto unk_it = encoder_.find("[UNK]");
      if (unk_it != encoder_.end()) {
        ids.push_back(unk_it->second);
      } else {
        ids.push_back(100);
      }
      break;
    }

    ids.push_back(match_id);
    current = current.substr(match_pos);
    is_first_piece = false;
  }

  return ids;
}

void BertTokenizer::encode(const std::string &str, std::vector<int> &ids) {
  std::vector<std::string> tokens;
  std::string current_token;
  size_t i = 0;

  while (i < str.size()) {
    current_token.clear();
    unsigned char c = static_cast<unsigned char>(str[i]);

    if ((c & 0x80) != 0) {
      int char_len = 1;
      if ((c & 0xE0) == 0xC0) {
        char_len = 2;
      } else if ((c & 0xF0) == 0xE0) {
        char_len = 3;
      } else if ((c & 0xF8) == 0xF0) {
        char_len = 4;
      }

      if (i + char_len <= str.size()) {
        current_token = str.substr(i, char_len);
        i += char_len;
      } else {
        ++i;
        continue;
      }
    } else if (isalnum(c)) {
      while (i < str.size() && isalnum(static_cast<unsigned char>(str[i]))) {
        current_token += tolower(str[i]);
        ++i;
      }
    } else if (ispunct(c)) {
      current_token = str[i];
      ++i;
    } else if (isspace(c)) {
      ++i;
      continue;
    } else {
      current_token = str[i];
      ++i;
    }

    if (!current_token.empty()) {
      tokens.push_back(current_token);
    }
  }

  for (const auto &token : tokens) {
    std::vector<int> token_ids = word_piece(token);
    for (int id : token_ids) {
      ids.push_back(id);
    }
  }
}

// HuggingfaceTokenizer Implementation

// Helpers for unicode conversion (simplified)
static std::wstring utf8_to_wstring(const char *str, size_t len) {
  std::wstring_convert<std::codecvt_utf8<wchar_t>> converter;
  return converter.from_bytes(str, str + len);
}

static std::string wstring_to_utf8(const std::wstring &str) {
  std::wstring_convert<std::codecvt_utf8<wchar_t>> converter;
  return converter.to_bytes(str);
}

bool HuggingfaceTokenizer::load_vocab(std::ifstream &tok_file) {
  std::string line;
  // 1. Get nums
  int vocab_len = 0;
  int merge_len = 0;
  if (std::getline(tok_file, line)) {
    std::istringstream line_str(line);
    line_str >> vocab_len >> merge_len;
  }

  // 2. Load vocab
  decoder_.resize(vocab_len);
  encoder_.reserve(vocab_len);

  for (int i = 0; i < vocab_len; i++) {
    std::getline(tok_file, line);

    decoder_[i] = line;
    encoder_.emplace(decoder_[i], i);
  }

  // 3. Load merge_rules
  bpe_ranks_.reserve(merge_len);
  for (int i = 0; i < merge_len; i++) {
    std::getline(tok_file, line);
    size_t d = line.find(' ');
    if (d != std::string::npos) {
      std::wstring first = utf8_to_wstring(line.data(), d);
      std::wstring second =
          utf8_to_wstring(line.data() + d + 1, line.size() - d - 1);
      bpe_ranks_.emplace(std::make_pair(first, second), i);
    }
  }

  // 4. bytes_to_unicode initialization (Simplified mapping)
  for (int c = 0; c < 256; c++) {
    b2u_[c] = (wchar_t)c; // Simplified identity for now. MNN has range logic.
    u2b_[(wchar_t)c] = c;
  }
  // TODO: implement full bytes_to_unicode logic if needed.
  return true;
}

void HuggingfaceTokenizer::bpe(const std::wstring &token,
                               const BPERanks &bpe_ranks,
                               std::vector<std::wstring> *result) {
  if (token.length() <= 1) {
    result->push_back(token);
    return;
  }

  std::vector<std::wstring> word;
  for (wchar_t c : token) {
    word.push_back(std::wstring(1, c));
  }

  while (word.size() > 1) {
    int min_rank = INT_MAX;
    std::pair<std::wstring, std::wstring> best_pair;
    int best_idx = -1;

    for (size_t i = 0; i < word.size() - 1; ++i) {
      std::pair<std::wstring, std::wstring> pair = {word[i], word[i + 1]};
      auto it = bpe_ranks.find(pair);
      if (it != bpe_ranks.end()) {
        if (it->second < min_rank) {
          min_rank = it->second;
          best_pair = pair;
          best_idx = i;
        }
      }
    }

    if (best_idx == -1)
      break;

    std::wstring new_token = best_pair.first + best_pair.second;
    std::vector<std::wstring> new_word;
    new_word.reserve(word.size());

    size_t i = 0;
    while (i < word.size()) {
      if (i < word.size() - 1 && word[i] == best_pair.first &&
          word[i + 1] == best_pair.second) {
        new_word.push_back(new_token);
        i += 2;
      } else {
        new_word.push_back(word[i]);
        i++;
      }
    }
    word = new_word;
  }
  *result = word;
}

void HuggingfaceTokenizer::encode(const std::string &str,
                                  std::vector<int> &ids) {
  // Basic implementation: split by generic delimiters, then BPE
  // This is a simplification of MNN's full regex split.

  // Convert to wstring for processing
  std::wstring wstr = utf8_to_wstring(str.c_str(), str.size());
  // Apply BPE
  std::vector<std::wstring> tokens;
  bpe(wstr, bpe_ranks_, &tokens);

  for (const auto &token : tokens) {
    std::string utf8_token = wstring_to_utf8(token);
    auto it = encoder_.find(utf8_token);
    if (it != encoder_.end()) {
      ids.push_back(it->second);
    } else {
      // UNK
      auto unk = encoder_.find("<unk>");
      if (unk != encoder_.end())
        ids.push_back(unk->second);
    }
  }
}

std::string HuggingfaceTokenizer::decode(int id) {
  if (id < 0 || id >= static_cast<int>(decoder_.size())) {
    return "";
  }
  return decoder_[id];
}

} // namespace inference
} // namespace tactorder
