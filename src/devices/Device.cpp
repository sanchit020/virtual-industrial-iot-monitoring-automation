#include "devices/Device.h"

Device::Device(const std::string& id,
               const std::string& machineId,
               const std::string& type)
    : id(id),
      machineId(machineId),
      type(type),
      online(false) {
}

const std::string& Device::getId() const {
    return id;
}

const std::string& Device::getMachineId() const {
    return machineId;
}

const std::string& Device::getType() const {
    return type;
}

bool Device::isOnline() const {
    return online;
}

void Device::setOnline(bool status) {
    online = status;
}