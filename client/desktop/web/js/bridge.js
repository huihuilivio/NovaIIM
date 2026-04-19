// NovaIIM JS Bridge — WebView2 ↔ C++ 通信层
//
// 发送: NovaBridge.send(action, data)     → C++ JsBridge::OnWebMessage
// 接收: NovaBridge.on(event, callback)    ← C++ JsBridge::PostEvent

window.NovaBridge = (function () {
    const listeners = {};

    // C++ 调用此方法推送事件
    window.__novaBridge = {
        onEvent: function (msg) {
            const event = msg.event;
            const data  = msg.data;
            const cbs = listeners[event];
            if (cbs) {
                cbs.forEach(function (cb) {
                    try { cb(data); } catch (e) { console.error('NovaBridge event error:', e); }
                });
            }
        }
    };

    return {
        /**
         * 发送消息到 C++
         * @param {string} action
         * @param {object} data
         */
        send: function (action, data) {
            var msg = Object.assign({ action: action }, data || {});
            window.chrome.webview.postMessage(JSON.stringify(msg));
        },

        /**
         * 监听 C++ 事件
         * @param {string} event
         * @param {function} callback
         */
        on: function (event, callback) {
            if (!listeners[event]) listeners[event] = [];
            listeners[event].push(callback);
        },

        /**
         * 移除事件监听
         * @param {string} event
         * @param {function} callback
         */
        off: function (event, callback) {
            if (!listeners[event]) return;
            if (!callback) {
                delete listeners[event];
            } else {
                listeners[event] = listeners[event].filter(function (cb) { return cb !== callback; });
            }
        }
    };
})();
