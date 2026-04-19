// NovaClient.mm — Objective-C++ 实现
// 桥接 nova_client C++ 共享库到 iOS/macOS

#import "NovaClient.h"

#include <core/client_config.h>
#include <core/client_context.h>
#include <core/event_bus.h>
#include <core/ui_dispatcher.h>
#include <net/connection_state.h>

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
    _context->Network().OnStateChanged([self](nova::client::ConnectionState state) {
        dispatch_async(dispatch_get_main_queue(), ^{
            _connectionState = static_cast<NovaConnectionState>(state);
            if ([self.delegate respondsToSelector:@selector(novaClient:didChangeState:)]) {
                [self.delegate novaClient:self didChangeState:self.connectionState];
            }
        });
    });

    // 监听推送消息
    _context->Events().Subscribe<nova::proto::PushMsg>([self](const nova::proto::PushMsg& msg) {
        dispatch_async(dispatch_get_main_queue(), ^{
            NovaPushMessage *pushMsg = [[NovaPushMessage alloc] init];
            pushMsg.conversationId = msg.conversation_id;
            pushMsg.senderUid = [NSString stringWithUTF8String:msg.sender_uid.c_str()];
            pushMsg.content = [NSString stringWithUTF8String:msg.content.c_str()];
            pushMsg.serverSeq = msg.server_seq;
            pushMsg.serverTime = msg.server_time;
            pushMsg.msgType = static_cast<int32_t>(msg.msg_type);

            if ([self.delegate respondsToSelector:@selector(novaClient:didReceiveMessage:)]) {
                [self.delegate novaClient:self didReceiveMessage:pushMsg];
            }
        });
    });

    _context->Network().Connect();
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
    pkt.seq = _context->Network().NextSeq();
    pkt.body = nova::proto::Serialize(req);

    _context->Requests().AddPending(pkt.seq,
        [self](const nova::proto::Packet& resp) {
            auto ack = nova::proto::Deserialize<nova::proto::LoginAck>(resp.body);
            dispatch_async(dispatch_get_main_queue(), ^{
                NovaLoginResult *result = [[NovaLoginResult alloc] init];
                if (ack) {
                    result.code = ack->code;
                    result.msg = [NSString stringWithUTF8String:ack->msg.c_str()];
                    result.uid = [NSString stringWithUTF8String:ack->uid.c_str()];
                    result.nickname = [NSString stringWithUTF8String:ack->nickname.c_str()];
                    result.avatar = [NSString stringWithUTF8String:ack->avatar.c_str()];

                    if (ack->code == 0) {
                        _context->SetUid(ack->uid);
                    }
                } else {
                    result.code = -1;
                    result.msg = @"Failed to decode login response";
                }

                if ([self.delegate respondsToSelector:@selector(novaClient:didLoginWithResult:)]) {
                    [self.delegate novaClient:self didLoginWithResult:result];
                }
            });
        },
        [self](uint32_t seq) {
            dispatch_async(dispatch_get_main_queue(), ^{
                NovaLoginResult *result = [[NovaLoginResult alloc] init];
                result.code = -1;
                result.msg = @"Login request timed out";
                if ([self.delegate respondsToSelector:@selector(novaClient:didLoginWithResult:)]) {
                    [self.delegate novaClient:self didLoginWithResult:result];
                }
            });
        }
    );

    _context->Network().Send(pkt);
}

- (void)logout {
    if (!_context) return;

    nova::proto::Packet pkt;
    pkt.cmd = static_cast<uint16_t>(nova::proto::Cmd::kLogout);
    pkt.seq = _context->Network().NextSeq();
    _context->Network().Send(pkt);
    _context->SetUid("");
}

- (void)sendTextMessage:(NSString *)content conversationId:(int64_t)conversationId {
    if (!_context || !_context->IsLoggedIn()) return;

    nova::proto::SendMsgReq req;
    req.conversation_id = conversationId;
    req.content = [content UTF8String];
    req.msg_type = nova::proto::MsgType::kText;

    nova::proto::Packet pkt;
    pkt.cmd = static_cast<uint16_t>(nova::proto::Cmd::kSendMsg);
    pkt.seq = _context->Network().NextSeq();
    pkt.body = nova::proto::Serialize(req);

    _context->Network().Send(pkt);
}

- (BOOL)isLoggedIn {
    return _context && _context->IsLoggedIn();
}

@end
