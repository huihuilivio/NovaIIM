// NovaClient.mm — Objective-C++ 实现
// 桥接 nova_sdk C++ 共享库到 iOS/macOS

#import "NovaClient.h"

#include <model/client_config.h>
#include <viewmodel/nova_client.h>
#include <viewmodel/ui_dispatcher.h>
#include <infra/connection_state.h>

#include <nova/protocol.h>

#include <dispatch/dispatch.h>
#include <memory>

@implementation NovaLoginResult
@end

@implementation NovaPushMessage
@end

@interface NovaClient () {
    std::unique_ptr<nova::client::ClientContext> _context;
    nova::client::ClientConfig _config;
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
        // 设置 UIDispatcher — 投递到主线程
        nova::client::UIDispatcher::Set([](std::function<void()> fn) {
            dispatch_async(dispatch_get_main_queue(), ^{
                fn();
            });
        });

        _config.device_type = "mobile";
        _config.log_level = "info";
    }
    return self;
}

- (void)configureWithHost:(NSString *)host
                     port:(uint16_t)port
                 deviceId:(NSString *)deviceId {
    _config.server_host = [host UTF8String];
    _config.server_port = port;
    _config.device_id = [deviceId UTF8String];
}

- (void)connect {
    if (_context) return;

    _context = std::make_unique<nova::client::ClientContext>(_config);
    _context->Init();

    // 监听连接状态
    __weak NovaClient *weakSelf = self;
    _context->Network().OnStateChanged([weakSelf](nova::client::ConnectionState state) {
        dispatch_async(dispatch_get_main_queue(), ^{
            NovaClient *strongSelf = weakSelf;
            if (!strongSelf) return;
            strongSelf->_connectionState = static_cast<NovaConnectionState>(state);
            if ([strongSelf.delegate respondsToSelector:@selector(novaClient:didChangeState:)]) {
                [strongSelf.delegate novaClient:strongSelf didChangeState:strongSelf.connectionState];
            }
        });
    });

    // 监听推送消息
    _context->Events().subscribe<nova::proto::PushMsg>("PushMsg", [weakSelf](const nova::proto::PushMsg& msg) {
        dispatch_async(dispatch_get_main_queue(), ^{
            NovaClient *strongSelf = weakSelf;
            if (!strongSelf) return;
            NovaPushMessage *pushMsg = [[NovaPushMessage alloc] init];
            pushMsg.conversationId = msg.conversation_id;
            pushMsg.senderUid = [NSString stringWithUTF8String:msg.sender_uid.c_str()];
            pushMsg.content = [NSString stringWithUTF8String:msg.content.c_str()];
            pushMsg.serverSeq = msg.server_seq;
            pushMsg.serverTime = msg.server_time;
            pushMsg.msgType = static_cast<int32_t>(msg.msg_type);

            if ([strongSelf.delegate respondsToSelector:@selector(novaClient:didReceiveMessage:)]) {
                [strongSelf.delegate novaClient:strongSelf didReceiveMessage:pushMsg];
            }
        });
    });

    _context->Connect();
}

- (void)disconnect {
    if (_context) {
        _context->Shutdown();
        _context.reset();
    }
}

- (void)loginWithEmail:(NSString *)email password:(NSString *)password {
    if (!_context) return;

    nova::proto::LoginReq req;
    req.email = [email UTF8String];
    req.password = [password UTF8String];
    req.device_id = _config.device_id;
    req.device_type = _config.device_type;

    nova::proto::Packet pkt;
    pkt.cmd = static_cast<uint16_t>(nova::proto::Cmd::kLogin);
    pkt.seq = _context->NextSeq();
    pkt.body = nova::proto::Serialize(req);

    __weak NovaClient *weakSelf = self;
    _context->Requests().AddPending(pkt.seq,
        [weakSelf](const nova::proto::Packet& resp) {
            auto ack = nova::proto::Deserialize<nova::proto::LoginAck>(resp.body);
            dispatch_async(dispatch_get_main_queue(), ^{
                NovaClient *strongSelf = weakSelf;
                if (!strongSelf) return;
                NovaLoginResult *result = [[NovaLoginResult alloc] init];
                if (ack) {
                    result.code = ack->code;
                    result.msg = [NSString stringWithUTF8String:ack->msg.c_str()];
                    result.uid = [NSString stringWithUTF8String:ack->uid.c_str()];
                    result.nickname = [NSString stringWithUTF8String:ack->nickname.c_str()];
                    result.avatar = [NSString stringWithUTF8String:ack->avatar.c_str()];

                    if (ack->code == 0 && strongSelf->_context) {
                        strongSelf->_context->SetAuthenticated(ack->uid);
                    }
                } else {
                    result.code = -1;
                    result.msg = @"Failed to decode login response";
                }

                if ([strongSelf.delegate respondsToSelector:@selector(novaClient:didLoginWithResult:)]) {
                    [strongSelf.delegate novaClient:strongSelf didLoginWithResult:result];
                }
            });
        },
        [weakSelf](uint32_t seq) {
            dispatch_async(dispatch_get_main_queue(), ^{
                NovaClient *strongSelf = weakSelf;
                if (!strongSelf) return;
                NovaLoginResult *result = [[NovaLoginResult alloc] init];
                result.code = -1;
                result.msg = @"Login request timed out";
                if ([strongSelf.delegate respondsToSelector:@selector(novaClient:didLoginWithResult:)]) {
                    [strongSelf.delegate novaClient:strongSelf didLoginWithResult:result];
                }
            });
        }
    );

    _context->SendPacket(pkt);
}

- (void)logout {
    if (!_context) return;

    nova::proto::Packet pkt;
    pkt.cmd = static_cast<uint16_t>(nova::proto::Cmd::kLogout);
    pkt.seq = _context->NextSeq();
    _context->SendPacket(pkt);
    _context->Shutdown();
}

- (void)sendTextMessage:(NSString *)content conversationId:(int64_t)conversationId {
    if (!_context || !_context->IsLoggedIn()) return;

    nova::proto::SendMsgReq req;
    req.conversation_id = conversationId;
    req.content = [content UTF8String];
    req.msg_type = nova::proto::MsgType::kText;

    nova::proto::Packet pkt;
    pkt.cmd = static_cast<uint16_t>(nova::proto::Cmd::kSendMsg);
    pkt.seq = _context->NextSeq();
    pkt.body = nova::proto::Serialize(req);

    _context->SendPacket(pkt);
}

- (BOOL)isLoggedIn {
    return _context && _context->IsLoggedIn();
}

@end
