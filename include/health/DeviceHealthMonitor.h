#ifndef DEVICE_HEALTH_MONITOR_H
#define DEVICE_HEALTH_MONITOR_H

#include <string>
#include <map>
#include <chrono>
#include <vector>

struct DeviceHealth {
    std::string machineId;
    std::string deviceId;
    bool online = false;
    bool valid = true;
    bool operational = true;
    std::string statusSummary = "UNKNOWN";
    std::string detail;
    std::chrono::system_clock::time_point lastSeen;
    int missedHeartbeats = 0;
};

class DeviceHealthMonitor {
private:
    std::map<std::string, DeviceHealth> devices;
    int timeoutSeconds;

    std::string makeKey(const std::string& machineId,
                        const std::string& deviceId) const;

public:
    explicit DeviceHealthMonitor(int timeoutSeconds = 10);

    void recordHeartbeat(const std::string& machineId,
                         const std::string& deviceId);

    void registerDevice(const std::string& machineId,
                         const std::string& deviceId);

    void updateDeviceStatus(const std::string& machineId,
                            const std::string& deviceId,
                            bool heartbeat,
                            bool valid,
                            bool operational,
                            const std::string& detail = "");

    void checkHealth();

    std::vector<DeviceHealth> getOfflineDevices() const;

    std::vector<DeviceHealth> getUnhealthyDevices() const;

    std::vector<DeviceHealth> getAllDevices() const;

    const DeviceHealth* getDeviceHealth(const std::string& machineId,
                                        const std::string& deviceId) const;

    bool isDeviceOnline(const std::string& machineId,
                        const std::string& deviceId) const;

    bool isDeviceValid(const std::string& machineId,
                       const std::string& deviceId) const;

    bool isDeviceOperational(const std::string& machineId,
                             const std::string& deviceId) const;

    bool isDeviceHealthy(const std::string& machineId,
                         const std::string& deviceId) const;

    std::vector<std::string> getMissingSensors(
        const std::string& machineId) const;
};

#endif
