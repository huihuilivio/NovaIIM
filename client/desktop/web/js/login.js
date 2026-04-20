// NovaIIM Login / Register Page

window.LoginPage = {
    render: function (container) {
        container.innerHTML =
            '<div class="login-page">' +
            '  <div class="login-card">' +
            '    <h1>NovaIIM</h1>' +
            '    <p class="subtitle">即时通讯客户端</p>' +
            // ---- 登录表单 ----
            '    <div id="login-form">' +
            '      <div class="form-group">' +
            '        <input type="email" id="login-email" placeholder="邮箱地址" />' +
            '      </div>' +
            '      <div class="form-group">' +
            '        <input type="password" id="login-password" placeholder="密码" />' +
            '      </div>' +
            '      <div class="login-error" id="login-error"></div>' +
            '      <button class="btn-primary" id="login-btn" disabled>登 录</button>' +
            '      <p class="switch-link">没有账号？<a href="#" id="to-register">立即注册</a></p>' +
            '    </div>' +
            // ---- 注册表单 ----
            '    <div id="register-form" style="display:none">' +
            '      <div class="form-group">' +
            '        <input type="email" id="reg-email" placeholder="邮箱地址" />' +
            '      </div>' +
            '      <div class="form-group">' +
            '        <input type="text" id="reg-nickname" placeholder="昵称" />' +
            '      </div>' +
            '      <div class="form-group">' +
            '        <input type="password" id="reg-password" placeholder="密码（至少6位）" />' +
            '      </div>' +
            '      <div class="form-group">' +
            '        <input type="password" id="reg-password2" placeholder="确认密码" />' +
            '      </div>' +
            '      <div class="login-error" id="reg-error"></div>' +
            '      <button class="btn-primary" id="reg-btn" disabled>注 册</button>' +
            '      <p class="switch-link">已有账号？<a href="#" id="to-login">返回登录</a></p>' +
            '    </div>' +
            '  </div>' +
            '</div>';

        // ---- 表单切换 ----
        var loginForm = document.getElementById('login-form');
        var regForm   = document.getElementById('register-form');

        document.getElementById('to-register').addEventListener('click', function (e) {
            e.preventDefault();
            loginForm.style.display = 'none';
            regForm.style.display   = 'block';
        });
        document.getElementById('to-login').addEventListener('click', function (e) {
            e.preventDefault();
            regForm.style.display   = 'none';
            loginForm.style.display = 'block';
        });

        // ---- 登录逻辑 ----
        var emailEl    = document.getElementById('login-email');
        var passwordEl = document.getElementById('login-password');
        var btnEl      = document.getElementById('login-btn');
        var errorEl    = document.getElementById('login-error');

        function updateLoginBtn() {
            btnEl.disabled = !(emailEl.value.trim() && passwordEl.value);
        }

        emailEl.addEventListener('input', updateLoginBtn);
        passwordEl.addEventListener('input', updateLoginBtn);

        passwordEl.addEventListener('keydown', function (e) {
            if (e.key === 'Enter' && !btnEl.disabled) {
                btnEl.click();
            }
        });

        btnEl.addEventListener('click', function () {
            errorEl.textContent = '';
            btnEl.disabled = true;
            btnEl.textContent = '登录中...';

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

        // ---- 注册逻辑 ----
        var regEmailEl    = document.getElementById('reg-email');
        var regNickEl     = document.getElementById('reg-nickname');
        var regPwdEl      = document.getElementById('reg-password');
        var regPwd2El     = document.getElementById('reg-password2');
        var regBtnEl      = document.getElementById('reg-btn');
        var regErrorEl    = document.getElementById('reg-error');

        function updateRegBtn() {
            regBtnEl.disabled = !(
                regEmailEl.value.trim() &&
                regNickEl.value.trim() &&
                regPwdEl.value &&
                regPwd2El.value
            );
        }

        regEmailEl.addEventListener('input', updateRegBtn);
        regNickEl.addEventListener('input', updateRegBtn);
        regPwdEl.addEventListener('input', updateRegBtn);
        regPwd2El.addEventListener('input', updateRegBtn);

        regPwd2El.addEventListener('keydown', function (e) {
            if (e.key === 'Enter' && !regBtnEl.disabled) {
                regBtnEl.click();
            }
        });

        regBtnEl.addEventListener('click', function () {
            regErrorEl.textContent = '';

            if (regPwdEl.value !== regPwd2El.value) {
                regErrorEl.textContent = '两次密码输入不一致';
                return;
            }
            if (regPwdEl.value.length < 6) {
                regErrorEl.textContent = '密码长度至少6位';
                return;
            }

            regBtnEl.disabled = true;
            regBtnEl.textContent = '注册中...';

            var regTimeout = setTimeout(function () {
                regErrorEl.textContent = '连接超时，请重试';
                regBtnEl.disabled = false;
                regBtnEl.textContent = '注 册';
            }, 15000);

            NovaBridge.send('connect');
            NovaBridge.send('register', {
                email: regEmailEl.value.trim(),
                nickname: regNickEl.value.trim(),
                password: regPwdEl.value
            });

            NovaBridge.on('registerResult', function (data) {
                clearTimeout(regTimeout);
                if (data.success) {
                    regErrorEl.textContent = '';
                    // 注册成功，自动切回登录并提示
                    regForm.style.display   = 'none';
                    loginForm.style.display = 'block';
                    emailEl.value = regEmailEl.value;
                    passwordEl.value = '';
                    errorEl.textContent = '';
                    errorEl.style.color = 'var(--success)';
                    errorEl.textContent = '注册成功，请登录';
                    updateLoginBtn();
                } else {
                    regErrorEl.textContent = data.msg || '注册失败';
                    regBtnEl.disabled = false;
                    regBtnEl.textContent = '注 册';
                }
            });
        });
    }
};
