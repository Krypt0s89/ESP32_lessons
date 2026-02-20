// #include <sstream>
// #include <iomanip>


// !!!!  Попробовать добавить mutex для считывания и отправки данных темп. сенсора


// Подключение стандартной библиотеки ввода-вывода
#include <stdio.h>
#include <math.h>

// Подключение драйвера I2C из ESP-IDF
#include "driver/i2c.h"

// Подключение FreeRTOS (для задержек)
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"


#include "WiFiManager/WiFiManager.hpp"
#include "HttpClient/HttpClient.hpp"

SemaphoreHandle_t xTemperatureMutex;

// I2C
#define I2C_MASTER_NUM I2C_NUM_0       // Номер I2C
#define I2C_MASTER_SDA_IO 21           // SDA
#define I2C_MASTER_SCL_IO 22           // SCL
#define I2C_MASTER_FREQ_HZ 100000      // Частота

// MPU6050
#define MPU6050_ADDR 0x68              // Адрес
#define MPU6050_PWR_MGMT_1 0x6B        // Регистр питания
#define MPU6050_TEMP_OUT_H 0x41        // Температура

//static const char *TAG = "MPU6050";    // Тег логирования

float current_temperature = 0.0;       // Глобальная переменная температуры
std::string curr_temp = "";   //избыточная строка? curr_temp объявляется в telegram_task


void telegram_task(void *pvParameters) {
    HttpClient http;
    std::string token = "8438508129:AAEaH39kNp5BDXTvC4kqovybLFZxN0M1IlA";
    std::string chat_id = "349691021";
    std::string curr_temp = ""; // Локальная переменная для хранения строки с температурой
    if (xSemaphoreTake(xTemperatureMutex, portMAX_DELAY) == pdTRUE) {
        float temp_temperature = round(current_temperature * 100.0) / 100.0;
        curr_temp = "Room temperature is: " + std::to_string(temp_temperature) + "°C";
        xSemaphoreGive(xTemperatureMutex);
    }
     // Округление до 2 знаков после запятой
    // std::stringstream stream;
    // stream << std::fixed << std::setprecision(2) << current_temperature;
    // std::string curr_temp = "Room temperature is: " + stream.str() + "C"; // Returns "x.xx" format

    http.sendTelegramMessage(token, chat_id, curr_temp);

    vTaskDelay(pdMS_TO_TICKS(60000)); // Отправляем сообщение каждые 60 секунд
}



void i2c_master_init()
{
    i2c_config_t conf = {
        .mode = I2C_MODE_MASTER,
        .sda_io_num = I2C_MASTER_SDA_IO,
        .scl_io_num = I2C_MASTER_SCL_IO,
        .sda_pullup_en = GPIO_PULLUP_ENABLE,
        .scl_pullup_en = GPIO_PULLUP_ENABLE,
        .master {.clk_speed = I2C_MASTER_FREQ_HZ}// Частота I2C
    };

    i2c_param_config(I2C_MASTER_NUM, &conf);           // Конфигурация
    i2c_driver_install(I2C_MASTER_NUM, conf.mode, 0, 0, 0); // Установка драйвера
}

void mpu6050_write_byte(uint8_t reg, uint8_t data)
{
    i2c_master_write_to_device(I2C_MASTER_NUM, MPU6050_ADDR,
                               (uint8_t[]){reg, data}, 2,
                               pdMS_TO_TICKS(100));
}

