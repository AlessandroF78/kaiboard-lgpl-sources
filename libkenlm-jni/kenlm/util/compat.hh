// kAIboard notice (GNU LGPL 2.1, section 2): this file was added by Alessandro Faedda on 2026-02-14.
// It is not part of KenLM as published at https://github.com/kpu/kenlm, commit
// 4cb443e60b7bf2c0ddf3c745378f76cb59e254e5.
// Content: a std::binary_function definition for C++17 builds, compiled only when __ANDROID__ is not
// defined.
// Licence: the same as KenLM, GNU LGPL 2.1 or later.
// Changed on 2026-06-04: the "#ifndef __ANDROID__" guard around the definition was added.
// These notice lines were added on 2026-11-26; nothing else in the file was touched then.
#ifndef KENLM_COMPAT_HH
#define KENLM_COMPAT_HH

// Compatibility shim for C++17 (std::binary_function removed)
#ifndef __ANDROID__
namespace std {
  template <typename Arg1, typename Arg2, typename Result>
  struct binary_function {
    typedef Arg1 first_argument_type;
    typedef Arg2 second_argument_type;
    typedef Result result_type;
  };
}
#endif // __ANDROID__

#endif // KENLM_COMPAT_HH
