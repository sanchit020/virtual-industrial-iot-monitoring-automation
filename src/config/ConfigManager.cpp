#include "config/ConfigManager.h"

#include <fstream>
#include <sstream>
#include <algorithm>

namespace {
std::string extractString(const std::string& content, const std::string& key) {
    std::string search = "\"" + key + "\"";
    size_t pos = content.find(search);
    if (pos == std::string::npos) {
        return "";
    }
    size_t colon = content.find(':', pos + search.length());
    if (colon == std::string::npos) {
        return "";
    }
    size_t firstQuote = content.find('\"', colon + 1);
    if (firstQuote == std::string::npos) {
        return "";
    }
    size_t secondQuote = content.find('\"', firstQuote + 1);
    if (secondQuote == std::string::npos) {
        return "";
    }
    return content.substr(firstQuote + 1, secondQuote - firstQuote - 1);
}

double extractDouble(const std::string& content, const std::string& key, double defaultVal) {
    std::string search = "\"" + key + "\"";
    size_t pos = content.find(search);
    if (pos == std::string::npos) {
        return defaultVal;
    }
    size_t colon = content.find(':', pos + search.length());
    if (colon == std::string::npos) {
        return defaultVal;
    }
    size_t start = content.find_first_of("0123456789.-", colon + 1);
    if (start == std::string::npos) {
        return defaultVal;
    }
    size_t end = content.find_first_not_of("0123456789.-", start);
    std::string valStr = (end == std::string::npos) ? content.substr(start) : content.substr(start, end - start);
    try {
        return std::stod(valStr);
    } catch (...) {
        return defaultVal;
    }
}

int extractInt(const std::string& content, const std::string& key, int defaultVal) {
    return static_cast<int>(extractDouble(content, key, defaultVal));
}
}

bool ConfigManager::loadFromFile(const std::string& filePath) {
    std::ifstream file(filePath);
    if (!file.is_open()) {
        config.machines = {
            {"machine1", "Machine A"},
            {"machine2", "Machine B"},
            {"machine3", "Machine C"}
        };
        return false;
    }

    std::stringstream buffer;
    buffer << file.rdbuf();
    std::string content = buffer.str();

    std::string fName = extractString(content, "name");
    if (!fName.empty()) {
        config.factoryName = fName;
    }

    std::string broker = extractString(content, "broker");
    if (!broker.empty()) {
        config.mqttBroker = broker;
    }

    config.mqttPort = extractInt(content, "port", config.mqttPort);

    std::string infUrl = extractString(content, "url");
    if (!infUrl.empty()) {
        config.influxUrl = infUrl;
    }

    std::string infOrg = extractString(content, "org");
    if (!infOrg.empty()) {
        config.influxOrg = infOrg;
    }

    std::string infBucket = extractString(content, "bucket");
    if (!infBucket.empty()) {
        config.influxBucket = infBucket;
    }

    std::string infToken = extractString(content, "token");
    if (!infToken.empty()) {
        config.influxToken = infToken;
    }

    config.fanThreshold = extractDouble(content, "fan_threshold_c", config.fanThreshold);
    config.pumpThreshold = extractDouble(content, "pump_threshold_c", config.pumpThreshold);
    config.shutdownThreshold = extractDouble(content, "shutdown_threshold_c", config.shutdownThreshold);

    size_t machinesPos = content.find("\"machines\"");
    if (machinesPos != std::string::npos) {
        size_t arrayStart = content.find('[', machinesPos);
        size_t arrayEnd = content.find(']', arrayStart);
        if (arrayStart != std::string::npos && arrayEnd != std::string::npos) {
            std::string machinesArray = content.substr(arrayStart, arrayEnd - arrayStart + 1);

            size_t currentPos = 0;
            while (true) {
                size_t objStart = machinesArray.find('{', currentPos);
                if (objStart == std::string::npos) {
                    break;
                }
                size_t objEnd = machinesArray.find('}', objStart);
                if (objEnd == std::string::npos) {
                    break;
                }

                std::string objStr = machinesArray.substr(objStart, objEnd - objStart + 1);
                std::string id = extractString(objStr, "id");
                std::string name = extractString(objStr, "name");

                if (!id.empty()) {
                    config.machines.push_back({id, name.empty() ? id : name});
                }

                currentPos = objEnd + 1;
            }
        }
    }

    if (config.machines.empty()) {
        config.machines = {
            {"machine1", "Machine A"},
            {"machine2", "Machine B"},
            {"machine3", "Machine C"}
        };
    }

    return true;
}

const FactoryConfig& ConfigManager::getConfig() const {
    return config;
}
