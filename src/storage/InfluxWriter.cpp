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

bool InfluxWriter::sendLineProtocol(const std::string& lineData) {
    if (url.empty() ||
        organization.empty() ||
        bucket.empty() ||
        token.empty()) {
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

    components.dwStructSize = sizeof(components);
    components.lpszHostName = host;
    components.dwHostNameLength = sizeof(host) / sizeof(wchar_t);
    components.lpszUrlPath = path;
    components.dwUrlPathLength = sizeof(path) / sizeof(wchar_t);

    if (!WinHttpCrackUrl(
            wideUrl.c_str(),
            static_cast<DWORD>(wideUrl.length()),
            0,
            &components)) {
        return false;
    }

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
        path,
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
    std::string line =
        "telemetry,machine=" + telemetry.machineId +
        ",device=" + telemetry.deviceId +
        ",sensor=" + telemetry.sensorType +
        ",quality=" + telemetry.quality +
        ",unit=" + (telemetry.unit.empty() ? "none" : telemetry.unit) +
        " value=" + std::to_string(telemetry.value);

    auto timestamp =
        std::chrono::duration_cast<std::chrono::nanoseconds>(
            telemetry.timestamp.time_since_epoch()
        ).count();

    line += " " + std::to_string(timestamp);

    return sendLineProtocol(line);
}

bool InfluxWriter::writeBatch(const std::vector<Telemetry>& batch) {
    if (batch.empty()) {
        return true;
    }

    std::ostringstream oss;

    for (size_t i = 0; i < batch.size(); ++i) {
        const auto& t = batch[i];

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

        if (i < batch.size() - 1) {
            oss << "\n";
        }
    }

    return sendLineProtocol(oss.str());
}

bool InfluxWriter::writeEvent(const Event& event) {
    std::string line =
        "events,machine=" + event.machineId +
        ",event_type=" + event.eventType +
        ",severity=" + Event::severityToString(event.severity) +
        " message=\"" + event.message + "\"";

    auto timestamp =
        std::chrono::duration_cast<std::chrono::nanoseconds>(
            event.timestamp.time_since_epoch()
        ).count();

    line += " " + std::to_string(timestamp);

    return sendLineProtocol(line);
}

bool InfluxWriter::writeEvents(const std::vector<Event>& events) {
    if (events.empty()) {
        return true;
    }

    std::ostringstream oss;

    for (size_t i = 0; i < events.size(); ++i) {
        const auto& e = events[i];

        oss << "events,machine=" << e.machineId
            << ",event_type=" << e.eventType
            << ",severity=" << Event::severityToString(e.severity)
            << " message=\"" << e.message << "\"";

        auto timestamp =
            std::chrono::duration_cast<std::chrono::nanoseconds>(
                e.timestamp.time_since_epoch()
            ).count();

        oss << " " << timestamp;

        if (i < events.size() - 1) {
            oss << "\n";
        }
    }

    return sendLineProtocol(oss.str());
}