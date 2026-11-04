#pragma once
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <winsock2.h>
#include <ws2tcpip.h>
#include <wininet.h>
#include <string>
#include <vector>
#include <iostream>
#include <chrono>
#include <sstream>
#include "json.hpp"

#pragma comment(lib, "ws2_32.lib")
#pragma comment(lib, "wininet.lib")

namespace Mercenary {

// ================= UNIVERSAL DNS & IP HOSTNAME RESOLVER =================

inline bool resolveHostToSockaddr(const std::string& host, uint16_t port, sockaddr_in& outAddr) {
    std::memset(&outAddr, 0, sizeof(outAddr));
    outAddr.sin_family = AF_INET;
    outAddr.sin_port = htons(port);

    if (host.empty() || host == "auto" || host == "localhost") {
        outAddr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
        return true;
    }

    // 1. Try fast dotted-decimal numeric IPv4 first
    if (inet_pton(AF_INET, host.c_str(), &outAddr.sin_addr) == 1) {
        return true;
    }

    // 2. Fall back to universal getaddrinfo for domain names, DDNS, hostnames
    addrinfo hints{};
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_DGRAM;
    addrinfo* res = nullptr;

    int status = getaddrinfo(host.c_str(), nullptr, &hints, &res);
    if (status == 0 && res != nullptr) {
        sockaddr_in* resolved = reinterpret_cast<sockaddr_in*>(res->ai_addr);
        outAddr.sin_addr = resolved->sin_addr;
        freeaddrinfo(res);
        return true;
    }

    return false;
}

// ================= CLOUD MASTER REGISTRY COORDINATOR =================

class CloudDiscoveryCoordinator {
public:
    static constexpr const char* SUPABASE_REST_URL = 
        "https://idstsvcwftitmgzaqyhm.supabase.co/rest/v1/mercenary_shards";
    static constexpr const char* SUPABASE_ACTIVE_VIEW_URL = 
        "https://idstsvcwftitmgzaqyhm.supabase.co/rest/v1/active_shards";
    static constexpr const char* SUPABASE_ANON_KEY = 
        "eyJhbGciOiJIUzI1NiIsInR5cCI6IkpXVCJ9.eyJpc3MiOiJzdXBhYmFzZSIsInJlZiI6Imlkc3RzdmN3ZnRpdG1nemFxeWhtIiwicm9sZSI6ImFub24iLCJpYXQiOjE3NzAwOTExNjksImV4cCI6MjA4NTY2NzE2OX0.sYXgiyTinecapxQm3T4JKGdbjzMBGxCx71ZjnYYCPrU";

    // Non-blocking / fast Public IPv4 Auto-Detection via echo services
    static std::string detectPublicIP() {
        const char* providers[] = {
            "https://api.ipify.org",
            "https://icanhazip.com"
        };

        for (const char* url : providers) {
            std::string result = httpGetRaw(url);
            // Clean up trailing whitespace/newlines
            while (!result.empty() && (result.back() == '\r' || result.back() == '\n' || isspace((unsigned char)result.back()))) {
                result.pop_back();
            }
            if (!result.empty() && result.find('.') != std::string::npos && result.length() <= 15) {
                return result;
            }
        }
        return "127.0.0.1";
    }

    // Register or upsert this server instance into the cloud registry
    static bool registerServerNode(const std::string& shardId,
                                   const std::string& name,
                                   const std::string& region,
                                   const std::string& host,
                                   uint16_t port,
                                   const std::string& role = "HexMaster",
                                   int activePlayers = 0,
                                   int maxCapacity = 3000) {
        nlohmann::json payload = {
            {"id", shardId},
            {"name", name},
            {"region", region},
            {"host", host},
            {"port", port},
            {"role", role},
            {"active_players", activePlayers},
            {"max_capacity", maxCapacity},
            {"is_official", true}
        };

        std::string jsonStr = payload.dump();
        std::string extraHeaders = "Prefer: resolution=merge-duplicates\r\n";
        return httpPostJson(SUPABASE_REST_URL, jsonStr, extraHeaders);
    }

    // Heartbeat pulse updating timestamp and current live player count
    static bool pulseHeartbeat(const std::string& shardId, int activePlayers) {
        std::string url = std::string(SUPABASE_REST_URL) + "?id=eq." + shardId;
        nlohmann::json payload = {
            {"active_players", activePlayers}
        };
        // Supabase PATCH updates the row and automatically triggers updated_at / we can update last_heartbeat
        // In PostgreSQL: we can post an upsert or patch. To ensure last_heartbeat refreshes, upsert is ideal.
        return httpPatchJson(url, payload.dump());
    }

    // Cleanly unregister this server when shutting down
    static bool unregisterServerNode(const std::string& shardId) {
        std::string url = std::string(SUPABASE_REST_URL) + "?id=eq." + shardId;
        return httpDelete(url);
    }

