#ifndef FACTORY_H
#define FACTORY_H

#include <vector>
#include <memory>
#include "factory/Machine.h"

class Factory {
private:
    std::vector<std::unique_ptr<Machine>> machines;

public:
    void addMachine(const std::string& id, const std::string& name);

    Machine* getMachine(const std::string& id);

    void update(double deltaTime);

    const std::vector<std::unique_ptr<Machine>>& getMachines() const;
};

#endif