// kAIboard notice (GNU LGPL 2.1, section 2): this file was changed by Alessandro Faedda on 2026-02-14.
// Change: the two #include lines that follow this notice were added, and std::binary_function
// was replaced by kenlm_compat::binary_function on class EntryCompare (std::binary_function is
// not part of C++17); empty lines at the end of the file.
// Original file: KenLM, https://github.com/kpu/kenlm, commit
// 4cb443e60b7bf2c0ddf3c745378f76cb59e254e5, lm/trie_sort.hh.
// These notice lines were added on 2026-11-26; nothing else in the file was touched then.
#include "../util/compat.hh"
#include "trie_sort_patch.hh"
// Step of trie builder: create sorted files.

#ifndef LM_TRIE_SORT_H
#define LM_TRIE_SORT_H

#include "max_order.hh"
#include "word_index.hh"

#include "../util/file.hh"
#include "../util/scoped.hh"

#include <cstddef>
#include <functional>
#include <string>
#include <vector>

#include <stdint.h>

namespace util {
class FilePiece;
} // namespace util

namespace lm {
class PositiveProbWarn;
namespace ngram {
class SortedVocabulary;
struct Config;

namespace trie {

class EntryCompare : public kenlm_compat::binary_function<const void*, const void*, bool> {
  public:
    explicit EntryCompare(unsigned char order) : order_(order) {}

    bool operator()(const void *first_void, const void *second_void) const {
      const WordIndex *first = static_cast<const WordIndex*>(first_void);
      const WordIndex *second = static_cast<const WordIndex*>(second_void);
      const WordIndex *end = first + order_;
      for (; first != end; ++first, ++second) {
        if (*first < *second) return true;
        if (*first > *second) return false;
      }
      return false;
    }
  private:
    unsigned char order_;
};

class RecordReader {
  public:
    RecordReader() : remains_(true) {}

    void Init(FILE *file, std::size_t entry_size);

    void *Data() { return data_.get(); }
    const void *Data() const { return data_.get(); }

    RecordReader &operator++() {
      std::size_t ret = fread(data_.get(), entry_size_, 1, file_);
      if (!ret) {
        UTIL_THROW_IF(!feof(file_), util::ErrnoException, "Error reading temporary file");
        remains_ = false;
      }
      return *this;
    }

    operator bool() const { return remains_; }

    void Rewind();

    std::size_t EntrySize() const { return entry_size_; }

    void Overwrite(const void *start, std::size_t amount);

  private:
    FILE *file_;

    util::scoped_malloc data_;

    bool remains_;

    std::size_t entry_size_;
};

class SortedFiles {
  public:
    // Build from ARPA
    SortedFiles(const Config &config, util::FilePiece &f, std::vector<uint64_t> &counts, std::size_t buffer, const std::string &file_prefix, SortedVocabulary &vocab);

    int StealUnigram() {
      return unigram_.release();
    }

    FILE *Full(unsigned char order) {
      return full_[order - 2].get();
    }

    FILE *Context(unsigned char of_order) {
      return context_[of_order - 2].get();
    }

  private:
    void ConvertToSorted(util::FilePiece &f, const SortedVocabulary &vocab, const std::vector<uint64_t> &counts, const std::string &prefix, unsigned char order, PositiveProbWarn &warn, void *mem, std::size_t mem_size);

    util::scoped_fd unigram_;

    util::scoped_FILE full_[KENLM_MAX_ORDER - 1], context_[KENLM_MAX_ORDER - 1];
};

} // namespace trie
} // namespace ngram
} // namespace lm

#endif // LM_TRIE_SORT_H


