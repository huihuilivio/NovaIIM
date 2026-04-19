package com.nova.client

/**
 * NovaIIM 客户端 — Kotlin 封装层
 *
 * 通过 JNI 调用 nova_client C++ 共享库
 */
class NovaClient private constructor() {

    companion object {
        @Volatile
        private var instance: NovaClient? = null

        fun shared(): NovaClient {
            return instance ?: synchronized(this) {
                instance ?: NovaClient().also { instance = it }
            }
        }

        init {
            System.loadLibrary("nova_jni")
        }
    }

    // ---- 回调接口 ----
    interface Callback {
        fun onConnectionStateChanged(state: Int)
        fun onLoginResult(code: Int, msg: String, uid: String, nickname: String)
        fun onMessageReceived(
            conversationId: Long,
            senderUid: String,
            content: String,
            serverSeq: Long,
            serverTime: Long,
            msgType: Int
        )
    }

    // ---- 连接状态常量 ----
    object State {
        const val DISCONNECTED  = 0
        const val CONNECTING    = 1
        const val CONNECTED     = 2
        const val AUTHENTICATED = 3
        const val RECONNECTING  = 4
    }

    // ---- 公开接口 ----

    fun configure(host: String, port: Int, deviceId: String) {
        nativeConfigure(host, port, deviceId)
    }

    fun setCallback(callback: Callback) {
        nativeSetCallback(callback)
    }

    fun connect() = nativeConnect()
    fun disconnect() = nativeDisconnect()
    fun login(email: String, password: String) = nativeLogin(email, password)
    fun sendTextMessage(conversationId: Long, content: String) =
        nativeSendTextMessage(conversationId, content)
    val isLoggedIn: Boolean get() = nativeIsLoggedIn()

    // ---- Native 方法 ----
    private external fun nativeConfigure(host: String, port: Int, deviceId: String)
    private external fun nativeSetCallback(callback: Callback)
    private external fun nativeConnect()
    private external fun nativeDisconnect()
    private external fun nativeLogin(email: String, password: String)
    private external fun nativeSendTextMessage(conversationId: Long, content: String)
    private external fun nativeIsLoggedIn(): Boolean
}
