#include "factory/Factory.h"

void Factory::addMachine(const std::string& id, const std::string& name) {
    machines.push_back(std::make_unique<Machine>(id, name));
}

Machine* Factory::getMachine(const std::string& id) {
    for (auto& machine : machines) {
        if (machine->getId() == id) {
            return machine.get();
        }
    }

    return nullptr;
}

void Factory::update(double deltaTime) {
    for (auto& machine : machines) {
        machine->update(deltaTime);
    }
}

const std::vector<std::unique_ptr<Machine>>& Factory::getMachines() const {
    return machines;
}