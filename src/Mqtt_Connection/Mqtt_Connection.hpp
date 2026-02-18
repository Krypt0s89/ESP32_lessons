#pragma once 
 
#include "mqtt_client.h"  
#include <string>         


class Mqtt_Connection {
private:

    esp_mqtt_client_handle_t client;

    
    static const char *TAG;


    static void mqtt_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data);

public:

    Mqtt_Connection() : client(nullptr) {}

    void begin(const std::string& broker_url);
    
    void publish(const std::string& topic, const std::string& data);
};