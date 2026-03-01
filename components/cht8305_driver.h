#pragma once
#include "soft_i2c.h"

// ============================================================================
// DELAY
// ============================================================================

#define CHT83XX_DELAY_MS(ms) soft_i2c_delay_ms(ms)

// ============================================================================
// КОНФИГУРАЦИЯ
// ============================================================================

#define CHT83XX_I2C_ADDR  0x40

// ============================================================================
// CHT8310 РЕГИСТРИ
// ============================================================================

#define CHT8310_REG_TEMP          0x00
#define CHT8310_REG_HUM           0x01
#define CHT8310_REG_STATUS        0x02
#define CHT8310_REG_CONFIG        0x03
#define CHT8310_REG_CONV_RATE     0x04  // Conversion rate
#define CHT8310_REG_TEMP_HIGH_LIM 0x05
#define CHT8310_REG_TEMP_LOW_LIM  0x06
#define CHT8310_REG_HUM_HIGH_LIM  0x07
#define CHT8310_REG_HUM_LOW_LIM   0x08
#define CHT8310_REG_ONESHOT       0x0F
#define CHT8310_REG_SOFT_RESET    0xFC
#define CHT8310_REG_MANUF_ID      0xFE
#define CHT8310_REG_VERSION       0xFF

// ============================================================================
// CONFIG REGISTER BITS
// ============================================================================

// Bits 15-13 (MSB)
#define CHT8310_CFG_ALERT_EN      (1 << 7)  // Bit 15: Alert enable (in MSB)
#define CHT8310_CFG_ALERT_POL     (1 << 6)  // Bit 14: Alert polarity
#define CHT8310_CFG_ALERT_MODE    (1 << 5)  // Bit 13: Alert mode

// Bits 4-3: Fault Queue
#define CHT8310_CFG_FQ_MASK       0x18      // Bits 4-3
#define CHT8310_CFG_FQ_1          0x00
#define CHT8310_CFG_FQ_2          0x08
#define CHT8310_CFG_FQ_4          0x10
#define CHT8310_CFG_FQ_6          0x18

// Bits 1-0: Mask
#define CHT8310_CFG_MASK_TEMP     (1 << 0)
#define CHT8310_CFG_MASK_HUM      (1 << 1)

// ============================================================================
// CONVERSION RATE VALUES
// ============================================================================

#define CHT8310_CONV_RATE_1S      0x00
#define CHT8310_CONV_RATE_5S      0x01
#define CHT8310_CONV_RATE_10S     0x02
#define CHT8310_CONV_RATE_60S     0x03
#define CHT8310_CONV_RATE_120S    0x04

// ============================================================================
// ALERT POLARITY (от OpenBeken) - ДОБАВИ ТУК!
// ============================================================================

typedef enum {
    CHT_POL_ACTIVE_LOW = 0,   // Alert pin = LOW when alert active
    CHT_POL_ACTIVE_HIGH = 1   // Alert pin = HIGH when alert active
} CHT_alert_pol_t;

// ============================================================================
// ГЛОБАЛНИ ПРОМЕНЛИВИ
// ============================================================================

static bool cht83xx_initialized = false;
static float cht83xx_temperature = 0;
static float cht83xx_humidity = 0;
static float cht83xx_base_temp = 0;  // Base temperature for alert
static float cht83xx_base_hum = 0;   // Base humidity for alert
static int8_t g_alert_pin = -1;

// ============================================================================
// LOW-LEVEL I2C
// ============================================================================

inline bool CHT83XX_WriteRegister(uint8_t reg, uint8_t* data, uint8_t len) {
    Soft_I2C_Start();
    if (!Soft_I2C_WriteByte(CHT83XX_I2C_ADDR << 1)) {
        Soft_I2C_Stop();
        return false;
    }
    if (!Soft_I2C_WriteByte(reg)) {
        Soft_I2C_Stop();
        return false;
    }
    for (uint8_t i = 0; i < len; i++) {
        if (!Soft_I2C_WriteByte(data[i])) {
            Soft_I2C_Stop();
            return false;
        }
    }
    Soft_I2C_Stop();
    return true;
}

