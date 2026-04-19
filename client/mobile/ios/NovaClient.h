#pragma once
// NovaClient — iOS Objective-C 公开接口
// Swift 可通过 Bridging Header 使用

#import <Foundation/Foundation.h>

NS_ASSUME_NONNULL_BEGIN

/// 连接状态
typedef NS_ENUM(NSInteger, NovaConnectionState) {
    NovaConnectionStateDisconnected = 0,
    NovaConnectionStateConnecting   = 1,
    NovaConnectionStateConnected    = 2,
    NovaConnectionStateAuthenticated = 3,
    NovaConnectionStateReconnecting = 4,
};

/// 登录结果
@interface NovaLoginResult : NSObject
@property (nonatomic, assign) int32_t code;
@property (nonatomic, copy) NSString *msg;
@property (nonatomic, copy) NSString *uid;
@property (nonatomic, copy) NSString *nickname;
@property (nonatomic, copy) NSString *avatar;
@end

/// 收到的推送消息
@interface NovaPushMessage : NSObject
@property (nonatomic, assign) int64_t conversationId;
@property (nonatomic, copy) NSString *senderUid;
@property (nonatomic, copy) NSString *content;
@property (nonatomic, assign) int64_t serverSeq;
@property (nonatomic, assign) int64_t serverTime;
@property (nonatomic, assign) int32_t msgType;
@end

/// NovaClient 代理协议
@protocol NovaClientDelegate <NSObject>
@optional
- (void)novaClient:(id)client didChangeState:(NovaConnectionState)state;
- (void)novaClient:(id)client didReceiveMessage:(NovaPushMessage *)message;
- (void)novaClient:(id)client didLoginWithResult:(NovaLoginResult *)result;
@end

/// NovaIIM 客户端 — iOS 封装
@interface NovaClient : NSObject

@property (nonatomic, weak, nullable) id<NovaClientDelegate> delegate;
@property (nonatomic, readonly) NovaConnectionState connectionState;

/// 单例
+ (instancetype)shared;

/// 配置服务器
- (void)configureWithHost:(NSString *)host
                     port:(uint16_t)port
                 deviceId:(NSString *)deviceId;

/// 连接服务器
- (void)connect;

/// 断开连接
- (void)disconnect;

/// 登录
- (void)loginWithEmail:(NSString *)email
              password:(NSString *)password;

/// 登出
- (void)logout;

/// 发送文本消息
- (void)sendTextMessage:(NSString *)content
         conversationId:(int64_t)conversationId;

/// 是否已登录
@property (nonatomic, readonly) BOOL isLoggedIn;

@end

NS_ASSUME_NONNULL_END
