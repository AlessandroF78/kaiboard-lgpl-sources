# Changes to KenLM made for kAIboard

Upstream: https://github.com/kpu/kenlm, commit `4cb443e60b7bf2c0ddf3c745378f76cb59e254e5`.
The folder `kenlm/` is that commit, complete, with the seven files below. Every other file is the upstream file,
byte for byte. Changes by Alessandro Faedda.

| File | Kind | Date | What |
|---|---|---|---|
| `lm/trie_sort.hh` | changed | 2026-02-14 | Two `#include` lines added at the top (`../util/compat.hh`, `trie_sort_patch.hh`); `std::binary_function` replaced by `kenlm_compat::binary_function` on class `EntryCompare` (`std::binary_function` is not part of C++17); empty lines at the end of the file. |
| `util/joint_sort.hh` | changed | 2026-02-14 | One line added at the top: `#include "compat.hh"`; one empty line at the end of the file. |
| `util/sized_iterator.hh` | changed | 2026-02-14 | One line added at the top: `#include "compat.hh"`; one empty line at the end of the file. |
| `lm/search_hashed.cc` | changed | 2026-02-14 | No change to the code. One empty line added at the end of the file; the file was saved with a UTF-8 byte order mark. |
| `lm/build_binary_stub.cc` | added | 2026-02-14 | Empty `LowerRestBuild` constructor and destructor and the `ProbingModel` instantiation, so that the library links without the model builder (the app only loads models). |
| `lm/trie_sort_patch.hh` | added | 2026-02-14 | `kenlm_compat::binary_function`, used by `lm/trie_sort.hh`. |
| `util/compat.hh` | added | 2026-02-14, changed 2026-06-04 | A `std::binary_function` definition for C++17 builds; on 2026-06-04 the `#ifndef __ANDROID__` guard was added around it, so it is not compiled for Android. |

Notes:

- The notice lines at the top of each of the seven files were added on 2026-11-26. Before that day the files had
  no notice. Nothing else in the files was touched on that day.
- Dates: the KenLM folder of the app was not under version control when the changes were made (only
  `util/compat.hh` was added to it later, on 2026-06-05). The dates above are the last-modified times of the files
  on the developer's disk.
- The four changed files and the three added files start with a UTF-8 byte order mark, as they are compiled.
- The three added files are licensed like KenLM: GNU LGPL 2.1 or later.
- The app compiles only part of KenLM: the list of compiled files is in `CMakeLists.txt`.
