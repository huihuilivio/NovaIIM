#include "http_client.h"

#include <hv/AsyncHttpClient.h>

#include <spdlog/spdlog.h>

namespace nova::client {

struct HttpClient::Impl {
    hv::AsyncHttpClient client;
    std::map<std::string, std::string> default_headers;
    int timeout_s = 10;
    std::string base_url;
};

HttpClient::HttpClient() : impl_(std::make_unique<Impl>()) {}
HttpClient::~HttpClient() = default;

void HttpClient::SetHeader(const std::string& key, const std::string& value) {
    impl_->default_headers[key] = value;
}

void HttpClient::RemoveHeader(const std::string& key) {
    impl_->default_headers.erase(key);
}

void HttpClient::SetTimeout(int timeout_ms) {
    impl_->timeout_s = (timeout_ms + 999) / 1000;  // 向上取整，至少 1s
    if (impl_->timeout_s < 1) impl_->timeout_s = 1;
}

void HttpClient::SetBaseUrl(const std::string& base_url) {
    impl_->base_url = base_url;
}

static std::string ResolveUrl(const std::string& base, const std::string& url) {
    if (url.find("://") != std::string::npos || base.empty()) {
        return url;
    }
    // base: "http://host:port", url: "/api/xxx"
    if (!base.empty() && base.back() == '/' && !url.empty() && url.front() == '/') {
        return base.substr(0, base.size() - 1) + url;
    }
    return base + url;
}

static HttpResponse ToResponse(const HttpResponsePtr& resp) {
    HttpResponse r;
    if (!resp) {
        spdlog::warn("[HttpClient] Request failed: no response");
        return r;
    }
    r.status_code = resp->status_code;
    r.body = resp->Body();
    for (auto& [k, v] : resp->headers) {
        r.headers[k] = v;
    }
    return r;
}

void HttpClient::Get(const std::string& url, ResponseCallback cb) {
    auto req = std::make_shared<HttpRequest>();
    req->method = HTTP_GET;
    req->url = ResolveUrl(impl_->base_url, url);
    req->timeout = impl_->timeout_s;
    for (auto& [k, v] : impl_->default_headers) {
        req->SetHeader(k.c_str(), v);
    }

    impl_->client.send(req, [cb = std::move(cb)](const HttpResponsePtr& resp) {
        if (cb) cb(ToResponse(resp));
    });
}

void HttpClient::Post(const std::string& url, const std::string& body, ResponseCallback cb) {
    auto req = std::make_shared<HttpRequest>();
    req->method = HTTP_POST;
    req->url = ResolveUrl(impl_->base_url, url);
    req->timeout = impl_->timeout_s;
    req->SetHeader("Content-Type", "application/json");
    for (auto& [k, v] : impl_->default_headers) {
        req->SetHeader(k.c_str(), v);
    }
    req->SetBody(body);

    impl_->client.send(req, [cb = std::move(cb)](const HttpResponsePtr& resp) {
        if (cb) cb(ToResponse(resp));
    });
}

void HttpClient::Put(const std::string& url, const std::string& body, ResponseCallback cb) {
    auto req = std::make_shared<HttpRequest>();
    req->method = HTTP_PUT;
    req->url = ResolveUrl(impl_->base_url, url);
    req->timeout = impl_->timeout_s;
    req->SetHeader("Content-Type", "application/json");
    for (auto& [k, v] : impl_->default_headers) {
        req->SetHeader(k.c_str(), v);
    }
    req->SetBody(body);

    impl_->client.send(req, [cb = std::move(cb)](const HttpResponsePtr& resp) {
        if (cb) cb(ToResponse(resp));
    });
}

void HttpClient::Delete(const std::string& url, ResponseCallback cb) {
    auto req = std::make_shared<HttpRequest>();
    req->method = HTTP_DELETE;
    req->url = ResolveUrl(impl_->base_url, url);
    req->timeout = impl_->timeout_s;
    for (auto& [k, v] : impl_->default_headers) {
        req->SetHeader(k.c_str(), v);
    }

    impl_->client.send(req, [cb = std::move(cb)](const HttpResponsePtr& resp) {
        if (cb) cb(ToResponse(resp));
    });
}

}  // namespace nova::client
