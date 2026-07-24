#include "services/HttpClient.h"

#include "core/Branding.h"
#include "Version.h"

#include <windows.h>
#include <winhttp.h>

#include <vector>

#include <cstdlib>

namespace cm {

namespace {

std::wstring ToWide(const std::string& value) {
    if (value.empty()) return L"";
    const int size = MultiByteToWideChar(CP_UTF8, 0, value.c_str(), -1, nullptr, 0);
    if (size <= 0) return L"";
    std::wstring wide(static_cast<size_t>(size), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, value.c_str(), -1, wide.data(), size);
    if (!wide.empty() && wide.back() == L'\0') {
        wide.pop_back();
    }
    return wide;
}

bool ParseUrl(const std::string& url, std::wstring& host, std::wstring& path, bool& secure,
              INTERNET_PORT& port) {
    secure = url.rfind("https://", 0) == 0;
    const auto schemeLen = secure ? 8u : (url.rfind("http://", 0) == 0 ? 7u : 0u);
    if (schemeLen == 0) return false;

    port = secure ? INTERNET_DEFAULT_HTTPS_PORT : INTERNET_DEFAULT_HTTP_PORT;

    const auto withoutScheme = url.substr(schemeLen);
    const auto slash = withoutScheme.find('/');
    auto hostPart = slash == std::string::npos ? withoutScheme : withoutScheme.substr(0, slash);
    path = slash == std::string::npos ? L"/" : ToWide(withoutScheme.substr(slash));

    const auto colon = hostPart.rfind(':');
    if (colon != std::string::npos) {
        const auto portStr = hostPart.substr(colon + 1);
        const auto hostOnly = hostPart.substr(0, colon);
        if (!hostOnly.empty() && !portStr.empty()) {
            char* end = nullptr;
            const unsigned long parsed = std::strtoul(portStr.c_str(), &end, 10);
            if (end != portStr.c_str() && *end == '\0' && parsed > 0 && parsed <= 65535) {
                port = static_cast<INTERNET_PORT>(parsed);
                hostPart = hostOnly;
            }
        }
    }

    host = ToWide(hostPart);
    return !host.empty();
}

bool UsesModuleUserAgent(const std::wstring& host) {
    if (host == L"bhm.blishhud.com" || host == L"assets.gw2dat.com") {
        return true;
    }
#if defined(CM_LOCAL_TEST)
    return host == L"localhost";
#else
    return host == L"addons.soeed.com";
#endif
}

std::string ReadHeader(HINTERNET request, DWORD headerId) {
    DWORD size = 0;
    WinHttpQueryHeaders(request, headerId, WINHTTP_HEADER_NAME_BY_INDEX, WINHTTP_NO_OUTPUT_BUFFER,
                        &size, WINHTTP_NO_HEADER_INDEX);
    if (GetLastError() != ERROR_INSUFFICIENT_BUFFER || size == 0) {
        return {};
    }
    std::vector<wchar_t> buffer(size / sizeof(wchar_t) + 1, L'\0');
    if (!WinHttpQueryHeaders(request, headerId, WINHTTP_HEADER_NAME_BY_INDEX, buffer.data(), &size,
                             WINHTTP_NO_HEADER_INDEX)) {
        return {};
    }
    const int utf8Size =
        WideCharToMultiByte(CP_UTF8, 0, buffer.data(), -1, nullptr, 0, nullptr, nullptr);
    if (utf8Size <= 0) return {};
    std::string out(static_cast<size_t>(utf8Size), '\0');
    WideCharToMultiByte(CP_UTF8, 0, buffer.data(), -1, out.data(), utf8Size, nullptr, nullptr);
    if (!out.empty() && out.back() == '\0') out.pop_back();
    return out;
}

}  // namespace

HttpResponse HttpRequestEx(const std::string& method,
                           const std::string& url,
                           const std::string& body,
                           const HttpRequestOptions& options) {
    HttpResponse response;
    std::wstring host;
    std::wstring path;
    bool secure = false;
    INTERNET_PORT port = INTERNET_DEFAULT_HTTP_PORT;
    if (!ParseUrl(url, host, path, secure, port)) {
        return response;
    }

    const bool moduleUserAgent = UsesModuleUserAgent(host);
    const std::wstring sessionAgent =
        moduleUserAgent ? ToWide(V_HTTP_USER_AGENT) : L"CommanderMarkers-GW2API/1.0.0";

    HINTERNET session = WinHttpOpen(sessionAgent.c_str(), WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
                                    WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
    if (!session) return response;

    WinHttpSetTimeouts(session, options.connectTimeoutMs, options.connectTimeoutMs,
                       options.readTimeoutMs, options.readTimeoutMs);

    HINTERNET connection = WinHttpConnect(session, host.c_str(), port, 0);
    if (!connection) {
        WinHttpCloseHandle(session);
        return response;
    }

    const DWORD flags = secure ? WINHTTP_FLAG_SECURE : 0;
    HINTERNET request = WinHttpOpenRequest(
        connection, ToWide(method).c_str(), path.c_str(), nullptr, WINHTTP_NO_REFERER,
        WINHTTP_DEFAULT_ACCEPT_TYPES, flags);
    if (!request) {
        WinHttpCloseHandle(connection);
        WinHttpCloseHandle(session);
        return response;
    }

    std::wstring headers;
    if (moduleUserAgent) {
        headers = L"User-Agent: " + ToWide(V_HTTP_USER_AGENT) + L"\r\n";
    }
    if (!options.bearerToken.empty()) {
        headers += L"Authorization: Bearer " + ToWide(options.bearerToken) + L"\r\n";
    }
    if (!options.ifNoneMatch.empty()) {
        headers += L"If-None-Match: " + ToWide(options.ifNoneMatch) + L"\r\n";
    }
    if (!body.empty() && !options.contentType.empty()) {
        headers += L"Content-Type: " + ToWide(options.contentType) + L"\r\n";
    }

    const void* requestData = body.empty() ? WINHTTP_NO_REQUEST_DATA : body.data();
    const DWORD requestSize = body.empty() ? 0 : static_cast<DWORD>(body.size());

    const BOOL sent = WinHttpSendRequest(
        request, headers.empty() ? WINHTTP_NO_ADDITIONAL_HEADERS : headers.c_str(),
        headers.empty() ? 0 : static_cast<DWORD>(-1),
        const_cast<void*>(requestData), requestSize, requestSize, 0);
    if (!sent || !WinHttpReceiveResponse(request, nullptr)) {
        WinHttpCloseHandle(request);
        WinHttpCloseHandle(connection);
        WinHttpCloseHandle(session);
        return response;
    }

    DWORD statusCode = 0;
    DWORD statusSize = sizeof(statusCode);
    if (WinHttpQueryHeaders(request, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
                            WINHTTP_HEADER_NAME_BY_INDEX, &statusCode, &statusSize,
                            WINHTTP_NO_HEADER_INDEX)) {
        response.statusCode = static_cast<int>(statusCode);
    }
    response.etag = ReadHeader(request, WINHTTP_QUERY_ETAG);

    DWORD available = 0;
    do {
        if (!WinHttpQueryDataAvailable(request, &available)) break;
        if (available == 0) break;

        std::vector<char> buffer(available);
        DWORD read = 0;
        if (!WinHttpReadData(request, buffer.data(), available, &read)) break;
        response.body.append(buffer.data(), buffer.data() + read);
    } while (available > 0);

    WinHttpCloseHandle(request);
    WinHttpCloseHandle(connection);
    WinHttpCloseHandle(session);

    return response;
}

HttpResponse HttpGetUrlEx(const std::string& url, const HttpRequestOptions& options) {
    return HttpRequestEx("GET", url, {}, options);
}

std::optional<std::string> HttpGetUrl(const std::string& url,
                                      const HttpRequestOptions& options) {
    const auto response = HttpGetUrlEx(url, options);
    if (response.statusCode < 200 || response.statusCode >= 300 || response.body.empty()) {
        return std::nullopt;
    }
    return response.body;
}

HttpResponse HttpPostJsonUrlEx(const std::string& url,
                               const std::string& jsonBody,
                               const HttpRequestOptions& options) {
    return HttpRequestEx("POST", url, jsonBody, options);
}

}  // namespace cm
