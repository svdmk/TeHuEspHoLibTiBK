#pragma once

#include "esphome/core/hal.h"

// ============================================================================
// DELAY FUNCTIONS
// ============================================================================

inline void soft_i2c_delay_us(uint32_t us) {
    esphome::delayMicroseconds(us);
}

inline void soft_i2c_delay_ms(uint32_t ms) {
    esphome::delayMicroseconds(ms * 1000UL);
}

// ============================================================================
// I2C КОНФИГУРАЦИЯ
// ============================================================================

static uint8_t g_scl = 20;
static uint8_t g_sda = 22;

#define I2C_DELAY_US  10

#define I2C_DELAY() soft_i2c_delay_us(I2C_DELAY_US)

// ============================================================================
// PIN CONTROL (базирано на твоя работещ код)
// ============================================================================

inline void sda_high() {
    pinMode(g_sda, OUTPUT);
    digitalWrite(g_sda, HIGH);
}

inline void sda_low() {
    pinMode(g_sda, OUTPUT);
    digitalWrite(g_sda, LOW);
}

inline void scl_high() {
    pinMode(g_scl, OUTPUT);
    digitalWrite(g_scl, HIGH);
}

inline void scl_low() {
    pinMode(g_scl, OUTPUT);
    digitalWrite(g_scl, LOW);
}

inline void sda_input() {
    pinMode(g_sda, INPUT_PULLUP);
}

inline int sda_read() {
    pinMode(g_sda, INPUT_PULLUP);
    return digitalRead(g_sda);
}

inline void i2c_release() {
    pinMode(g_sda, INPUT);
    pinMode(g_scl, INPUT);
}

// ============================================================================
// SOFT I2C ФУНКЦИИ
// ============================================================================

inline void Soft_I2C_SetPins(uint8_t scl, uint8_t sda) {
    g_scl = scl;
    g_sda = sda;
}

inline void Soft_I2C_Init() {
    sda_high();
    scl_high();
    I2C_DELAY();
    ESP_LOGI("I2C", "Init: SCL=P%d, SDA=P%d", g_scl, g_sda);
}

inline void Soft_I2C_Start() {
    sda_high();
    scl_high();
    I2C_DELAY();
    sda_low();
    I2C_DELAY();
    scl_low();
}

inline void Soft_I2C_Stop() {
    sda_low();
    I2C_DELAY();
    scl_high();
    I2C_DELAY();
    sda_high();
    I2C_DELAY();
    // Освобождаване на шината
    i2c_release();
}

inline bool Soft_I2C_WriteByte(uint8_t data) {
    for (int i = 0; i < 8; i++) {
        if (data & 0x80) {
            sda_high();
        } else {
            sda_low();
        }
        data <<= 1;
        I2C_DELAY();
        scl_high();
        I2C_DELAY();
        scl_low();
    }
    
    // Четене на ACK
    sda_input();
    I2C_DELAY();
    scl_high();
    bool ack = !digitalRead(g_sda);  // ACK = LOW = true
    I2C_DELAY();
    scl_low();
    
    return ack;
}

inline uint8_t Soft_I2C_ReadByte(bool send_ack) {
    uint8_t data = 0;
    
    sda_input();
    
    for (int i = 0; i < 8; i++) {
        I2C_DELAY();
        scl_high();
        I2C_DELAY();
        if (digitalRead(g_sda)) {
            data |= (1 << (7 - i));
        }
        scl_low();
    }
    
    // Изпращане на ACK или NACK
    I2C_DELAY();
    if (send_ack) {
        sda_low();  // ACK
    } else {
        sda_high(); // NACK
    }
    scl_high();
    I2C_DELAY();
    scl_low();
    
    return data;
}

// Aliases за съвместимост с OpenBeken стил
inline void Soft_I2C_SendByte(uint8_t data) {
    Soft_I2C_WriteByte(data);
}

inline int Soft_I2C_WaitAck() {
    // Вече е направено в WriteByte, но за съвместимост
    return 0;
}

// ============================================================================
// UTILITY ФУНКЦИИ
// ============================================================================

inline void Soft_I2C_Scan() {
    ESP_LOGI("I2C", "Scanning (SCL=P%d, SDA=P%d)...", g_scl, g_sda);
    int found = 0;
    
    for (uint8_t addr = 0x08; addr < 0x78; addr++) {
        Soft_I2C_Start();
        bool ack = Soft_I2C_WriteByte(addr << 1);
        Soft_I2C_Stop();
        
        if (ack) {
            ESP_LOGI("I2C", "  Found: 0x%02X", addr);
            found++;
        }
        soft_i2c_delay_us(100);
    }
    
    if (found == 0) {
        ESP_LOGW("I2C", "  No devices!");
    } else {
        ESP_LOGI("I2C", "  %d device(s)", found);
    }
}

inline void Soft_I2C_Recover() {
    ESP_LOGI("I2C", "Recovery...");
    sda_input();
    for (int i = 0; i < 9; i++) {
        scl_low();
        soft_i2c_delay_us(I2C_DELAY_US * 2);
        scl_high();
        soft_i2c_delay_us(I2C_DELAY_US * 2);
    }
    Soft_I2C_Stop();
}

inline void Soft_I2C_TestPins() {
    ESP_LOGI("I2C", "=== PIN TEST v3 ===");
    ESP_LOGI("I2C", "SCL=P%d, SDA=P%d", g_scl, g_sda);
    
    // Test SCL
    scl_low();
    soft_i2c_delay_us(1000);
    int scl_low_read = digitalRead(g_scl);
    scl_high();
    soft_i2c_delay_us(1000);
    int scl_high_read = digitalRead(g_scl);
    ESP_LOGI("I2C", "  SCL: low=%d, high=%d", scl_low_read, scl_high_read);
    
    // Test SDA
    sda_low();
    soft_i2c_delay_us(1000);
    int sda_low_read = digitalRead(g_sda);
    sda_high();
    soft_i2c_delay_us(1000);
    int sda_high_read = digitalRead(g_sda);
    ESP_LOGI("I2C", "  SDA: low=%d, high=%d", sda_low_read, sda_high_read);
    
    i2c_release();
    
    if (scl_low_read == 0 && scl_high_read == 1 && 
        sda_low_read == 0 && sda_high_read == 1) {
        ESP_LOGI("I2C", "=== PINS OK! ===");
    } else {
        ESP_LOGE("I2C", "=== PIN PROBLEM! ===");
    }
}
