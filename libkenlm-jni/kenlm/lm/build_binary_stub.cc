// kAIboard notice (GNU LGPL 2.1, section 2): this file was added by Alessandro Faedda on 2026-02-14.
// It is not part of KenLM as published at https://github.com/kpu/kenlm, commit
// 4cb443e60b7bf2c0ddf3c745378f76cb59e254e5.
// Content: empty LowerRestBuild constructor and destructor and the ProbingModel instantiation, so that
// the library links without the model builder (the app only loads models).
// Licence: the same as KenLM, GNU LGPL 2.1 or later.
// These notice lines were added on 2026-11-26; nothing else in the file was touched then.
// Stub per build_binary - funzioni vuote solo per linking
#include "lm/model.hh"
#include "lm/config.hh"
#include "lm/vocab.hh"
#include "util/file_piece.hh"

namespace lm {
namespace ngram {

template <class Model>
LowerRestBuild<Model>::LowerRestBuild(const Config &config, unsigned int order, const typename Model::Vocabulary &vocab) {
    // Stub - non implementato (non serve per loading modelli)
}

template <class Model>
LowerRestBuild<Model>::~LowerRestBuild() {
    // Stub - non implementato
}

// Template instantiations per ProbingModel
template class LowerRestBuild<ProbingModel>;

} // namespace ngram
} // namespace lm
