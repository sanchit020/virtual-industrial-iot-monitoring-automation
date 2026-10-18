#ifndef EVENT_LOGGER_H
#define EVENT_LOGGER_H

#include <vector>
#include <string>
#include <iostream>
#include "events/Event.h"

class EventLogger {
private:
    std::vector<Event> events;

public:
    void log(const std::string& machineId,
             const std::string& eventType,
             const std::string& message,
             EventSeverity severity);

    const std::vector<Event>& getEvents() const;

    std::vector<Event> getEventsByMachine(const std::string& machineId) const;

    std::vector<Event> getEventsBySeverity(EventSeverity severity) const;

    size_t getEventCount() const;

    void clear();

    void printLast(size_t count = 1) const;
};

#endif