inline bool CHT83XX_WriteRegister16(uint8_t reg, uint16_t value) {
    uint8_t data[2] = { (uint8_t)(value >> 8), (uint8_t)(value & 0xFF) };
    return CHT83XX_WriteRegister(reg, data, 2);
}

inline bool CHT83XX_ReadRegister(uint8_t reg, uint8_t* data, uint8_t len) {
    Soft_I2C_Start();
    if (!Soft_I2C_WriteByte(CHT83XX_I2C_ADDR << 1)) {
        Soft_I2C_Stop();
        return false;
    }
    if (!Soft_I2C_WriteByte(reg)) {
        Soft_I2C_Stop();
        return false;
    }
    Soft_I2C_Stop();
    
    Soft_I2C_Start();
    if (!Soft_I2C_WriteByte((CHT83XX_I2C_ADDR << 1) | 1)) {
        Soft_I2C_Stop();
        return false;
    }
    for (uint8_t i = 0; i < len; i++) {
        data[i] = Soft_I2C_ReadByte(i < (len - 1));
    }
    Soft_I2C_Stop();
    return true;
}

inline uint16_t CHT83XX_ReadRegister16(uint8_t reg) {
    uint8_t data[2];
    if (!CHT83XX_ReadRegister(reg, data, 2)) {
        return 0xFFFF;
    }
    return (data[0] << 8) | data[1];
}

// ============================================================================
// TEMPERATURE/HUMIDITY CONVERSION
// ============================================================================

inline float CHT8310_RawToTemp(uint16_t raw) {
    int16_t temp_raw = (int16_t)raw;
    return (float)(temp_raw >> 3) * 0.03125f;
}

inline uint16_t CHT8310_TempToRaw(float temp) {
    int16_t raw = (int16_t)(temp / 0.03125f) << 3;
    return (uint16_t)raw;
}

inline float CHT8310_RawToHum(uint16_t raw) {
    return ((float)(raw & 0x7FFF) / 32768.0f) * 100.0f;
}

inline uint16_t CHT8310_HumToRaw(float hum) {
    return (uint16_t)((hum / 100.0f) * 32768.0f) & 0x7FFF;
}

// ============================================================================
// INITIALIZATION
// ============================================================================

inline void CHT83XX_SetAlertPin(int8_t pin) {
    g_alert_pin = pin;
    if (pin >= 0) {
        pinMode(pin, INPUT);
        ESP_LOGI("CHT83XX", "Alert pin: P%d", pin);
    }
}

inline void CHT83XX_Init() {
    ESP_LOGI("CHT83XX", "================================");
    ESP_LOGI("CHT83XX", "Init CHT8310");
    ESP_LOGI("CHT83XX", "================================");
    
    Soft_I2C_Init();
    
    Soft_I2C_Start();
    bool ack = Soft_I2C_WriteByte(CHT83XX_I2C_ADDR << 1);
    Soft_I2C_Stop();
    
    if (!ack) {
        ESP_LOGE("CHT83XX", "NOT FOUND!");
        cht83xx_initialized = false;
        return;
    }
    
    cht83xx_initialized = true;
    ESP_LOGI("CHT83XX", "Init OK!");
}

// ============================================================================
// READING
// ============================================================================

inline bool CHT83XX_TriggerMeasurement() {
    uint8_t data[2] = {0x00, 0x00};
    return CHT83XX_WriteRegister(CHT8310_REG_ONESHOT, data, 2);
}

inline bool CHT83XX_ReadAll() {
    if (!cht83xx_initialized) return false;
    
    CHT83XX_TriggerMeasurement();
    CHT83XX_DELAY_MS(20);
    
    uint8_t temp_data[2], hum_data[2];
    
    if (!CHT83XX_ReadRegister(CHT8310_REG_TEMP, temp_data, 2)) return false;
    if (!CHT83XX_ReadRegister(CHT8310_REG_HUM, hum_data, 2)) return false;
    
    uint16_t temp_raw = (temp_data[0] << 8) | temp_data[1];
    uint16_t hum_raw = (hum_data[0] << 8) | hum_data[1];
    
    cht83xx_temperature = CHT8310_RawToTemp(temp_raw);
    cht83xx_humidity = CHT8310_RawToHum(hum_raw);
    
    if (cht83xx_humidity < 0) cht83xx_humidity = 0;
    if (cht83xx_humidity > 100) cht83xx_humidity = 100;
    
    return true;
}

