/*
 * pinyin_wrapper.cpp - FASE 2.1
 * JNI bridge: com.alessandro.kaiboard.predict.PinyinNative
 * Plan A: opaque handle (jlong) -> PinyinHolder { PinyinIME, PinyinContext }
 */

#include <android/log.h>
#include <jni.h>
#include <memory>
#include <string>

#include <libime/core/userlanguagemodel.h>
#include <libime/pinyin/pinyincontext.h>
#include <libime/pinyin/pinyindictionary.h>
#include <libime/pinyin/pinyinime.h>

#define TAG "PinyinNative"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO,  TAG, __VA_ARGS__)
#define LOGD(...) __android_log_print(ANDROID_LOG_DEBUG, TAG, __VA_ARGS__)
#define LOGW(...) __android_log_print(ANDROID_LOG_WARN,  TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, TAG, __VA_ARGS__)

// ---------------------------------------------------------------------------
// Internal state holder — one per nativeInit call
// ---------------------------------------------------------------------------
struct PinyinHolder {
    std::unique_ptr<libime::PinyinIME>     ime;
    std::unique_ptr<libime::PinyinContext> ctx;
};

static inline PinyinHolder *fromHandle(jlong h) {
    return reinterpret_cast<PinyinHolder *>(static_cast<uintptr_t>(h));
}

static std::string jstr(JNIEnv *env, jstring js) {
    if (!js) return {};
    const char *c = env->GetStringUTFChars(js, nullptr);
    std::string s(c);
    env->ReleaseStringUTFChars(js, c);
    return s;
}

