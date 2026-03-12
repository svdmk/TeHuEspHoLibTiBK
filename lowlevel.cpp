#include <Arduino.h>
#include <WiFi.h>
#include <PubSubClient.h>

// --- РЕГИСТРИ ---
#define REG_GPIO_BASE         0x00802800
#define REG_GPIO_CFG_P20      (REG_GPIO_BASE + 20 * 4)
#define REG_GPIO_CFG_P22      (REG_GPIO_BASE + 22 * 4)
#define REG_GPIO_DATA_INPUT   (REG_GPIO_BASE + 0x43 * 4)

// Пример: конфигуриране на пин 20 като вход с pull-up
// В BK7231N, GPIO_CFG регистрите обикновено имат следния формат:
// [2:0] - функция (000=вход, 001=изход, 010=алтернативна функция и т.н.)
// [4:3] - pull-up/pull-down (00=без, 01=pull-up, 10=pull-down)

//volatile uint32_t* p20_cfg = (volatile uint32_t*)REG_GPIO_CFG_P20;
//*p20_cfg = (0 << 0) |    // режим: вход (0)
           (1 << 3);     // pull-up включен

// BK7231N GPIO Config: Bit0=OutputEnable, Bit2=PullUp, Bit3=OutputVal
// SDA_HIGH поставя пина във входящ режим с Pull-up (High-Z)
#define SDA_HIGH() *((volatile uint32_t*)REG_GPIO_CFG_P22) = 0x04 
#define SDA_LOW()  *((volatile uint32_t*)REG_GPIO_CFG_P22) = 0x01 // Output Enable (Val 0)
#define SCL_HIGH() *((volatile uint32_t*)REG_GPIO_CFG_P20) = 0x04 
#define SCL_LOW()  *((volatile uint32_t*)REG_GPIO_CFG_P20) = 0x01


// Прочитаме всички входни пинове
//uint32_t all_inputs = *(volatile uint32_t*)REG_GPIO_DATA_INPUT;

// Проверяваме конкретен пин (например пин 20)
//bool pin20_state = (all_inputs >> 20) & 0x1;

// Директна проверка на 22-ри бит в регистъра за данни
#define SDA_READ() ((*((volatile uint32_t*)REG_GPIO_DATA_INPUT) >> 22) & 0x01)

void i2c_delay() { 
    delayMicroseconds(50); 
}
void i2c_start() {
    SDA_HIGH(); SCL_HIGH(); i2c_delay();
    SDA_LOW(); i2c_delay();
    SCL_LOW(); i2c_delay();
}

void i2c_stop() {
    SDA_LOW(); i2c_delay();
    SCL_HIGH(); i2c_delay();
    SDA_HIGH(); i2c_delay();
}

bool i2c_write(uint8_t byte) {
    for (int i = 0; i < 8; i++) {
        if (byte & 0x80) SDA_HIGH(); else SDA_LOW();
        byte <<= 1; i2c_delay();
        SCL_HIGH(); i2c_delay();
        SCL_LOW(); i2c_delay();
    }
    SDA_HIGH(); 
    i2c_delay();
    SCL_HIGH(); i2c_delay();
    bool ack = (SDA_READ() == 0); // Проверка за ACK
    SCL_LOW(); i2c_delay();
    return ack;
}

uint8_t i2c_read(bool ack) {
    uint8_t byte = 0;
    SDA_HIGH(); 
    i2c_delay();
    for (int i = 0; i < 8; i++) {
        SCL_HIGH(); i2c_delay();
        if (SDA_READ()) byte |= (1 << (7 - i));
        SCL_LOW(); i2c_delay();
    }
    if (ack) SDA_LOW(); else SDA_HIGH();
    i2c_delay();
    SCL_HIGH(); i2c_delay();
    SCL_LOW(); i2c_delay();
    SDA_HIGH(); 
    return byte;
}



const char* ssid = "SVA968";
const char* password = "Beh4et12";
WiFiClient cl; 
PubSubClient mqtt(cl);
unsigned long lastMsg = 0;

void setup() {
    // Отключване на JTAG
    *((volatile uint32_t *)0x00800040) &= ~(1 << 16); 
    
    Serial.begin(115200);
    WiFi.config(IPAddress(192, 168, 110, 33), IPAddress(192, 168, 110, 1), IPAddress(255, 255, 255, 0));
    WiFi.begin(ssid, password);
    mqtt.setServer("192.168.110.20", 1883); // Сложи твоето MQTT IP
}

void loop() {
    // Поддържане на MQTT връзката
    if (!mqtt.connected()) {
        if (WiFi.status() == WL_CONNECTED) {
            mqtt.connect("CBU_Sensor_Loop", "moshass", "hassmos");
        }
    }
    mqtt.loop();

    unsigned long now = millis();
    if (now - lastMsg > 10000) { // На всеки 10 секунди
        lastMsg = now;

        // 1. СЪБУЖДАНЕ
        i2c_start();
        i2c_write(0x40 << 1);
        i2c_write(0x35); i2c_write(0x17);
        i2c_stop();
        delay(20);

        // 2. ИЗМЕРВАНЕ
        i2c_start();
        if (i2c_write(0x40 << 1)) {
            i2c_write(0x78); i2c_write(0x66);
            i2c_stop();
            
            delay(100); // Време за сензора

            // 3. ЧЕТЕНЕ НА 6 БАЙТА
            i2c_start();
            i2c_write((0x40 << 1) | 1);
            
            uint8_t d[6];
            for (int i = 0; i < 6; i++) {
                d[i] = i2c_read(i < 5);
            }
            i2c_stop();

            // Дебъг лог
            Serial.printf("RAW: %02X %02X %02X %02X %02X %02X\n", d[0], d[1], d[2], d[3], d[4], d[5]);

            // Ако първият байт не е нула или FF, смятаме
            if (d[0] != 0x00 && d[0] != 0xFF) {
                uint16_t t_raw = ((uint16_t)d[0] << 8) | d[1];
                uint16_t h_raw = ((uint16_t)d[3] << 8) | d[4];

                float t = (float)t_raw * 175.0 / 65536.0 - 45.0;
                float h = (float)h_raw * 100.0 / 65536.0;

                char msg[64];
                snprintf(msg, 64, "{\"t\":%.1f, \"h\":%.1f}", t, h);
                mqtt.publish("home/sensor/cbu", msg);
                Serial.printf("УСПЕХ: T:%.1f H:%.1f\n", t, h);
            } else {
                Serial.println("ГРЕШКА: Сензорът върна празни данни.");
            }
        } else {
            i2c_stop();
            Serial.println("ГРЕШКА: Сензорът не отговаря на 0x40.");
        }
    }
}
