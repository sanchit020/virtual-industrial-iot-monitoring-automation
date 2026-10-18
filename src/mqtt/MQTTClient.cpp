#include "mqtt/MQTTClient.h"

#include <iostream>
#include <chrono>

namespace {
class Callback : public virtual mqtt::callback {
private:
    std::function<void(const std::string&, const std::string&)> callback;

public:
    explicit Callback(
        std::function<void(const std::string&, const std::string&)> cb)
        : callback(std::move(cb)) {}

    void message_arrived(mqtt::const_message_ptr msg) override {
        if (callback) {
            callback(
                msg->get_topic_ref().str(),
                msg->get_payload_ref().str()
            );
        }
    }
};
}

MQTTClient::MQTTClient(const std::string& brokerAddress, int port)
    : brokerAddress(brokerAddress),
      port(port),
      connected(false),
      clientId("IndustrialIoTClient"),
      client(nullptr) {
}

bool MQTTClient::connect() {
    try {
        std::string serverAddress =
            "tcp://" + brokerAddress + ":" + std::to_string(port);

        client = std::make_unique<mqtt::async_client>(
            serverAddress,
            clientId
        );

        mqtt::connect_options options;

        client->connect(options)->wait();

        connected = true;

        std::cout << "[MQTT] Connected to "
                  << serverAddress << std::endl;

        return true;
    }
    catch (const mqtt::exception& e) {
        std::cerr << "[MQTT] Connection failed: "
                  << e.what() << std::endl;

        connected = false;
        return false;
    }
}

void MQTTClient::disconnect() {
    if (!client || !connected) {
        return;
    }

    try {
        client->disconnect()->wait();

        connected = false;

        std::cout << "[MQTT] Disconnected" << std::endl;
    }
    catch (const mqtt::exception& e) {
        std::cerr << "[MQTT] Disconnect failed: "
                  << e.what() << std::endl;
    }
}

bool MQTTClient::publish(const std::string& topic,
                         const std::string& message) {
    if (!client || !connected) {
        return false;
    }

    try {
        auto mqttMessage = mqtt::make_message(topic, message);
        mqttMessage->set_qos(0);

        client->publish(mqttMessage);

        std::cout << "[MQTT] Published: "
                  << topic << " -> "
                  << message << std::endl;

        return true;
    }
    catch (const mqtt::exception& e) {
        std::cerr << "[MQTT] Publish failed: "
                  << e.what() << std::endl;
        return false;
    }
}

bool MQTTClient::subscribe(
    const std::string& topic,
    std::function<void(const std::string&, const std::string&)> callback) {

    if (!client || !connected) {
        return false;
    }

    try {
        messageCallback = std::move(callback);

        client->set_callback(
            Callback(messageCallback)
        );

        client->subscribe(topic, 1)->wait();

        std::cout << "[MQTT] Subscribed to: "
                  << topic << std::endl;

        return true;
    }
    catch (const mqtt::exception& e) {
        std::cerr << "[MQTT] Subscribe failed: "
                  << e.what() << std::endl;

        return false;
    }
}

bool MQTTClient::isConnected() const {
    return connected;
}