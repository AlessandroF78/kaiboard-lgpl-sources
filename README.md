# kaiboard-lgpl-sources

Source code of the LGPL-licensed libraries that are compiled into the Android keyboard app **kAIboard**
(package `com.alessandro.kaiboard`), with the changes made for the app, the two JNI bridge files of the app that
are linked with them, and the instructions to rebuild the two native libraries.

This repository holds only what the GNU Lesser General Public License asks the distributor of these libraries to
make available. It is not the source code of the app.

## What is here

| Folder | Content | Licence |
|---|---|---|
| `libkenlm-jni/kenlm/` | KenLM, complete source at upstream commit `4cb443e60b7bf2c0ddf3c745378f76cb59e254e5`, with seven files changed or added for the app. Each of the seven carries a notice at its top; the list is in `libkenlm-jni/KENLM-CHANGES.md`. | GNU LGPL 2.1 or later; some files have their own licences, listed in `libkenlm-jni/kenlm/LICENSE` |
| `libkenlm-jni/kenlm_wrapper.cpp`, `libkenlm-jni/CMakeLists.txt` | The JNI bridge of the app for KenLM and its build file. Together with the folder above they build `libkenlm-jni.so`. | see "The two bridge files" below |
| `libpinyin-jni/libime/` | libime 1.1.14, complete source at upstream commit `8b400d014b992f814522d07d18f32da7e893789f`, unchanged, with its KenLM submodule (same KenLM commit as above, unchanged). | GNU LGPL 2.1 or later (`libpinyin-jni/libime/LICENSES/`) |
| `libpinyin-jni/fcitx5/` | fcitx5 5.1.20, source at upstream commit `04eff1334eb2eab45245c492bf5b85be4c4eda6b`, unchanged, without its submodule `third_party/yoga` (https://github.com/facebook/yoga, not LGPL and not used by this build). The app uses only the library Fcitx5Utils built from it. | GNU LGPL 2.1 or later (`libpinyin-jni/fcitx5/LICENSES/`) |
| `libpinyin-jni/pinyin_wrapper.cpp`, `libpinyin-jni/libime-wrapper/CMakeLists.txt` | The JNI bridge of the app for libime and the CMake file used to build libime for Android. They build `libpinyin-jni.so`. | see "The two bridge files" below |
| `zh-data/` | Where the source form of the Chinese Pinyin data is, and how the two data files are made from it. | GNU LGPL 2.1 or later |
| `BUILD.md` | How the two libraries are built, and how to use a rebuilt library with the app. | |
| `PINS.txt` | The exact upstream commits and the compiler package. | |
| `LGPL-2.1.txt` | The licence text. | |

No file of libime or fcitx5 was changed. The only changed library files are the seven KenLM files.

## Written offer (GNU LGPL 2.1, section 6)

This offer is valid for three years from the day you received the app. On request to **support@kaiboard.eu** we
give you the complete source code of these libraries with our changes, the source of the Chinese data and the code
of the two bridges, so that you can modify the libraries and rebuild them, at no more than the cost of sending it.
The same material is in this repository.

## The two bridge files

`libkenlm-jni/kenlm_wrapper.cpp` and `libpinyin-jni/pinyin_wrapper.cpp` are part of the app, not of the libraries.
They are published here, as source, for the purpose of section 6 of the GNU LGPL 2.1: so that a user can modify
the libraries, relink them with the bridge and use the result with the app. You may use, compile and modify them
for that purpose. All other rights are reserved. Copyright (c) 2026 Alessandro Faedda.

The files `libkenlm-jni/CMakeLists.txt` and `libpinyin-jni/libime-wrapper/CMakeLists.txt` are build scripts and
are provided on the same terms.

## Which app versions

- `libkenlm-jni.so` is compiled by the app build from `libkenlm-jni/` for the ABIs arm64-v8a and armeabi-v7a.
  From version code 140 on, the app is built from exactly the seven KenLM files of this repository. Up to version
  code 139 the same seven files were used without the notice lines at their top (comment lines only).
- `libpinyin-jni.so` (arm64-v8a only) from version code 140 on is built from `libpinyin-jni/pinyin_wrapper.cpp` as
  it is here: SHA-256 `c1bfb1eeda611fda6421fcb78563b6ba167a8cac2a8e909594fe6cfdb1421ce2`, 1975984 bytes.
  Up to version code 139 the bridge had one more line (a debug log call in `nativeCommit`); see `BUILD.md`.

## Trademark

"kAIboard" is a registered trademark of the developer. This repository gives no right to use it.

## Contact

Alessandro Faedda, Hamburg, Germany - support@kaiboard.eu - https://kaiboard.eu
