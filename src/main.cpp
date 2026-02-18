#include <stdio.h>
#include <inttypes.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "freertos/event_groups.h" // Добавили библиотеку Event Groups
#include "driver/gpio.h"
#include "esp_log.h"
#include "esp_timer.h"

#define BLINK_GPIO    GPIO_NUM_26
#define STATUS_GPIO    GPIO_NUM_14   // LED статус isMeasuring, нажатия кнопки
#define TRIG_GPIO     GPIO_NUM_4
#define ECHO_GPIO     GPIO_NUM_19
#define BUTTON_GPIO   GPIO_NUM_18


static const char *TAG = "RTOS_EVENT_GROUP";

// Определение битов в Event Group
#define SENSOR_RUNNING_BIT (1 << 0) // 0-й бит: 1 - сенсор работает, 0 - выключен

// --- ГЛОБАЛЬНЫЕ РЕСУРСЫ ---
typedef struct {
    float distance;
    uint32_t timestamp;
} sensor_data_t;

sensor_data_t g_latest_sensor_data = { .distance = 100.0f, .timestamp = 0 };
bool is_measuring = false;

SemaphoreHandle_t xDistanceMutex;
SemaphoreHandle_t xButtonSemaphore;
EventGroupHandle_t xSystemEventGroup; // Хендл группы событий

// ISR для кнопки
void IRAM_ATTR button_isr_handler(void* arg) {
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    xSemaphoreGiveFromISR(xButtonSemaphore, &xHigherPriorityTaskWoken);
    if (xHigherPriorityTaskWoken) {
        portYIELD_FROM_ISR();
    }
}

// 1. Задача индикатора статуса (Event Group Consumer)
void vTaskStatusLED(void *pvParameters) {
    gpio_reset_pin(STATUS_GPIO);
    gpio_set_direction(STATUS_GPIO, GPIO_MODE_OUTPUT);

    while (1) {
        // Ждем, пока установится бит SENSOR_RUNNING_BIT
        // xWaitForBits(группа, биты, очищать_при_выходе, ждать_всех_бит, время_ожидания)
        EventBits_t bits = xEventGroupWaitBits(
            xSystemEventGroup,
            SENSOR_RUNNING_BIT,
            pdFALSE,        // Не сбрасывать бит автоматически (мы управляем им вручную)
            pdTRUE,         // Ждать бит
            portMAX_DELAY   // Ждать вечно
        );

        if (is_measuring) {
            gpio_set_level(STATUS_GPIO, 1); // Включаем LED, если сенсор активен
        } else {
            gpio_set_level(STATUS_GPIO, 0);
        }
        
        // Маленькая задержка, чтобы задача не съела процессор, если бит часто меняется
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}

// 2. Задача мигания (как и была, Core 0)
void vTaskBlink(void *pvParameters) {
    gpio_reset_pin(BLINK_GPIO);
    gpio_set_direction(BLINK_GPIO, GPIO_MODE_OUTPUT);
    int delay_ms = 500;

    while (1) {
        if (xSemaphoreTake(xDistanceMutex, pdMS_TO_TICKS(10)) == pdTRUE) {
            float dist = g_latest_sensor_data.distance;
            xSemaphoreGive(xDistanceMutex);

            if (dist < 10.0f)      delay_ms = 100;
            else if (dist < 30.0f) delay_ms = 250;
            else                   delay_ms = 800;
        }

        int final_delay = is_measuring ? delay_ms : 1000;
        gpio_set_level(BLINK_GPIO, 1);
        vTaskDelay(pdMS_TO_TICKS(final_delay));
        gpio_set_level(BLINK_GPIO, 0);
        vTaskDelay(pdMS_TO_TICKS(final_delay));
    }
}

// 3. Задача управления и сенсора (Event Group Producer)
void vTaskUltrasonic(void *pvParameters) {
    gpio_reset_pin(TRIG_GPIO);
    gpio_set_direction(TRIG_GPIO, GPIO_MODE_OUTPUT);
    gpio_reset_pin(ECHO_GPIO);
    gpio_set_direction(ECHO_GPIO, GPIO_MODE_INPUT);

    while (1) {
        if (xSemaphoreTake(xButtonSemaphore, 0) == pdTRUE) {
            is_measuring = !is_measuring;
            
            if (is_measuring) {
                ESP_LOGW(TAG, "Sensor START");
                // УСТАНАВЛИВАЕМ бит в группе событий
                xEventGroupSetBits(xSystemEventGroup, SENSOR_RUNNING_BIT);
            } else {
                ESP_LOGW(TAG, "Sensor STOP");
                // СБРАСЫВАЕМ бит в группе событий
                xEventGroupClearBits(xSystemEventGroup, SENSOR_RUNNING_BIT);
            }
            vTaskDelay(pdMS_TO_TICKS(300));
        }

        if (is_measuring) {
            gpio_set_level(TRIG_GPIO, 0);
            esp_rom_delay_us(2);
            gpio_set_level(TRIG_GPIO, 1);
            esp_rom_delay_us(10);
            gpio_set_level(TRIG_GPIO, 0);

            uint32_t start = esp_timer_get_time();
            while (gpio_get_level(ECHO_GPIO) == 0 && (esp_timer_get_time() - start < 100000));
            uint32_t echo_start = esp_timer_get_time();
            while (gpio_get_level(ECHO_GPIO) == 1 && (esp_timer_get_time() - echo_start < 100000));
            uint32_t echo_end = esp_timer_get_time();

            sensor_data_t newData;
            newData.distance = ((float)(echo_end - echo_start) * 0.0343) / 2;
            newData.timestamp = (uint32_t)(esp_timer_get_time() / 1000);

            if (xSemaphoreTake(xDistanceMutex, pdMS_TO_TICKS(50)) == pdTRUE) {
                g_latest_sensor_data = newData;
                xSemaphoreGive(xDistanceMutex);
            }
            vTaskDelay(pdMS_TO_TICKS(200));
        } else {
            vTaskDelay(pdMS_TO_TICKS(100));
        }
    }
}

extern "C" void app_main(void) {
    // Инициализация инструментов RTOS
    xButtonSemaphore = xSemaphoreCreateBinary();
    xDistanceMutex = xSemaphoreCreateMutex();
    xSystemEventGroup = xEventGroupCreate(); // Создание Event Group

    // Конфиг кнопки
    gpio_config_t btn_conf = {
        .pin_bit_mask = (1ULL << BUTTON_GPIO),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .intr_type = GPIO_INTR_NEGEDGE
    };
    gpio_config(&btn_conf);
    gpio_install_isr_service(0);
    gpio_isr_handler_add(BUTTON_GPIO, button_isr_handler, NULL);

    // Запуск задач
    xTaskCreatePinnedToCore(vTaskBlink, "Blink", 2048, NULL, 1, NULL, 0);
    xTaskCreatePinnedToCore(vTaskUltrasonic, "Ultra", 2048, NULL, 2, NULL, 1);
    xTaskCreatePinnedToCore(vTaskStatusLED, "StatusLED", 2048, NULL, 1, NULL, 0);
}