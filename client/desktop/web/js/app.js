// NovaIIM App Router

window.NovaApp = {
    currentUser: null,

    navigate: function (page) {
        var app = document.getElementById('app');
        if (!app) return;

        // 清除旧的事件监听
        NovaBridge.off('loginResult');
        NovaBridge.off('connectionState');
        NovaBridge.off('newMessage');
        NovaBridge.off('sendMsgResult');

        switch (page) {
            case 'login':
                LoginPage.render(app);
                break;
            case 'main':
                MainPage.render(app);
                break;
            default:
                LoginPage.render(app);
        }
    }
};

// 启动
document.addEventListener('DOMContentLoaded', function () {
    NovaApp.navigate('login');
});
