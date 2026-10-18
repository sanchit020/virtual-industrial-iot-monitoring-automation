#include "config/ConfigManager.h"

#include <fstream>
#include <sstream>
#include <cctype>
#include <map>
#include <set>
#include <vector>
#include <stdexcept>

namespace {

enum class JsonType { Null, Boolean, Number, String, Array, Object };

struct JsonVal {
    JsonType type = JsonType::Null;
    bool boolVal = false;
    double numVal = 0.0;
    std::string strVal;
    std::vector<JsonVal> arrVal;
    std::map<std::string, JsonVal> objVal;

    bool contains(const std::string& key) const {
        return type == JsonType::Object && objVal.find(key) != objVal.end();
    }

    const JsonVal& operator[](const std::string& key) const {
        static const JsonVal emptyVal;
        if (type == JsonType::Object) {
            auto it = objVal.find(key);
            if (it != objVal.end()) {
                return it->second;
            }
        }
        return emptyVal;
    }

    std::string asString(const std::string& fallback = "") const {
        return type == JsonType::String ? strVal : fallback;
    }

    double asDouble(double fallback = 0.0) const {
        return type == JsonType::Number ? numVal : fallback;
    }

    int asInt(int fallback = 0) const {
        return type == JsonType::Number ? static_cast<int>(numVal) : fallback;
    }
};

class SimpleJsonParser {
private:
    const std::string& text;
    size_t index = 0;

    void skipWhitespace() {
        while (index < text.size() && (std::isspace(static_cast<unsigned char>(text[index])) || text[index] == '\0')) {
            index++;
        }
    }

    char peek() {
        skipWhitespace();
        if (index >= text.size()) return '\0';
        return text[index];
    }

    char get() {
        skipWhitespace();
        if (index >= text.size()) return '\0';
        return text[index++];
    }

    std::string parseString() {
        if (get() != '\"') return "";
        std::string s;
        while (index < text.size()) {
            char c = text[index++];
            if (c == '\"') {
                return s;
            }
            if (c == '\\' && index < text.size()) {
                char esc = text[index++];
                if (esc == '\"') s += '\"';
                else if (esc == '\\') s += '\\';
                else if (esc == '/') s += '/';
                else if (esc == 'b') s += '\b';
                else if (esc == 'f') s += '\f';
                else if (esc == 'n') s += '\n';
                else if (esc == 'r') s += '\r';
                else if (esc == 't') s += '\t';
                else s += esc;
            } else {
                s += c;
            }
        }
        return s;
    }

    JsonVal parseNumber() {
        skipWhitespace();
        size_t start = index;
        if (index < text.size() && (text[index] == '-' || text[index] == '+')) {
            index++;
        }
        while (index < text.size() && (std::isdigit(static_cast<unsigned char>(text[index])) || text[index] == '.' || text[index] == 'e' || text[index] == 'E' || text[index] == '+' || text[index] == '-')) {
            index++;
        }
        std::string s = text.substr(start, index - start);
        JsonVal val;
        val.type = JsonType::Number;
        try {
            val.numVal = std::stod(s);
        } catch (...) {
            val.numVal = 0.0;
        }
        return val;
    }

    JsonVal parseArray() {
        JsonVal val;
        val.type = JsonType::Array;
        get();
        skipWhitespace();
        if (peek() == ']') {
            get();
            return val;
        }
        while (index < text.size()) {
            val.arrVal.push_back(parseValue());
            skipWhitespace();
            char c = peek();
            if (c == ']') {
                get();
                break;
            }
            if (c == ',') {
                get();
            } else {
                break;
            }
        }
        return val;
    }

    JsonVal parseObject() {
        JsonVal val;
        val.type = JsonType::Object;
        get();
        skipWhitespace();
        if (peek() == '}') {
            get();
            return val;
        }
        while (index < text.size()) {
            skipWhitespace();
            if (peek() != '\"') break;
            std::string key = parseString();
            skipWhitespace();
            if (peek() == ':') {
                get();
            }
            val.objVal[key] = parseValue();
            skipWhitespace();
            char c = peek();
            if (c == '}') {
                get();
                break;
            }
            if (c == ',') {
                get();
            } else {
                break;
            }
        }
        return val;
    }

public:
    explicit SimpleJsonParser(const std::string& t) : text(t) {}

    JsonVal parseValue() {
        skipWhitespace();
        char c = peek();
        if (c == '{') return parseObject();
        if (c == '[') return parseArray();
        if (c == '\"') {
            JsonVal val;
            val.type = JsonType::String;
            val.strVal = parseString();
            return val;
        }
        if (c == 't' || c == 'T') {
            index += 4;
            JsonVal val;
            val.type = JsonType::Boolean;
            val.boolVal = true;
            return val;
        }
        if (c == 'f' || c == 'F') {
            index += 5;
            JsonVal val;
            val.type = JsonType::Boolean;
            val.boolVal = false;
            return val;
        }
        if (c == 'n' || c == 'N') {
            index += 4;
            JsonVal val;
            val.type = JsonType::Null;
            return val;
        }
        if (std::isdigit(static_cast<unsigned char>(c)) || c == '-' || c == '+') {
            return parseNumber();
        }
        return JsonVal{};
    }
};

}

