#pragma once
// NovaIIM 协议错误码 —— 客户端 / 服务端共用
//
// 约定：code=0 表示成功，<0 通用错误，>0 业务错误
// 业务错误码分段：user 1001-1099, kick 1101-1199, msg 2001-2099,
//   file 3001-3099, sync 4001-4099, friend 5001-5099, conv 6001-6099, group 7001-7099

#include <cstdint>

namespace nova::errc {

struct Error {
    int32_t code;
    const char* msg;
};

// ================================================================
// 通用
// ================================================================
// clang-format off
inline constexpr Error kOk                      {0,    "ok"};
inline constexpr Error kInvalidBody             {-1,   "invalid body"};
inline constexpr Error kNotAuthenticated        {-2,   "not authenticated"};
inline constexpr Error kDatabaseError           {-100, "database error"};
inline constexpr Error kServerBusy              {-503, "server busy"};
// clang-format on

// ================================================================
// User (1001 - 1099)
// ================================================================
namespace user {

// clang-format off
inline constexpr Error kEmailRequired           {1001, "email is required"};
inline constexpr Error kInvalidCredentials      {1002, "invalid credentials"};
inline constexpr Error kPasswordRequired        {1003, "password is required"};
inline constexpr Error kUserBanned              {1004, "user is banned"};
inline constexpr Error kRateLimited             {1005, "too many attempts, try later"};

// ---- 注册 ----
inline constexpr Error kEmailInvalid            {1006, "invalid email format"};
inline constexpr Error kEmailTooLong            {1007, "email must be at most 255 characters"};
inline constexpr Error kEmailAlreadyExists      {1008, "email already registered"};
inline constexpr Error kNicknameRequired        {1010, "nickname is required"};
inline constexpr Error kNicknameTooLong         {1011, "nickname must be at most 100 characters"};
inline constexpr Error kNicknameInvalid         {1012, "nickname contains invalid characters"};
inline constexpr Error kPasswordTooShort        {1013, "password must be at least 6 characters"};
inline constexpr Error kPasswordTooLong         {1014, "password must be at most 128 characters"};
inline constexpr Error kRegisterFailed          {1016, "registration failed"};

// ---- 搜索 / 资料 ----    
inline constexpr Error kSearchKeywordEmpty      {1017, "search keyword is required"};
inline constexpr Error kSearchKeywordTooLong    {1018, "search keyword too long"};
inline constexpr Error kUserNotFound            {1019, "user not found"};
inline constexpr Error kNothingToUpdate         {1020, "nothing to update"};
inline constexpr Error kUpdateProfileFailed     {1021, "failed to update profile"};
inline constexpr Error kAvatarPathTooLong       {1022, "avatar path exceeds 512 characters"};
inline constexpr Error kAlreadyLoggedIn         {1023, "already logged in on this device"};
// clang-format on

}  // namespace user

// ================================================================
// Kick (1101 - 1199)
// ================================================================
namespace kick {

// clang-format off
inline constexpr Error kSameDeviceTypeRelogin    {1101, "your account logged in on another device of the same type"};
inline constexpr Error kAdminKick                {1102, "you have been kicked by the administrator"};
// clang-format on

}  // namespace kick

// ================================================================
// Message (2001 - 2099)
// ================================================================
namespace msg {

// clang-format off
inline constexpr Error kContentEmpty            {2001, "content is empty"};
inline constexpr Error kContentTooLarge         {2002, "content too large"};
inline constexpr Error kInvalidConversation     {2003, "invalid conversation_id"};
inline constexpr Error kConversationNotFound    {2004, "conversation not found"};
inline constexpr Error kNotMember               {2005, "not a member of this conversation"};
inline constexpr Error kMsgNotFound             {2006, "message not found"};
inline constexpr Error kRecallTimeout           {2007, "recall time limit exceeded"};
inline constexpr Error kRecallNoPermission      {2008, "no permission to recall this message"};
inline constexpr Error kRecallAlready           {2009, "message already recalled"};
// clang-format on

}  // namespace msg

// ================================================================
// File (3001 - 3099)
// ================================================================
namespace file {

// clang-format off
inline constexpr Error kFileNameRequired         {3001, "file_name is required"};
inline constexpr Error kFileSizeInvalid          {3002, "invalid file_size"};
inline constexpr Error kFileSizeTooLarge         {3003, "file too large"};
inline constexpr Error kMimeTypeRequired         {3004, "mime_type is required"};
inline constexpr Error kFileNotFound             {3005, "file not found"};
inline constexpr Error kUploadFailed             {3006, "upload failed"};
inline constexpr Error kInvalidFileType          {3007, "invalid file_type"};
// clang-format on

}  // namespace file

// ================================================================
// Sync (4001 - 4099)
// ================================================================
namespace sync {

// clang-format off
inline constexpr Error kNotMember {4001, "not a member of this conversation"};
// clang-format on

}  // namespace sync

// ================================================================
// Friend (5001 - 5099)
// ================================================================
namespace friend_ {

// clang-format off
inline constexpr Error kCannotAddSelf       {5001, "cannot add yourself"};
inline constexpr Error kAlreadyFriends      {5002, "already friends"};
inline constexpr Error kRequestPending      {5003, "friend request already pending"};
inline constexpr Error kRequestNotFound     {5004, "friend request not found"};
inline constexpr Error kNotFriends          {5005, "not friends"};
inline constexpr Error kAlreadyBlocked      {5006, "user already blocked"};
inline constexpr Error kNotBlocked          {5007, "user not blocked"};
inline constexpr Error kBlockedByTarget     {5008, "blocked by target user"};
inline constexpr Error kTargetUidRequired   {5009, "target_uid is required"};
inline constexpr Error kRequestIdRequired   {5010, "request_id is required"};
inline constexpr Error kInvalidAction       {5011, "action must be accept or reject"};
// clang-format on

}  // namespace friend_

// ================================================================
// Conversation (6001 - 6099)
// ================================================================
namespace conv {

// clang-format off
inline constexpr Error kNotMember                {6001, "not a member of this conversation"};
inline constexpr Error kConversationNotFound     {6002, "conversation not found"};
inline constexpr Error kAlreadyMuted             {6003, "conversation already muted"};
inline constexpr Error kNotMuted                 {6004, "conversation not muted"};
inline constexpr Error kAlreadyPinned            {6005, "conversation already pinned"};
inline constexpr Error kNotPinned                {6006, "conversation not pinned"};
// clang-format on

}  // namespace conv

// ================================================================
// Group (7001 - 7099)
// ================================================================
namespace group {

// clang-format off
inline constexpr Error kNameRequired             {7001, "group name is required"};
inline constexpr Error kNameTooLong              {7002, "group name must be at most 100 characters"};
inline constexpr Error kNoticeTooLong            {7003, "notice must be at most 1000 characters"};
inline constexpr Error kNotEnoughMembers         {7004, "need at least 2 initial members"};
inline constexpr Error kNotMember                {7005, "not a member of this group"};
inline constexpr Error kNotOwner                 {7006, "only group owner can do this"};
inline constexpr Error kNotAdminOrOwner          {7007, "only admin or owner can do this"};
inline constexpr Error kCannotKickSelf           {7008, "cannot kick yourself"};
inline constexpr Error kCannotKickHigherRole     {7009, "cannot kick higher role member"};
inline constexpr Error kOwnerCannotLeave         {7010, "owner cannot leave, dismiss or transfer first"};
inline constexpr Error kAlreadyMember            {7011, "already a member"};
inline constexpr Error kGroupNotFound            {7012, "group not found"};
inline constexpr Error kRequestPending           {7013, "join request already pending"};
inline constexpr Error kRequestNotFound          {7014, "join request not found"};
inline constexpr Error kInvalidRole              {7015, "invalid role value"};
inline constexpr Error kCannotSetOwner           {7016, "cannot set owner role via this command"};
inline constexpr Error kNothingToUpdate          {7017, "nothing to update"};
// clang-format on

}  // namespace group

}  // namespace nova::errc
