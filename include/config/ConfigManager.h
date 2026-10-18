#ifndef CONFIG_MANAGER_H
#define CONFIG_MANAGER_H

#include <string>
#include <vector>

struct MachineConfig {
    std::string id;
    std::string name;
};

struct FactoryConfig {
    std::string factoryName = "Industrial IoT Factory";
    std::vector<MachineConfig> machines;
    std::string mqttBroker = "127.0.0.1";
    int mqttPort = 1883;
    std::string influxUrl = "http://localhost:8086";
    std::string influxOrg = "athena";
    std::string influxBucket = "industrial_iot";
    std::string influxToken = "";
    double fanThreshold = 70.0;
    double pumpThreshold = 80.0;
    double shutdownThreshold = 95.0;
};

class ConfigManager {
private:
    FactoryConfig config;

public:
    bool loadFromFile(const std::string& filePath);
    const FactoryConfig& getConfig() const;
};

#endif
