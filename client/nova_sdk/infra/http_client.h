#pragma once
// HttpClient — libhv HTTP 客户端封装
//
// 纯网络层：异步 HTTP 请求（GET / POST / PUT / DELETE）
// 线程安全，可在任意线程调用

#include <functional>
#include <map>
#include <memory>
#include <string>

namespace nova::client {

/// HTTP 响应结构
struct HttpResponse {
    int status_code = 0;
    std::string body;
    std::map<std::string, std::string> headers;
};

class HttpClient {
public:
    using ResponseCallback = std::function<void(const HttpResponse& resp)>;

    HttpClient();
    ~HttpClient();

    HttpClient(const HttpClient&) = delete;
    HttpClient& operator=(const HttpClient&) = delete;

    /// 设置默认请求头（如 Authorization）
    void SetHeader(const std::string& key, const std::string& value);

    /// 移除默认请求头
    void RemoveHeader(const std::string& key);

    /// 设置请求超时（毫秒）
    void SetTimeout(int timeout_ms);

    /// 设置 base URL（后续请求可用相对路径）
    void SetBaseUrl(const std::string& base_url);

    /// 异步 GET
    void Get(const std::string& url, ResponseCallback cb);

    /// 异步 POST（JSON body）
    void Post(const std::string& url, const std::string& body, ResponseCallback cb);

    /// 异步 PUT（JSON body）
    void Put(const std::string& url, const std::string& body, ResponseCallback cb);

    /// 异步 DELETE
    void Delete(const std::string& url, ResponseCallback cb);

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

}  // namespace nova::client
