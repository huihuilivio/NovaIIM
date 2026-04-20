// nova_jni.cpp — Android JNI 封装
// 桥接 nova_sdk C++ 共享库到 Kotlin/Java
//
// Java 类: com.nova.client.NovaClient (native methods)

#include <jni.h>

#include <viewmodel/nova_client.h>
#include <viewmodel/ui_dispatcher.h>

#include <android/log.h>

#include <memory>
#include <mutex>
#include <string>

#define LOG_TAG "NovaJNI"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

namespace {

std::mutex g_mutex;
std::unique_ptr<nova::client::NovaClient> g_client;
std::string g_config_path;

JavaVM* g_jvm = nullptr;
jobject g_callback_ref = nullptr;
std::atomic<bool> g_shutdown{false};

std::string JStringToString(JNIEnv* env, jstring jstr) {
    if (!jstr) return "";
    const char* chars = env->GetStringUTFChars(jstr, nullptr);
    std::string result(chars);
    env->ReleaseStringUTFChars(jstr, chars);
    return result;
}

JNIEnv* GetJNIEnv() {
    JNIEnv* env = nullptr;
    if (g_jvm) {
        g_jvm->GetEnv(reinterpret_cast<void**>(&env), JNI_VERSION_1_6);
        if (!env) {
            g_jvm->AttachCurrentThread(&env, nullptr);
        }
    }
    return env;
}

}  // namespace

extern "C" {

JNIEXPORT jint JNI_OnLoad(JavaVM* vm, void* /*reserved*/) {
    g_jvm = vm;
    return JNI_VERSION_1_6;
}

JNIEXPORT void JNICALL
Java_com_nova_client_NovaClient_nativeConfigure(
        JNIEnv* env, jobject /*thiz*/,
        jstring configPath) {
    g_config_path = JStringToString(env, configPath);
    LOGI("Configured with path: %s", g_config_path.c_str());
}

JNIEXPORT void JNICALL
Java_com_nova_client_NovaClient_nativeSetCallback(
        JNIEnv* env, jobject /*thiz*/, jobject callback) {
    std::lock_guard lock(g_mutex);
    if (g_callback_ref) {
        env->DeleteGlobalRef(g_callback_ref);
    }
    g_callback_ref = callback ? env->NewGlobalRef(callback) : nullptr;
}

JNIEXPORT void JNICALL
Java_com_nova_client_NovaClient_nativeConnect(JNIEnv* /*env*/, jobject /*thiz*/) {
    std::lock_guard lock(g_mutex);
    if (g_client) return;

    g_shutdown = false;
    g_client = std::make_unique<nova::client::NovaClient>(g_config_path);
    g_client->Init();

    g_client->App()->State().Observe([](nova::client::ClientState state) {
        if (g_shutdown) return;
        auto* env = GetJNIEnv();
        if (!env) return;
        std::lock_guard cb_lock(g_mutex);
        if (!g_callback_ref) return;
        jclass cls = env->GetObjectClass(g_callback_ref);
        jmethodID mid = env->GetMethodID(cls, "onConnectionStateChanged", "(I)V");
        if (mid) {
            env->CallVoidMethod(g_callback_ref, mid, static_cast<jint>(state));
        }
    });

    g_client->Chat()->OnMessageReceived([](const nova::client::ReceivedMessage& msg) {
        if (g_shutdown) return;
        auto* env = GetJNIEnv();
        if (!env) return;
        std::lock_guard cb_lock(g_mutex);
        if (!g_callback_ref) return;
        jclass cls = env->GetObjectClass(g_callback_ref);
        jmethodID mid = env->GetMethodID(cls, "onMessageReceived",
            "(JLjava/lang/String;Ljava/lang/String;JJI)V");
        if (mid) {
            jstring senderUid = env->NewStringUTF(msg.sender_uid.c_str());
            jstring content   = env->NewStringUTF(msg.content.c_str());
            env->CallVoidMethod(g_callback_ref, mid,
                static_cast<jlong>(msg.conversation_id),
                senderUid, content,
                static_cast<jlong>(msg.server_seq),
                static_cast<jlong>(msg.server_time),
                static_cast<jint>(msg.msg_type));
            env->DeleteLocalRef(senderUid);
            env->DeleteLocalRef(content);
        }
    });

    g_client->Connect();
    LOGI("Connected");
}

JNIEXPORT void JNICALL
Java_com_nova_client_NovaClient_nativeDisconnect(JNIEnv* /*env*/, jobject /*thiz*/) {
    g_shutdown = true;
    std::lock_guard lock(g_mutex);
    if (g_client) {
        g_client->Shutdown();
        g_client.reset();
    }
    LOGI("Disconnected");
}

JNIEXPORT void JNICALL
Java_com_nova_client_NovaClient_nativeLogin(
        JNIEnv* env, jobject /*thiz*/,
        jstring email, jstring password) {
    std::lock_guard lock(g_mutex);
    if (!g_client) return;

    auto email_str    = JStringToString(env, email);
    auto password_str = JStringToString(env, password);

    g_client->Login()->Login(email_str, password_str,
        [](const nova::client::LoginResult& result) {
            if (g_shutdown) return;
            auto* env = GetJNIEnv();
            if (!env) return;
            std::lock_guard cb_lock(g_mutex);
            if (!g_callback_ref) return;
            jclass cls = env->GetObjectClass(g_callback_ref);
            jmethodID mid = env->GetMethodID(cls, "onLoginResult",
                "(ILjava/lang/String;Ljava/lang/String;Ljava/lang/String;)V");
            if (mid) {
                int code = result.success ? 0 : -1;
                jstring msg      = env->NewStringUTF(result.msg.c_str());
                jstring uid      = env->NewStringUTF(result.uid.c_str());
                jstring nickname = env->NewStringUTF(result.nickname.c_str());
                env->CallVoidMethod(g_callback_ref, mid, code, msg, uid, nickname);
                env->DeleteLocalRef(msg);
                env->DeleteLocalRef(uid);
                env->DeleteLocalRef(nickname);
            }
        });
}

JNIEXPORT void JNICALL
Java_com_nova_client_NovaClient_nativeSendTextMessage(
        JNIEnv* env, jobject /*thiz*/,
        jlong conversationId, jstring content) {
    if (!g_client || !g_client->Login()->LoggedIn().Get()) return;

    auto content_str = JStringToString(env, content);
    g_client->Chat()->SendTextMessage(static_cast<int64_t>(conversationId), content_str);
}

JNIEXPORT jboolean JNICALL
Java_com_nova_client_NovaClient_nativeIsLoggedIn(JNIEnv* /*env*/, jobject /*thiz*/) {
    return g_client && g_client->Login()->LoggedIn().Get() ? JNI_TRUE : JNI_FALSE;
}

}  // extern "C"
