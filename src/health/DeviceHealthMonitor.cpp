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
    } else {
        DeviceHealth health{
            machineId,
            deviceId,
            true,
            std::chrono::system_clock::now(),
            0
        };
        devices[key] = health;
    }
}

void DeviceHealthMonitor::registerDevice(
    const std::string& machineId,
    const std::string& deviceId) {

    std::string key = makeKey(machineId, deviceId);

    if (devices.find(key) == devices.end()) {
        DeviceHealth health{
            machineId,
            deviceId,
            false,
            std::chrono::system_clock::time_point{},
            0
        };
        devices[key] = health;
    }
}

void DeviceHealthMonitor::checkHealth() {
    auto now = std::chrono::system_clock::now();

    for (auto& pair : devices) {
        auto& health = pair.second;

        if (health.lastSeen.time_since_epoch().count() == 0) {
            health.online = false;
            health.missedHeartbeats++;
            continue;
        }

        auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(
            now - health.lastSeen
        ).count();

        if (elapsed > timeoutSeconds) {
            health.online = false;
            health.missedHeartbeats++;
        } else {
            health.online = true;
            health.missedHeartbeats = 0;
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
    if (health) {
        return health->online;
    }
    return false;
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
