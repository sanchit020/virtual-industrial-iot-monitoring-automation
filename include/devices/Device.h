#ifndef DEVICE_H
#define DEVICE_H

#include <string>

class Device {
protected:
    std::string id;
    std::string machineId;
    std::string type;
    bool online;

public:
    Device(const std::string& id,
           const std::string& machineId,
           const std::string& type);

    virtual ~Device() = default;

    const std::string& getId() const;
    const std::string& getMachineId() const;
    const std::string& getType() const;

    bool isOnline() const;
    void setOnline(bool status);
};

#endif