inline float CHT83XX_ReadTemperature() {
    CHT83XX_ReadAll();
    return cht83xx_temperature;
}

inline float CHT83XX_ReadHumidity() {
    return cht83xx_humidity;
}

// ============================================================================
// ALERT FUNCTIONS (OpenBeken style)
// CHT_Alert [TempDiff] [HumDiff] [Freq] [FQ]
// ============================================================================

inline uint8_t CHT83XX_FreqToConvRate(int freq) {
    switch (freq) {
        case 1:   return CHT8310_CONV_RATE_1S;
        case 5:   return CHT8310_CONV_RATE_5S;
        case 10:  return CHT8310_CONV_RATE_10S;
        case 60:  return CHT8310_CONV_RATE_60S;
        case 120: return CHT8310_CONV_RATE_120S;
        default:  return CHT8310_CONV_RATE_1S;
    }
}

inline uint8_t CHT83XX_FQToConfig(int fq) {
    switch (fq) {
        case 1:  return CHT8310_CFG_FQ_1;
        case 2:  return CHT8310_CFG_FQ_2;
        case 4:  return CHT8310_CFG_FQ_4;
        case 6:  return CHT8310_CFG_FQ_6;
        default: return CHT8310_CFG_FQ_1;
    }
}

inline void CHT83XX_Alert(float temp_diff, float hum_diff, int freq, int fq) {
    ESP_LOGI("CHT83XX", "=== SET ALERT (OpenBeken style) ===");
    ESP_LOGI("CHT83XX", "  TempDiff: %.2fC", temp_diff);
    ESP_LOGI("CHT83XX", "  HumDiff:  %.2f%%", hum_diff);
    ESP_LOGI("CHT83XX", "  Freq:     %ds", freq);
    ESP_LOGI("CHT83XX", "  FQ:       %d", fq);
    
    // Read current temperature and humidity as base values
    CHT83XX_ReadAll();
    cht83xx_base_temp = cht83xx_temperature;
    cht83xx_base_hum = cht83xx_humidity;
    
    ESP_LOGI("CHT83XX", "  Base Temp: %.2fC", cht83xx_base_temp);
    ESP_LOGI("CHT83XX", "  Base Hum:  %.2f%%", cht83xx_base_hum);
    
    // Calculate high/low limits based on base + difference
    float temp_high = cht83xx_base_temp + temp_diff;
    float temp_low = cht83xx_base_temp - temp_diff;
    float hum_high = cht83xx_base_hum + hum_diff;
    float hum_low = cht83xx_base_hum - hum_diff;
    
    // Clamp values
    if (temp_high > 125.0f) temp_high = 125.0f;
    if (temp_low < -40.0f) temp_low = -40.0f;
    if (hum_high > 100.0f) hum_high = 100.0f;
    if (hum_low < 0.0f) hum_low = 0.0f;
    
    ESP_LOGI("CHT83XX", "  Temp limits: %.2f - %.2fC", temp_low, temp_high);
    ESP_LOGI("CHT83XX", "  Hum limits:  %.2f - %.2f%%", hum_low, hum_high);
    
    // Convert to raw
    uint16_t temp_high_raw = CHT8310_TempToRaw(temp_high);
    uint16_t temp_low_raw = CHT8310_TempToRaw(temp_low);
    uint16_t hum_high_raw = CHT8310_HumToRaw(hum_high);
    uint16_t hum_low_raw = CHT8310_HumToRaw(hum_low);
    
    // Write limit registers
    CHT83XX_WriteRegister16(CHT8310_REG_TEMP_HIGH_LIM, temp_high_raw);
    CHT83XX_WriteRegister16(CHT8310_REG_TEMP_LOW_LIM, temp_low_raw);
    
    // Only set humidity limits if hum_diff > 0
    if (hum_diff > 0.0f) {
        CHT83XX_WriteRegister16(CHT8310_REG_HUM_HIGH_LIM, hum_high_raw);
        CHT83XX_WriteRegister16(CHT8310_REG_HUM_LOW_LIM, hum_low_raw);
    }
    
    // Set conversion rate
    uint8_t conv_rate = CHT83XX_FreqToConvRate(freq);
    CHT83XX_WriteRegister16(CHT8310_REG_CONV_RATE, conv_rate);
    ESP_LOGI("CHT83XX", "  Conv rate reg: 0x%02X", conv_rate);
    
    // Build config register
    uint16_t cfg = CHT83XX_ReadRegister16(CHT8310_REG_CONFIG);
    ESP_LOGI("CHT83XX", "  Config before: 0x%04X", cfg);
    
    // Clear and set relevant bits
    cfg &= 0x00FF;  // Clear MSB
    cfg |= (CHT8310_CFG_ALERT_EN << 8);  // Enable alert
    cfg |= (CHT8310_CFG_ALERT_POL << 8); // Active high (since you have resistor, no pull-up)
    
    // Set fault queue in LSB
    cfg &= ~CHT8310_CFG_FQ_MASK;
    cfg |= CHT83XX_FQToConfig(fq);
    
    // Unmask temp alert
    cfg &= ~CHT8310_CFG_MASK_TEMP;
    
    // Mask or unmask humidity based on hum_diff
    if (hum_diff > 0.0f) {
        cfg &= ~CHT8310_CFG_MASK_HUM;  // Unmask
    } else {
        cfg |= CHT8310_CFG_MASK_HUM;   // Mask (ignore humidity)
    }
    
    ESP_LOGI("CHT83XX", "  Config after: 0x%04X", cfg);
    CHT83XX_WriteRegister16(CHT8310_REG_CONFIG, cfg);
    
    CHT83XX_DELAY_MS(10);
    
    // Verify
    uint16_t cfg_verify = CHT83XX_ReadRegister16(CHT8310_REG_CONFIG);
    uint16_t th_verify = CHT83XX_ReadRegister16(CHT8310_REG_TEMP_HIGH_LIM);
    uint16_t tl_verify = CHT83XX_ReadRegister16(CHT8310_REG_TEMP_LOW_LIM);
    
    ESP_LOGI("CHT83XX", "  Verify CONFIG: 0x%04X", cfg_verify);
    ESP_LOGI("CHT83XX", "  Verify TEMP_H: 0x%04X (%.2fC)", th_verify, CHT8310_RawToTemp(th_verify));
    ESP_LOGI("CHT83XX", "  Verify TEMP_L: 0x%04X (%.2fC)", tl_verify, CHT8310_RawToTemp(tl_verify));
    
    ESP_LOGI("CHT83XX", "=== ALERT CONFIGURED ===");
}

