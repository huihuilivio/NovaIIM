#include "cache_db.h"

#include <infra/logger.h>

#include <filesystem>

namespace nova::client {

static constexpr const char* kLogTag = "CacheDB";

CacheDatabase::~CacheDatabase() {
    Close();
}

bool CacheDatabase::Open(const std::string& data_dir, const std::string& uid) {
    if (open_) {
        Close();
    }

    // 构建路径: {data_dir}/{uid}/cache.db
    std::filesystem::path dir = std::filesystem::path(data_dir) / uid;
    std::error_code ec;
    std::filesystem::create_directories(dir, ec);
    if (ec) {
        NOVA_NLOG_ERROR(kLogTag, "Failed to create cache directory: {} ({})", dir.string(), ec.message());
        return false;
    }

    auto db_path = (dir / "cache.db").string();
    if (!db_.connect(db_path)) {
        NOVA_NLOG_ERROR(kLogTag, "Failed to open cache database: {}", db_path);
        return false;
    }

    // PRAGMA 配置
    db_.query_s<std::tuple<std::string>>("PRAGMA journal_mode=WAL");
    db_.query_s<std::tuple<int>>("PRAGMA foreign_keys=ON");
    db_.query_s<std::tuple<int>>("PRAGMA busy_timeout=3000");
    db_.query_s<std::tuple<int>>("PRAGMA synchronous=NORMAL");

    if (!InitSchema()) {
        NOVA_NLOG_ERROR(kLogTag, "Failed to initialize cache schema");
        db_.disconnect();
        return false;
    }

    open_ = true;
    NOVA_NLOG_INFO(kLogTag, "Cache database opened: {}", db_path);
    return true;
}

void CacheDatabase::Close() {
    if (open_) {
        db_.disconnect();
        open_ = false;
        NOVA_NLOG_INFO(kLogTag, "Cache database closed");
    }
}

bool CacheDatabase::InitSchema() {
    // 注册自增主键
    ormpp::add_auto_key_field("nova::client::CachedUser", "id");
    ormpp::add_auto_key_field("nova::client::CachedFriend", "id");
    ormpp::add_auto_key_field("nova::client::CachedMessage", "id");
    ormpp::add_auto_key_field("nova::client::CachedConversation", "id");
    ormpp::add_auto_key_field("nova::client::CachedGroup", "id");
    ormpp::add_auto_key_field("nova::client::CachedGroupMember", "id");
    ormpp::add_auto_key_field("nova::client::SyncState", "id");

    bool ok = true;

    ok = ok && db_.create_datatable<CachedUser>(ormpp_auto_key{"id"}, ormpp_unique{{"uid"}});
    ok = ok && db_.create_datatable<CachedFriend>(ormpp_auto_key{"id"}, ormpp_unique{{"uid"}});
    ok = ok && db_.create_datatable<CachedMessage>(ormpp_auto_key{"id"});
    ok = ok && db_.create_datatable<CachedConversation>(ormpp_auto_key{"id"}, ormpp_unique{{"conversation_id"}});
    ok = ok && db_.create_datatable<CachedGroup>(ormpp_auto_key{"id"}, ormpp_unique{{"conversation_id"}});
    ok = ok && db_.create_datatable<CachedGroupMember>(
        ormpp_auto_key{"id"}, ormpp_unique{{"conversation_id", "uid"}});
    ok = ok && db_.create_datatable<SyncState>(ormpp_auto_key{"id"}, ormpp_unique{{"conversation_id"}});

    // 索引
    db_.execute("CREATE INDEX IF NOT EXISTS idx_cmsg_conv_seq ON cached_messages(conversation_id, server_seq)");
    db_.execute("CREATE INDEX IF NOT EXISTS idx_cmsg_conv_time ON cached_messages(conversation_id, server_time)");
    db_.execute("CREATE INDEX IF NOT EXISTS idx_cmsg_client_id ON cached_messages(client_msg_id)");
    db_.execute("CREATE INDEX IF NOT EXISTS idx_cconv_conv_id ON cached_conversations(conversation_id)");
    db_.execute("CREATE INDEX IF NOT EXISTS idx_cgrp_conv_id ON cached_groups(conversation_id)");
    db_.execute("CREATE INDEX IF NOT EXISTS idx_cgm_conv_id ON cached_group_members(conversation_id)");
    db_.execute("CREATE INDEX IF NOT EXISTS idx_sync_conv_id ON sync_state(conversation_id)");

    if (!ok) {
        NOVA_NLOG_ERROR(kLogTag, "Failed to create cache tables");
    }
    return ok;
}

}  // namespace nova::client
