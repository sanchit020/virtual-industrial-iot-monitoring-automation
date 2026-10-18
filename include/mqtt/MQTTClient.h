#ifndef MQTT_CLIENT_H
#define MQTT_CLIENT_H

#include <string>
#include <functional>
#include <memory>

#include <mqtt/async_client.h>

class MQTTClient {
private:
    std::string brokerAddress;
    int port;
    bool connected;

    std::string clientId;
    std::unique_ptr<mqtt::async_client> client;

    std::function<void(const std::string&, const std::string&)> messageCallback;

public:
    MQTTClient(const std::string& brokerAddress, int port);

    bool connect();
    void disconnect();

    bool publish(const std::string& topic,
                 const std::string& message);

    bool subscribe(
        const std::string& topic,
        std::function<void(const std::string&, const std::string&)> callback
    );

    bool isConnected() const;
};

#endif