inline void CHT83XX_DisableAlert() {
    uint16_t cfg = CHT83XX_ReadRegister16(CHT8310_REG_CONFIG);
    cfg &= ~(CHT8310_CFG_ALERT_EN << 8);
    cfg |= CHT8310_CFG_MASK_TEMP;
    cfg |= CHT8310_CFG_MASK_HUM;
    CHT83XX_WriteRegister16(CHT8310_REG_CONFIG, cfg);
    ESP_LOGI("CHT83XX", "Alerts disabled");
}

inline bool CHT83XX_IsAlertActive() {
    if (g_alert_pin < 0) return false;
    return digitalRead(g_alert_pin) == HIGH;
}

// ============================================================================
// DEBUG
// ============================================================================

inline void CHT83XX_ScanRegisters() {
    ESP_LOGI("CHT83XX", "=== REGISTER SCAN ===");
    for (uint8_t reg = 0x00; reg <= 0x10; reg++) {
        uint16_t val = CHT83XX_ReadRegister16(reg);
        ESP_LOGI("CHT83XX", "  Reg 0x%02X = 0x%04X", reg, val);
    }
    ESP_LOGI("CHT83XX", "  Reg 0xFE = 0x%04X (MFR ID)", CHT83XX_ReadRegister16(0xFE));
    ESP_LOGI("CHT83XX", "  Reg 0xFF = 0x%04X (VER)", CHT83XX_ReadRegister16(0xFF));
    ESP_LOGI("CHT83XX", "=== END ===");
}

