#ifndef EVENT_H
#define EVENT_H

#include <string>
#include <chrono>

enum class EventSeverity {
    INFO,
    WARNING,
    CRITICAL
};

struct Event {
    std::string machineId;
    std::string eventType;
    std::string message;
    EventSeverity severity;
    std::chrono::system_clock::time_point timestamp;

    static std::string severityToString(EventSeverity severity) {
        switch (severity) {
            case EventSeverity::INFO:     return "INFO";
            case EventSeverity::WARNING:  return "WARNING";
            case EventSeverity::CRITICAL: return "CRITICAL";
            default:                      return "UNKNOWN";
        }
    }
};

#endif
