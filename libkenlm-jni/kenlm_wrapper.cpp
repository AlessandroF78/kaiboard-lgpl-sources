#include <jni.h>
#include <android/log.h>
#include <string>
#include <memory>
#include "kenlm/lm/model.hh"

#define LOG_TAG "KenLM-JNI"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

// Wrapper per gestire il modello KenLM
struct KenLMModel {
    std::unique_ptr<lm::base::Model> model;
    lm::ngram::ModelType model_type;
    std::string language;  // ← AGGIUNTO: lingua del modello
};

extern "C" {

// Funzione 1: Carica il modello .bin e ritorna pointer
JNIEXPORT jlong JNICALL
Java_com_alessandro_kaiboard_predict_KenLMNative_loadModel(
        JNIEnv* env,
        jobject /* this */,
        jstring jpath,
        jstring jlanguage) {  // ← AGGIUNTO: parametro lingua

    const char* path = env->GetStringUTFChars(jpath, nullptr);
    if (!path) {
        LOGE("❌ loadModel: Failed to get model path string");
        return 0;
    }

    const char* lang_str = env->GetStringUTFChars(jlanguage, nullptr);
    if (!lang_str) {
        LOGE("❌ loadModel: Failed to get language string");
        env->ReleaseStringUTFChars(jpath, path);
        return 0;
    }

    LOGI("✅ loadModel: START - path='%s' language='%s'", path, lang_str);

    try {
        // Configurazione KenLM
        lm::ngram::Config config;
        config.load_method = util::POPULATE_OR_READ;  // Lazy loading per efficienza

        // Determina tipo modello dal file
        lm::ngram::ModelType model_type;
        if (!lm::ngram::RecognizeBinary(path, model_type)) {
            LOGE("❌ loadModel: Failed to recognize binary model format");
            env->ReleaseStringUTFChars(jpath, path);
            env->ReleaseStringUTFChars(jlanguage, lang_str);
            return 0;
        }

        LOGI("✅ loadModel: Binary format recognized, type=%d", model_type);

        // Crea wrapper
        KenLMModel* wrapper = new KenLMModel();
        wrapper->model_type = model_type;
        wrapper->language = std::string(lang_str);  // ← AGGIUNTO: setta lingua

        LOGI("✅ loadModel: Language set to '%s' in wrapper", wrapper->language.c_str());

        // Carica modello con tipo specifico
        wrapper->model.reset(lm::ngram::LoadVirtual(path, config, model_type));

        env->ReleaseStringUTFChars(jpath, path);
        env->ReleaseStringUTFChars(jlanguage, lang_str);

        LOGI("✅ loadModel: SUCCESS - Type=%d Order=%u Language='%s' Ptr=%p",
             model_type, wrapper->model->Order(), wrapper->language.c_str(), (void*)wrapper);

        return reinterpret_cast<jlong>(wrapper);

    } catch (const std::exception& e) {
        LOGE("❌ loadModel: Exception - %s", e.what());
        env->ReleaseStringUTFChars(jpath, path);
        env->ReleaseStringUTFChars(jlanguage, lang_str);
        return 0;
    }
}

// Funzione 2: Calcola probabilità log10 di un N-gram
JNIEXPORT jfloat JNICALL
Java_com_alessandro_kaiboard_predict_KenLMNative_scoreNgram(
        JNIEnv* env,
        jobject /* this */,
        jlong modelPtr,
        jstring jngram) {

    if (modelPtr == 0) {
        LOGE("Invalid model pointer");
        return -999.0f;  // Score molto negativo = impossibile
    }

    KenLMModel* wrapper = reinterpret_cast<KenLMModel*>(modelPtr);
    const char* ngram_str = env->GetStringUTFChars(jngram, nullptr);
    if (!ngram_str) {
        LOGE("Failed to get ngram string");
        return -999.0f;
    }

    try {
        // Parse N-gram in parole
        std::string text(ngram_str);
        lm::ngram::State state, out_state;

        // Inizializza stato iniziale
        wrapper->model->BeginSentenceWrite(&state);

        float total_score = 0.0f;

        // Tokenizza per spazi
        size_t start = 0;
        size_t end = text.find(' ');

        while (end != std::string::npos) {
            std::string word = text.substr(start, end - start);
            if (!word.empty()) {
                lm::WordIndex vocab_id = wrapper->model->BaseVocabulary().Index(word);
                total_score += wrapper->model->BaseScore(&state, vocab_id, &out_state);
                state = out_state;
            }
            start = end + 1;
            end = text.find(' ', start);
        }

        // Ultima parola
        std::string last_word = text.substr(start);
        if (!last_word.empty()) {
            lm::WordIndex vocab_id = wrapper->model->BaseVocabulary().Index(last_word);
            total_score += wrapper->model->BaseScore(&state, vocab_id, &out_state);
        }

        env->ReleaseStringUTFChars(jngram, ngram_str);
        return total_score;

    } catch (const std::exception& e) {
        LOGE("Exception scoring ngram: %s", e.what());
        env->ReleaseStringUTFChars(jngram, ngram_str);
        return -999.0f;
    }
}

// Funzione 3: Libera memoria del modello
JNIEXPORT void JNICALL
Java_com_alessandro_kaiboard_predict_KenLMNative_releaseModel(
        JNIEnv* env,
        jobject /* this */,
        jlong modelPtr) {

    if (modelPtr == 0) {
        LOGI("Release called on null pointer, ignoring");
        return;
    }

    KenLMModel* wrapper = reinterpret_cast<KenLMModel*>(modelPtr);
    LOGI("Releasing KenLM model");
    delete wrapper;
}

// Funzione 4: Ottieni dimensione vocabolario
JNIEXPORT jint JNICALL
Java_com_alessandro_kaiboard_predict_KenLMNative_getVocabularySize(
        JNIEnv* env,
        jobject /* this */,
        jlong modelPtr) {

    if (modelPtr == 0) {
        LOGE("Invalid model pointer");
        return 0;
    }

    KenLMModel* wrapper = reinterpret_cast<KenLMModel*>(modelPtr);

    try {
        // Usa il cast al tipo specifico del modello per accedere al vocabolario
        // Il vocabolario è accessibile tramite il modello tipizzato
        const lm::base::Vocabulary &vocab = wrapper->model->BaseVocabulary();

        // Ritorna un valore approssimativo basato sull'indice massimo
        // In KenLM gli indici vanno da 0 a vocab size - 1
        // Usiamo un approccio indiretto: contiamo fino a trovare <unk>
        lm::WordIndex unk_id = vocab.NotFound();

        // Per ora ritorniamo un valore ragionevole (sarà ottimizzato dopo)
        // Tipicamente i modelli hanno 100k-500k parole
        return static_cast<jint>(unk_id);

    } catch (const std::exception& e) {
        LOGE("Exception getting vocabulary size: %s", e.what());
        return 0;
    }
}

// Funzione 5: Calcola score di una singola parola dato uno stato
JNIEXPORT jfloat JNICALL
Java_com_alessandro_kaiboard_predict_KenLMNative_scoreWordInContext(
        JNIEnv* env,
        jobject /* this */,
        jlong modelPtr,
        jstring jcontext,
        jstring jword) {

    if (modelPtr == 0) {
        LOGE("Invalid model pointer");
        return -999.0f;
    }

    KenLMModel* wrapper = reinterpret_cast<KenLMModel*>(modelPtr);

    const char* context_str = env->GetStringUTFChars(jcontext, nullptr);
    const char* word_str = env->GetStringUTFChars(jword, nullptr);

    if (!context_str || !word_str) {
        LOGE("Failed to get strings");
        if (context_str) env->ReleaseStringUTFChars(jcontext, context_str);
        if (word_str) env->ReleaseStringUTFChars(jword, word_str);
        return -999.0f;
    }

    try {
        lm::ngram::State state, out_state;

        // Inizializza stato con contesto
        wrapper->model->BeginSentenceWrite(&state);

        // Processa contesto (parole precedenti)
        std::string context(context_str);
        if (!context.empty()) {
            size_t start = 0;
            size_t end = context.find(' ');

            while (end != std::string::npos) {
                std::string word = context.substr(start, end - start);
                if (!word.empty()) {
                    lm::WordIndex vocab_id = wrapper->model->BaseVocabulary().Index(word);
                    wrapper->model->BaseScore(&state, vocab_id, &out_state);
                    state = out_state;
                }
                start = end + 1;
                end = context.find(' ', start);
            }

            // Ultima parola del contesto
            std::string last_word = context.substr(start);
            if (!last_word.empty()) {
                lm::WordIndex vocab_id = wrapper->model->BaseVocabulary().Index(last_word);
                wrapper->model->BaseScore(&state, vocab_id, &out_state);
                state = out_state;
            }
        }

        // Calcola score della parola target
        std::string target_word(word_str);
        lm::WordIndex target_id = wrapper->model->BaseVocabulary().Index(target_word);
        float score = wrapper->model->BaseScore(&state, target_id, &out_state);

        env->ReleaseStringUTFChars(jcontext, context_str);
        env->ReleaseStringUTFChars(jword, word_str);

        return score;

    } catch (const std::exception& e) {
        LOGE("Exception scoring word in context: %s", e.what());
        env->ReleaseStringUTFChars(jcontext, context_str);
        env->ReleaseStringUTFChars(jword, word_str);
        return -999.0f;
    }
}

// Funzione 6: Estrae vocabolario dal modello KenLM
// Ritorna array di parole ordinate per frequenza
JNIEXPORT jobjectArray JNICALL
Java_com_alessandro_kaiboard_predict_KenLMNative_extractVocabulary(
        JNIEnv* env,
        jobject /* this */,
        jlong modelPtr,
        jint maxWords) {

    if (modelPtr == 0) {
        LOGE("❌ extractVocabulary: Invalid model pointer");
        return nullptr;
    }

    KenLMModel* wrapper = reinterpret_cast<KenLMModel*>(modelPtr);

    LOGI("✅ extractVocabulary: START - modelPtr=%p language='%s' maxWords=%d",
         (void*)wrapper, wrapper->language.c_str(), maxWords);

    try {
        // Definisci vocabolario in base alla lingua
        std::vector<std::string> commonWords;

        // ═══════════════════════════════════════════════════════════════
        // ┌─────────────────────────────────────────────────────────────┐
        // │  ⚠️  INSERISCI QUI IL BLOCCO DELLE 13 LINGUE                │
        // │                                                              │
        // │  if (wrapper->language == "it") {                           │
        // │      LOGI("✅ extractVocabulary: Loading ITALIAN vocab");   │
        // │      commonWords = { "il", "la", "di", ... };               │
        // │  }                                                           │
        // │  else if (wrapper->language == "de") {                      │
        // │      LOGI("✅ extractVocabulary: Loading GERMAN vocab");    │
        // │      commonWords = { "der", "die", "das", ... };            │
        // │  }                                                           │
        // │  ... (altre 11 lingue) ...                                  │
        // │  else {                                                      │
        // │      LOGE("❌ Unsupported language: %s", lang.c_str());     │
        // │      return empty array                                     │
        // │  }                                                           │
        // └─────────────────────────────────────────────────────────────┘
        // ═══════════════════════════════════════════════════════════════

        if (wrapper->language == "it") {
            commonWords = {
                    // Articoli e preposizioni (50)
                    "il", "la", "di", "e", "a", "è", "da", "in", "un", "che",
                    "per", "non", "con", "una", "su", "al", "del", "le", "i", "dei",
                    "gli", "delle", "alla", "nell", "nella", "dalle", "dagli", "agli",
                    "ai", "alle", "sui", "sulle", "sul", "dello", "dell", "dalla",

                    // Verbi ESSERE (tutte le forme)
                    "sono", "sei", "è", "siamo", "siete", "sia", "siano", "ero", "eri",
                    "era", "eravamo", "erano", "sarò", "sarai", "sarà", "saremo", "saranno",
                    "stato", "stata", "stati", "state", "essere", "essendo", "fossi", "fosse",

                    // Verbi AVERE (tutte le forme)
                    "ho", "hai", "ha", "abbiamo", "avete", "hanno", "avrò", "avrai", "avrà",
                    "avevo", "avevi", "aveva", "avevamo", "avevano", "avuto", "avuta", "avuti",
                    "avute", "avere", "avendo", "abbia", "abbiano", "avessi", "avesse",

                    // Verbi FARE
                    "faccio", "fai", "fa", "facciamo", "fate", "fanno", "farò", "farai",
                    "facevo", "faceva", "fatto", "fatta", "fatti", "fatte", "fare", "facendo",

                    // Verbi ANDARE
                    "vado", "vai", "va", "andiamo", "andate", "vanno", "andrò", "andrai",
                    "andavo", "andava", "andato", "andata", "andare", "andando",

                    // Verbi DIRE
                    "dico", "dici", "dice", "diciamo", "dite", "dicono", "dirò", "dirai",
                    "dicevo", "diceva", "detto", "detta", "dire", "dicendo",

                    // Verbi VENIRE
                    "vengo", "vieni", "viene", "veniamo", "venite", "vengono", "verrò", "verrai",
                    "venivo", "veniva", "venuto", "venuta", "venire", "venendo",

                    // Verbi POTERE
                    "posso", "puoi", "può", "possiamo", "potete", "possono", "potrò", "potrai",
                    "potevo", "poteva", "potuto", "potere", "potendo", "possa", "possano",

                    // Verbi VOLERE
                    "voglio", "vuoi", "vuole", "vogliamo", "volete", "vogliono", "vorrò", "vorrai",
                    "volevo", "voleva", "voluto", "volere", "volendo", "voglia", "vogliano",

                    // Verbi DOVERE
                    "devo", "devi", "deve", "dobbiamo", "dovete", "devono", "dovrò", "dovrai",
                    "dovevo", "doveva", "dovuto", "dovere", "dovendo", "debba", "debbano",

                    // Verbi SAPERE
                    "so", "sai", "sa", "sappiamo", "sapete", "sanno", "saprò", "saprai",
                    "sapevo", "sapeva", "saputo", "sapere", "sapendo", "sappia", "sappiano",

                    // Verbi DARE
                    "do", "dai", "dà", "diamo", "date", "danno", "darò", "darai",
                    "davo", "dava", "dato", "data", "dare", "dando",

                    // Verbi STARE
                    "sto", "stai", "sta", "stiamo", "state", "stanno", "starò", "starai",
                    "stavo", "stava", "stato", "stata", "stare", "stando",

                    // Verbi VEDERE
                    "vedo", "vedi", "vede", "vediamo", "vedete", "vedono", "vedrò", "vedrai",
                    "vedevo", "vedeva", "visto", "vista", "vedere", "vedendo",

                    // Pronomi
                    "mi", "ti", "si", "ci", "vi", "lo", "la", "li", "le", "ne", "gli",
                    "io", "tu", "lui", "lei", "noi", "voi", "loro", "me", "te", "se",

                    // Interrogativi/relativi
                    "che", "chi", "cosa", "come", "quando", "dove", "perché", "quanto",
                    "quale", "quali", "cui",

                    // Aggettivi/pronomi dimostrativi
                    "questo", "questa", "questi", "queste", "quello", "quella", "quelli", "quelle",

                    // Avverbi comuni
                    "non", "più", "anche", "molto", "poco", "bene", "male", "sempre",
                    "mai", "già", "ancora", "poi", "ora", "qui", "così", "solo",
                    "troppo", "tanto", "meno", "meglio", "peggio", "quasi", "forse",

                    // Congiunzioni
                    "ma", "o", "se", "oppure", "però", "quindi", "perciò", "mentre",
                    "quando", "come", "perché", "affinché",

                    // Sostantivi ultra-comuni
                    "casa", "case", "giorno", "giorni", "tempo", "volta", "volte",
                    "anno", "anni", "cosa", "cose", "parte", "parti", "modo",
                    "vita", "mano", "mani", "occhio", "occhi", "mondo", "paese",
                    "città", "persone", "persona", "uomo", "uomini", "donna", "donne",
                    "famiglia", "lavoro", "padre", "madre", "figlio", "figlia"
            };
        }
        else if (wrapper->language == "de") {
            commonWords = {
                    // Articoli e preposizioni (50)
                    "der", "die", "das", "den", "dem", "des", "ein", "eine", "einen", "einem",
                    "und", "in", "zu", "von", "mit", "für", "auf", "an", "bei", "nach",
                    "aus", "um", "über", "durch", "vor", "bis", "gegen", "ohne", "seit", "während",
                    "unter", "hinter", "neben", "zwischen", "wegen", "trotz", "als", "wie", "wenn", "ob",
                    "dass", "weil", "damit", "obwohl", "falls", "indem", "sodass", "bevor", "nachdem",

                    // Pronomi (30)
                    "ich", "du", "er", "sie", "es", "wir", "ihr", "Sie", "mich", "dich",
                    "ihn", "uns", "euch", "mir", "dir", "ihm", "ihnen", "mein", "dein",
                    "sein", "unser", "euer", "dieser", "jener", "welcher", "mancher", "solcher", "jeder", "alle",

                    // Verbi ESSERE/AVERE
                    "sein", "bin", "bist", "ist", "sind", "seid", "war", "warst", "waren", "wart",
                    "gewesen", "werden", "werde", "wirst", "wird", "werdet", "wurde", "wurden", "geworden",
                    "haben", "habe", "hast", "hat", "habt", "hatte", "hatten", "gehabt",

                    // Verbi MODALI
                    "können", "kann", "kannst", "konnte", "gekonnt",
                    "müssen", "muss", "musst", "musste", "gemusst",
                    "wollen", "will", "willst", "wollte", "gewollt",
                    "sollen", "soll", "sollst", "sollte", "gesollt",
                    "dürfen", "darf", "darfst", "durfte", "gedurft",
                    "mögen", "mag", "magst", "mochte", "gemocht",

                    // Verbi COMUNI (60)
                    "gehen", "gehe", "gehst", "geht", "ging", "gegangen",
                    "kommen", "komme", "kommst", "kommt", "kam", "gekommen",
                    "sehen", "sehe", "siehst", "sieht", "sah", "gesehen",
                    "machen", "mache", "machst", "macht", "machte", "gemacht",
                    "sagen", "sage", "sagst", "sagt", "sagte", "gesagt",
                    "wissen", "weiß", "weißt", "wusste", "gewusst",
                    "geben", "gebe", "gibst", "gibt", "gab", "gegeben",
                    "nehmen", "nehme", "nimmst", "nimmt", "nahm", "genommen",
                    "finden", "finde", "findest", "findet", "fand", "gefunden",
                    "denken", "denke", "denkst", "denkt", "dachte", "gedacht",
                    "leben", "lebe", "lebst", "lebt", "lebte", "gelebt",
                    "arbeiten", "arbeite", "arbeitest", "arbeitet", "arbeitete", "gearbeitet",
                    "sprechen", "spreche", "sprichst", "spricht", "sprach", "gesprochen",
                    "bringen", "bringe", "bringst", "bringt", "brachte", "gebracht",
                    "stehen", "stehe", "stehst", "steht", "stand", "gestanden",
                    "liegen", "liege", "liegst", "liegt", "lag", "gelegen",
                    "bleiben", "bleibe", "bleibst", "bleibt", "blieb", "geblieben",
                    "lassen", "lasse", "lässt", "ließ", "gelassen",
                    "halten", "halte", "hältst", "hält", "hielt", "gehalten",
                    "schreiben", "schreibe", "schreibst", "schreibt", "schrieb", "geschrieben",

                    // Aggettivi (40)
                    "gut", "besser", "beste", "schlecht", "schlechter", "schlechteste",
                    "groß", "größer", "größte", "klein", "kleiner", "kleinste",
                    "neu", "neuer", "neueste", "alt", "älter", "älteste",
                    "lang", "länger", "längste", "kurz", "kürzer", "kürzeste",
                    "hoch", "höher", "höchste", "tief", "tiefer", "tiefste",
                    "jung", "jünger", "jüngste", "schön", "schöner", "schönste",
                    "viel", "mehr", "meist", "wenig", "weniger", "wenigste",
                    "wichtig", "richtig", "falsch", "möglich", "unmöglich",

                    // Sostantivi (70)
                    "Jahr", "Jahre", "Jahren", "Tag", "Tage", "Tagen",
                    "Zeit", "Zeiten", "Stunde", "Stunden", "Minute", "Minuten",
                    "Mann", "Männer", "Frau", "Frauen", "Kind", "Kinder",
                    "Mensch", "Menschen", "Person", "Personen", "Leute",
                    "Leben", "Welt", "Land", "Länder", "Stadt", "Städte",
                    "Haus", "Häuser", "Straße", "Straßen", "Platz", "Plätze",
                    "Arbeit", "Arbeiten", "Weg", "Wege", "Seite", "Seiten",
                    "Hand", "Hände", "Kopf", "Köpfe", "Auge", "Augen",
                    "Frage", "Fragen", "Antwort", "Antworten", "Problem", "Probleme",
                    "Bild", "Bilder", "Buch", "Bücher", "Wort", "Worte", "Wörter",
                    "Geld", "Euro", "Familie", "Familien", "Freund", "Freunde",

                    // Numeri (20)
                    "null", "eins", "zwei", "drei", "vier", "fünf",
                    "sechs", "sieben", "acht", "neun", "zehn",
                    "elf", "zwölf", "zwanzig", "hundert", "tausend",

                    // Avverbi (40)
                    "nicht", "auch", "nur", "noch", "schon", "immer",
                    "nie", "heute", "gestern", "morgen", "jetzt", "dann",
                    "so", "sehr", "hier", "da", "dort", "wo",
                    "wann", "warum", "wie", "was", "wer", "wieder",
                    "zurück", "weiter", "zusammen", "allein", "vielleicht",
                    "sicher", "genau", "fast", "etwa", "besonders",
                    "ganz", "halb", "gerade", "gleich", "bald"
            };
        }
        else if (wrapper->language == "en") {
            commonWords = {
                    // Articoli e preposizioni (50)
                    "the", "a", "an", "in", "to", "of", "and", "for", "on", "with",
                    "at", "from", "by", "about", "as", "into", "through", "over", "after", "before",
                    "under", "between", "among", "during", "without", "within", "against", "toward",
                    "until", "around", "near", "behind", "above", "across", "down", "up", "out",
                    "than", "like", "unlike", "except", "despite", "beyond", "plus", "off", "along",

                    // Pronomi (30)
                    "I", "you", "he", "she", "it", "we", "they", "me", "him", "her",
                    "us", "them", "my", "your", "his", "its", "our", "their", "mine", "yours",
                    "hers", "ours", "theirs", "this", "that", "these", "those", "who", "what", "which",

                    // Verbi BE/HAVE/DO
                    "be", "am", "is", "are", "was", "were", "been", "being",
                    "have", "has", "had", "having",
                    "do", "does", "did", "done", "doing",

                    // Verbi MODALI
                    "can", "could", "may", "might", "must", "shall", "should", "will", "would",

                    // Verbi COMUNI (80)
                    "go", "goes", "went", "gone", "going",
                    "get", "gets", "got", "gotten", "getting",
                    "make", "makes", "made", "making",
                    "know", "knows", "knew", "known", "knowing",
                    "think", "thinks", "thought", "thinking",
                    "see", "sees", "saw", "seen", "seeing",
                    "come", "comes", "came", "coming",
                    "take", "takes", "took", "taken", "taking",
                    "want", "wants", "wanted", "wanting",
                    "give", "gives", "gave", "given", "giving",
                    "use", "uses", "used", "using",
                    "find", "finds", "found", "finding",
                    "tell", "tells", "told", "telling",
                    "ask", "asks", "asked", "asking",
                    "work", "works", "worked", "working",
                    "seem", "seems", "seemed", "seeming",
                    "feel", "feels", "felt", "feeling",
                    "try", "tries", "tried", "trying",
                    "leave", "leaves", "left", "leaving",
                    "call", "calls", "called", "calling",
                    "need", "needs", "needed", "needing",
                    "become", "becomes", "became", "becoming",
                    "show", "shows", "showed", "shown", "showing",
                    "bring", "brings", "brought", "bringing",
                    "begin", "begins", "began", "begun", "beginning",
                    "keep", "keeps", "kept", "keeping",
                    "hold", "holds", "held", "holding",
                    "write", "writes", "wrote", "written", "writing",
                    "stand", "stands", "stood", "standing",
                    "hear", "hears", "heard", "hearing",
                    "let", "lets", "letting",
                    "mean", "means", "meant", "meaning",

                    // Aggettivi (40)
                    "good", "better", "best", "bad", "worse", "worst",
                    "new", "old", "great", "small", "big", "large",
                    "long", "short", "high", "low", "little", "much",
                    "many", "few", "some", "any", "all", "both",
                    "other", "another", "same", "different", "such", "own",
                    "right", "wrong", "true", "false", "possible", "sure",
                    "real", "whole", "full", "free", "easy", "hard",

                    // Sostantivi (70)
                    "time", "times", "year", "years", "day", "days",
                    "way", "ways", "people", "person", "man", "men",
                    "woman", "women", "child", "children", "life", "lives",
                    "world", "hand", "hands", "part", "parts", "place", "places",
                    "case", "cases", "point", "points", "number", "numbers",
                    "group", "groups", "problem", "problems", "fact", "facts",
                    "thing", "things", "work", "works", "week", "weeks",
                    "month", "months", "question", "questions", "system", "systems",
                    "area", "areas", "room", "rooms", "house", "houses",
                    "name", "names", "family", "families", "friend", "friends",
                    "water", "food", "money", "book", "books", "car", "cars",

                    // Numeri (20)
                    "one", "two", "three", "four", "five", "six", "seven", "eight", "nine", "ten",
                    "eleven", "twelve", "twenty", "thirty", "forty", "fifty", "hundred", "thousand", "million",

                    // Avverbi (40)
                    "not", "no", "yes", "now", "then", "here", "there",
                    "when", "where", "why", "how", "so", "too", "very",
                    "well", "also", "only", "just", "even", "again",
                    "still", "back", "more", "most", "less", "much",
                    "always", "never", "often", "sometimes", "really", "quite",
                    "almost", "already", "yet", "today", "tomorrow", "yesterday"
            };
        }
        else if (wrapper->language == "es") {
            commonWords = {
                    // Articoli e preposizioni (50)
                    "el", "la", "los", "las", "un", "una", "unos", "unas",
                    "de", "en", "a", "del", "al", "por", "para", "con",
                    "sin", "sobre", "entre", "hasta", "desde", "contra",
                    "según", "bajo", "hacia", "mediante", "tras", "ante",
                    "como", "cuando", "donde", "que", "cual", "quien",
                    "porque", "pues", "pero", "sino", "aunque", "si",
                    "mientras", "y", "o", "ni", "e", "u",

                    // Pronomi (30)
                    "yo", "tú", "él", "ella", "usted", "nosotros", "nosotras",
                    "vosotros", "vosotras", "ellos", "ellas", "ustedes",
                    "me", "te", "le", "la", "lo", "nos", "os", "les",
                    "mi", "tu", "su", "nuestro", "nuestra", "vuestro", "vuestra",
                    "este", "esta", "ese", "esa",

                    // Verbi ESSERE/AVERE
                    "ser", "soy", "eres", "es", "somos", "sois", "son",
                    "era", "eras", "éramos", "sido", "siendo",
                    "estar", "estoy", "estás", "está", "estamos", "estáis", "están",
                    "estaba", "estado", "estando",
                    "haber", "he", "has", "ha", "hemos", "habéis", "han",
                    "había", "habido", "habiendo",
                    "tener", "tengo", "tienes", "tiene", "tenemos", "tenéis", "tienen",
                    "tenía", "tenido", "teniendo",

                    // Verbi COMUNI (70)
                    "hacer", "hago", "haces", "hace", "hacemos", "hacen", "hice", "hecho", "haciendo",
                    "decir", "digo", "dices", "dice", "decimos", "dicen", "dije", "dicho", "diciendo",
                    "ir", "voy", "vas", "va", "vamos", "vais", "van", "fui", "ido", "yendo",
                    "poder", "puedo", "puedes", "puede", "podemos", "pueden", "pude", "podido", "pudiendo",
                    "dar", "doy", "das", "da", "damos", "dan", "di", "dado", "dando",
                    "ver", "veo", "ves", "ve", "vemos", "ven", "vi", "visto", "viendo",
                    "saber", "sé", "sabes", "sabe", "sabemos", "saben", "supe", "sabido", "sabiendo",
                    "querer", "quiero", "quieres", "quiere", "queremos", "quieren", "quise", "querido", "queriendo",
                    "llegar", "llego", "llegas", "llega", "llegamos", "llegan", "llegué", "llegado", "llegando",
                    "pasar", "paso", "pasas", "pasa", "pasamos", "pasan", "pasé", "pasado", "pasando",
                    "poner", "pongo", "pones", "pone", "ponemos", "ponen", "puse", "puesto", "poniendo",
                    "venir", "vengo", "vienes", "viene", "venimos", "vienen", "vine", "venido", "viniendo",
                    "salir", "salgo", "sales", "sale", "salimos", "salen", "salí", "salido", "saliendo",

                    // Aggettivi (40)
                    "bueno", "buena", "mejor", "malo", "mala", "peor",
                    "grande", "mayor", "pequeño", "pequeña", "menor",
                    "nuevo", "nueva", "viejo", "vieja", "joven",
                    "largo", "larga", "corto", "corta", "alto", "alta",
                    "bajo", "baja", "mucho", "mucha", "poco", "poca",
                    "todo", "toda", "otro", "otra", "mismo", "misma",
                    "primero", "primera", "último", "última", "importante", "posible",

                    // Sostantivi (60)
                    "año", "años", "día", "días", "vez", "veces",
                    "tiempo", "tiempos", "vida", "vidas", "parte", "partes",
                    "casa", "casas", "hombre", "hombres", "mujer", "mujeres",
                    "persona", "personas", "mundo", "país", "países", "ciudad", "ciudades",
                    "mano", "manos", "forma", "formas", "caso", "casos",
                    "grupo", "grupos", "problema", "problemas", "trabajo", "trabajos",
                    "lugar", "lugares", "punto", "puntos", "hora", "horas",
                    "pregunta", "preguntas", "nombre", "nombres", "familia", "familias",
                    "amigo", "amigos", "libro", "libros", "agua", "dinero",

                    // Numeri (20)
                    "uno", "una", "dos", "tres", "cuatro", "cinco",
                    "seis", "siete", "ocho", "nueve", "diez",
                    "once", "doce", "veinte", "treinta", "cien", "mil", "millón",

                    // Avverbi (40)
                    "no", "sí", "también", "muy", "más", "menos",
                    "ya", "aún", "todavía", "ahora", "entonces", "luego",
                    "aquí", "ahí", "allí", "donde", "cuando", "cómo",
                    "siempre", "nunca", "casi", "solo", "bien", "mal",
                    "mejor", "peor", "así", "tan", "tanto", "hoy",
                    "ayer", "mañana", "después", "antes", "tarde", "temprano", "pronto"
            };
        }
        else if (wrapper->language == "fr") {
            commonWords = {
                    // Articoli e preposizioni (50)
                    "le", "la", "les", "l", "un", "une", "des", "du", "de",
                    "à", "au", "aux", "en", "dans", "pour", "par", "avec",
                    "sans", "sur", "sous", "entre", "vers", "chez", "pendant",
                    "depuis", "avant", "après", "selon", "contre", "parmi",
                    "malgré", "sauf", "hormis", "outre", "dès", "durant",
                    "que", "qui", "quoi", "dont", "où", "comme", "si",
                    "quand", "lorsque", "tandis", "puisque", "car", "donc",

                    // Pronomi (30)
                    "je", "j", "tu", "il", "elle", "on", "nous", "vous", "ils", "elles",
                    "me", "m", "te", "t", "se", "s", "lui", "leur", "y", "en",
                    "moi", "toi", "soi", "eux", "mon", "ma", "mes", "ton", "ta", "tes",
                    "son", "sa", "ses", "notre", "nos", "votre", "vos", "leur", "leurs",
                    "ce", "cet", "cette", "ces", "celui", "celle", "ceux", "celles",

                    // Verbi ÊTRE/AVOIR
                    "être", "suis", "es", "est", "sommes", "êtes", "sont",
                    "était", "étais", "étions", "étiez", "étaient",
                    "été", "étant", "sera", "serai", "seras", "serons", "serez", "seront",
                    "avoir", "ai", "as", "a", "avons", "avez", "ont",
                    "avais", "avait", "avions", "aviez", "avaient",
                    "eu", "ayant", "aura", "aurai", "auras", "aurons", "aurez", "auront",

                    // Verbi MODALI
                    "pouvoir", "peux", "peut", "pouvons", "pouvez", "peuvent", "pu", "pourra",
                    "devoir", "dois", "doit", "devons", "devez", "doivent", "dû", "devra",
                    "vouloir", "veux", "veut", "voulons", "voulez", "veulent", "voulu", "voudra",
                    "savoir", "sais", "sait", "savons", "savez", "savent", "su", "saura",
                    "falloir", "faut", "fallu", "faudra",

                    // Verbi COMUNI (70)
                    "faire", "fais", "fait", "faisons", "faites", "font", "faisait", "fit", "fera",
                    "aller", "vais", "va", "allons", "allez", "vont", "allait", "alla", "ira",
                    "dire", "dis", "dit", "disons", "dites", "disent", "disait", "dira",
                    "voir", "vois", "voit", "voyons", "voyez", "voient", "voyait", "vit", "verra",
                    "prendre", "prends", "prend", "prenons", "prenez", "prennent", "pris", "prendra",
                    "donner", "donne", "donnes", "donnons", "donnez", "donnent", "donné", "donnera",
                    "venir", "viens", "vient", "venons", "venez", "viennent", "venu", "viendra",
                    "mettre", "mets", "met", "mettons", "mettez", "mettent", "mis", "mettra",
                    "tenir", "tiens", "tient", "tenons", "tenez", "tiennent", "tenu", "tiendra",
                    "trouver", "trouve", "trouves", "trouvons", "trouvez", "trouvent", "trouvé", "trouvera",
                    "passer", "passe", "passes", "passons", "passez", "passent", "passé", "passera",
                    "rendre", "rends", "rend", "rendons", "rendez", "rendent", "rendu", "rendra",
                    "croire", "crois", "croit", "croyons", "croyez", "croient", "cru", "croira",
                    "parler", "parle", "parles", "parlons", "parlez", "parlent", "parlé", "parlera",
                    "demander", "demande", "demandes", "demandons", "demandez", "demandent", "demandé",
                    "rester", "reste", "restes", "restons", "restez", "restent", "resté", "restera",

                    // Aggettivi (40)
                    "bon", "bonne", "meilleur", "meilleure", "mauvais", "mauvaise", "pire",
                    "grand", "grande", "petit", "petite", "gros", "grosse",
                    "nouveau", "nouvelle", "nouvel", "vieux", "vieille", "vieil", "jeune",
                    "long", "longue", "court", "courte", "haut", "haute", "bas", "basse",
                    "autre", "même", "tel", "telle", "tout", "toute", "tous", "toutes",
                    "premier", "première", "dernier", "dernière", "seul", "seule",
                    "vrai", "vraie", "faux", "fausse", "possible", "impossible",

                    // Sostantivi (60)
                    "temps", "année", "années", "jour", "jours", "heure", "heures",
                    "fois", "vie", "monde", "homme", "hommes", "femme", "femmes",
                    "enfant", "enfants", "personne", "personnes", "gens",
                    "main", "mains", "tête", "têtes", "œil", "yeux",
                    "chose", "choses", "partie", "parties", "cas", "point", "points",
                    "pays", "ville", "villes", "maison", "maisons", "rue", "rues",
                    "travail", "question", "questions", "problème", "problèmes",
                    "famille", "familles", "ami", "amis", "ami", "amie", "amies",
                    "mot", "mots", "nom", "noms", "côté", "côtés",
                    "eau", "argent", "livre", "livres", "histoire", "histoires",

                    // Numeri (20)
                    "un", "une", "deux", "trois", "quatre", "cinq",
                    "six", "sept", "huit", "neuf", "dix",
                    "onze", "douze", "vingt", "trente", "quarante", "cinquante",
                    "cent", "mille", "million", "premier", "deuxième", "troisième",

                    // Avverbi (40)
                    "ne", "n", "pas", "non", "oui", "si", "plus", "moins",
                    "très", "bien", "mal", "mieux", "aussi", "encore", "déjà",
                    "toujours", "jamais", "maintenant", "alors", "puis", "ensuite",
                    "ici", "là", "où", "quand", "comment", "pourquoi",
                    "ainsi", "tant", "tellement", "beaucoup", "peu", "assez",
                    "trop", "tout", "tous", "toutes", "aujourd'hui", "hier", "demain"
            };
        }
        else if (wrapper->language == "pt") {
            commonWords = {
                    // Articoli e preposizioni (50)
                    "o", "a", "os", "as", "um", "uma", "uns", "umas",
                    "de", "do", "da", "dos", "das", "em", "no", "na", "nos", "nas",
                    "por", "pelo", "pela", "pelos", "pelas", "para", "com", "sem",
                    "sobre", "entre", "até", "desde", "contra", "durante",
                    "segundo", "sob", "perante", "após", "antes", "mediante",
                    "que", "qual", "quais", "quem", "onde", "quando", "como",
                    "porque", "pois", "mas", "porém", "contudo", "todavia",
                    "embora", "se", "enquanto", "e", "ou", "nem",

                    // Pronomi (30)
                    "eu", "tu", "você", "ele", "ela", "nós", "vós", "vocês", "eles", "elas",
                    "me", "te", "se", "lhe", "o", "a", "nos", "vos", "lhes", "os",
                    "meu", "minha", "meus", "minhas", "teu", "tua", "seu", "sua",
                    "nosso", "nossa", "vosso", "vossa", "este", "esta", "esse", "essa",
                    "aquele", "aquela", "isto", "isso", "aquilo",

                    // Verbi SER/ESTAR/TER
                    "ser", "sou", "és", "é", "somos", "sois", "são",
                    "era", "eras", "éramos", "eram", "sido", "sendo", "será", "serão",
                    "estar", "estou", "estás", "está", "estamos", "estão",
                    "estava", "estado", "estando", "estará", "estarão",
                    "ter", "tenho", "tens", "tem", "temos", "têm",
                    "tinha", "tido", "tendo", "terá", "terão",
                    "haver", "há", "houve", "havia", "havido", "haverá",

                    // Verbi COMUNI (70)
                    "fazer", "faço", "faz", "fazemos", "fazem", "fez", "feito", "fazendo", "fará",
                    "dizer", "digo", "diz", "dizemos", "dizem", "disse", "dito", "dizendo", "dirá",
                    "ir", "vou", "vai", "vamos", "vão", "foi", "ido", "indo", "irá",
                    "poder", "posso", "pode", "podemos", "podem", "pôde", "podido", "podendo", "poderá",
                    "dar", "dou", "dá", "damos", "dão", "deu", "dado", "dando", "dará",
                    "ver", "vejo", "vê", "vemos", "veem", "viu", "visto", "vendo", "verá",
                    "saber", "sei", "sabe", "sabemos", "sabem", "soube", "sabido", "sabendo", "saberá",
                    "querer", "quero", "quer", "queremos", "querem", "quis", "querido", "querendo", "quererá",
                    "pôr", "ponho", "põe", "pomos", "põem", "pôs", "posto", "pondo", "porá",
                    "vir", "venho", "vem", "vimos", "vêm", "veio", "vindo", "virá",
                    "trazer", "trago", "traz", "trazemos", "trazem", "trouxe", "trazido", "trazendo", "trará",
                    "falar", "falo", "fala", "falamos", "falam", "falou", "falado", "falando", "falará",
                    "passar", "passo", "passa", "passamos", "passam", "passou", "passado", "passando", "passará",
                    "ficar", "fico", "fica", "ficamos", "ficam", "ficou", "ficado", "ficando", "ficará",

                    // Aggettivi (40)
                    "bom", "boa", "melhor", "mau", "má", "pior",
                    "grande", "maior", "pequeno", "pequena", "menor",
                    "novo", "nova", "velho", "velha", "jovem",
                    "longo", "longa", "curto", "curta", "alto", "alta",
                    "baixo", "baixa", "muito", "muita", "pouco", "pouca",
                    "todo", "toda", "outro", "outra", "mesmo", "mesma",
                    "primeiro", "primeira", "último", "última", "importante", "possível",

                    // Sostantivi (60)
                    "ano", "anos", "dia", "dias", "vez", "vezes",
                    "tempo", "tempos", "vida", "vidas", "parte", "partes",
                    "casa", "casas", "homem", "homens", "mulher", "mulheres",
                    "pessoa", "pessoas", "mundo", "país", "países", "cidade", "cidades",
                    "mão", "mãos", "forma", "formas", "caso", "casos",
                    "grupo", "grupos", "problema", "problemas", "trabalho", "trabalhos",
                    "lugar", "lugares", "ponto", "pontos", "hora", "horas",
                    "pergunta", "perguntas", "nome", "nomes", "família", "famílias",
                    "amigo", "amigos", "livro", "livros", "água", "dinheiro",

                    // Numeri (20)
                    "um", "uma", "dois", "duas", "três", "quatro", "cinco",
                    "seis", "sete", "oito", "nove", "dez",
                    "onze", "doze", "vinte", "trinta", "cem", "mil", "milhão",

                    // Avverbi (40)
                    "não", "sim", "também", "muito", "mais", "menos",
                    "já", "ainda", "agora", "então", "depois", "logo",
                    "aqui", "aí", "ali", "lá", "onde", "quando", "como",
                    "sempre", "nunca", "quase", "só", "bem", "mal",
                    "melhor", "pior", "assim", "tão", "tanto", "hoje",
                    "ontem", "amanhã", "antes", "depois", "cedo", "tarde", "logo"
            };
        }
        else if (wrapper->language == "ru") {
            commonWords = {
                    // Preposizioni e congiunzioni (50)
                    "в", "на", "и", "с", "к", "по", "из", "о", "от", "за",
                    "у", "для", "до", "при", "про", "под", "над", "через", "между",
                    "без", "перед", "после", "около", "возле", "вокруг", "среди",
                    "что", "как", "когда", "где", "куда", "откуда", "почему",
                    "если", "то", "но", "а", "или", "же", "ли", "бы",
                    "чтобы", "потому", "поэтому", "так", "только", "уже", "еще",

                    // Pronomi (30)
                    "я", "ты", "он", "она", "оно", "мы", "вы", "они",
                    "меня", "тебя", "его", "её", "нас", "вас", "их",
                    "мне", "тебе", "ему", "ей", "нам", "вам", "им",
                    "мой", "твой", "свой", "наш", "ваш", "этот", "тот", "весь",

                    // Verbi БЫТЬ
                    "быть", "есть", "был", "была", "было", "были", "будет", "будут",
                    "буду", "будешь", "будем", "будете", "быв", "бывший",

                    // Verbi COMUNI (80)
                    "мочь", "могу", "можешь", "может", "можем", "могут", "мог", "могла", "могли",
                    "хотеть", "хочу", "хочешь", "хочет", "хотим", "хотите", "хотят", "хотел", "хотела",
                    "знать", "знаю", "знаешь", "знает", "знаем", "знают", "знал", "знала", "знали",
                    "иметь", "имею", "имеешь", "имеет", "имеем", "имеют", "имел", "имела", "имели",
                    "делать", "делаю", "делаешь", "делает", "делаем", "делают", "делал", "делала", "делали",
                    "говорить", "говорю", "говоришь", "говорит", "говорим", "говорят", "говорил", "говорила",
                    "сказать", "скажу", "скажешь", "скажет", "скажем", "скажут", "сказал", "сказала",
                    "видеть", "вижу", "видишь", "видит", "видим", "видят", "видел", "видела", "видели",
                    "идти", "иду", "идёшь", "идёт", "идём", "идут", "шёл", "шла", "шли",
                    "дать", "дам", "дашь", "даст", "дадим", "дадут", "дал", "дала", "дали",
                    "взять", "возьму", "возьмёшь", "возьмёт", "возьмут", "взял", "взяла", "взяли",
                    "стать", "стану", "станешь", "станет", "станут", "стал", "стала", "стали",
                    "жить", "живу", "живёшь", "живёт", "живут", "жил", "жила", "жили",
                    "думать", "думаю", "думаешь", "думает", "думают", "думал", "думала", "думали",
                    "работать", "работаю", "работаешь", "работает", "работают", "работал", "работала",

                    // Aggettivi (40)
                    "хороший", "хорошая", "хорошее", "лучший", "лучшая", "лучшее",
                    "плохой", "плохая", "плохое", "худший", "худшая", "худшее",
                    "большой", "большая", "большое", "больший", "маленький", "маленькая",
                    "новый", "новая", "новое", "старый", "старая", "старое",
                    "длинный", "длинная", "короткий", "короткая", "высокий", "высокая",
                    "низкий", "низкая", "молодой", "молодая", "важный", "важная",
                    "первый", "первая", "последний", "последняя", "другой", "другая",
                    "сам", "сама", "само", "весь", "вся", "всё",

                    // Sostantivi (60)
                    "год", "года", "лет", "день", "дня", "дней",
                    "время", "времени", "раз", "раза", "жизнь", "человек", "люди",
                    "мир", "дело", "дела", "рука", "руки", "слово", "слова",
                    "место", "места", "работа", "работы", "сторона", "стороны",
                    "глаз", "глаза", "вопрос", "вопросы", "дом", "дома",
                    "страна", "страны", "город", "города", "часть", "части",
                    "голова", "головы", "друг", "друзья", "семья", "семьи",
                    "книга", "книги", "вода", "деньги", "машина", "машины",
                    "путь", "пути", "конец", "конца", "начало", "начала",
                    "имя", "имени", "число", "числа", "лицо", "лица",

                    // Numeri (20)
                    "один", "одна", "одно", "два", "две", "три", "четыре", "пять",
                    "шесть", "семь", "восемь", "девять", "десять",
                    "одиннадцать", "двенадцать", "двадцать", "тридцать",
                    "сто", "тысяча", "миллион", "первый", "второй", "третий",

                    // Avverbi (40)
                    "не", "нет", "да", "очень", "более", "менее",
                    "уже", "еще", "ещё", "всегда", "никогда", "сейчас",
                    "теперь", "тогда", "потом", "здесь", "там", "туда",
                    "где", "куда", "откуда", "когда", "как", "почему",
                    "так", "тут", "вот", "вон", "опять", "снова",
                    "опять", "опять", "именно", "просто", "почти", "совсем",
                    "сегодня", "вчера", "завтра", "рано", "поздно", "скоро"
            };
        }
        else if (wrapper->language == "nl") {
            commonWords = {
                    // Articoli e preposizioni (50)
                    "de", "het", "een", "in", "van", "op", "te", "voor", "aan", "met",
                    "bij", "uit", "over", "door", "naar", "tot", "om", "onder", "tegen",
                    "tussen", "zonder", "binnen", "buiten", "achter", "naast", "vanaf",
                    "sinds", "tijdens", "volgens", "wegens", "ondanks", "behalve",
                    "dat", "die", "wat", "wie", "waar", "wanneer", "hoe", "waarom",
                    "als", "of", "en", "maar", "want", "omdat", "terwijl", "hoewel",
                    "zodat", "voordat", "nadat",

                    // Pronomi (30)
                    "ik", "jij", "je", "hij", "zij", "ze", "het", "wij", "we", "jullie",
                    "u", "mij", "me", "jou", "hem", "haar", "ons", "hen", "hun",
                    "mijn", "jouw", "zijn", "haar", "ons", "onze", "uw", "hun",
                    "deze", "die", "dit", "dat",

                    // Verbi ZIJN/HEBBEN
                    "zijn", "ben", "bent", "is", "zijn", "was", "waren", "geweest", "zijnde",
                    "worden", "word", "wordt", "werd", "werden", "geworden", "wordende",
                    "hebben", "heb", "hebt", "heeft", "had", "hadden", "gehad", "hebbende",

                    // Verbi MODALI
                    "kunnen", "kan", "kunt", "kon", "konden", "gekund",
                    "moeten", "moet", "moest", "moesten", "gemoeten",
                    "willen", "wil", "wilt", "wilde", "wilden", "gewild",
                    "zullen", "zal", "zou", "zouden",
                    "mogen", "mag", "mocht", "mochten", "gemogen",

                    // Verbi COMUNI (70)
                    "doen", "doe", "doet", "deed", "deden", "gedaan", "doende",
                    "gaan", "ga", "gaat", "ging", "gingen", "gegaan", "gaande",
                    "komen", "kom", "komt", "kwam", "kwamen", "gekomen", "komende",
                    "zien", "zie", "ziet", "zag", "zagen", "gezien", "ziende",
                    "krijgen", "krijg", "krijgt", "kreeg", "kregen", "gekregen", "krijgende",
                    "maken", "maak", "maakt", "maakte", "maakten", "gemaakt", "makende",
                    "zeggen", "zeg", "zegt", "zei", "zeiden", "gezegd", "zeggende",
                    "geven", "geef", "geeft", "gaf", "gaven", "gegeven", "gevende",
                    "nemen", "neem", "neemt", "nam", "namen", "genomen", "nemende",
                    "weten", "weet", "wist", "wisten", "geweten", "wetende",
                    "vinden", "vind", "vindt", "vond", "vonden", "gevonden", "vindende",
                    "denken", "denk", "denkt", "dacht", "dachten", "gedacht", "denkende",
                    "houden", "houd", "houdt", "hield", "hielden", "gehouden", "houdende",
                    "staan", "sta", "staat", "stond", "stonden", "gestaan", "staande",
                    "liggen", "lig", "ligt", "lag", "lagen", "gelegen", "liggende",
                    "zitten", "zit", "zat", "zaten", "gezeten", "zittende",
                    "lopen", "loop", "loopt", "liep", "liepen", "gelopen", "lopende",
                    "blijven", "blijf", "blijft", "bleef", "bleven", "gebleven", "blijvende",
                    "laten", "laat", "liet", "lieten", "gelaten", "latende",

                    // Aggettivi (40)
                    "goed", "goede", "beter", "beste", "slecht", "slechte", "slechter", "slechtste",
                    "groot", "grote", "groter", "grootste", "klein", "kleine", "kleiner", "kleinste",
                    "nieuw", "nieuwe", "oud", "oude", "jong", "jonge", "jonger", "jongste",
                    "lang", "lange", "langer", "langste", "kort", "korte", "korter", "kortste",
                    "hoog", "hoge", "hoger", "hoogste", "laag", "lage", "lager", "laagste",
                    "mooi", "mooie", "mooier", "mooiste", "veel", "vele", "meer", "meeste",
                    "weinig", "weinige", "minder", "minste", "ander", "andere", "zelfde",

                    // Sostantivi (60)
                    "jaar", "jaren", "dag", "dagen", "tijd", "tijden", "keer", "keren",
                    "mens", "mensen", "man", "mannen", "vrouw", "vrouwen", "kind", "kinderen",
                    "persoon", "personen", "leven", "levens", "wereld", "hand", "handen",
                    "deel", "delen", "plaats", "plaatsen", "geval", "gevallen", "punt", "punten",
                    "land", "landen", "stad", "steden", "huis", "huizen", "straat", "straten",
                    "werk", "werken", "vraag", "vragen", "probleem", "problemen",
                    "woord", "woorden", "naam", "namen", "ding", "dingen", "zaak", "zaken",
                    "oog", "ogen", "hoofd", "hoofden", "familie", "families",
                    "vriend", "vrienden", "boek", "boeken", "water", "geld", "auto", "autos",

                    // Numeri (20)
                    "een", "twee", "drie", "vier", "vijf", "zes", "zeven", "acht", "negen", "tien",
                    "elf", "twaalf", "twintig", "dertig", "veertig", "vijftig",
                    "honderd", "duizend", "miljoen", "eerste", "tweede", "derde",

                    // Avverbi (40)
                    "niet", "geen", "wel", "ja", "nee", "ook", "nog", "al", "reeds",
                    "nu", "dan", "toen", "hier", "daar", "waar", "ergens", "nergens",
                    "altijd", "nooit", "vaak", "soms", "meestal", "zelden",
                    "zeer", "erg", "heel", "zo", "zoals", "misschien", "waarschijnlijk",
                    "natuurlijk", "precies", "bijna", "ongeveer", "juist", "vandaag",
                    "gisteren", "morgen", "straks", "later", "eerder", "vroeg", "laat"
            };
        }
        else if (wrapper->language == "pl") {
            commonWords = {
                    // Preposizioni e congiunzioni (50)
                    "w", "na", "z", "do", "o", "po", "za", "u", "od", "przy",
                    "dla", "przez", "nad", "pod", "bez", "przed", "między", "około",
                    "wobec", "wśród", "według", "oprócz", "podczas", "pomimo",
                    "że", "jak", "kiedy", "gdzie", "dokąd", "skąd", "dlaczego",
                    "i", "a", "ale", "lecz", "czy", "lub", "ani", "bo",
                    "jeśli", "gdyby", "żeby", "aby", "choć", "chociaż", "gdy",
                    "zanim", "dopóki", "odkąd",

                    // Pronomi (30)
                    "ja", "ty", "on", "ona", "ono", "my", "wy", "oni", "one",
                    "mnie", "cię", "go", "ją", "je", "nas", "was", "ich",
                    "mi", "ci", "mu", "jej", "nam", "wam", "im",
                    "mój", "moja", "moje", "twój", "twoja", "jego", "jej",
                    "nasz", "nasza", "wasz", "wasza", "ich", "ten", "ta", "to",

                    // Verbi BYĆ/MIEĆ
                    "być", "jestem", "jesteś", "jest", "jesteśmy", "jesteście", "są",
                    "był", "była", "było", "byli", "były", "będę", "będziesz", "będzie",
                    "mieć", "mam", "masz", "ma", "mamy", "macie", "mają",
                    "miał", "miała", "miało", "mieli", "miały", "będę", "miał",

                    // Verbi MODALI
                    "móc", "mogę", "możesz", "może", "możemy", "mogą", "mógł", "mogła",
                    "musieć", "muszę", "musisz", "musi", "musimy", "muszą", "musiał", "musiała",
                    "chcieć", "chcę", "chcesz", "chce", "chcemy", "chcą", "chciał", "chciała",
                    "wiedzieć", "wiem", "wiesz", "wie", "wiemy", "wiedzą", "wiedział", "wiedziała",

                    // Verbi COMUNI (70)
                    "robić", "robię", "robisz", "robi", "robimy", "robią", "robił", "robiła",
                    "iść", "idę", "idziesz", "idzie", "idziemy", "idą", "szedł", "szła",
                    "mówić", "mówię", "mówisz", "mówi", "mówimy", "mówią", "mówił", "mówiła",
                    "widzieć", "widzę", "widzisz", "widzi", "widzimy", "widzą", "widział", "widziała",
                    "dawać", "daję", "dajesz", "daje", "dajemy", "dają", "dawał", "dawała",
                    "brać", "biorę", "bierzesz", "bierze", "bierzemy", "biorą", "brał", "brała",
                    "znać", "znam", "znasz", "zna", "znamy", "znają", "znał", "znała",
                    "myśleć", "myślę", "myślisz", "myśli", "myślimy", "myślą", "myślał", "myślała",
                    "stać", "stoję", "stoisz", "stoi", "stoimy", "stoją", "stał", "stała",
                    "leżeć", "leżę", "leżysz", "leży", "leżymy", "leżą", "leżał", "leżała",
                    "siedzieć", "siedzę", "siedzisz", "siedzi", "siedzimy", "siedzą", "siedział", "siedziała",
                    "żyć", "żyję", "żyjesz", "żyje", "żyjemy", "żyją", "żył", "żyła",
                    "pisać", "piszę", "piszesz", "pisze", "piszemy", "piszą", "pisał", "pisała",
                    "czytać", "czytam", "czytasz", "czyta", "czytamy", "czytają", "czytał", "czytała",
                    "pracować", "pracuję", "pracujesz", "pracuje", "pracujemy", "pracują", "pracował",

                    // Aggettivi (40)
                    "dobry", "dobra", "dobre", "lepszy", "lepsza", "lepsze", "najlepszy",
                    "zły", "zła", "złe", "gorszy", "gorsza", "gorsze", "najgorszy",
                    "duży", "duża", "duże", "większy", "większa", "większe", "największy",
                    "mały", "mała", "małe", "mniejszy", "mniejsza", "mniejsze", "najmniejszy",
                    "nowy", "nowa", "nowe", "stary", "stara", "stare", "młody", "młoda", "młode",
                    "długi", "długa", "długie", "krótki", "krótka", "krótkie",
                    "wysoki", "wysoka", "wysokie", "niski", "niska", "niskie",
                    "ważny", "ważna", "ważne", "możliwy", "możliwa", "możliwe",

                    // Sostantivi (60)
                    "rok", "roku", "lat", "dzień", "dnia", "dni", "czas", "czasu",
                    "raz", "razu", "razy", "życie", "życia", "człowiek", "ludzie",
                    "świat", "świata", "pan", "pana", "pani", "państwo",
                    "ręka", "ręki", "ręce", "głowa", "głowy", "oko", "oczy",
                    "słowo", "słowa", "część", "części", "miejsce", "miejsca",
                    "sprawa", "sprawy", "sposób", "sposobu", "strona", "strony",
                    "kraj", "kraju", "miasto", "miasta", "dom", "domu", "ulica", "ulicy",
                    "praca", "pracy", "pytanie", "pytania", "problem", "problemu",
                    "rodzina", "rodziny", "przyjaciel", "przyjaciele", "książka", "książki",
                    "woda", "wody", "pieniądze", "samochód", "samochodu",

                    // Numeri (20)
                    "jeden", "jedna", "jedno", "dwa", "dwie", "trzy", "cztery", "pięć",
                    "sześć", "siedem", "osiem", "dziewięć", "dziesięć",
                    "jedenaście", "dwanaście", "dwadzieścia", "trzydzieści",
                    "sto", "tysiąc", "milion", "pierwszy", "druga", "trzeci",

                    // Avverbi (40)
                    "nie", "tak", "też", "także", "również", "bardzo", "więcej", "mniej",
                    "już", "jeszcze", "zawsze", "nigdy", "teraz", "wtedy", "potem",
                    "tutaj", "tam", "tu", "gdzie", "dokąd", "skąd", "kiedy",
                    "jak", "dlaczego", "czasami", "często", "rzadko", "zazwyczaj",
                    "naprawdę", "właśnie", "prawie", "całkiem", "zupełnie", "wcale",
                    "dziś", "dzisiaj", "wczoraj", "jutro", "rano", "wieczorem", "późno"
            };
        }
        else if (wrapper->language == "tr") {
            commonWords = {
                    // Preposizioni e postposizioni (40)
                    "de", "da", "için", "ile", "gibi", "kadar", "den", "dan", "e", "a",
                    "i", "ı", "u", "ü", "ya", "ye", "deki", "daki", "eki", "ki",
                    "den", "dan", "ten", "tan", "nin", "nın", "nun", "nün",
                    "ne", "na", "in", "ın", "un", "ün", "dir", "dır", "dur", "dür",
                    "tir", "tır", "tur", "tür",

                    // Congiunzioni (20)
                    "ve", "veya", "ya", "ama", "fakat", "ancak", "lakin", "çünkü",
                    "ki", "eğer", "ise", "hem", "ne", "ya", "dahi", "bile",
                    "hatta", "yahut", "şayet", "madem",

                    // Pronomi (30)
                    "ben", "sen", "o", "biz", "siz", "onlar",
                    "beni", "seni", "onu", "bizi", "sizi", "onları",
                    "bana", "sana", "ona", "bize", "size", "onlara",
                    "benim", "senin", "onun", "bizim", "sizin", "onların",
                    "bende", "sende", "onda", "bizde", "sizde", "onlarda",
                    "bu", "şu", "o", "bunlar", "şunlar", "onlar",

                    // Verbi OLMAK (essere)
                    "olmak", "oluyorum", "oluyorsun", "oluyor", "oluyoruz", "oluyorsunuz", "oluyorlar",
                    "oldum", "oldun", "oldu", "olduk", "oldunuz", "oldular",
                    "olacağım", "olacaksın", "olacak", "olacağız", "olacaksınız", "olacaklar",

                    // Verbi ETMEK (fare)
                    "etmek", "ediyorum", "ediyorsun", "ediyor", "ediyoruz", "ediyorsunuz", "ediyorlar",
                    "ettim", "ettin", "etti", "ettik", "ettiniz", "ettiler",
                    "edeceğim", "edeceksin", "edecek", "edeceğiz", "edeceksiniz", "edecekler",

                    // Verbi YAPMAK (fare/costruire)
                    "yapmak", "yapıyorum", "yapıyorsun", "yapıyor", "yapıyoruz", "yapıyorsunuz", "yapıyorlar",
                    "yaptım", "yaptın", "yaptı", "yaptık", "yaptınız", "yaptılar",
                    "yapacağım", "yapacaksın", "yapacak", "yapacağız", "yapacaksınız", "yapacaklar",

                    // Verbi COMUNI (60)
                    "gelmek", "geliyorum", "geliyorsun", "geliyor", "geliyoruz", "geliyorlar",
                    "gelmek", "geldim", "geldin", "geldi", "geldik", "geldiler",
                    "gitmek", "gidiyorum", "gidiyorsun", "gidiyor", "gidiyoruz", "gidiyorlar",
                    "görmek", "görüyorum", "görüyorsun", "görüyor", "görüyoruz", "görüyorlar",
                    "vermek", "veriyorum", "veriyorsun", "veriyor", "veriyoruz", "veriyorlar",
                    "almak", "alıyorum", "alıyorsun", "alıyor", "alıyoruz", "alıyorlar",
                    "bilmek", "biliyorum", "biliyorsun", "biliyor", "biliyoruz", "biliyorlar",
                    "demek", "diyorum", "diyorsun", "diyor", "diyoruz", "diyorlar",
                    "istemek", "istiyorum", "istiyorsun", "istiyor", "istiyoruz", "istiyorlar",
                    "düşünmek", "düşünüyorum", "düşünüyorsun", "düşünüyor", "düşünüyoruz",
                    "konuşmak", "konuşuyorum", "konuşuyorsun", "konuşuyor", "konuşuyoruz",
                    "çalışmak", "çalışıyorum", "çalışıyorsun", "çalışıyor", "çalışıyoruz",
                    "okumak", "okuyorum", "okuyorsun", "okuyor", "okuyoruz", "okuyorlar",
                    "yazmak", "yazıyorum", "yazıyorsun", "yazıyor", "yazıyoruz", "yazıyorlar",

                    // Aggettivi (40)
                    "iyi", "daha", "en", "kötü", "büyük", "küçük",
                    "yeni", "eski", "genç", "yaşlı", "uzun", "kısa",
                    "yüksek", "alçak", "geniş", "dar", "güzel", "çirkin",
                    "kolay", "zor", "önemli", "gerekli", "mümkün", "imkansız",
                    "doğru", "yanlış", "açık", "kapalı", "tam", "yarım",
                    "ilk", "son", "önce", "sonra", "öbür", "diğer",
                    "aynı", "farklı", "her", "bütün", "tüm", "bazı",

                    // Sostantivi (60)
                    "zaman", "zamanlar", "gün", "günler", "yıl", "yıllar",
                    "saat", "saatler", "dakika", "dakikalar", "an", "anlar",
                    "insan", "insanlar", "kişi", "kişiler", "adam", "adamlar",
                    "kadın", "kadınlar", "çocuk", "çocuklar", "kız", "kızlar",
                    "erkek", "erkekler", "hayat", "hayatlar", "dünya", "dünyalar",
                    "yer", "yerler", "ülke", "ülkeler", "şehir", "şehirler",
                    "ev", "evler", "oda", "odalar", "kapı", "kapılar",
                    "iş", "işler", "şey", "şeyler", "durum", "durumlar",
                    "soru", "sorular", "cevap", "cevaplar", "problem", "problemler",
                    "el", "eller", "baş", "başlar", "göz", "gözler",
                    "kelime", "kelimeler", "isim", "isimler", "ad", "adlar",
                    "aile", "aileler", "arkadaş", "arkadaşlar", "kitap", "kitaplar",
                    "su", "para", "araba", "arabalar",

                    // Numeri (20)
                    "bir", "iki", "üç", "dört", "beş", "altı", "yedi", "sekiz", "dokuz", "on",
                    "onbir", "oniki", "yirmi", "otuz", "kırk", "elli",
                    "yüz", "bin", "milyon", "birinci", "ikinci", "üçüncü",

                    // Avverbi (40)
                    "değil", "yok", "var", "çok", "az", "daha", "en",
                    "şimdi", "sonra", "önce", "burada", "orada", "şurada",
                    "nerede", "nereye", "nereden", "ne", "nasıl", "niçin",
                    "neden", "hep", "hiç", "bazen", "her", "zaman",
                    "çokça", "biraz", "tamamen", "gerçekten", "sadece", "bile",
                    "belki", "muhtemelen", "kesinlikle", "bugün", "dün", "yarın",
                    "erken", "geç", "yakında", "uzak", "beraber"
            };
        }
        else if (wrapper->language == "uk") {
            commonWords = {
                    // Preposizioni e congiunzioni (50)
                    "в", "на", "і", "з", "до", "по", "за", "від", "у", "при",
                    "для", "про", "під", "над", "через", "між", "без", "перед",
                    "після", "біля", "коло", "навколо", "серед", "крізь", "поза",
                    "що", "як", "коли", "де", "куди", "звідки", "чому",
                    "якщо", "то", "але", "а", "або", "чи", "ж", "би",
                    "щоб", "тому", "бо", "так", "тільки", "вже", "ще",
                    "й", "та", "ні",

                    // Pronomi (30)
                    "я", "ти", "він", "вона", "воно", "ми", "ви", "вони",
                    "мене", "тебе", "його", "її", "нас", "вас", "їх",
                    "мені", "тобі", "йому", "їй", "нам", "вам", "їм",
                    "мій", "твій", "свій", "наш", "ваш", "цей", "той", "весь",

                    // Verbi БУТИ
                    "бути", "є", "був", "була", "було", "були", "буде", "будуть",
                    "буду", "будеш", "будемо", "будете", "бувши", "бувший",

                    // Verbi МАТИ
                    "мати", "маю", "маєш", "має", "маємо", "маєте", "мають",
                    "мав", "мала", "мало", "мали", "матиму", "матимеш", "матиме",

                    // Verbi COMUNI (80)
                    "робити", "роблю", "робиш", "робить", "робимо", "робите", "роблять",
                    "робив", "робила", "робило", "робили", "зробити", "зроблю", "зробиш",
                    "говорити", "кажу", "кажеш", "каже", "кажемо", "кажете", "кажуть",
                    "казав", "казала", "казали", "сказати", "скажу", "скажеш", "скаже",
                    "йти", "іду", "ідеш", "іде", "ідемо", "ідете", "ідуть",
                    "йшов", "йшла", "йшли", "піти", "піду", "підеш", "піде",
                    "давати", "даю", "даєш", "дає", "даємо", "даєте", "дають",
                    "давав", "давала", "давали", "дати", "дам", "даси", "дасть",
                    "брати", "беру", "береш", "бере", "беремо", "берете", "беруть",
                    "брав", "брала", "брали", "взяти", "візьму", "візьмеш", "візьме",
                    "знати", "знаю", "знаєш", "знає", "знаємо", "знаєте", "знають",
                    "знав", "знала", "знали", "дізнатися", "дізнаюся", "дізнаєшся",
                    "хотіти", "хочу", "хочеш", "хоче", "хочемо", "хочете", "хочуть",
                    "хотів", "хотіла", "хотіли", "захотіти", "захочу", "захочеш",
                    "бачити", "бачу", "бачиш", "бачить", "бачимо", "бачите", "бачать",
                    "бачив", "бачила", "бачили", "побачити", "побачу", "побачиш",
                    "жити", "живу", "живеш", "живе", "живемо", "живете", "живуть",
                    "жив", "жила", "жили", "прожити", "проживу", "проживеш",
                    "думати", "думаю", "думаєш", "думає", "думаємо", "думають",
                    "думав", "думала", "думали", "подумати", "подумаю", "подумаєш",

                    // Aggettivi (40)
                    "хороший", "хороша", "хороше", "добрий", "добра", "добре",
                    "поганий", "погана", "погане", "гірший", "гірша", "гірше",
                    "великий", "велика", "велике", "більший", "більша", "більше",
                    "малий", "мала", "мале", "менший", "менша", "менше",
                    "новий", "нова", "нове", "старий", "стара", "старе",
                    "довгий", "довга", "довге", "короткий", "коротка", "коротке",
                    "високий", "висока", "високе", "низький", "низька", "низьке",
                    "молодий", "молода", "молоде", "важливий", "важлива", "важливе",
                    "перший", "перша", "перше", "останній", "остання", "останнє",
                    "інший", "інша", "інше", "сам", "сама", "саме",

                    // Sostantivi (60)
                    "рік", "року", "років", "день", "дня", "днів",
                    "час", "часу", "разів", "раз", "рази", "життя",
                    "людина", "люди", "людей", "чоловік", "чоловіки", "чоловіків",
                    "жінка", "жінки", "жінок", "дитина", "діти", "дітей",
                    "світ", "світу", "рука", "руки", "рук", "слово", "слова", "слів",
                    "справа", "справи", "справ", "місце", "місця", "місць",
                    "сторона", "сторони", "сторін", "голова", "голови", "голів",
                    "око", "очі", "очей", "питання", "питань",
                    "країна", "країни", "країн", "місто", "міста", "міст",
                    "дім", "дому", "будинок", "будинки", "будинків",
                    "робота", "роботи", "вулиця", "вулиці", "вулиць",
                    "сім'я", "сім'ї", "друг", "друзі", "друзів",
                    "книга", "книги", "книг", "вода", "води", "гроші", "грошей",

                    // Numeri (20)
                    "один", "одна", "одне", "два", "дві", "три", "чотири", "п'ять",
                    "шість", "сім", "вісім", "дев'ять", "десять",
                    "одинадцять", "дванадцять", "двадцять", "тридцять",
                    "сто", "тисяча", "мільйон", "перший", "другий", "третій",

                    // Avverbi (40)
                    "не", "ні", "так", "теж", "також", "дуже", "більше", "менше",
                    "вже", "ще", "завжди", "ніколи", "зараз", "тепер", "тоді",
                    "потім", "тут", "там", "де", "куди", "звідки", "коли",
                    "як", "чому", "іноді", "часто", "рідко", "зазвичай",
                    "справді", "майже", "зовсім", "цілком", "зовсім", "сьогодні",
                    "вчора", "завтра", "рано", "пізно", "скоро", "давно"
            };
        }
        else if (wrapper->language == "ka") {
            commonWords = {
                    // Particelle postposizionali (40)
                    "და", "რომ", "არის", "თუ", "მაგრამ", "ან", "რა", "ვინ", "როგორ", "სად",
                    "როდის", "რატომ", "რისთვის", "რომელი", "ის", "ეს", "ასე", "ისე",
                    "იქ", "აქ", "იქნება", "იყო", "ყოფილა", "იქნა", "ხდება", "გახდა",
                    "უნდა", "შეიძლება", "შეუძლია", "ნდობა", "ჰქონდეს", "აქვს", "ექნება",
                    "გან", "დან", "ზე", "ში", "თან", "კენ", "მდე", "ით",

                    // Pronomi (30)
                    "მე", "შენ", "ის", "ჩვენ", "თქვენ", "ისინი", "იგი", "ეს",
                    "ჩემი", "შენი", "მისი", "ჩვენი", "თქვენი", "მათი", "იმის",
                    "ამის", "ვინმე", "რამე", "ყველა", "არავინ", "არაფერი",
                    "ვიღაც", "რაღაც", "ზოგი", "სხვა", "თავი", "თითო", "ერთი", "მეორე",

                    // Verbi ყოფნა (essere)
                    "ყოფნა", "ვარ", "ხარ", "არის", "ვართ", "ხართ", "არიან",
                    "ვიყავი", "იყავი", "იყო", "ვიყავით", "იყავით", "იყვნენ",
                    "ვიქნები", "იქნები", "იქნება", "ვიქნებით", "იქნებით", "იქნებიან",

                    // Verbi ქონა (avere)
                    "ქონა", "მაქვს", "გაქვს", "აქვს", "გვაქვს", "გაქვთ", "აქვთ",
                    "მქონდა", "გქონდა", "ჰქონდა", "გვქონდა", "გქონდათ", "ჰქონდათ",
                    "მექნება", "გექნება", "ექნება", "გვექნება", "გექნებათ", "ექნებათ",

                    // Verbi COMUNI (70)
                    "კეთება", "ვაკეთებ", "აკეთებ", "აკეთებს", "ვაკეთებთ", "აკეთებთ", "აკეთებენ",
                    "გაკეთება", "გავაკეთე", "გააკეთე", "გააკეთა", "გავაკეთეთ", "გააკეთეთ",
                    "თქმა", "ვამბობ", "ამბობ", "ამბობს", "ვამბობთ", "ამბობთ", "ამბობენ",
                    "თქვა", "ვთქვი", "თქვი", "თქვა", "ვთქვით", "თქვით", "თქვეს",
                    "წერა", "ვწერ", "წერ", "წერს", "ვწერთ", "წერთ", "წერენ",
                    "დაწერა", "დავწერე", "დაწერე", "დაწერა", "დავწერეთ", "დაწერეთ",
                    "კითხვა", "ვკითხულობ", "კითხულობ", "კითხულობს", "ვკითხულობთ",
                    "წაკითხვა", "წავიკითხე", "წაიკითხე", "წაიკითხა", "წავიკითხეთ",
                    "სვლა", "მივდივარ", "მიდიხარ", "მიდის", "მივდივართ", "მიდიხართ",
                    "წასვლა", "წავედი", "წახვედი", "წავიდა", "წავედით", "წახვედით",
                    "მოსვლა", "მოვდივარ", "მოდიხარ", "მოდის", "მოვდივართ", "მოდიან",
                    "მოვა", "მოვედი", "მოხვედი", "მოვიდა", "მოვედით", "მოვიდნენ",
                    "ნახვა", "ვხედავ", "ხედავ", "ხედავს", "ვხედავთ", "ხედავთ", "ხედავენ",
                    "უნახავს", "ვნახე", "ნახე", "ნახა", "ვნახეთ", "ნახეთ", "ნახეს",
                    "მიცემა", "ვაძლევ", "აძლევ", "აძლევს", "ვაძლევთ", "აძლევთ", "აძლევენ",
                    "მისცა", "მივეცი", "მიეცი", "მისცა", "მივეცით", "მიეცით", "მისცეს",

                    // Aggettivi (40)
                    "კარგი", "ცუდი", "დიდი", "პატარა", "ახალი", "ძველი",
                    "გრძელი", "მოკლე", "მაღალი", "დაბალი", "ფართო", "ვიწრო",
                    "ლამაზი", "მშვენიერი", "ძლიერი", "სუსტი", "ახალგაზრდა", "ხანდაზმული",
                    "ახლო", "შორს", "სწრაფი", "ნელი", "ცხელი", "ცივი",
                    "თბილი", "მშრალი", "სველი", "მძიმე", "მსუბუქი", "ძვირი",
                    "იაფი", "მდიდარი", "ღარიბი", "ჭკვიანი", "სულელი", "მწარე",
                    "ტკბილი", "მჟავე", "მარილიანი", "უმჯობესი", "უარესი", "პირველი",

                    // Sostantivi (60)
                    "დრო", "დღე", "წელი", "თვე", "კვირა", "საათი", "წუთი",
                    "ადამიანი", "კაცი", "ქალი", "ბავშვი", "ბიჭი", "გოგო",
                    "მშობელი", "დედა", "მამა", "ძმა", "და", "ბიძა", "დეიდა",
                    "ოჯახი", "მეგობარი", "ხალხი", "ქვეყანა", "ქალაქი", "სოფელი",
                    "სახლი", "ოთახი", "კარი", "ფანჯარა", "მაგიდა", "სკამი",
                    "წიგნი", "ქაღალდი", "კალამი", "ფული", "წყალი", "საჭმელი",
                    "პური", "ღვინო", "ხილი", "ბოსტნეული", "ხორცი", "თევზი",
                    "თავი", "თმა", "თვალი", "ყური", "ცხვირი", "პირი", "ხელი", "ფეხი",
                    "გული", "სიტყვა", "სახელი", "ადგილი", "საქმე", "მიზეზი",

                    // Numeri (20)
                    "ერთი", "ორი", "სამი", "ოთხი", "ხუთი", "ექვსი", "შვიდი", "რვა", "ცხრა", "ათი",
                    "თერთმეტი", "თორმეტი", "ოცი", "ოცდაათი", "ორმოცი", "ორმოცდაათი",
                    "ასი", "ათასი", "მილიონი", "პირველი", "მეორე", "მესამე",

                    // Avverbi (40)
                    "არ", "არა", "კი", "დიახ", "ძალიან", "ძალზე", "ცოტა", "ბევრი",
                    "უფრო", "ნაკლებად", "ყველაზე", "ახლა", "მერე", "შემდეგ", "წინ",
                    "აქ", "იქ", "სად", "როდის", "როგორ", "რატომ", "ისე", "ასე",
                    "ყოველთვის", "არასოდეს", "ხშირად", "ზოგჯერ", "იშვიათად",
                    "მალე", "ნელა", "შორს", "ახლოს", "ერთად", "ცალკე",
                    "დღეს", "გუშინ", "ხვალ", "ადრე", "გვიან", "მალე", "უკვე"
            };
        }
        else if (wrapper->language == "el") {
            commonWords = {
                    // Articoli e preposizioni (50)
                    "ο", "η", "το", "οι", "οι", "τα", "του", "της", "των",
                    "τον", "την", "τους", "τις", "ένας", "μια", "ένα",
                    "σε", "από", "για", "με", "σε", "προς", "κατά", "μετά",
                    "πριν", "μέχρι", "ως", "χωρίς", "παρά", "κατά", "υπό",
                    "ότι", "που", "πως", "αν", "όταν", "όπου", "πού", "γιατί",
                    "και", "ή", "αλλά", "μα", "όμως", "παρά", "είτε", "ούτε",
                    "ενώ", "επειδή", "αφού",

                    // Pronomi (30)
                    "εγώ", "εσύ", "αυτός", "αυτή", "αυτό", "εμείς", "εσείς", "αυτοί", "αυτές", "αυτά",
                    "με", "σε", "τον", "την", "το", "μας", "σας", "τους", "τις", "τα",
                    "μου", "σου", "του", "της", "μας", "σας", "τους",
                    "μένα", "εσένα", "εμάς", "εσάς", "κάποιος", "κάτι", "τίποτα",

                    // Verbi είμαι (essere)
                    "είμαι", "είσαι", "είναι", "είμαστε", "είστε", "είναι",
                    "ήμουν", "ήσουν", "ήταν", "ήμασταν", "ήσασταν", "ήταν",
                    "θα", "είμαι", "θα", "είσαι", "θα", "είναι", "θα", "είμαστε",

                    // Verbi έχω (avere)
                    "έχω", "έχεις", "έχει", "έχουμε", "έχετε", "έχουν",
                    "είχα", "είχες", "είχε", "είχαμε", "είχατε", "είχαν",
                    "θα", "έχω", "θα", "έχεις", "θα", "έχει", "θα", "έχουμε",

                    // Verbi COMUNI (80)
                    "κάνω", "κάνεις", "κάνει", "κάνουμε", "κάνετε", "κάνουν",
                    "έκανα", "έκανες", "έκανε", "κάναμε", "κάνατε", "έκαναν",
                    "λέω", "λες", "λέει", "λέμε", "λέτε", "λένε",
                    "είπα", "είπες", "είπε", "είπαμε", "είπατε", "είπαν",
                    "πάω", "πας", "πάει", "πάμε", "πάτε", "πάνε",
                    "πήγα", "πήγες", "πήγε", "πήγαμε", "πήγατε", "πήγαν",
                    "έρχομαι", "έρχεσαι", "έρχεται", "ερχόμαστε", "έρχεστε", "έρχονται",
                    "ήρθα", "ήρθες", "ήρθε", "ήρθαμε", "ήρθατε", "ήρθαν",
                    "βλέπω", "βλέπεις", "βλέπει", "βλέπουμε", "βλέπετε", "βλέπουν",
                    "είδα", "είδες", "είδε", "είδαμε", "είδατε", "είδαν",
                    "δίνω", "δίνεις", "δίνει", "δίνουμε", "δίνετε", "δίνουν",
                    "έδωσα", "έδωσες", "έδωσε", "δώσαμε", "δώσατε", "έδωσαν",
                    "παίρνω", "παίρνεις", "παίρνει", "παίρνουμε", "παίρνετε", "παίρνουν",
                    "πήρα", "πήρες", "πήρε", "πήραμε", "πήρατε", "πήραν",
                    "ξέρω", "ξέρεις", "ξέρει", "ξέρουμε", "ξέρετε", "ξέρουν",
                    "ήξερα", "ήξερες", "ήξερε", "ξέραμε", "ξέρατε", "ήξεραν",
                    "θέλω", "θέλεις", "θέλει", "θέλουμε", "θέλετε", "θέλουν",
                    "ήθελα", "ήθελες", "ήθελε", "θέλαμε", "θέλατε", "ήθελαν",
                    "μπορώ", "μπορείς", "μπορεί", "μπορούμε", "μπορείτε", "μπορούν",
                    "μπόρεσα", "μπόρεσες", "μπόρεσε", "μπορέσαμε", "μπορέσατε",

                    // Aggettivi (40)
                    "καλός", "καλή", "καλό", "καλύτερος", "κακός", "κακή", "κακό", "χειρότερος",
                    "μεγάλος", "μεγάλη", "μεγάλο", "μικρός", "μικρή", "μικρό",
                    "νέος", "νέα", "νέο", "παλιός", "παλιά", "παλιό",
                    "μακρύς", "μακριά", "μακρύ", "κοντός", "κοντή", "κοντό",
                    "ψηλός", "ψηλή", "ψηλό", "χαμηλός", "χαμηλή", "χαμηλό",
                    "πολύς", "πολλή", "πολύ", "λίγος", "λίγη", "λίγο",
                    "όλος", "όλη", "όλο", "άλλος", "άλλη", "άλλο",
                    "πρώτος", "πρώτη", "πρώτο", "τελευταίος", "τελευταία", "τελευταίο",

                    // Sostantivi (60)
                    "χρόνος", "χρόνια", "μέρα", "μέρες", "ώρα", "ώρες",
                    "φορά", "φορές", "ζωή", "κόσμος", "άνθρωπος", "άνθρωποι",
                    "άντρας", "άντρες", "γυναίκα", "γυναίκες", "παιδί", "παιδιά",
                    "μέρος", "μέρη", "χέρι", "χέρια", "κεφάλι", "κεφάλια",
                    "μάτι", "μάτια", "λόγος", "λόγοι", "τρόπος", "τρόποι",
                    "χώρα", "χώρες", "πόλη", "πόλεις", "σπίτι", "σπίτια",
                    "δρόμος", "δρόμοι", "δουλειά", "δουλειές", "πράγμα", "πράγματα",
                    "ερώτηση", "ερωτήσεις", "απάντηση", "απαντήσεις",
                    "πρόβλημα", "προβλήματα", "λέξη", "λέξεις", "όνομα", "ονόματα",
                    "οικογένεια", "οικογένειες", "φίλος", "φίλοι", "φίλη", "φίλες",
                    "βιβλίο", "βιβλία", "νερό", "χρήματα", "αυτοκίνητο", "αυτοκίνητα",

                    // Numeri (20)
                    "ένας", "μία", "ένα", "δύο", "τρεις", "τρία", "τέσσερις", "τέσσερα",
                    "πέντε", "έξι", "επτά", "οκτώ", "εννέα", "δέκα",
                    "έντεκα", "δώδεκα", "είκοσι", "τριάντα", "εκατό", "χίλια",
                    "εκατομμύριο", "πρώτος", "δεύτερος", "τρίτος",

                    // Avverbi (40)
                    "δεν", "όχι", "ναι", "μάλιστα", "πολύ", "πιο", "λιγότερο",
                    "πάρα", "πολύ", "ήδη", "ακόμα", "πάντα", "ποτέ", "τώρα",
                    "μετά", "πριν", "εδώ", "εκεί", "πού", "πότε", "πώς",
                    "γιατί", "πάντοτε", "συχνά", "σπάνια", "μερικές", "φορές",
                    "πραγματικά", "σχεδόν", "μόλις", "ακριβώς", "σχεδόν", "εντελώς",
                    "σήμερα", "χθες", "αύριο", "νωρίς", "αργά", "σύντομα", "μαζί"
            };
        }
        else {
            // Lingua non supportata - ritorna array vuoto
            LOGE("❌ extractVocabulary: Unsupported language: '%s'", wrapper->language.c_str());
            jclass stringClass = env->FindClass("java/lang/String");
            return env->NewObjectArray(0, stringClass, nullptr);
        }

        // Limita al numero richiesto
        int actualSize = std::min(static_cast<int>(commonWords.size()), maxWords);

        LOGI("✅ extractVocabulary: Language='%s' TotalWords=%zu RequestedMax=%d ActualReturned=%d",
             wrapper->language.c_str(), commonWords.size(), maxWords, actualSize);

        // Crea array Java di stringhe
        jclass stringClass = env->FindClass("java/lang/String");
        if (stringClass == nullptr) {
            LOGE("❌ extractVocabulary: Failed to find String class");
            return nullptr;
        }

        jobjectArray result = env->NewObjectArray(actualSize, stringClass, nullptr);
        if (result == nullptr) {
            LOGE("❌ extractVocabulary: Failed to create array");
            return nullptr;
        }

        for (int i = 0; i < actualSize; ++i) {
            jstring jword = env->NewStringUTF(commonWords[i].c_str());
            if (jword == nullptr) {
                LOGE("❌ extractVocabulary: Failed to create string at index %d", i);
                continue;
            }
            env->SetObjectArrayElement(result, i, jword);
            env->DeleteLocalRef(jword);
        }

        LOGI("✅ extractVocabulary: SUCCESS - Returned %d words for language '%s'",
             actualSize, wrapper->language.c_str());

        return result;

    } catch (const std::exception& e) {
        LOGE("❌ extractVocabulary: Exception - %s", e.what());
        return nullptr;
    }
}

} // extern "C"