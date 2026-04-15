#include "admin_session_dao_impl.h"

namespace nova {

bool AdminSessionDaoImpl::Insert(const AdminSession& session) {
    return db_.DB().insert(session) == 1;
}

bool AdminSessionDaoImpl::IsRevoked(const std::string& token_hash) {
    auto res = db_.DB().query_s<AdminSession>("token_hash=?", token_hash);
    if (res.empty()) return false;
    return res[0].revoked == 1;
}

bool AdminSessionDaoImpl::RevokeByUser(int64_t user_id) {
    auto sessions = db_.DB().query_s<AdminSession>("user_id=? AND revoked=0", user_id);
    for (auto& s : sessions) {
        s.revoked = 1;
        db_.DB().update_some<&AdminSession::revoked>(s);
    }
    return true;
}

bool AdminSessionDaoImpl::RevokeByTokenHash(const std::string& token_hash) {
    auto sessions = db_.DB().query_s<AdminSession>("token_hash=?", token_hash);
    if (sessions.empty()) return false;
    sessions[0].revoked = 1;
    return db_.DB().update_some<&AdminSession::revoked>(sessions[0]) == 1;
}

} // namespace nova
