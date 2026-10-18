#include "storage/InfluxWriter.h"

#include <windows.h>
#include <winhttp.h>

#include <chrono>
#include <string>
#include <sstream>

#pragma comment(lib, "winhttp.lib")

InfluxWriter::InfluxWriter(const std::string& url,
                           const std::string& organization,
                           const std::string& bucket,
                           const std::string& token)
    : url(url),
      organization(organization),
      bucket(bucket),
      token(token) {
}

size_t InfluxWriter::getPendingTelemetryCount() const {
    return pendingTelemetryRetry.size();
}

size_t InfluxWriter::getPendingEventCount() const {
    return pendingEventsRetry.size();
}

void InfluxWriter::clearRetryBuffers() {
    pendingTelemetryRetry.clear();
    pendingEventsRetry.clear();
}

bool InfluxWriter::sendLineProtocol(const std::string& lineData) {
    if (url.empty() ||
        organization.empty() ||
        bucket.empty() ||
        token.empty() ||
        lineData.empty()) {
        return false;
    }

    std::string endpoint =
        url +
        "/api/v2/write?org=" + organization +
        "&bucket=" + bucket +
        "&precision=ns";

    std::wstring wideUrl(endpoint.begin(), endpoint.end());

    URL_COMPONENTS components{};
    wchar_t host[256]{};
    wchar_t path[2048]{};
    wchar_t extra[2048]{};

    components.dwStructSize = sizeof(components);
    components.lpszHostName = host;
    components.dwHostNameLength = sizeof(host) / sizeof(wchar_t);
    components.lpszUrlPath = path;
    components.dwUrlPathLength = sizeof(path) / sizeof(wchar_t);
    components.lpszExtraInfo = extra;
    components.dwExtraInfoLength = sizeof(extra) / sizeof(wchar_t);

    if (!WinHttpCrackUrl(
            wideUrl.c_str(),
            static_cast<DWORD>(wideUrl.length()),
            0,
            &components)) {
        return false;
    }

    std::wstring fullPath = std::wstring(path) + std::wstring(extra);

    HINTERNET session = WinHttpOpen(
        L"VirtualIndustrialIoT/1.0",
        WINHTTP_ACCESS_TYPE_NO_PROXY,
        WINHTTP_NO_PROXY_NAME,
        WINHTTP_NO_PROXY_BYPASS,
        0
    );

    if (!session) {
        return false;
    }

    HINTERNET connection = WinHttpConnect(
        session,
        host,
        components.nPort,
        0
    );

    if (!connection) {
        WinHttpCloseHandle(session);
        return false;
    }

    HINTERNET request = WinHttpOpenRequest(
        connection,
        L"POST",
        fullPath.c_str(),
        nullptr,
        WINHTTP_NO_REFERER,
        WINHTTP_DEFAULT_ACCEPT_TYPES,
        components.nScheme == INTERNET_SCHEME_HTTPS
            ? WINHTTP_FLAG_SECURE
            : 0
    );

    if (!request) {
        WinHttpCloseHandle(connection);
        WinHttpCloseHandle(session);
        return false;
    }

    std::wstring authHeader =
        L"Authorization: Token " +
        std::wstring(token.begin(), token.end());

    WinHttpAddRequestHeaders(
        request,
        authHeader.c_str(),
        -1L,
        WINHTTP_ADDREQ_FLAG_ADD
    );

    WinHttpAddRequestHeaders(
        request,
        L"Content-Type: text/plain; charset=utf-8",
        -1L,
        WINHTTP_ADDREQ_FLAG_ADD
    );

    std::string data = lineData;

    BOOL sent = WinHttpSendRequest(
        request,
        WINHTTP_NO_ADDITIONAL_HEADERS,
        0,
        data.data(),
        static_cast<DWORD>(data.size()),
        static_cast<DWORD>(data.size()),
        0
    );

    if (!sent) {
        WinHttpCloseHandle(request);
        WinHttpCloseHandle(connection);
        WinHttpCloseHandle(session);
        return false;
    }

    BOOL received = WinHttpReceiveResponse(
        request,
        nullptr
    );

    if (!received) {
        WinHttpCloseHandle(request);
        WinHttpCloseHandle(connection);
        WinHttpCloseHandle(session);
        return false;
    }

    DWORD statusCode = 0;
    DWORD statusSize = sizeof(statusCode);

    WinHttpQueryHeaders(
        request,
        WINHTTP_QUERY_STATUS_CODE |
        WINHTTP_QUERY_FLAG_NUMBER,
        WINHTTP_HEADER_NAME_BY_INDEX,
        &statusCode,
        &statusSize,
        WINHTTP_NO_HEADER_INDEX
    );

    WinHttpCloseHandle(request);
    WinHttpCloseHandle(connection);
    WinHttpCloseHandle(session);

    return statusCode >= 200 && statusCode < 300;
}

