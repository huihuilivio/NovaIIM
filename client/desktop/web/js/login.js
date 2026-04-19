// NovaIIM Login Page

window.LoginPage = {
    render: function (container) {
        container.innerHTML =
            '<div class="login-page">' +
            '  <div class="login-card">' +
            '    <h1>NovaIIM</h1>' +
            '    <p class="subtitle">即时通讯客户端</p>' +
            '    <div class="form-group">' +
            '      <input type="email" id="login-email" placeholder="邮箱地址" />' +
            '    </div>' +
            '    <div class="form-group">' +
            '      <input type="password" id="login-password" placeholder="密码" />' +
            '    </div>' +
            '    <div class="login-error" id="login-error"></div>' +
            '    <button class="btn-primary" id="login-btn" disabled>登 录</button>' +
            '  </div>' +
            '</div>';

        var emailEl    = document.getElementById('login-email');
        var passwordEl = document.getElementById('login-password');
        var btnEl      = document.getElementById('login-btn');
        var errorEl    = document.getElementById('login-error');

        function updateBtn() {
            btnEl.disabled = !(emailEl.value.trim() && passwordEl.value);
        }

        emailEl.addEventListener('input', updateBtn);
        passwordEl.addEventListener('input', updateBtn);

        passwordEl.addEventListener('keydown', function (e) {
            if (e.key === 'Enter' && !btnEl.disabled) {
                btnEl.click();
            }
        });

        btnEl.addEventListener('click', function () {
            errorEl.textContent = '';
            btnEl.disabled = true;
            btnEl.textContent = '登录中...';

            // 超时恢复按钮状态
            var loginTimeout = setTimeout(function () {
                errorEl.textContent = '连接超时，请重试';
                btnEl.disabled = false;
                btnEl.textContent = '登 录';
            }, 15000);

            NovaBridge.send('connect');
            NovaBridge.send('login', {
                email: emailEl.value.trim(),
                password: passwordEl.value
            });

            // 监听登录结果
            NovaBridge.on('loginResult', function (data) {
                clearTimeout(loginTimeout);
                if (data.success) {
                    NovaApp.currentUser = {
                        uid: data.uid,
                        nickname: data.nickname || data.uid
                    };
                    NovaApp.navigate('main');
                } else {
                    errorEl.textContent = data.msg || '登录失败';
                    btnEl.disabled = false;
                    btnEl.textContent = '登 录';
                }
            });
        });
    }
};
