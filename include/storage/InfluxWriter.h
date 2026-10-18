#ifndef INFLUX_WRITER_H
#define INFLUX_WRITER_H

#include <string>
#include <vector>
#include "telemetry/Telemetry.h"
#include "events/Event.h"

class InfluxWriter {
private:
    std::string url;
    std::string organization;
    std::string bucket;
    std::string token;

    bool sendLineProtocol(const std::string& lineData);

public:
    InfluxWriter(const std::string& url,
                 const std::string& organization,
                 const std::string& bucket,
                 const std::string& token);

    bool write(const Telemetry& telemetry);

    bool writeBatch(const std::vector<Telemetry>& batch);

    bool writeEvent(const Event& event);

    bool writeEvents(const std::vector<Event>& events);
};

#endif