bool InfluxWriter::write(const Telemetry& telemetry) {
    return writeBatch({telemetry});
}

bool InfluxWriter::writeBatch(const std::vector<Telemetry>& batch) {
    std::vector<Telemetry> toSend;
    toSend.reserve(pendingTelemetryRetry.size() + batch.size());
    for (const auto& item : pendingTelemetryRetry) {
        toSend.push_back(item);
    }
    for (const auto& item : batch) {
        toSend.push_back(item);
    }

    if (toSend.empty()) {
        return true;
    }

    std::ostringstream oss;
    for (size_t i = 0; i < toSend.size(); ++i) {
        const auto& t = toSend[i];

        oss << "telemetry,machine=" << t.machineId
            << ",device=" << t.deviceId
            << ",sensor=" << t.sensorType
            << ",quality=" << t.quality
            << ",unit=" << (t.unit.empty() ? "none" : t.unit)
            << " value=" << std::to_string(t.value);

        auto timestamp =
            std::chrono::duration_cast<std::chrono::nanoseconds>(
                t.timestamp.time_since_epoch()
            ).count();

        oss << " " << timestamp;

        if (i < toSend.size() - 1) {
            oss << "\n";
        }
    }

    bool success = sendLineProtocol(oss.str());
    if (success) {
        pendingTelemetryRetry.clear();
        return true;
    } else {
        pendingTelemetryRetry = std::move(toSend);
        if (pendingTelemetryRetry.size() > 5000) {
            pendingTelemetryRetry.erase(
                pendingTelemetryRetry.begin(),
                pendingTelemetryRetry.begin() + (pendingTelemetryRetry.size() - 5000)
            );
        }
        return false;
    }
}

bool InfluxWriter::writeEvent(const Event& event) {
    return writeEvents({event});
}

bool InfluxWriter::writeEvents(const std::vector<Event>& events) {
    std::vector<Event> toSend;
    toSend.reserve(pendingEventsRetry.size() + events.size());
    for (const auto& item : pendingEventsRetry) {
        toSend.push_back(item);
    }
    for (const auto& item : events) {
        toSend.push_back(item);
    }

    if (toSend.empty()) {
        return true;
    }

    std::ostringstream oss;
    for (size_t i = 0; i < toSend.size(); ++i) {
        const auto& e = toSend[i];

        oss << "events,machine=" << e.machineId
            << ",event_type=" << e.eventType
            << ",severity=" << Event::severityToString(e.severity)
            << " message=\"" << e.message << "\"";

        auto timestamp =
            std::chrono::duration_cast<std::chrono::nanoseconds>(
                e.timestamp.time_since_epoch()
            ).count();

        oss << " " << timestamp;

        if (i < toSend.size() - 1) {
            oss << "\n";
        }
    }

    bool success = sendLineProtocol(oss.str());
    if (success) {
        pendingEventsRetry.clear();
        return true;
    } else {
        pendingEventsRetry = std::move(toSend);
        if (pendingEventsRetry.size() > 1000) {
            pendingEventsRetry.erase(
                pendingEventsRetry.begin(),
                pendingEventsRetry.begin() + (pendingEventsRetry.size() - 1000)
            );
        }
        return false;
    }
}