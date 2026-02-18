#pragma once
#include "nvs_flash.h"
#include "esp_wifi.h"      
#include "esp_event.h"
#include "esp_log.h"     
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#ifdef __cplusplus
#include <string>
#endif

static const char* TAG = "WiFiManager";

class WiFiManager {
public:
    WiFiManager(const std::string& ssid, const std::string& password);

    esp_err_t connect();
private:
    std::string _ssid;
    std::string _password;

    EventGroupHandle_t _wifi_event_group;
    static const int WIFI_CONNECTED_BIT = BIT0;
    static const int WIFI_FAIL_BIT = BIT1;

    static void wifi_event_handler(void* arg, esp_event_base_t event_base,
                                   int32_t event_id, void* event_data);
    
    int _retry_count = 0;
    const int MAX_RETRY = 5;

};