inline void CHT83XX_DumpConfig() {
    ESP_LOGI("CHT83XX", "=== CURRENT CONFIG ===");
    
    uint16_t cfg = CHT83XX_ReadRegister16(CHT8310_REG_CONFIG);
    uint16_t conv = CHT83XX_ReadRegister16(CHT8310_REG_CONV_RATE);
    uint16_t th = CHT83XX_ReadRegister16(CHT8310_REG_TEMP_HIGH_LIM);
    uint16_t tl = CHT83XX_ReadRegister16(CHT8310_REG_TEMP_LOW_LIM);
    uint16_t hh = CHT83XX_ReadRegister16(CHT8310_REG_HUM_HIGH_LIM);
    uint16_t hl = CHT83XX_ReadRegister16(CHT8310_REG_HUM_LOW_LIM);
    
    ESP_LOGI("CHT83XX", "  CONFIG: 0x%04X", cfg);
    ESP_LOGI("CHT83XX", "    Alert EN:   %d", (cfg >> 15) & 1);
    ESP_LOGI("CHT83XX", "    Alert POL:  %d", (cfg >> 14) & 1);
    ESP_LOGI("CHT83XX", "    Alert MODE: %d", (cfg >> 13) & 1);
    ESP_LOGI("CHT83XX", "    FQ:         %d", (cfg >> 3) & 3);
    ESP_LOGI("CHT83XX", "    Temp mask:  %d", cfg & 1);
    ESP_LOGI("CHT83XX", "    Hum mask:   %d", (cfg >> 1) & 1);
    ESP_LOGI("CHT83XX", "  CONV_RATE: 0x%04X", conv);
    ESP_LOGI("CHT83XX", "  TEMP_H: %.2fC (0x%04X)", CHT8310_RawToTemp(th), th);
    ESP_LOGI("CHT83XX", "  TEMP_L: %.2fC (0x%04X)", CHT8310_RawToTemp(tl), tl);
    ESP_LOGI("CHT83XX", "  HUM_H:  %.2f%% (0x%04X)", CHT8310_RawToHum(hh), hh);
    ESP_LOGI("CHT83XX", "  HUM_L:  %.2f%% (0x%04X)", CHT8310_RawToHum(hl), hl);
    ESP_LOGI("CHT83XX", "  Base T: %.2fC", cht83xx_base_temp);
    ESP_LOGI("CHT83XX", "  Base H: %.2f%%", cht83xx_base_hum);
    
    if (g_alert_pin >= 0) {
        ESP_LOGI("CHT83XX", "  Alert pin P%d: %s", g_alert_pin, 
                 digitalRead(g_alert_pin) ? "HIGH" : "LOW");
    }
}

// ... съществуващите функции ...

// ============================================================================
// ALERT SETUP с полярност (ДОБАВИ В КРАЯ)
// ============================================================================

