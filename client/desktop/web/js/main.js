// NovaIIM Main Page — 三栏布局（侧边栏 + 会话列表 + 聊天区）

window.MainPage = {
    state: {
        conversations: [
            { id: 1, name: '张三', lastMsg: '你好！', time: '10:30' },
            { id: 2, name: '项目群', lastMsg: '[图片]', time: '昨天' },
            { id: 3, name: '李四', lastMsg: '好的', time: '周一' }
        ],
        activeConv: null,
        messages: [],
        connectionState: 'disconnected'
    },

    render: function (container) {
        var self = this;

        container.innerHTML =
            '<div class="main-page">' +
            '  <!-- 侧边栏 -->' +
            '  <div class="sidebar">' +
            '    <div class="avatar">' + (NovaApp.currentUser.nickname || 'U').charAt(0) + '</div>' +
            '    <button class="nav-btn active" title="消息">💬</button>' +
            '    <button class="nav-btn" title="联系人">👤</button>' +
            '    <button class="nav-btn" title="群组">👥</button>' +
            '    <div class="spacer"></div>' +
            '    <button class="nav-btn" id="btn-logout" title="退出">⚙</button>' +
            '  </div>' +
            '  <!-- 会话列表 -->' +
            '  <div class="conv-list">' +
            '    <div class="conv-search">' +
            '      <input type="text" placeholder="🔍 搜索" />' +
            '    </div>' +
            '    <div class="conv-items" id="conv-items"></div>' +
            '  </div>' +
            '  <!-- 聊天区 -->' +
            '  <div class="chat-area" id="chat-area">' +
            '    <div class="chat-placeholder">选择一个会话开始聊天</div>' +
            '  </div>' +
            '</div>' +
            '<div class="status-bar">' +
            '  <span class="status-dot disconnected" id="status-dot"></span>' +
            '  <span id="status-text">未连接</span>' +
            '</div>';

        // 渲染会话列表
        self.renderConversations();

        // 退出按钮
        document.getElementById('btn-logout').addEventListener('click', function () {
            NovaBridge.send('disconnect');
            NovaApp.currentUser = null;
            NovaApp.navigate('login');
        });

        // 监听连接状态
        NovaBridge.on('connectionState', function (data) {
            self.state.connectionState = data.state;
            self.updateStatusBar();
        });

        // 监听新消息
        NovaBridge.on('newMessage', function (data) {
            if (!data) return;
            // 更新会话最后消息
            var conv = self.state.conversations.find(function (c) {
                return c.id === data.conversationId;
            });
            if (conv) {
                conv.lastMsg = data.content;
                conv.time = self.formatTime(data.serverTime);
                self.renderConversations();
            }

            // 如果当前会话，追加消息
            if (self.state.activeConv && self.state.activeConv.id === data.conversationId) {
                self.state.messages.push({
                    sender: data.senderUid,
                    content: data.content,
                    self: false
                });
                self.renderMessages();
            }
        });

        // 监听发送结果
        NovaBridge.on('sendMsgResult', function (data) {
            if (!data.success) {
                console.warn('Send message failed:', data.msg);
            }
        });
    },

    renderConversations: function () {
        var self = this;
        var el = document.getElementById('conv-items');
        if (!el) return;

        el.innerHTML = self.state.conversations.map(function (conv) {
            var active = self.state.activeConv && self.state.activeConv.id === conv.id ? ' active' : '';
            return '<div class="conv-item' + active + '" data-id="' + conv.id + '">' +
                   '  <div class="conv-avatar">' + conv.name.charAt(0) + '</div>' +
                   '  <div class="conv-info">' +
                   '    <div class="conv-header">' +
                   '      <span class="conv-name">' + self.escapeHtml(conv.name) + '</span>' +
                   '      <span class="conv-time">' + self.escapeHtml(conv.time) + '</span>' +
                   '    </div>' +
                   '    <div class="conv-last-msg">' + self.escapeHtml(conv.lastMsg) + '</div>' +
                   '  </div>' +
                   '</div>';
        }).join('');

        // 绑定点击
        el.querySelectorAll('.conv-item').forEach(function (item) {
            item.addEventListener('click', function () {
                var id = parseInt(item.getAttribute('data-id'));
                var conv = self.state.conversations.find(function (c) { return c.id === id; });
                if (conv) {
                    self.state.activeConv = conv;
                    self.state.messages = [];
                    self.renderConversations();
                    self.renderChatArea();
                }
            });
        });
    },

    renderChatArea: function () {
        var self = this;
        var el = document.getElementById('chat-area');
        if (!el || !self.state.activeConv) return;

        el.innerHTML =
            '<div class="chat-header">' + self.escapeHtml(self.state.activeConv.name) + '</div>' +
            '<div class="chat-messages" id="chat-messages"></div>' +
            '<div class="chat-input-area">' +
            '  <input type="text" id="chat-input" placeholder="输入消息..." />' +
            '  <button id="chat-send">发送</button>' +
            '</div>';

        var inputEl = document.getElementById('chat-input');
        var sendBtn = document.getElementById('chat-send');

        function doSend() {
            var text = inputEl.value.trim();
            if (!text) return;

            NovaBridge.send('sendMessage', {
                to: String(self.state.activeConv.id),
                content: text
            });

            self.state.messages.push({
                sender: NovaApp.currentUser.uid,
                content: text,
                self: true
            });

            inputEl.value = '';
            self.renderMessages();
        }

        sendBtn.addEventListener('click', doSend);
        inputEl.addEventListener('keydown', function (e) {
            if (e.key === 'Enter') doSend();
        });

        self.renderMessages();
    },

    renderMessages: function () {
        var self = this;
        var el = document.getElementById('chat-messages');
        if (!el) return;

        el.innerHTML = self.state.messages.map(function (msg) {
            var cls = msg.self ? 'chat-msg self' : 'chat-msg';
            var initial = msg.self
                ? (NovaApp.currentUser.nickname || 'U').charAt(0)
                : (self.state.activeConv.name || '?').charAt(0);
            return '<div class="' + cls + '">' +
                   '  <div class="msg-avatar">' + initial + '</div>' +
                   '  <div class="msg-bubble">' + self.escapeHtml(msg.content) + '</div>' +
                   '</div>';
        }).join('');

        el.scrollTop = el.scrollHeight;
    },

    updateStatusBar: function () {
        var dot  = document.getElementById('status-dot');
        var text = document.getElementById('status-text');
        if (!dot || !text) return;

        var s = this.state.connectionState;
        var lower = (s || '').toLowerCase();
        dot.className = 'status-dot ' + lower;

        var labels = {
            'Disconnected': '未连接',
            'Connecting':   '连接中...',
            'Connected':    '已连接',
            'Authenticated':'已认证',
            'Reconnecting': '重连中...'
        };
        text.textContent = labels[s] || s;
    },

    formatTime: function (epochMs) {
        if (!epochMs) return '';
        var d = new Date(epochMs);
        return d.getHours().toString().padStart(2, '0') + ':' +
               d.getMinutes().toString().padStart(2, '0');
    },

    escapeHtml: function (str) {
        if (!str) return '';
        return str.replace(/&/g, '&amp;')
                  .replace(/</g, '&lt;')
                  .replace(/>/g, '&gt;')
                  .replace(/"/g, '&quot;');
    }
};
