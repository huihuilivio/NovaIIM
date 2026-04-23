// NovaClient.mm — Objective-C++ 实现
// 桥接 nova_sdk C++ 共享库到 iOS/macOS

#import "NovaClient.h"

#include <viewmodel/nova_client.h>
#include <viewmodel/ui_dispatcher.h>

#include <dispatch/dispatch.h>
#include <memory>
#include <string>

// 安全包装：若 c_str() 为 nullptr 或非法 UTF-8，返回 @""，避免 crash
static inline NSString* NovaSafeNSString(const char* s) {
    if (!s) return @"";
    NSString* r = [NSString stringWithUTF8String:s];
    return r ?: @"";
}

@implementation NovaLoginResult
@end

@implementation NovaPushMessage
@end

@interface NovaClient () {
    std::unique_ptr<nova::client::NovaClient> _client;
    std::string _configPath;
}
@end

@implementation NovaClient

+ (instancetype)shared {
    static NovaClient *instance = nil;
    static dispatch_once_t onceToken;
    dispatch_once(&onceToken, ^{
        instance = [[NovaClient alloc] init];
    });
    return instance;
}

- (instancetype)init {
    self = [super init];
    if (self) {
        nova::client::UIDispatcher::Set([](std::function<void()> fn) {
            dispatch_async(dispatch_get_main_queue(), ^{
                fn();
            });
        });

        _configPath = {};
    }
    return self;
}

- (void)configureWithPath:(NSString *)path {
    _configPath = path ? [path UTF8String] : "";
}

- (void)connect {
    if (_client) return;

    _client = std::make_unique<nova::client::NovaClient>(_configPath);
    _client->Init();

    __weak NovaClient *weakSelf = self;

    _client->App()->State().Observe([weakSelf](nova::client::ClientState state) {
        dispatch_async(dispatch_get_main_queue(), ^{
            NovaClient *strongSelf = weakSelf;
            if (!strongSelf) return;
            strongSelf->_connectionState = static_cast<NovaConnectionState>(state);
            if ([strongSelf.delegate respondsToSelector:@selector(novaClient:didChangeState:)]) {
                [strongSelf.delegate novaClient:strongSelf didChangeState:strongSelf.connectionState];
            }
        });
    });

    _client->Chat()->OnMessageReceived([weakSelf](const nova::client::ReceivedMessage& msg) {
        dispatch_async(dispatch_get_main_queue(), ^{
            NovaClient *strongSelf = weakSelf;
            if (!strongSelf) return;
            NovaPushMessage *pushMsg = [[NovaPushMessage alloc] init];
            pushMsg.conversationId = msg.conversation_id;
            pushMsg.senderUid = NovaSafeNSString(msg.sender_uid.c_str());
            pushMsg.content = NovaSafeNSString(msg.content.c_str());
            pushMsg.serverSeq = msg.server_seq;
            pushMsg.serverTime = msg.server_time;
            pushMsg.msgType = msg.msg_type;
            if ([strongSelf.delegate respondsToSelector:@selector(novaClient:didReceiveMessage:)]) {
                [strongSelf.delegate novaClient:strongSelf didReceiveMessage:pushMsg];
            }
        });
    });

    _client->Connect();
}

- (void)disconnect {
    if (_client) {
        _client->Shutdown();
        _client.reset();
    }
}

- (void)loginWithEmail:(NSString *)email password:(NSString *)password {
    if (!_client) return;

    auto email_str = std::string(email ? [email UTF8String] : "");
    auto pass_str  = std::string(password ? [password UTF8String] : "");

    __weak NovaClient *weakSelf = self;
    _client->Login()->Login(email_str, pass_str,
        [weakSelf](const nova::client::LoginResult& lr) {
            dispatch_async(dispatch_get_main_queue(), ^{
                NovaClient *strongSelf = weakSelf;
                if (!strongSelf) return;
                NovaLoginResult *result = [[NovaLoginResult alloc] init];
                result.code = lr.success ? 0 : -1;
                result.msg = NovaSafeNSString(lr.msg.c_str());
                result.uid = NovaSafeNSString(lr.uid.c_str());
                result.nickname = NovaSafeNSString(lr.nickname.c_str());
                result.avatar = NovaSafeNSString(lr.avatar.c_str());
                if ([strongSelf.delegate respondsToSelector:@selector(novaClient:didLoginWithResult:)]) {
                    [strongSelf.delegate novaClient:strongSelf didLoginWithResult:result];
                }
            });
        });
}

- (void)logout {
    if (!_client) return;
    _client->Login()->Logout();
}

- (void)sendTextMessage:(NSString *)content conversationId:(int64_t)conversationId {
    if (!_client || !_client->Login()->LoggedIn().Get()) return;
    if (!content) return;
    _client->Chat()->SendTextMessage(conversationId, std::string([content UTF8String]));
}

- (BOOL)isLoggedIn {
    return _client && _client->Login()->LoggedIn().Get();
}

@end