// ---------------------------------------------------------------------------
// JNI exports
// ---------------------------------------------------------------------------
extern "C" {

// nativeInit(dictPath: String, lmPath: String): Long (0 = failure)
JNIEXPORT jlong JNICALL
Java_com_alessandro_kaiboard_predict_PinyinNative_nativeInit(
        JNIEnv *env, jclass /*clazz*/,
        jstring jDictPath, jstring jLmPath) {
    std::string dictPath = jstr(env, jDictPath);
    std::string lmPath   = jstr(env, jLmPath);
    LOGI("nativeInit: dict=%s lm=%s", dictPath.c_str(), lmPath.c_str());

    auto *p = new (std::nothrow) PinyinHolder();
    if (!p) { LOGE("nativeInit: OOM"); return 0L; }

    try {
        auto dict = std::make_unique<libime::PinyinDictionary>();
        auto lm   = std::make_unique<libime::UserLanguageModel>(lmPath.c_str());
        p->ime = std::make_unique<libime::PinyinIME>(std::move(dict), std::move(lm));
        p->ime->dict()->load(libime::PinyinDictionary::SystemDict,
                             dictPath.c_str(),
                             libime::PinyinDictFormat::Binary);
        p->ctx = std::make_unique<libime::PinyinContext>(p->ime.get());
        LOGI("nativeInit: OK handle=%p", (void*)p);
    } catch (const std::exception &e) {
        LOGE("nativeInit: exception: %s", e.what());
        delete p; return 0L;
    } catch (...) {
        LOGE("nativeInit: unknown exception");
        delete p; return 0L;
    }
    return static_cast<jlong>(reinterpret_cast<uintptr_t>(p));
}

// nativeReset(h: Long) — ctx.clear()
JNIEXPORT void JNICALL
Java_com_alessandro_kaiboard_predict_PinyinNative_nativeReset(
        JNIEnv * /*env*/, jclass /*clazz*/, jlong h) {
    auto *p = fromHandle(h);
    if (!p || !p->ctx) { LOGW("nativeReset: null handle"); return; }
    p->ctx->clear();
}

// nativeType(h: Long, latin: String) — ctx.type(latin)
JNIEXPORT void JNICALL
Java_com_alessandro_kaiboard_predict_PinyinNative_nativeType(
        JNIEnv *env, jclass /*clazz*/, jlong h, jstring jLatin) {
    auto *p = fromHandle(h);
    if (!p || !p->ctx) { LOGW("nativeType: null handle"); return; }
    p->ctx->type(jstr(env, jLatin));
}

// nativeGetCandidates(h: Long): Array<String>
JNIEXPORT jobjectArray JNICALL
Java_com_alessandro_kaiboard_predict_PinyinNative_nativeGetCandidates(
        JNIEnv *env, jclass /*clazz*/, jlong h) {
    jclass sc = env->FindClass("java/lang/String");
    auto *p = fromHandle(h);
    if (!p || !p->ctx) return env->NewObjectArray(0, sc, nullptr);

    const auto &cands = p->ctx->candidates();
    jobjectArray arr = env->NewObjectArray((jsize)cands.size(), sc, nullptr);
    if (!arr) { LOGE("nativeGetCandidates: NewObjectArray failed"); return nullptr; }

    jsize idx = 0;
    for (const auto &c : cands) {
        jstring js = env->NewStringUTF(c.toString().c_str());
        env->SetObjectArrayElement(arr, idx++, js);
        env->DeleteLocalRef(js);
    }
    LOGD("nativeGetCandidates: %zu candidates", cands.size());
    return arr;
}

// nativeSelect(h: Long, index: Int) — ctx.select(index)
JNIEXPORT void JNICALL
Java_com_alessandro_kaiboard_predict_PinyinNative_nativeSelect(
        JNIEnv * /*env*/, jclass /*clazz*/, jlong h, jint index) {
    auto *p = fromHandle(h);
    if (!p || !p->ctx) { LOGW("nativeSelect: null handle"); return; }
    p->ctx->select(static_cast<size_t>(index));
    LOGD("nativeSelect: index=%d selected=%s", (int)index,
         p->ctx->selected() ? "true" : "false");
}

// nativeIsComplete(h: Long): Boolean — ctx.selected()
JNIEXPORT jboolean JNICALL
Java_com_alessandro_kaiboard_predict_PinyinNative_nativeIsComplete(
        JNIEnv * /*env*/, jclass /*clazz*/, jlong h) {
    auto *p = fromHandle(h);
    if (!p || !p->ctx) return JNI_FALSE;
    return p->ctx->selected() ? JNI_TRUE : JNI_FALSE;
}

// nativeCommit(h: Long): String — ctx.selectedSentence()
JNIEXPORT jstring JNICALL
Java_com_alessandro_kaiboard_predict_PinyinNative_nativeCommit(
        JNIEnv *env, jclass /*clazz*/, jlong h) {
    auto *p = fromHandle(h);
    if (!p || !p->ctx) return env->NewStringUTF("");
    const std::string &s = p->ctx->selectedSentence();
    return env->NewStringUTF(s.c_str());
}

// nativeLearn(h: Long) — ctx.learn()
JNIEXPORT void JNICALL
Java_com_alessandro_kaiboard_predict_PinyinNative_nativeLearn(
        JNIEnv * /*env*/, jclass /*clazz*/, jlong h) {
    auto *p = fromHandle(h);
    if (!p || !p->ctx) { LOGW("nativeLearn: null handle"); return; }
    p->ctx->learn();
    LOGD("nativeLearn: done");
}

// nativeBackspace(h: Long) — erase last preedit char via ctx.erase()
JNIEXPORT void JNICALL
Java_com_alessandro_kaiboard_predict_PinyinNative_nativeBackspace(
        JNIEnv * /*env*/, jclass /*clazz*/, jlong h) {
    auto *p = fromHandle(h);
    if (!p || !p->ctx) { LOGW("nativeBackspace: null handle"); return; }
    size_t sz = p->ctx->size();
    if (sz == 0) { LOGD("nativeBackspace: empty preedit"); return; }
    p->ctx->erase(sz - 1, sz);
    LOGD("nativeBackspace: new size=%zu", p->ctx->size());
}

// nativeRelease(h: Long) — destroy PinyinHolder and all owned resources
JNIEXPORT void JNICALL
Java_com_alessandro_kaiboard_predict_PinyinNative_nativeRelease(
        JNIEnv * /*env*/, jclass /*clazz*/, jlong h) {
    auto *p = fromHandle(h);
    if (!p) { LOGW("nativeRelease: null handle"); return; }
    LOGI("nativeRelease: freeing handle=%p", (void*)p);
    delete p;
}

} // extern "C"