ConfigManager::ConfigManager() {
    resetToDefaults();
}

void ConfigManager::resetToDefaults() {
    config.factoryName = "Industrial IoT Factory";
    config.machines.clear();
    config.machines.push_back({"machine1", "Machine A"});
    config.machines.push_back({"machine2", "Machine B"});
    config.machines.push_back({"machine3", "Machine C"});
    config.mqttBroker = "127.0.0.1";
    config.mqttPort = 1883;
    config.influxUrl = "http://localhost:8086";
    config.influxOrg = "athena";
    config.influxBucket = "industrial_iot";
    config.influxToken = "";
    config.fanThreshold = 70.0;
    config.pumpThreshold = 80.0;
    config.shutdownThreshold = 95.0;
}

bool ConfigManager::validateConfig() {
    bool valid = true;

    if (config.mqttPort <= 0 || config.mqttPort > 65535) {
        config.mqttPort = 1883;
        valid = false;
    }

    if (config.mqttBroker.empty()) {
        config.mqttBroker = "127.0.0.1";
        valid = false;
    }

    if (config.influxOrg.empty()) {
        config.influxOrg = "athena";
        valid = false;
    }

    if (config.influxBucket.empty()) {
        config.influxBucket = "industrial_iot";
        valid = false;
    }

    if (config.fanThreshold <= 0.0 ||
        config.pumpThreshold <= config.fanThreshold ||
        config.shutdownThreshold <= config.pumpThreshold) {
        config.fanThreshold = 70.0;
        config.pumpThreshold = 80.0;
        config.shutdownThreshold = 95.0;
        valid = false;
    }

    if (config.machines.empty()) {
        config.machines.push_back({"machine1", "Machine A"});
        config.machines.push_back({"machine2", "Machine B"});
        config.machines.push_back({"machine3", "Machine C"});
        valid = false;
    } else {
        std::set<std::string> seenIds;
        std::vector<MachineConfig> deduped;
        for (const auto& m : config.machines) {
            if (!m.id.empty() && seenIds.find(m.id) == seenIds.end()) {
                seenIds.insert(m.id);
                deduped.push_back(m);
            }
        }
        if (deduped.empty()) {
            config.machines.push_back({"machine1", "Machine A"});
            valid = false;
        } else {
            config.machines = deduped;
        }
    }

    return valid;
}

bool ConfigManager::loadFromFile(const std::string& filePath) {
    std::ifstream file(filePath);
    if (!file.is_open()) {
        resetToDefaults();
        return false;
    }

    std::stringstream buffer;
    buffer << file.rdbuf();
    std::string content = buffer.str();

    if (content.empty()) {
        resetToDefaults();
        return false;
    }

    SimpleJsonParser parser(content);
    JsonVal root = parser.parseValue();
    if (root.type != JsonType::Object) {
        resetToDefaults();
        return false;
    }

    config.machines.clear();

    const auto& fObj = root["factory"];
    if (fObj.type == JsonType::Object) {
        if (fObj.contains("name")) {
            config.factoryName = fObj["name"].asString(config.factoryName);
        }
        const auto& machArr = fObj["machines"];
        if (machArr.type == JsonType::Array) {
            for (const auto& item : machArr.arrVal) {
                if (item.type == JsonType::Object) {
                    std::string id = item["id"].asString();
                    std::string name = item["name"].asString(id);
                    if (!id.empty()) {
                        config.machines.push_back({id, name});
                    }
                }
            }
        }
    }

    const auto& mqttObj = root["mqtt"];
    if (mqttObj.type == JsonType::Object) {
        config.mqttBroker = mqttObj["broker"].asString(config.mqttBroker);
        config.mqttPort = mqttObj["port"].asInt(config.mqttPort);
    }

    const auto& influxObj = root["influxdb"];
    if (influxObj.type == JsonType::Object) {
        config.influxUrl = influxObj["url"].asString(config.influxUrl);
        config.influxOrg = influxObj["org"].asString(config.influxOrg);
        config.influxBucket = influxObj["bucket"].asString(config.influxBucket);
        config.influxToken = influxObj["token"].asString(config.influxToken);
    }

    const auto& autoObj = root["automation"];
    if (autoObj.type == JsonType::Object) {
        config.fanThreshold = autoObj["fan_threshold_c"].asDouble(config.fanThreshold);
        config.pumpThreshold = autoObj["pump_threshold_c"].asDouble(config.pumpThreshold);
        config.shutdownThreshold = autoObj["shutdown_threshold_c"].asDouble(config.shutdownThreshold);
    }

    validateConfig();
    return true;
}

const FactoryConfig& ConfigManager::getConfig() const {
    return config;
}

void ConfigManager::setConfig(const FactoryConfig& newConfig) {
    config = newConfig;
    validateConfig();
}
