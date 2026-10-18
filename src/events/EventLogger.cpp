#include "events/EventLogger.h"

#include <algorithm>

void EventLogger::log(const std::string& machineId,
                      const std::string& eventType,
                      const std::string& message,
                      EventSeverity severity) {
    Event event{
        machineId,
        eventType,
        message,
        severity,
        std::chrono::system_clock::now()
    };

    events.push_back(event);

    std::string prefix;
    if (severity == EventSeverity::CRITICAL) {
        prefix = "[CRITICAL]";
    } else if (severity == EventSeverity::WARNING) {
        prefix = "[WARNING] ";
    } else {
        prefix = "[INFO]    ";
    }

    std::cout << prefix << " [" << machineId << "] "
              << eventType << ": " << message << std::endl;
}

const std::vector<Event>& EventLogger::getEvents() const {
    return events;
}

std::vector<Event> EventLogger::getEventsByMachine(
    const std::string& machineId) const {

    std::vector<Event> filtered;
    for (const auto& event : events) {
        if (event.machineId == machineId) {
            filtered.push_back(event);
        }
    }
    return filtered;
}

std::vector<Event> EventLogger::getEventsBySeverity(
    EventSeverity severity) const {

    std::vector<Event> filtered;
    for (const auto& event : events) {
        if (event.severity == severity) {
            filtered.push_back(event);
        }
    }
    return filtered;
}

size_t EventLogger::getEventCount() const {
    return events.size();
}

void EventLogger::clear() {
    events.clear();
}

void EventLogger::printLast(size_t count) const {
    size_t start = 0;
    if (events.size() > count) {
        start = events.size() - count;
    }

    for (size_t i = start; i < events.size(); ++i) {
        const auto& event = events[i];
        std::cout << "[" << Event::severityToString(event.severity) << "] "
                  << "[" << event.machineId << "] "
                  << event.eventType << ": "
                  << event.message << std::endl;
    }
}
