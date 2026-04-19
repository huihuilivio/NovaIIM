// nova_jni.cpp — Android JNI 封装
// 桥接 nova_client C++ 共享库到 Kotlin/Java
//
// Java 类: com.nova.client.NovaClient (native methods)

#include <jni.h>

#include <core/client_config.h>
#include <core/client_context.h>
#include <core/event_bus.h>
#include <core/ui_dispatcher.h>

#include <nova/protocol.h>

#include <android/log.h>

#include <memory>
#include <string>

#define LOG_TAG "NovaJNI"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

namespace {

std::unique_ptr<nova::client::ClientContext> g_context;
nova::client::ClientConfig g_config;

JavaVM* g_jvm = nullptr;
jobject g_callback_ref = nullptr;

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

// void configure(String host, int port, String deviceId)
JNIEXPORT void JNICALL
Java_com_nova_client_NovaClient_nativeConfigure(
        JNIEnv* env, jobject /*thiz*/,
        jstring host, jint port, jstring deviceId) {
    g_config.server_host = JStringToString(env, host);
    g_config.server_port = static_cast<uint16_t>(port);
    g_config.device_id   = JStringToString(env, deviceId);
    g_config.device_type = "mobile";
    g_config.log_level   = "info";
    LOGI("Configured: %s:%d", g_config.server_host.c_str(), g_config.server_port);
}

// void setCallback(NovaCallback callback)
JNIEXPORT void JNICALL
Java_com_nova_client_NovaClient_nativeSetCallback(
        JNIEnv* env, jobject /*thiz*/, jobject callback) {
    if (g_callback_ref) {
        env->DeleteGlobalRef(g_callback_ref);
    }
    g_callback_ref = callback ? env->NewGlobalRef(callback) : nullptr;
}

// void connect()
JNIEXPORT void JNICALL
Java_com_nova_client_NovaClient_nativeConnect(JNIEnv* /*env*/, jobject /*thiz*/) {
    if (g_context) return;

    g_context = std::make_unique<nova::client::ClientContext>(g_config);
    g_context->Init();

    // 连接状态回调
    g_context->Network().OnStateChanged([](nova::client::ConnectionState state) {
        auto* env = GetJNIEnv();
        if (!env || !g_callback_ref) return;

        jclass cls = env->GetObjectClass(g_callback_ref);
        jmethodID mid = env->GetMethodID(cls, "onConnectionStateChanged", "(I)V");
        if (mid) {
            env->CallVoidMethod(g_callback_ref, mid, static_cast<jint>(state));
        }
    });

    // 推送消息回调
    g_context->Events().Subscribe<nova::proto::PushMsg>([](const nova::proto::PushMsg& msg) {
        auto* env = GetJNIEnv();
        if (!env || !g_callback_ref) return;

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

    g_context->Network().Connect();
    LOGI("Connected");
}

// void disconnect()
JNIEXPORT void JNICALL
Java_com_nova_client_NovaClient_nativeDisconnect(JNIEnv* /*env*/, jobject /*thiz*/) {
    if (g_context) {
        g_context->Shutdown();
        g_context.reset();
    }
    LOGI("Disconnected");
}

// void login(String email, String password)
JNIEXPORT void JNICALL
Java_com_nova_client_NovaClient_nativeLogin(
        JNIEnv* env, jobject /*thiz*/,
        jstring email, jstring password) {
    if (!g_context) return;

    nova::proto::LoginReq req;
    req.email     = JStringToString(env, email);
    req.password  = JStringToString(env, password);
    req.device_id = g_config.device_id;
    req.device_type = g_config.device_type;

    nova::proto::Packet pkt;
    pkt.cmd  = static_cast<uint16_t>(nova::proto::Cmd::kLogin);
    pkt.seq  = g_context->Network().NextSeq();
    pkt.body = nova::proto::Serialize(req);

    g_context->Requests().AddPending(pkt.seq,
        [](const nova::proto::Packet& resp) {
            auto ack = nova::proto::Deserialize<nova::proto::LoginAck>(resp.body);
            auto* env = GetJNIEnv();
            if (!env || !g_callback_ref) return;

            jclass cls = env->GetObjectClass(g_callback_ref);
            jmethodID mid = env->GetMethodID(cls, "onLoginResult",
                "(ILjava/lang/String;Ljava/lang/String;Ljava/lang/String;)V");
            if (mid && ack) {
                jstring msg      = env->NewStringUTF(ack->msg.c_str());
                jstring uid      = env->NewStringUTF(ack->uid.c_str());
                jstring nickname = env->NewStringUTF(ack->nickname.c_str());
                env->CallVoidMethod(g_callback_ref, mid,
                    ack->code, msg, uid, nickname);
                env->DeleteLocalRef(msg);
                env->DeleteLocalRef(uid);
                env->DeleteLocalRef(nickname);

                if (ack->code == 0) {
                    g_context->SetUid(ack->uid);
                }
            }
        },
        [](uint32_t /*seq*/) {
            auto* env = GetJNIEnv();
            if (!env || !g_callback_ref) return;

            jclass cls = env->GetObjectClass(g_callback_ref);
            jmethodID mid = env->GetMethodID(cls, "onLoginResult",
                "(ILjava/lang/String;Ljava/lang/String;Ljava/lang/String;)V");
            if (mid) {
                jstring msg = env->NewStringUTF("Login timed out");
                jstring empty = env->NewStringUTF("");
                env->CallVoidMethod(g_callback_ref, mid, -1, msg, empty, empty);
                env->DeleteLocalRef(msg);
                env->DeleteLocalRef(empty);
            }
        }
    );

    g_context->Network().Send(pkt);
}

// void sendTextMessage(long conversationId, String content)
JNIEXPORT void JNICALL
Java_com_nova_client_NovaClient_nativeSendTextMessage(
        JNIEnv* env, jobject /*thiz*/,
        jlong conversationId, jstring content) {
    if (!g_context || !g_context->IsLoggedIn()) return;

    nova::proto::SendMsgReq req;
    req.conversation_id = static_cast<int64_t>(conversationId);
    req.content  = JStringToString(env, content);
    req.msg_type = nova::proto::MsgType::kText;

    nova::proto::Packet pkt;
    pkt.cmd  = static_cast<uint16_t>(nova::proto::Cmd::kSendMsg);
    pkt.seq  = g_context->Network().NextSeq();
    pkt.body = nova::proto::Serialize(req);

    g_context->Network().Send(pkt);
}

// boolean isLoggedIn()
JNIEXPORT jboolean JNICALL
Java_com_nova_client_NovaClient_nativeIsLoggedIn(JNIEnv* /*env*/, jobject /*thiz*/) {
    return g_context && g_context->IsLoggedIn() ? JNI_TRUE : JNI_FALSE;
}

}  // extern "C"