inline void CHT83XX_AlertWithPolarity(float temp_diff, float hum_diff, int freq, int fq, CHT_alert_pol_t polarity) {
    ESP_LOGI("CHT83XX", "=== ALERT SETUP ===");
    ESP_LOGI("CHT83XX", "  TempDiff: %.2fC", temp_diff);
    ESP_LOGI("CHT83XX", "  HumDiff:  %.2f%%", hum_diff);
    ESP_LOGI("CHT83XX", "  Freq:     %ds", freq);
    ESP_LOGI("CHT83XX", "  FQ:       %d", fq);
    ESP_LOGI("CHT83XX", "  Polarity: %s", polarity == CHT_POL_ACTIVE_LOW ? "ACTIVE_LOW" : "ACTIVE_HIGH");
    
    // Read current values as baseline
    if (!CHT83XX_ReadAll()) {
        ESP_LOGE("CHT83XX", "Failed to read sensor!");
        return;
    }
    
    float temp = cht83xx_temperature;
    float hum = cht83xx_humidity;
    cht83xx_base_temp = temp;
    cht83xx_base_hum = hum;
    
    ESP_LOGI("CHT83XX", "  Base: T=%.2fC, H=%.2f%%", temp, hum);
    
    // Calculate limits
    float temp_high = temp + temp_diff;
    float temp_low = temp - temp_diff;
    float hum_high = (hum_diff > 0) ? (hum + hum_diff) : 100.0f;
    float hum_low = (hum_diff > 0) ? (hum - hum_diff) : 0.0f;
    
    // Clamp
    if (temp_high > 125.0f) temp_high = 125.0f;
    if (temp_low < -40.0f) temp_low = -40.0f;
    if (hum_high > 100.0f) hum_high = 100.0f;
    if (hum_low < 0.0f) hum_low = 0.0f;
    
    ESP_LOGI("CHT83XX", "  Limits: T=%.2f-%.2fC, H=%.2f-%.2f%%", temp_low, temp_high, hum_low, hum_high);
    
    // Write limits
    CHT83XX_WriteRegister16(CHT8310_REG_TEMP_HIGH_LIM, CHT8310_TempToRaw(temp_high));
    CHT83XX_WriteRegister16(CHT8310_REG_TEMP_LOW_LIM, CHT8310_TempToRaw(temp_low));
    CHT83XX_WriteRegister16(CHT8310_REG_HUM_HIGH_LIM, CHT8310_HumToRaw(hum_high));
    CHT83XX_WriteRegister16(CHT8310_REG_HUM_LOW_LIM, CHT8310_HumToRaw(hum_low));
    
    // Conversion rate
    uint8_t conv_rate = 0;
    if (freq <= 1) conv_rate = 0;
    else if (freq <= 5) conv_rate = 1;
    else if (freq <= 10) conv_rate = 2;
    else if (freq <= 60) conv_rate = 3;
    else conv_rate = 4;
    CHT83XX_WriteRegister16(CHT8310_REG_CONV_RATE, conv_rate);
    
    // Fault queue bits (bits 4-3)
    uint8_t fq_bits = 0;
    if (fq <= 1) fq_bits = 0x00;
    else if (fq <= 2) fq_bits = 0x08;
    else if (fq <= 4) fq_bits = 0x10;
    else fq_bits = 0x18;
    
    // ===== BUILD CONFIG =====
    uint16_t cfg = 0;
    
    // Bit 15: Alert Enable
    cfg |= (1 << 15);
    
    // Bit 14: Polarity
    if (polarity == CHT_POL_ACTIVE_HIGH) {
        cfg |= (1 << 14);
    }
    
    // Bit 13: INTERRUPT MODE (важно за wake-up!)
    cfg |= (1 << 13);  // <--- ТОВА Е КЛЮЧОВАТА ПРОМЯНА!
    
    // Bits 4-3: Fault Queue
    cfg |= fq_bits;
    
    // Bit 1: Mask humidity if hum_diff = 0
    if (hum_diff <= 0) {
        cfg |= 0x02;
    }
    
    ESP_LOGI("CHT83XX", "  Config: 0x%04X (INTERRUPT MODE)", cfg);
    CHT83XX_WriteRegister16(CHT8310_REG_CONFIG, cfg);
    
    CHT83XX_DELAY_MS(10);
    
    // Verify
    uint16_t cfg_read = CHT83XX_ReadRegister16(CHT8310_REG_CONFIG);
    ESP_LOGI("CHT83XX", "  Verify: 0x%04X", cfg_read);
    ESP_LOGI("CHT83XX", "    Alert EN:   %d", (cfg_read >> 15) & 1);
    ESP_LOGI("CHT83XX", "    Alert POL:  %d", (cfg_read >> 14) & 1);
    ESP_LOGI("CHT83XX", "    Alert MODE: %d (1=INTERRUPT)", (cfg_read >> 13) & 1);
    
    ESP_LOGI("CHT83XX", "=== ALERT READY (INTERRUPT MODE) ===");
}

// Функция за изчистване на alert (чете регистрите)
inline void CHT83XX_ClearAlert() {
    // В interrupt mode, четенето на статус/данни изчиства alert-а
    CHT83XX_ReadAll();
    ESP_LOGI("CHT83XX", "Alert cleared by reading sensor");
}
