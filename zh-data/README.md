# Chinese Pinyin data

The app downloads two data files for Chinese input, on demand, from the release `v1.1` of
https://github.com/AlessandroF78/kaiboard-dictionaries :

| File in the app | Size | SHA-256 |
|---|---|---|
| `sc.dict` | 4495410 bytes | `e82c66be594be96ab19b63dc007e49b625a28f4ddd59adb508cf931decf7a281` |
| `zh_CN.lm` | 25626482 bytes | `6befd99fc39dc4eb8ba4410d24109fe73b94ee4655fa252ba739cbc308e349a7` |

(The release holds them compressed, as `sc.dict.gz` and `zh_CN.lm.gz`.) They are, byte for byte, the files
`libime/data/sc.dict` and `libime/data/zh_CN.lm` of https://github.com/fcitx5-android/prebuilt at commit
`0bb1f2684837ec0b2e56514e3ec7e58270bcd900`. They were not changed for the app.

## Source form

The two files are built from two text archives published by the libime project. The same two archives are
attached to the release `v1.1` of kaiboard-dictionaries, next to the data files:

| Archive | Size | SHA-256 | Holds |
|---|---|---|---|
| `dict-20260430.tar.zst` | 2314701 bytes | `3edc008d90fcd61b9967b9e590f396189d7a00fa74c45ecde0d9850a0fdd6241` | `dict_sc.txt`, `dict_extb.txt` |
| `lm_sc.arpa-20250113.tar.zst` | 42045779 bytes | `ee83ecf20d52e8bccdba4cf6cd57183d53c257713a5eb77ee3a63d50fc3796dd` | `lm_sc.arpa` |

Tools (both are part of libime, whose source is in `../libpinyin-jni/libime/`):

    libime_pinyindict dict_sc.txt sc.dict
    libime_slm_build_binary -s -a 22 -q 8 trie lm_sc.arpa sc.lm

These are the commands of the libime build file `data/CMakeLists.txt` (`sc.lm` is the file that the
fcitx5-android project publishes under the name `zh_CN.lm`). The developer of kAIboard did not run them: the app
uses the two files as built by the fcitx5-android project. `dict_extb.txt` is the source of `extb.dict`, which the
app does not use.

## Licence

GNU LGPL 2.1 or later, as part of libime (copyright Yuking 2002-2005, CSSlayer 2014-2026, as stated in the
Debian copyright file of the libime package). The archives themselves contain no licence file.
