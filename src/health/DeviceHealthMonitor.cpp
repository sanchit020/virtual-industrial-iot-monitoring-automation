#include "health/DeviceHealthMonitor.h"

std::string DeviceHealthMonitor::makeKey(
    const std::string& machineId,
    const std::string& deviceId) const {
    return machineId + ":" + deviceId;
}

DeviceHealthMonitor::DeviceHealthMonitor(int timeoutSeconds)
    : timeoutSeconds(timeoutSeconds) {
}

void DeviceHealthMonitor::recordHeartbeat(
    const std::string& machineId,
    const std::string& deviceId) {

    std::string key = makeKey(machineId, deviceId);
    auto it = devices.find(key);
    if (it != devices.end()) {
        it->second.lastSeen = std::chrono::system_clock::now();
        it->second.online = true;
        it->second.missedHeartbeats = 0;
        if (it->second.statusSummary == "OFFLINE" || it->second.statusSummary == "UNKNOWN") {
            it->second.statusSummary = (it->second.valid && it->second.operational) ? "HEALTHY" : "DEGRADED";
        }
    } else {
        DeviceHealth health;
        health.machineId = machineId;
        health.deviceId = deviceId;
        health.online = true;
        health.valid = true;
        health.operational = true;
        health.statusSummary = "HEALTHY";
        health.lastSeen = std::chrono::system_clock::now();
        health.missedHeartbeats = 0;
        devices[key] = health;
    }
}

void DeviceHealthMonitor::registerDevice(
    const std::string& machineId,
    const std::string& deviceId) {

    std::string key = makeKey(machineId, deviceId);
    if (devices.find(key) == devices.end()) {
        DeviceHealth health;
        health.machineId = machineId;
        health.deviceId = deviceId;
        health.online = false;
        health.valid = true;
        health.operational = true;
        health.statusSummary = "INITIALIZING";
        health.lastSeen = std::chrono::system_clock::time_point{};
        health.missedHeartbeats = 0;
        devices[key] = health;
    }
}

void DeviceHealthMonitor::updateDeviceStatus(
    const std::string& machineId,
    const std::string& deviceId,
    bool heartbeat,
    bool valid,
    bool operational,
    const std::string& detail) {

    std::string key = makeKey(machineId, deviceId);
    auto it = devices.find(key);
    if (it == devices.end()) {
        registerDevice(machineId, deviceId);
        it = devices.find(key);
    }

    if (heartbeat) {
        it->second.lastSeen = std::chrono::system_clock::now();
        it->second.online = true;
        it->second.missedHeartbeats = 0;
    }

    it->second.valid = valid;
    it->second.operational = operational;
    it->second.detail = detail;

    if (!it->second.online) {
        it->second.statusSummary = "OFFLINE";
    } else if (!operational) {
        it->second.statusSummary = "HARDWARE_FAULT";
    } else if (!valid) {
        it->second.statusSummary = "DATA_INVALID";
    } else {
        it->second.statusSummary = "HEALTHY";
    }
}

void DeviceHealthMonitor::checkHealth() {
    auto now = std::chrono::system_clock::now();

    for (auto& pair : devices) {
        auto& health = pair.second;

        if (health.lastSeen.time_since_epoch().count() == 0) {
            health.online = false;
            health.missedHeartbeats++;
            health.statusSummary = "OFFLINE";
            continue;
        }

        auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(
            now - health.lastSeen
        ).count();

        if (elapsed > timeoutSeconds) {
            health.online = false;
            health.missedHeartbeats++;
            health.statusSummary = "OFFLINE";
        } else {
            health.online = true;
            health.missedHeartbeats = 0;
            if (!health.operational) {
                health.statusSummary = "HARDWARE_FAULT";
            } else if (!health.valid) {
                health.statusSummary = "DATA_INVALID";
            } else {
                health.statusSummary = "HEALTHY";
            }
        }
    }
}

std::vector<DeviceHealth> DeviceHealthMonitor::getOfflineDevices() const {
    std::vector<DeviceHealth> offline;
    for (const auto& pair : devices) {
        if (!pair.second.online) {
            offline.push_back(pair.second);
        }
    }
    return offline;
}

std::vector<DeviceHealth> DeviceHealthMonitor::getUnhealthyDevices() const {
    std::vector<DeviceHealth> unhealthy;
    for (const auto& pair : devices) {
        if (!pair.second.online || !pair.second.valid || !pair.second.operational) {
            unhealthy.push_back(pair.second);
        }
    }
    return unhealthy;
}

std::vector<DeviceHealth> DeviceHealthMonitor::getAllDevices() const {
    std::vector<DeviceHealth> all;
    for (const auto& pair : devices) {
        all.push_back(pair.second);
    }
    return all;
}

const DeviceHealth* DeviceHealthMonitor::getDeviceHealth(
    const std::string& machineId,
    const std::string& deviceId) const {

    std::string key = makeKey(machineId, deviceId);
    auto it = devices.find(key);
    if (it != devices.end()) {
        return &it->second;
    }
    return nullptr;
}

bool DeviceHealthMonitor::isDeviceOnline(
    const std::string& machineId,
    const std::string& deviceId) const {

    const DeviceHealth* health = getDeviceHealth(machineId, deviceId);
    return health ? health->online : false;
}

bool DeviceHealthMonitor::isDeviceValid(
    const std::string& machineId,
    const std::string& deviceId) const {

    const DeviceHealth* health = getDeviceHealth(machineId, deviceId);
    return health ? (health->online && health->valid) : false;
}

bool DeviceHealthMonitor::isDeviceOperational(
    const std::string& machineId,
    const std::string& deviceId) const {

    const DeviceHealth* health = getDeviceHealth(machineId, deviceId);
    return health ? (health->online && health->operational) : false;
}

bool DeviceHealthMonitor::isDeviceHealthy(
    const std::string& machineId,
    const std::string& deviceId) const {

    const DeviceHealth* health = getDeviceHealth(machineId, deviceId);
    return health ? (health->online && health->valid && health->operational) : false;
}

std::vector<std::string> DeviceHealthMonitor::getMissingSensors(
    const std::string& machineId) const {

    std::vector<std::string> missing;
    for (const auto& pair : devices) {
        if (pair.second.machineId == machineId && !pair.second.online) {
            missing.push_back(pair.second.deviceId);
        }
    }
    return missing;
}