    // Fetch list of active shards from Supabase REST
    static std::string fetchActiveShardsJson() {
        std::string url = std::string(SUPABASE_ACTIVE_VIEW_URL) + "?select=*&order=last_heartbeat.desc";
        return httpGetRaw(url.c_str(), true);
    }

private:
    static std::string httpGetRaw(const char* url, bool useAuth = false) {
        HINTERNET hInternet = InternetOpenA("MercenaryEngine/1.0", INTERNET_OPEN_TYPE_PRECONFIG, NULL, NULL, 0);
        if (!hInternet) return "";

        DWORD netFlags = INTERNET_FLAG_RELOAD | INTERNET_FLAG_NO_CACHE_WRITE | INTERNET_FLAG_SECURE | INTERNET_FLAG_IGNORE_REDIRECT_TO_HTTPS;
        std::string headers;
        if (useAuth) {
            headers = std::string("apikey: ") + SUPABASE_ANON_KEY + "\r\nAuthorization: Bearer " + SUPABASE_ANON_KEY + "\r\n";
        }

        HINTERNET hUrl = InternetOpenUrlA(hInternet, url, 
                                          headers.empty() ? NULL : headers.c_str(), 
                                          static_cast<DWORD>(headers.length()), 
                                          netFlags, 0);
        if (!hUrl) {
            InternetCloseHandle(hInternet);
            return "";
        }

        std::string body;
        char buffer[4096];
        DWORD bytesRead = 0;
        while (InternetReadFile(hUrl, buffer, sizeof(buffer), &bytesRead) && bytesRead > 0) {
            body.append(buffer, bytesRead);
        }

        InternetCloseHandle(hUrl);
        InternetCloseHandle(hInternet);
        return body;
    }

    static bool httpPostJson(const std::string& urlStr, const std::string& jsonData, const std::string& extraHeaders = "") {
        return executeWinINetRequest(urlStr, "POST", jsonData, extraHeaders);
    }

    static bool httpPatchJson(const std::string& urlStr, const std::string& jsonData) {
        return executeWinINetRequest(urlStr, "PATCH", jsonData, "");
    }

    static bool httpDelete(const std::string& urlStr) {
        return executeWinINetRequest(urlStr, "DELETE", "", "");
    }

    static bool executeWinINetRequest(const std::string& urlStr, const std::string& method, const std::string& body, const std::string& extraHeaders) {
        // Parse URL
        URL_COMPONENTSA urlComp{};
        urlComp.dwStructSize = sizeof(urlComp);
        char host[256] = {0};
        char path[1024] = {0};
        urlComp.lpszHostName = host;
        urlComp.dwHostNameLength = sizeof(host);
        urlComp.lpszUrlPath = path;
        urlComp.dwUrlPathLength = sizeof(path);

        if (!InternetCrackUrlA(urlStr.c_str(), static_cast<DWORD>(urlStr.length()), 0, &urlComp)) {
            return false;
        }

        HINTERNET hInternet = InternetOpenA("MercenaryFleet/1.0", INTERNET_OPEN_TYPE_PRECONFIG, NULL, NULL, 0);
        if (!hInternet) return false;

        INTERNET_PORT port = urlComp.nPort;
        if (port == 0) port = (urlComp.nScheme == INTERNET_SCHEME_HTTPS) ? INTERNET_DEFAULT_HTTPS_PORT : INTERNET_DEFAULT_HTTP_PORT;

        HINTERNET hConnect = InternetConnectA(hInternet, host, port, NULL, NULL, INTERNET_SERVICE_HTTP, 0, 0);
        if (!hConnect) {
            InternetCloseHandle(hInternet);
            return false;
        }

        DWORD reqFlags = INTERNET_FLAG_RELOAD | INTERNET_FLAG_NO_CACHE_WRITE;
        if (urlComp.nScheme == INTERNET_SCHEME_HTTPS) {
            reqFlags |= INTERNET_FLAG_SECURE | INTERNET_FLAG_IGNORE_CERT_CN_INVALID | INTERNET_FLAG_IGNORE_CERT_DATE_INVALID;
        }

        HINTERNET hRequest = HttpOpenRequestA(hConnect, method.c_str(), path, NULL, NULL, NULL, reqFlags, 0);
        if (!hRequest) {
            InternetCloseHandle(hConnect);
            InternetCloseHandle(hInternet);
            return false;
        }

        std::string headers = std::string("apikey: ") + SUPABASE_ANON_KEY + "\r\n"
                            + "Authorization: Bearer " + SUPABASE_ANON_KEY + "\r\n"
                            + "Content-Type: application/json\r\n"
                            + extraHeaders;

        BOOL sent = HttpSendRequestA(hRequest, headers.c_str(), static_cast<DWORD>(headers.length()),
                                     body.empty() ? NULL : const_cast<char*>(body.c_str()),
                                     static_cast<DWORD>(body.length()));

        DWORD statusCode = 0;
        DWORD statusSize = sizeof(statusCode);
        if (sent) {
            HttpQueryInfoA(hRequest, HTTP_QUERY_STATUS_CODE | HTTP_QUERY_FLAG_NUMBER, &statusCode, &statusSize, NULL);
        }

        InternetCloseHandle(hRequest);
        InternetCloseHandle(hConnect);
        InternetCloseHandle(hInternet);

        return sent && (statusCode >= 200 && statusCode < 300);
    }
};

} // namespace Mercenary
