#include "WiFiManager.hpp"

WiFiManager::WiFiManager(const std::string& ssid, const std::string& password)
    : _ssid(ssid), _password(password) {}

void WiFiManager::wifi_event_handler(void* arg, esp_event_base_t event_base,
                                int32_t event_id, void* event_data) {
    
    WiFiManager* obj = static_cast<WiFiManager*>(arg);

    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
    } 
    else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        if (obj->_retry_count < obj->MAX_RETRY) {
            esp_wifi_connect(); 
            obj->_retry_count++;
            ESP_LOGI(TAG, "Connection attempt... Attempt %d", obj->_retry_count);
        } else {
            xEventGroupSetBits(obj->_wifi_event_group, WIFI_FAIL_BIT);
        }
    } 
    else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;
        ESP_LOGI(TAG, "IP received: " IPSTR, IP2STR(&event->ip_info.ip));
        obj->_retry_count = 0; 
        xEventGroupSetBits(obj->_wifi_event_group, WIFI_CONNECTED_BIT);
    }
}

esp_err_t WiFiManager::connect() {
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase()); 
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    _wifi_event_group = xEventGroupCreate();

    ESP_ERROR_CHECK(esp_netif_init());               
    ESP_ERROR_CHECK(esp_event_loop_create_default()); 
    esp_netif_create_default_wifi_sta();             

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    
    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID, 
                    &WiFiManager::wifi_event_handler, this, nullptr));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT, IP_EVENT_STA_GOT_IP, 
                    &WiFiManager::wifi_event_handler, this, nullptr));

    wifi_config_t wifi_config = {};
    strncpy((char*)wifi_config.sta.ssid, _ssid.c_str(), sizeof(wifi_config.sta.ssid));
    strncpy((char*)wifi_config.sta.password, _password.c_str(), sizeof(wifi_config.sta.password));

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start()); // Запуск Wi-Fi драйвера

    EventBits_t bits = xEventGroupWaitBits(_wifi_event_group,
            WIFI_CONNECTED_BIT | WIFI_FAIL_BIT,
            pdFALSE,  
            pdFALSE,  
            portMAX_DELAY);

    if (bits & WIFI_CONNECTED_BIT) {
        ESP_LOGI(TAG, "Successfully connected to %s", _ssid.c_str());
        return ESP_OK;
    } else {
        ESP_LOGE(TAG, "Failed to connect to %s after maximum attempts", _ssid.c_str());
        return ESP_FAIL;
    }
}