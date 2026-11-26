// kAIboard notice (GNU LGPL 2.1, section 2): this file was added by Alessandro Faedda on 2026-02-14.
// It is not part of KenLM as published at https://github.com/kpu/kenlm, commit
// 4cb443e60b7bf2c0ddf3c745378f76cb59e254e5.
// Content: kenlm_compat::binary_function, a replacement for std::binary_function (not part of C++17),
// used by lm/trie_sort.hh.
// Licence: the same as KenLM, GNU LGPL 2.1 or later.
// These notice lines were added on 2026-11-26; nothing else in the file was touched then.
// Fix per C++17: sostituisci std::binary_function
namespace kenlm_compat {
  template <typename Arg1, typename Arg2, typename Result>
  struct binary_function {
    typedef Arg1 first_argument_type;
    typedef Arg2 second_argument_type;
    typedef Result result_type;
  };
}
