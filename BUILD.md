# How the two libraries are built

## 1. libkenlm-jni.so

Built by the Gradle build of the app (Android Gradle Plugin 9.0.0, `externalNativeBuild` with CMake 3.22.1,
Android NDK 25.2.9519653, C++17, ABIs arm64-v8a and armeabi-v7a, minSdk 26) from the folder `libkenlm-jni/`:

- `CMakeLists.txt` - the build file, as used by the app. It compiles `kenlm_wrapper.cpp` and the KenLM files it
  lists into one shared library, with `KENLM_MAX_ORDER=6`, `NDEBUG` and
  `-Wl,-z,max-page-size=16384` (16 KB pages).
- `kenlm/` - KenLM with the seven changed or added files (`KENLM-CHANGES.md`).
- The build file expects the Boost headers in a folder `include/` next to it (`include/boost/...`). The app uses
  the headers of Boost 1.90.0 (https://www.boost.org, Boost Software License 1.0). They are not copied here.

To build outside the app, with the same NDK:

    cmake -S libkenlm-jni -B build-kenlm -GNinja \
      -DCMAKE_TOOLCHAIN_FILE=$NDK/build/cmake/android.toolchain.cmake \
      -DANDROID_ABI=arm64-v8a -DANDROID_PLATFORM=android-26 -DCMAKE_BUILD_TYPE=Release
    ninja -C build-kenlm

This standalone command line was NOT run by the developer; the library in the app is built by Gradle, which
passes its own flags. The CMake target, and so the library, is named `kenlm-jni`; the JNI functions are those
of `kenlm_wrapper.cpp` (Java class `com.alessandro.kaiboard.predict.KenLMNative`).

## 2. libpinyin-jni.so (arm64-v8a)

Built by hand on Linux (Ubuntu 24.04 under WSL 2), outside the app build. The steps below were run again on
2026-11-26 and gave, from the bridge file as it was in app version code 139, a library byte-identical to the one
in that version (SHA-256 `c76c747fc9cbdf2dba472413fd02528b2a7acaa84c7d43177559ae5c50231fe9`, 1976048 bytes), and
from `libpinyin-jni/pinyin_wrapper.cpp` as published here the library of version code 140
(SHA-256 `c1bfb1eeda611fda6421fcb78563b6ba167a8cac2a8e909594fe6cfdb1421ce2`, 1975984 bytes).

The only difference between the two bridge files: the published one does not have the line

        LOGD("nativeCommit: \"%s\"", s.c_str());

which stood in function `nativeCommit` (line 146 of the older file).

### Tools

- Android NDK r28b for Linux, revision 28.1.13356709 (`android-ndk-r28b-linux.zip`, 722264879 bytes,
  SHA-1 `f574d3165405bd59ffc5edaadac02689075a729f`); compiler Clang 19.0.0 (r530567d).
- CMake 3.28.3, Ninja 1.11.1, pkg-config 1.8.1, extra-cmake-modules (ECM) from the Ubuntu package.
- Static libraries and headers of the other parts, from https://github.com/fcitx5-android/prebuilt at commit
  `0bb1f2684837ec0b2e56514e3ec7e58270bcd900` (folders `boost/`, `zstd/`, `marisa/`, `libintl-lite/`; they are not
  LGPL and are not in the file tree of this repository). Of these, only code of `libzstd.a` and of the Boost
  headers ends up in the library; `libmarisa.a`, `libintl.a`, `libboost_iostreams.a` and `libboost_container.a`
  are on the link line but no member of them is linked in (linker map of 2026-11-26).
- A copy of the folders used from that commit (Boost 1.90.0, zstd 1.5.7, marisa-trie, libintl-lite, for
  arm64-v8a), unchanged and with their licence texts, is attached to the release `app-140` of this repository:
  `kaiboard-lgpl-prebuilt-parts-0bb1f268.tar.gz`, 11165510 bytes,
  SHA-256 `34316acf32d7e84713045dd94f8cbe27289ab39d2c8610f48b4af85913470837`. Unpacked, its top folder can be
  used as `$S/prebuilt` below.

### Layout used below

    S=/tmp/libime_scratch          # any folder; the same absolute path gives the same bytes
    $S/ndk-linux                   # the unpacked NDK
    $S/libime-src                  # = libpinyin-jni/libime of this repository
    $S/fcitx5-src                  # = libpinyin-jni/fcitx5 of this repository
    $S/prebuilt                    # the checkout of fcitx5-android/prebuilt, or the unpacked release archive
    $S/build                       # build output
    NDK=$S/ndk-linux; PREBUILT=$S/prebuilt; BUILD=$S/build; INSTALL=$BUILD/install

### A. Fcitx5Utils (static)

    mkdir -p $BUILD/fcitx5-build && cd $BUILD/fcitx5-build
    cmake $S/fcitx5-src -GNinja \
      -DCMAKE_TOOLCHAIN_FILE=$NDK/build/cmake/android.toolchain.cmake \
      -DANDROID_ABI=arm64-v8a -DANDROID_PLATFORM=android-26 -DANDROID_STL=c++_shared \
      -DANDROID_SUPPORT_FLEXIBLE_PAGE_SIZES=ON -DCMAKE_CXX_STANDARD=17 -DCMAKE_BUILD_TYPE=Release \
      -DCMAKE_INSTALL_PREFIX=$INSTALL -DCMAKE_CXX_FLAGS='-fexperimental-library' \
      -DECM_DIR=/usr/share/ECM/cmake \
      -DENABLE_TESTING_ADDONS=OFF -DENABLE_TEST=OFF -DENABLE_ENCHANT=OFF -DENABLE_X11=OFF \
      -DENABLE_WAYLAND=OFF -DENABLE_DBUS=OFF -DENABLE_SERVER=OFF -DENABLE_KEYBOARD=OFF \
      -DENABLE_XDGAUTOSTART=OFF -DENABLE_EMOJI=OFF -DENABLE_LIBUUID=OFF -DBUILD_SPELL_DICT=OFF \
      -DUSE_SYSTEMD=OFF -DBUILD_SHARED_LIBS=OFF -DEVENT_LOOP_BACKEND=none \
      -DLibIntl_INCLUDE_DIR=$PREBUILT/libintl-lite/arm64-v8a/include \
      -DLibIntl_LIBRARY=$PREBUILT/libintl-lite/arm64-v8a/lib/libintl.a
    ninja install

### B. Boost CMake folders without the version suffix

In `$PREBUILT/boost/arm64-v8a/lib/cmake`, for every folder `boost_<name>-1.90.0` (and `-1.90.0-static`) make a
folder `boost_<name>` holding a copy of its `*.cmake` files.

### C. pkg-config file for zstd

`$BUILD/pkgconfig/libzstd.pc`:

    Name: zstd
    Description: fast lossless compression algorithm library
    Version: 1.5.5
    Libs: -L/tmp/libime_scratch/prebuilt/zstd/arm64-v8a/lib -lzstd
    Cflags: -I/tmp/libime_scratch/prebuilt/zstd/arm64-v8a/include

This is the file as it was used for the build, line for line. Its `Version: 1.5.5` line does not state the
version of the library: the library in `zstd/arm64-v8a` is zstd 1.5.7 (`zstd.h`).

### D. libime (libIMECore.a, libIMEPinyin.a)

Copy `libpinyin-jni/libime-wrapper/CMakeLists.txt` to `$BUILD/libime-wrapper/CMakeLists.txt`, then:

    mkdir -p $BUILD/libime-wrapper-build && cd $BUILD/libime-wrapper-build
    PKG_CONFIG_PATH=$BUILD/pkgconfig cmake $BUILD/libime-wrapper -GNinja \
      -DCMAKE_TOOLCHAIN_FILE=$NDK/build/cmake/android.toolchain.cmake \
      -DANDROID_ABI=arm64-v8a -DANDROID_PLATFORM=android-26 -DANDROID_STL=c++_shared \
      -DANDROID_SUPPORT_FLEXIBLE_PAGE_SIZES=ON -DCMAKE_CXX_STANDARD=17 -DCMAKE_BUILD_TYPE=Release \
      -DCMAKE_INSTALL_PREFIX=$INSTALL "-DCMAKE_PREFIX_PATH=$INSTALL;$PREBUILT/boost/arm64-v8a" \
      -DCMAKE_FIND_ROOT_PATH_MODE_PACKAGE=BOTH -DCMAKE_CXX_FLAGS='-fexperimental-library' \
      -DECM_DIR=/usr/share/ECM/cmake -DFcitx5Utils_DIR=$INSTALL/lib/cmake/Fcitx5Utils \
      -DLibIntl_DIR=$PREBUILT/libintl-lite/arm64-v8a/lib/cmake \
      -DPREBUILT_DIR=$PREBUILT -DLIBIME_SRC_DIR=$S/libime-src
    ninja IMECore IMEPinyin

### E. Link and strip

    LB=$BUILD/libime-wrapper-build
    $NDK/toolchains/llvm/prebuilt/linux-x86_64/bin/aarch64-linux-android26-clang++ \
      -std=c++20 -fexperimental-library -shared -fPIC -O2 \
      -o libpinyin-jni.so pinyin_wrapper.cpp \
      -I$S/libime-src/src -I$LB/libime-build/src -I$LB/libime-build \
      -I$INSTALL/include/Fcitx5/Utils -I$PREBUILT/boost/include -I$PREBUILT/zstd/arm64-v8a/include \
      $LB/lib/libIMEPinyin.a $LB/lib/libIMECore.a $INSTALL/lib/libFcitx5Utils.a \
      $PREBUILT/boost/arm64-v8a/lib/libboost_iostreams.a $PREBUILT/boost/arm64-v8a/lib/libboost_container.a \
      $PREBUILT/zstd/arm64-v8a/lib/libzstd.a $PREBUILT/marisa/arm64-v8a/lib/libmarisa.a \
      $PREBUILT/libintl-lite/arm64-v8a/lib/libintl.a \
      -Wl,-z,max-page-size=16384 -llog
    $NDK/toolchains/llvm/prebuilt/linux-x86_64/bin/llvm-strip --strip-unneeded libpinyin-jni.so

The byte-identical result was obtained with the bridge at `$BUILD/pinyin-wrapper-fixed/pinyin_wrapper.cpp` and
the output in `$BUILD/pinyin-wrapper-fixed/arm64-v8a/`, with the extra link option
`-Wl,-Map=<output folder>/libpinyin-jni.map`, and with `S=/tmp/libime_scratch` (absolute paths of the build tree
are part of the compiler input).

The library needs `libc++_shared.so` of the same NDK next to it; the app ships it.

### Using a rebuilt library with the app

Replace `lib/arm64-v8a/libpinyin-jni.so` (or `libkenlm-jni.so`) inside the app package and sign the package again
with your own key; Android installs only signed packages, and a package signed with another key cannot update
the copy installed from Google Play (it must be installed as a fresh install).