void mpu6050_read_temp(void *pvParameters)
{
    uint8_t data[2]; // Буфер

    if (i2c_master_write_read_device(I2C_MASTER_NUM, MPU6050_ADDR,
                                     (uint8_t[]){MPU6050_TEMP_OUT_H}, 1,
                                     data, 2,
                                     pdMS_TO_TICKS(100)) == ESP_OK)
    {
        int16_t raw = (data[0] << 8) | data[1];     // Объединение байт
        if (xSemaphoreTake(xTemperatureMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
            current_temperature = (raw / 340.0) + 36.53; // Формула из даташита
            xSemaphoreGive(xTemperatureMutex);
        }
        current_temperature = (raw / 340.0) + 36.53; // Формула из даташита
        ESP_LOGI(TAG, "Temp: %.2f C", current_temperature);
    }

    vTaskDelay(pdMS_TO_TICKS(5000));
}






extern "C" void app_main(void) {
    xTemperatureMutex = xSemaphoreCreateMutex(); // Инициализация мьютекса для температуры

    i2c_master_init();                 // I2C старт
    mpu6050_write_byte(MPU6050_PWR_MGMT_1, 0); // Выход из sleep

    WiFiManager wifi("Kryptos", "13572468");
    // // Запуск задачи чтения температуры на Core 1
    xTaskCreatePinnedToCore(mpu6050_read_temp, "reading_task", 2048, NULL, 2, NULL, 1);
    
    if (wifi.connect() == ESP_OK) {
        printf("We connected\n");
        
        HttpClient http;
        xTaskCreate(telegram_task, "telegram_task", 8192, NULL, 2, NULL);
        }
}


// // WORKING MPU6050 TEMP SENSOR

// // Подключение стандартной библиотеки ввода-вывода
// #include <stdio.h>

// // Подключение драйвера I2C из ESP-IDF
// #include "driver/i2c.h"

// // Подключение FreeRTOS (для задержек)
// #include "freertos/FreeRTOS.h"
// #include "freertos/task.h"

// // ------------------- НАСТРОЙКИ I2C -------------------

// // Номер I2C порта (ESP32 имеет 2 порта, используем 0)
// #define I2C_MASTER_NUM I2C_NUM_0

// // GPIO пин для SDA
// #define I2C_MASTER_SDA_IO GPIO_NUM_21

// // GPIO пин для SCL
// #define I2C_MASTER_SCL_IO GPIO_NUM_22

// // Частота I2C (100 кГц)
// #define I2C_MASTER_FREQ_HZ 100000

// // Адрес MPU6050 (если AD0 = GND)
// #define MPU6050_ADDR 0x68

// // Регистр питания MPU6050
// #define MPU6050_PWR_MGMT_1 0x6B

// // Регистр старшего байта температуры
// #define MPU6050_TEMP_OUT_H 0x41

// // -----------------------------------------------------

// // Функция инициализации I2C
// void i2c_master_init()
// {
//     // Создаем структуру конфигурации I2C
//     i2c_config_t conf = {
//         .mode = I2C_MODE_MASTER,               // Режим мастера
//         .sda_io_num = I2C_MASTER_SDA_IO,       // Назначаем SDA
//         .scl_io_num = I2C_MASTER_SCL_IO,       // Назначаем SCL
//         .sda_pullup_en = GPIO_PULLUP_ENABLE,   // Включаем подтяжку SDA
//         .scl_pullup_en = GPIO_PULLUP_ENABLE,   // Включаем подтяжку SCL
//         .master {.clk_speed = I2C_MASTER_FREQ_HZ}// Частота I2C
//     };

//     // Применяем параметры конфигурации
//     i2c_param_config(I2C_MASTER_NUM, &conf);

//     // Устанавливаем драйвер I2C
//     i2c_driver_install(I2C_MASTER_NUM, conf.mode, 0, 0, 0);
// }

// // Функция записи одного байта в регистр MPU6050
// esp_err_t mpu6050_write_byte(uint8_t reg_addr, uint8_t data)
// {
//     return i2c_master_write_to_device(
//         I2C_MASTER_NUM,    // Номер I2C
//         MPU6050_ADDR,      // Адрес устройства
//         (uint8_t[]){reg_addr, data}, // Массив из регистра и данных
//         2,                 // Количество байт
//         pdMS_TO_TICKS(100) // Таймаут
//     );
// }

// // Функция чтения нескольких байт из MPU6050
// esp_err_t mpu6050_read(uint8_t reg_addr, uint8_t *data, size_t len)
// {
//     return i2c_master_write_read_device(
//         I2C_MASTER_NUM,    // Номер I2C
//         MPU6050_ADDR,      // Адрес устройства
//         &reg_addr,         // Регистр для чтения
//         1,                 // Размер адреса
//         data,              // Буфер для данных
//         len,               // Количество байт
//         pdMS_TO_TICKS(100) // Таймаут
//     );
// }

// // Главная функция ESP-IDF
// extern "C" void app_main(void) {
    
//     // Инициализируем I2C
//     i2c_master_init();

//     // Вывод сообщения в монитор порта
//     printf("MPU6050 Temperature Reader\n");

//     // Выводим MPU6050 из спящего режима (записываем 0 в PWR_MGMT_1)
//     mpu6050_write_byte(MPU6050_PWR_MGMT_1, 0);

//     while (1)
//     {
//         uint8_t temp_data[2];  // Буфер для двух байт температуры

//         // Читаем 2 байта температуры
//         if (mpu6050_read(MPU6050_TEMP_OUT_H, temp_data, 2) == ESP_OK)
//         {
//             // Объединяем старший и младший байт
//             int16_t raw_temp = (temp_data[0] << 8) | temp_data[1];

//             // Преобразуем в градусы Цельсия (формула из даташита)
//             float temperature = (raw_temp / 340.0) + 36.53;

//             // Выводим температуру
//             printf("Temperature: %.2f °C\n", temperature);
//         }
//         else
//         {
//             // Сообщение об ошибке чтения
//             printf("Error reading temperature\n");
//         }

//         // Задержка 1 секунда
//         vTaskDelay(pdMS_TO_TICKS(1000));
//     }
// }


//====== Lesson 22 Telegram message =========


// #include "WiFiManager/WiFiManager.hpp"
// #include "HttpClient/HttpClient.hpp"

// void telegram_task(void *pvParameters) {
//     HttpClient http;
//     std::string token = "8438508129:AAEaH39kNp5BDXTvC4kqovybLFZxN0M1IlA";
//     std::string chat_id = "349691021";
    
//     http.sendTelegramMessage(token, chat_id, "The Baltic-Suzdal Reign will prevail");
    
//     vTaskDelete(NULL); 
// }

// extern "C" void app_main(void) {

//     WiFiManager wifi("Kryptos", "13572468");

//     if (wifi.connect() == ESP_OK) {
//        HttpClient http;
//         // std::string response = http.get("https://jsonplaceholder.typicode.com/posts/1");
//         // printf("Response: %s\n", response.c_str());

//         // std::string result = http.post("https://jsonplaceholder.typicode.com/posts", "{\"status\":\"ok\"}");

//         // printf("POST Response: %s\n", result.c_str());
         

//         xTaskCreate(telegram_task, "telegram_task", 8192, NULL, 5, NULL);
//     }
  
// }


// // LESSON WIFI


// #include "../WiFiManager/WiFiManager.hpp"
// #include "../Mqtt_Connection/Mqtt_Connection.hpp"
// #include "../GPIO/GPIO.hpp"
// #include "examples.hpp"

// void Wi_Fi_connection_example(void) {
//     GPIO joystick_button(GPIO_NUM_14, GPIO_MODE_INPUT);
//     GPIO joystick_x(ADC_CHANNEL_6);
//     GPIO joystick_y(ADC_CHANNEL_4);

//     WiFiManager wifi("SSID", "PASSWORD");

//     if (wifi.connect() == ESP_OK) {
//         printf("We connected\n");

//         Mqtt_Connection mqtt;
//         mqtt.begin("mqtt://broker.emqx.io");

//         vTaskDelay(pdMS_TO_TICKS(2000));
//         while (true) {
//              mqtt.publish("school/test/sensor", "{\"temp\": 25.4, \"status\": \"OK\"}");
//          }
//     }

//     while (true) {
//         vTaskDelay(pdMS_TO_TICKS(5000));
//         printf("Joystick X: %d, Y: %d, Button: %d\n", joystick_x.get_level(), joystick_y.get_level(), joystick_button.get_level());
//     }

// }

// // LESSON21

// #include <stdio.h>
// #include <inttypes.h>
// #include "freertos/FreeRTOS.h"
// #include "freertos/task.h"
// #include "freertos/semphr.h"
// #include "freertos/event_groups.h" // Добавили библиотеку Event Groups
// #include "driver/gpio.h"
// #include "esp_log.h"
// #include "esp_timer.h"

// #define BLINK_GPIO    GPIO_NUM_26
// #define STATUS_GPIO    GPIO_NUM_14   // LED статус isMeasuring, нажатия кнопки
// #define TRIG_GPIO     GPIO_NUM_4
// #define ECHO_GPIO     GPIO_NUM_19
// #define BUTTON_GPIO   GPIO_NUM_18


// static const char *TAG = "RTOS_EVENT_GROUP";

// // Определение битов в Event Group
// #define SENSOR_RUNNING_BIT (1 << 0) // 0-й бит: 1 - сенсор работает, 0 - выключен

// // --- ГЛОБАЛЬНЫЕ РЕСУРСЫ ---
// typedef struct {
//     float distance;
//     uint32_t timestamp;
// } sensor_data_t;

// sensor_data_t g_latest_sensor_data = { .distance = 100.0f, .timestamp = 0 };
// bool is_measuring = false;

// SemaphoreHandle_t xDistanceMutex;
// SemaphoreHandle_t xButtonSemaphore;
// EventGroupHandle_t xSystemEventGroup; // Хендл группы событий

// // ISR для кнопки
// void IRAM_ATTR button_isr_handler(void* arg) {
//     BaseType_t xHigherPriorityTaskWoken = pdFALSE;
//     xSemaphoreGiveFromISR(xButtonSemaphore, &xHigherPriorityTaskWoken);
//     if (xHigherPriorityTaskWoken) {
//         portYIELD_FROM_ISR();
//     }
// }

// // 1. Задача индикатора статуса (Event Group Consumer)
// void vTaskStatusLED(void *pvParameters) {
//     gpio_reset_pin(STATUS_GPIO);
//     gpio_set_direction(STATUS_GPIO, GPIO_MODE_OUTPUT);

//     while (1) {
//         // Ждем, пока установится бит SENSOR_RUNNING_BIT
//         // xWaitForBits(группа, биты, очищать_при_выходе, ждать_всех_бит, время_ожидания)
//         EventBits_t bits = xEventGroupWaitBits(
//             xSystemEventGroup,
//             SENSOR_RUNNING_BIT,
//             pdFALSE,        // Не сбрасывать бит автоматически (мы управляем им вручную)
//             pdTRUE,         // Ждать бит
//             portMAX_DELAY   // Ждать вечно
//         );

//         if (is_measuring) {
//             gpio_set_level(STATUS_GPIO, 1); // Включаем LED, если сенсор активен
//         } else {
//             gpio_set_level(STATUS_GPIO, 0);
//         }
        
//         // Маленькая задержка, чтобы задача не съела процессор, если бит часто меняется
//         vTaskDelay(pdMS_TO_TICKS(100));
//     }
// }

// // 2. Задача мигания (как и была, Core 0)
// void vTaskBlink(void *pvParameters) {
//     gpio_reset_pin(BLINK_GPIO);
//     gpio_set_direction(BLINK_GPIO, GPIO_MODE_OUTPUT);
//     int delay_ms = 500;

//     while (1) {
//         if (xSemaphoreTake(xDistanceMutex, pdMS_TO_TICKS(10)) == pdTRUE) {
//             float dist = g_latest_sensor_data.distance;
//             xSemaphoreGive(xDistanceMutex);

//             if (dist < 10.0f)      delay_ms = 100;
//             else if (dist < 30.0f) delay_ms = 250;
//             else                   delay_ms = 800;
//         }

//         int final_delay = is_measuring ? delay_ms : 1000;
//         gpio_set_level(BLINK_GPIO, 1);
//         vTaskDelay(pdMS_TO_TICKS(final_delay));
//         gpio_set_level(BLINK_GPIO, 0);
//         vTaskDelay(pdMS_TO_TICKS(final_delay));
//     }
// }

// // 3. Задача управления и сенсора (Event Group Producer)
// void vTaskUltrasonic(void *pvParameters) {
//     gpio_reset_pin(TRIG_GPIO);
//     gpio_set_direction(TRIG_GPIO, GPIO_MODE_OUTPUT);
//     gpio_reset_pin(ECHO_GPIO);
//     gpio_set_direction(ECHO_GPIO, GPIO_MODE_INPUT);

//     while (1) {
//         if (xSemaphoreTake(xButtonSemaphore, 0) == pdTRUE) {
//             is_measuring = !is_measuring;
            
//             if (is_measuring) {
//                 ESP_LOGW(TAG, "Sensor START");
//                 // УСТАНАВЛИВАЕМ бит в группе событий
//                 xEventGroupSetBits(xSystemEventGroup, SENSOR_RUNNING_BIT);
//             } else {
//                 ESP_LOGW(TAG, "Sensor STOP");
//                 // СБРАСЫВАЕМ бит в группе событий
//                 xEventGroupClearBits(xSystemEventGroup, SENSOR_RUNNING_BIT);
//             }
//             vTaskDelay(pdMS_TO_TICKS(300));
//         }

//         if (is_measuring) {
//             gpio_set_level(TRIG_GPIO, 0);
//             esp_rom_delay_us(2);
//             gpio_set_level(TRIG_GPIO, 1);
//             esp_rom_delay_us(10);
//             gpio_set_level(TRIG_GPIO, 0);

//             uint32_t start = esp_timer_get_time();
//             while (gpio_get_level(ECHO_GPIO) == 0 && (esp_timer_get_time() - start < 100000));
//             uint32_t echo_start = esp_timer_get_time();
//             while (gpio_get_level(ECHO_GPIO) == 1 && (esp_timer_get_time() - echo_start < 100000));
//             uint32_t echo_end = esp_timer_get_time();

//             sensor_data_t newData;
//             newData.distance = ((float)(echo_end - echo_start) * 0.0343) / 2;
//             newData.timestamp = (uint32_t)(esp_timer_get_time() / 1000);

//             if (xSemaphoreTake(xDistanceMutex, pdMS_TO_TICKS(50)) == pdTRUE) {
//                 g_latest_sensor_data = newData;
//                 xSemaphoreGive(xDistanceMutex);
//             }
//             vTaskDelay(pdMS_TO_TICKS(200));
//         } else {
//             vTaskDelay(pdMS_TO_TICKS(100));
//         }
//     }
// }

// extern "C" void app_main(void) {
//     // Инициализация инструментов RTOS
//     xButtonSemaphore = xSemaphoreCreateBinary();
//     xDistanceMutex = xSemaphoreCreateMutex();
//     xSystemEventGroup = xEventGroupCreate(); // Создание Event Group

//     // Конфиг кнопки
//     gpio_config_t btn_conf = {
//         .pin_bit_mask = (1ULL << BUTTON_GPIO),
//         .mode = GPIO_MODE_INPUT,
//         .pull_up_en = GPIO_PULLUP_ENABLE,
//         .intr_type = GPIO_INTR_NEGEDGE
//     };
//     gpio_config(&btn_conf);
//     gpio_install_isr_service(0);
//     gpio_isr_handler_add(BUTTON_GPIO, button_isr_handler, NULL);

//     // Запуск задач
//     xTaskCreatePinnedToCore(vTaskBlink, "Blink", 2048, NULL, 1, NULL, 0);
//     xTaskCreatePinnedToCore(vTaskUltrasonic, "Ultra", 2048, NULL, 2, NULL, 1);
//     xTaskCreatePinnedToCore(vTaskStatusLED, "StatusLED", 2048, NULL, 1, NULL, 0);
// }