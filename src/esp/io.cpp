#include "io.hpp"

#include "board.hpp"
#include "web.hpp"

#include "driver/gpio.h"
#include "driver/i2c_master.h"
#include "driver/ledc.h"
#include "esp_attr.h"
#include "esp_rom_sys.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstdio>
#include <cstring>

namespace io {
namespace {
using board::Pin;
using board::PinKind;

std::int64_t now_us() { return esp_timer_get_time(); }

// ============================================================== impulzusbemenetek (ISR)
struct PulseChannel {
    volatile std::uint32_t count = 0;
    volatile std::uint32_t total = 0;
    volatile std::int64_t last_edge_us = 0;
    volatile std::uint32_t last_period_us = 0;
    std::uint32_t min_period_us = 0;
    std::uint32_t taken = 0;
};
portMUX_TYPE pulse_mux = portMUX_INITIALIZER_UNLOCKED;
PulseChannel wheel_ch, fan_ch, meter_ch[2];

void IRAM_ATTR pulse_isr(void *arg) {
    auto *ch = static_cast<PulseChannel *>(arg);
    const std::int64_t t = esp_timer_get_time();
    portENTER_CRITICAL_ISR(&pulse_mux);
    const std::int64_t dt = t - ch->last_edge_us;
    if (ch->last_edge_us == 0 || dt >= static_cast<std::int64_t>(ch->min_period_us)) {
        if (ch->last_edge_us != 0) ch->last_period_us = dt > 0xFFFFFFFF ? 0xFFFFFFFFu : static_cast<std::uint32_t>(dt);
        ch->last_edge_us = t;
        ch->count = ch->count + 1;
        ch->total = ch->total + 1;
    }
    portEXIT_CRITICAL_ISR(&pulse_mux);
}

/// Lekérdezett (polled) él beírása – az I/O bővítő reed bemeneteinél a mintavevő taszk hívja.
void polled_edge(PulseChannel &ch, std::int64_t t) {
    portENTER_CRITICAL(&pulse_mux);
    if (ch.last_edge_us != 0) {
        const std::int64_t dt = t - ch.last_edge_us;
        ch.last_period_us = dt > 0xFFFFFFFF ? 0xFFFFFFFFu : static_cast<std::uint32_t>(dt);
    }
    ch.last_edge_us = t;
    ch.count = ch.count + 1;
    ch.total = ch.total + 1;
    portEXIT_CRITICAL(&pulse_mux);
}

seeder::PulseSample take(PulseChannel &ch) {
    seeder::PulseSample s;
    std::int64_t last = 0;
    portENTER_CRITICAL(&pulse_mux);
    s.count = ch.count;
    ch.count = 0;
    s.last_period_us = ch.last_period_us;
    last = ch.last_edge_us;
    portEXIT_CRITICAL(&pulse_mux);
    if (last != 0) {
        const std::int64_t since = now_us() - last;
        s.us_since_last = since > 0xFFFFFFFE ? 0xFFFFFFFEu : static_cast<std::uint32_t>(since < 0 ? 0 : since);
    }
    return s;
}

bool setup_input(const Pin &p, PulseChannel *ch) {
    if (p.kind != PinKind::Gpio) return true;
    gpio_config_t cfg{};
    cfg.pin_bit_mask = 1ULL << p.num;
    cfg.mode = GPIO_MODE_INPUT;
    cfg.pull_up_en = GPIO_PULLUP_ENABLE;      // opto / érintkező GND-re húz
    cfg.pull_down_en = GPIO_PULLDOWN_DISABLE;
    cfg.intr_type = ch ? GPIO_INTR_NEGEDGE : GPIO_INTR_DISABLE;
    if (gpio_config(&cfg) != ESP_OK) return false;
    if (ch) return gpio_isr_handler_add(static_cast<gpio_num_t>(p.num), pulse_isr, ch) == ESP_OK;
    return true;
}

// ============================================================== I2C: I/O bővítő, ADS1115
i2c_master_bus_handle_t bus = nullptr;
i2c_master_dev_handle_t exp_dev = nullptr, ads_dev = nullptr;
constexpr int I2C_TIMEOUT_MS = 20;

std::atomic<bool> exp_ok{false}, ads_ok{false};
std::atomic<std::uint8_t> exp_inputs{0xFF};      // port0 nyers értéke (0 = zárt érintkező)
std::atomic<std::uint8_t> exp_out_wanted{0};     // kívánt kimeneti bitek (1 = aktív/magas)
std::atomic<bool> outputs_enabled{false};        // első apply() után
std::uint8_t exp_out_written = 0;
bool exp_out_valid = false;

constexpr std::uint8_t expander_output_mask() {
    std::uint8_t m = 0;
    const Pin outs[] = {board::CLUTCH[0], board::CLUTCH[1], board::VALVE, board::LAMP[0], board::LAMP[1], board::COOLER};
    for (const Pin &p : outs)
        if (p.kind == PinKind::Expander) m = static_cast<std::uint8_t>(m | (1u << p.num));
    return m;
}
constexpr std::uint8_t OUT_MASK = expander_output_mask();

bool i2c_add(std::uint8_t addr, i2c_master_dev_handle_t &dev) {
    if (dev) return true;
    if (i2c_master_probe(bus, addr, I2C_TIMEOUT_MS) != ESP_OK) return false;
    i2c_device_config_t d{};
    d.dev_addr_length = I2C_ADDR_BIT_LEN_7;
    d.device_address = addr;
    d.scl_speed_hz = 400000;
    if (i2c_master_bus_add_device(bus, &d, &dev) != ESP_OK) {
        dev = nullptr;
        return false;
    }
    return true;
}

bool reg_write(i2c_master_dev_handle_t dev, std::uint8_t reg, const std::uint8_t *data, std::size_t n) {
    std::uint8_t buf[4] = {reg, 0, 0, 0};
    std::memcpy(buf + 1, data, std::min<std::size_t>(n, 3));
    return dev && i2c_master_transmit(dev, buf, n + 1, I2C_TIMEOUT_MS) == ESP_OK;
}
bool reg_read(i2c_master_dev_handle_t dev, std::uint8_t reg, std::uint8_t *data, std::size_t n) {
    return dev && i2c_master_transmit_receive(dev, &reg, 1, data, n, I2C_TIMEOUT_MS) == ESP_OK;
}

// PCA9555: 0 bemenet, 2 kimenet, 6 konfig (port0). TCA9554: 0 bemenet, 1 kimenet, 3 konfig.
constexpr bool PCA = board::EXPANDER == board::Expander::PCA9555;
constexpr std::uint8_t REG_IN = 0x00, REG_OUT = PCA ? 0x02 : 0x01, REG_CFG = PCA ? 0x06 : 0x03;

bool expander_write_outputs() {
    // nem kimeneti bitek: 1 (PCA9555 bemenetek felhúzott szintje)
    const std::uint8_t v = static_cast<std::uint8_t>((exp_out_wanted.load() & OUT_MASK) | (PCA ? ~OUT_MASK : 0));
    if (!reg_write(exp_dev, REG_OUT, &v, 1)) return false;
    exp_out_written = v;
    exp_out_valid = true;
    return true;
}

bool expander_attach() {
    if (board::EXPANDER == board::Expander::None || !bus) return false;
    if (!i2c_add(board::EXPANDER_ADDR, exp_dev)) return false;
    exp_out_valid = false;
    if (!outputs_enabled.load()) {
        // még nincs kimeneti parancs: minden láb bemenet (nagy impedancia) marad
        const std::uint8_t all_in = 0xFF;
        return reg_write(exp_dev, REG_CFG, &all_in, 1);
    }
    if (!expander_write_outputs()) return false;
    const std::uint8_t cfg = static_cast<std::uint8_t>(0xFF & ~OUT_MASK);
    return reg_write(exp_dev, REG_CFG, &cfg, 1);
}

// ---- ADS1115 (egylövéses mérés, ±6,144 V tartomány, 128 SPS)
constexpr float ADS_LSB_V = 6.144f / 32768.0f;
std::atomic<float> ads_volt[4];

bool ads_read(int ch, float &volts) {
    const std::uint16_t cfg = static_cast<std::uint16_t>(0x8000 | ((4 + (ch & 3)) << 12) | (0 << 9) | 0x0100 |
                                                         (4 << 5) | 0x0003);
    const std::uint8_t c[2] = {static_cast<std::uint8_t>(cfg >> 8), static_cast<std::uint8_t>(cfg & 0xFF)};
    if (!reg_write(ads_dev, 0x01, c, 2)) return false;
    vTaskDelay(pdMS_TO_TICKS(10));
    std::uint8_t r[2] = {0, 0};
    if (!reg_read(ads_dev, 0x00, r, 2)) return false;
    const auto raw = static_cast<std::int16_t>((r[0] << 8) | r[1]);
    volts = raw * ADS_LSB_V;
    return true;
}

// ============================================================== HX711 (bitbang, 128-as erősítés, A csatorna)
std::atomic<bool> scale_valid{false};
std::atomic<std::int32_t> scale_raw{0};
std::atomic<std::int64_t> scale_time_us{0};
portMUX_TYPE hx_mux = portMUX_INITIALIZER_UNLOCKED;

bool hx711_read(std::int32_t &value) {
    if (board::HX711_DATA < 0) return false;
    const auto dout = static_cast<gpio_num_t>(board::HX711_DATA);
    const auto sck = static_cast<gpio_num_t>(board::HX711_CLOCK);
    if (gpio_get_level(dout) != 0) return false; // nincs kész mérés
    std::uint32_t v = 0;
    portENTER_CRITICAL(&hx_mux);
    for (int i = 0; i < 24; ++i) {
        gpio_set_level(sck, 1);
        esp_rom_delay_us(1);
        v = (v << 1) | (gpio_get_level(dout) ? 1u : 0u);
        gpio_set_level(sck, 0);
        esp_rom_delay_us(1);
    }
    gpio_set_level(sck, 1);   // 25. impulzus: következő mérés A csatorna, 128×
    esp_rom_delay_us(1);
    gpio_set_level(sck, 0);
    portEXIT_CRITICAL(&hx_mux);
    if (v & 0x800000u) v |= 0xFF000000u;
    value = static_cast<std::int32_t>(v);
    return value != -1 && value != 0x7FFFFF; // telítés / nincs cella
}

// ============================================================== DS18B20 (1-Wire bitbang)
portMUX_TYPE ow_mux = portMUX_INITIALIZER_UNLOCKED;
gpio_num_t ow_pin() { return static_cast<gpio_num_t>(board::ONEWIRE_GPIO); }

bool ow_reset() {
    const gpio_num_t p = ow_pin();
    gpio_set_level(p, 0);
    esp_rom_delay_us(480);
    portENTER_CRITICAL(&ow_mux);
    gpio_set_level(p, 1);
    esp_rom_delay_us(70);
    const bool presence = gpio_get_level(p) == 0;
    portEXIT_CRITICAL(&ow_mux);
    esp_rom_delay_us(410);
    return presence;
}
void ow_write_bit(bool b) {
    const gpio_num_t p = ow_pin();
    portENTER_CRITICAL(&ow_mux);
    gpio_set_level(p, 0);
    esp_rom_delay_us(b ? 6 : 60);
    gpio_set_level(p, 1);
    esp_rom_delay_us(b ? 64 : 10);
    portEXIT_CRITICAL(&ow_mux);
}
bool ow_read_bit() {
    const gpio_num_t p = ow_pin();
    portENTER_CRITICAL(&ow_mux);
    gpio_set_level(p, 0);
    esp_rom_delay_us(6);
    gpio_set_level(p, 1);
    esp_rom_delay_us(9);
    const bool b = gpio_get_level(p) != 0;
    portEXIT_CRITICAL(&ow_mux);
    esp_rom_delay_us(55);
    return b;
}
void ow_write(std::uint8_t v) {
    for (int i = 0; i < 8; ++i) ow_write_bit((v >> i) & 1u);
}
std::uint8_t ow_read() {
    std::uint8_t v = 0;
    for (int i = 0; i < 8; ++i) v = static_cast<std::uint8_t>(v | (ow_read_bit() ? (1u << i) : 0u));
    return v;
}
std::uint8_t crc8(const std::uint8_t *d, int n) {
    std::uint8_t crc = 0;
    for (int i = 0; i < n; ++i) {
        std::uint8_t b = d[i];
        for (int j = 0; j < 8; ++j) {
            const bool mix = (crc ^ b) & 1u;
            crc >>= 1;
            if (mix) crc ^= 0x8C;
            b >>= 1;
        }
    }
    return crc;
}

constexpr int MAX_SENSORS = 2;
std::uint8_t roms[MAX_SENSORS][8];
int rom_count = 0;
std::atomic<float> temps[MAX_SENSORS];
std::atomic<int> sensors_found{0};

/// ROM keresés (Maxim AN187), legfeljebb MAX_SENSORS eszköz, ROM szerint növekvő sorrendben.
int ow_search() {
    int found = 0;
    std::uint8_t rom[8] = {};
    int last_discrepancy = 0;
    bool last_device = false;
    while (!last_device && found < MAX_SENSORS) {
        if (!ow_reset()) break;
        ow_write(0xF0);
        int last_zero = 0;
        for (int bit = 1; bit <= 64; ++bit) {
            const bool id = ow_read_bit(), cmp = ow_read_bit();
            if (id && cmp) return found;        // nincs válasz
            bool dir;
            if (id != cmp) {
                dir = id;
            } else {
                const bool prev = (rom[(bit - 1) / 8] >> ((bit - 1) % 8)) & 1u;
                dir = bit < last_discrepancy ? prev : bit == last_discrepancy;
                if (!dir) last_zero = bit;
            }
            if (dir) rom[(bit - 1) / 8] = static_cast<std::uint8_t>(rom[(bit - 1) / 8] | (1u << ((bit - 1) % 8)));
            else rom[(bit - 1) / 8] = static_cast<std::uint8_t>(rom[(bit - 1) / 8] & ~(1u << ((bit - 1) % 8)));
            ow_write_bit(dir);
        }
        last_discrepancy = last_zero;
        if (last_discrepancy == 0) last_device = true;
        if (crc8(rom, 7) == rom[7] && rom[0] == 0x28) std::memcpy(roms[found++], rom, 8);
    }
    return found;
}

bool ds_read(const std::uint8_t *rom, float &c) {
    if (!ow_reset()) return false;
    ow_write(0x55);
    for (int i = 0; i < 8; ++i) ow_write(rom[i]);
    ow_write(0xBE);
    std::uint8_t sp[9];
    for (auto &b : sp) b = ow_read();
    if (crc8(sp, 8) != sp[8]) return false;
    const auto raw = static_cast<std::int16_t>((sp[1] << 8) | sp[0]);
    c = raw / 16.0f;
    return c > -55.0f && c < 125.0f && !(raw == 0x0550); // 85 °C = bekapcsolási alapérték
}

// ============================================================== kimenetek
std::atomic<bool> gpio_out_ready{false};
std::atomic<float> pwm_now[2];
std::atomic<int> pwm_freq{0};
bool pwm_ready[2] = {false, false};

void set_out(const Pin &p, bool level, std::uint8_t &exp_bits) {
    if (p.kind == PinKind::Gpio) gpio_set_level(static_cast<gpio_num_t>(p.num), level ? 1 : 0);
    else if (p.kind == PinKind::Expander && level) exp_bits = static_cast<std::uint8_t>(exp_bits | (1u << p.num));
}

void gpio_outputs_init() {
    const Pin outs[] = {board::CLUTCH[0], board::CLUTCH[1], board::VALVE, board::LAMP[0], board::LAMP[1], board::COOLER};
    for (const Pin &p : outs) {
        if (p.kind != PinKind::Gpio) continue;
        gpio_reset_pin(static_cast<gpio_num_t>(p.num));
        gpio_set_direction(static_cast<gpio_num_t>(p.num), GPIO_MODE_OUTPUT);
    }
}

void pwm_setup(int freq) {
    for (int i = 0; i < 2; ++i) {
        if (board::MOTOR_PWM_GPIO[i] < 0) continue;
        if (!pwm_ready[i]) {
            ledc_timer_config_t t{};
            t.speed_mode = LEDC_LOW_SPEED_MODE;
            t.duty_resolution = LEDC_TIMER_10_BIT;
            t.timer_num = LEDC_TIMER_0;
            t.freq_hz = static_cast<std::uint32_t>(freq);
            t.clk_cfg = LEDC_AUTO_CLK;
            if (ledc_timer_config(&t) != ESP_OK) return;
            ledc_channel_config_t c{};
            c.gpio_num = board::MOTOR_PWM_GPIO[i];
            c.speed_mode = LEDC_LOW_SPEED_MODE;
            c.channel = static_cast<ledc_channel_t>(LEDC_CHANNEL_0 + i);
            c.timer_sel = LEDC_TIMER_0;
            c.duty = 0;
            c.hpoint = 0;
            if (ledc_channel_config(&c) != ESP_OK) return;
            pwm_ready[i] = true;
        } else {
            ledc_set_freq(LEDC_LOW_SPEED_MODE, LEDC_TIMER_0, static_cast<std::uint32_t>(freq));
        }
    }
    pwm_freq = freq;
}

// ============================================================== taszkok
void sample_task(void *) {
    // I/O bővítő: érintkezők + reed impulzusok (3 egyező minta = stabil állapot)
    bool stable[8] = {}, prev_raw[8] = {};
    std::uint8_t same[8] = {};
    for (bool &b : stable) b = false;
    std::int64_t last_probe = 0, last_refresh = 0;
    int failures = 0;
    TickType_t wake = xTaskGetTickCount();
    for (;;) {
        vTaskDelayUntil(&wake, std::max<TickType_t>(1, pdMS_TO_TICKS(board::SAMPLE_MS)));
        const std::int64_t t = now_us();
        if (!exp_ok.load()) {
            if (t - last_probe < 5000000) continue;
            last_probe = t;
            if (!expander_attach()) continue;
            exp_ok = true;
            failures = 0;
            web::log("INFO", "I/O bővítő csatlakozva (0x%02X)", board::EXPANDER_ADDR);
        }
        if (outputs_enabled.load()) {
            const std::uint8_t want = static_cast<std::uint8_t>((exp_out_wanted.load() & OUT_MASK) | (PCA ? ~OUT_MASK : 0));
            if (!exp_out_valid || want != exp_out_written || t - last_refresh > 1000000) {
                if (!exp_out_valid) {
                    expander_write_outputs();
                    const std::uint8_t cfg = static_cast<std::uint8_t>(0xFF & ~OUT_MASK);
                    reg_write(exp_dev, REG_CFG, &cfg, 1);
                } else {
                    expander_write_outputs();
                }
                last_refresh = t;
            }
        }
        if (!PCA) continue;   // a TCA9554-en csak relék vannak
        std::uint8_t v = 0xFF;
        if (!reg_read(exp_dev, REG_IN, &v, 1)) {
            if (++failures == 20) {
                exp_ok = false;
                exp_out_valid = false;
                last_probe = t;
                web::log("ERROR", "I/O bővítő olvasási hiba: az érintkezős bemenetek inaktívak");
            }
            continue;
        }
        failures = 0;
        exp_inputs = v;
        for (int i = 0; i < 2; ++i) {
            if (board::METER[i].kind != PinKind::Expander) continue;
            const int b = board::METER[i].num;
            const bool closed = ((v >> b) & 1u) == 0;
            if (closed == prev_raw[b]) {
                if (same[b] < 255) ++same[b];
            } else {
                same[b] = 1;
                prev_raw[b] = closed;
            }
            if (same[b] >= 3 && stable[b] != closed) {
                stable[b] = closed;
                if (closed) polled_edge(meter_ch[i], t);
            }
        }
    }
}

void analog_task(void *) {
    std::int64_t last_ads_probe = -10000000, last_ow_search = -20000000;
    bool ow_converting = false;
    std::int64_t ow_started = 0;
    for (;;) {
        vTaskDelay(pdMS_TO_TICKS(20));
        const std::int64_t t = now_us();
        // ADS1115: 4 csatorna körbe (A0 nyomás, A1–A2 motoráram, A3 tartalék)
        if (bus) {
            if (!ads_ok.load() && t - last_ads_probe > 10000000) {
                last_ads_probe = t;
                if (i2c_add(board::ADS1115_ADDR, ads_dev)) {
                    ads_ok = true;
                    web::log("INFO", "ADS1115 csatlakozva (0x%02X)", board::ADS1115_ADDR);
                }
            }
            if (ads_ok.load()) {
                static int ch = 0;
                float volts = 0.0f;
                if (ads_read(ch, volts)) {
                    ads_volt[ch] = volts;
                } else {
                    ads_ok = false;
                    ads_volt[ch] = -1.0f;
                    web::log("WARN", "ADS1115 nem válaszol");
                }
                ch = (ch + 1) & 3;
            }
        }
        // HX711: kb. 10 Hz-en kész mérés
        std::int32_t raw = 0;
        if (hx711_read(raw)) {
            scale_raw = raw;
            scale_time_us = t;
            scale_valid = true;
        } else if (t - scale_time_us.load() > 1000000) {
            scale_valid = false;
        }
        // DS18B20: keresés, majd 1 s-os ciklus (konverzió 750 ms)
        if (board::ONEWIRE_GPIO >= 0) {
            if (rom_count == 0) {
                if (t - last_ow_search > 10000000) {
                    last_ow_search = t;
                    rom_count = ow_search();
                    sensors_found = rom_count;
                    if (rom_count) web::log("INFO", "DS18B20 hőmérők: %d db", rom_count);
                }
            } else if (!ow_converting) {
                if (ow_reset()) {
                    ow_write(0xCC);   // skip ROM: mindegyik mér
                    ow_write(0x44);
                    ow_converting = true;
                    ow_started = t;
                } else {
                    rom_count = 0;
                    for (auto &x : temps) x = -300.0f;
                }
            } else if (t - ow_started > 800000) {
                ow_converting = false;
                int bad = 0;
                for (int i = 0; i < MAX_SENSORS; ++i) {
                    float c = 0.0f;
                    if (i < rom_count && ds_read(roms[i], c)) temps[i] = c;
                    else {
                        temps[i] = -300.0f;
                        ++bad;
                    }
                }
                if (bad == rom_count) rom_count = 0; // mind eltűnt: újrakeresés
            }
        }
    }
}
} // namespace

bool init() {
    for (auto &v : ads_volt) v = -1.0f;
    for (auto &t : temps) t = -300.0f;
    for (auto &p : pwm_now) p = 0.0f;
    wheel_ch.min_period_us = board::WHEEL_MIN_PERIOD_US;
    fan_ch.min_period_us = board::FAN_MIN_PERIOD_US;
    for (auto &m : meter_ch) m.min_period_us = board::METER_MIN_PERIOD_US;

    bool ok = true;
    const esp_err_t e = gpio_install_isr_service(0);
    if (e != ESP_OK && e != ESP_ERR_INVALID_STATE) {
        web::log("ERROR", "GPIO megszakítás szolgáltatás hiba: %d", e);
        ok = false;
    }
    ok &= setup_input(board::WHEEL, &wheel_ch);
    ok &= setup_input(board::FAN, &fan_ch);
    for (int i = 0; i < 2; ++i) ok &= setup_input(board::METER[i], &meter_ch[i]);
    ok &= setup_input(board::WORK, nullptr);
    for (const Pin &p : board::HOPPER) ok &= setup_input(p, nullptr);
    if (!ok) web::log("ERROR", "Bemenet beállítási hiba");

    // HX711
    if (board::HX711_DATA >= 0) {
        gpio_reset_pin(static_cast<gpio_num_t>(board::HX711_CLOCK));
        gpio_set_direction(static_cast<gpio_num_t>(board::HX711_CLOCK), GPIO_MODE_OUTPUT);
        gpio_set_level(static_cast<gpio_num_t>(board::HX711_CLOCK), 0);
        gpio_reset_pin(static_cast<gpio_num_t>(board::HX711_DATA));
        gpio_set_direction(static_cast<gpio_num_t>(board::HX711_DATA), GPIO_MODE_INPUT);
        gpio_set_pull_mode(static_cast<gpio_num_t>(board::HX711_DATA), GPIO_PULLUP_ONLY);
    }
    // 1-Wire: nyitott kollektoros kimenet + bemenet
    if (board::ONEWIRE_GPIO >= 0) {
        gpio_reset_pin(ow_pin());
        gpio_set_direction(ow_pin(), GPIO_MODE_INPUT_OUTPUT_OD);
        gpio_set_pull_mode(ow_pin(), GPIO_PULLUP_ONLY);
        gpio_set_level(ow_pin(), 1);
    }

    // I2C busz
    i2c_master_bus_config_t cfg{};
    cfg.i2c_port = I2C_NUM_0;
    cfg.sda_io_num = static_cast<gpio_num_t>(board::I2C_SDA);
    cfg.scl_io_num = static_cast<gpio_num_t>(board::I2C_SCL);
    cfg.clk_source = I2C_CLK_SRC_DEFAULT;
    cfg.glitch_ignore_cnt = 7;
    cfg.flags.enable_internal_pullup = true;
    if (i2c_new_master_bus(&cfg, &bus) != ESP_OK) {
        bus = nullptr;
        web::log("ERROR", "I2C busz hiba (SDA GPIO%d, SCL GPIO%d)", board::I2C_SDA, board::I2C_SCL);
    } else if (expander_attach()) {
        exp_ok = true;
        web::log("INFO", "I/O bővítő megtalálva (0x%02X)", board::EXPANDER_ADDR);
    } else {
        web::log("WARN", "I/O bővítő nem található (0x%02X)", board::EXPANDER_ADDR);
    }
    xTaskCreatePinnedToCore(sample_task, "seed_io", 4096, nullptr, 6, nullptr, 1);
    xTaskCreatePinnedToCore(analog_task, "seed_an", 4096, nullptr, 3, nullptr, 1);
    return ok;
}

app::HwInputs read(const seeder::Settings &s) {
    app::HwInputs in;
    in.wheel = take(wheel_ch);
    in.fan = take(fan_ch);
    for (int i = 0; i < 2; ++i) in.meter[i] = take(meter_ch[i]);
    const std::uint8_t xin = exp_inputs.load();
    const bool xvalid = exp_ok.load() && PCA;
    auto closed = [&](const Pin &p) {
        if (p.kind == PinKind::Gpio) return gpio_get_level(static_cast<gpio_num_t>(p.num)) == 0;
        if (p.kind == PinKind::Expander) return xvalid && ((xin >> p.num) & 1u) == 0;
        return false;
    };
    in.work_contact = closed(board::WORK);
    for (int i = 0; i < 2; ++i) in.hopper_contact[i] = closed(board::HOPPER[i]);
    // NC érintkező kiesett bővítőnél: "nyitott"-nak látszana → a mag szűrése után riaszt; ez szándékos
    const int ch = std::max(0, std::min(3, s.pressure_ch));
    in.pressure_v = ads_ok.load() ? ads_volt[ch].load() : -1.0f;
    const int a = s.temp_swap ? 1 : 0;
    in.temp_c[0] = temps[a].load();
    in.temp_c[1] = temps[1 - a].load();
    in.scale_valid = scale_valid.load();
    in.scale_raw = static_cast<float>(scale_raw.load());
    return in;
}

void apply(const seeder::Outputs &o, const seeder::Settings &s) {
    if (!gpio_out_ready.exchange(true)) gpio_outputs_init();
    std::uint8_t bits = 0;
    set_out(board::CLUTCH[0], o.clutch_level[0], bits);
    set_out(board::CLUTCH[1], o.clutch_level[1], bits);
    set_out(board::VALVE, o.valve_level, bits);
    set_out(board::LAMP[0], o.lamp_level[0], bits);
    set_out(board::LAMP[1], o.lamp_level[1], bits);
    set_out(board::COOLER, o.cooler_level, bits);
    exp_out_wanted = bits;
    outputs_enabled = true;
    // adagolómotorok
    const int freq = std::max(100, std::min(20000, s.pwm_freq_hz));
    if (board::MOTOR_PWM_GPIO[0] >= 0 && pwm_freq.load() != freq) pwm_setup(freq);
    for (int i = 0; i < 2; ++i) {
        if (!pwm_ready[i]) continue;
        const float pct = std::max(0.0f, std::min(100.0f, o.motor_pwm_pct[i]));
        if (std::fabs(pct - pwm_now[i].load()) < 0.05f) continue;
        const auto duty = static_cast<std::uint32_t>(std::lround(pct / 100.0f * 1023.0f));
        ledc_set_duty(LEDC_LOW_SPEED_MODE, static_cast<ledc_channel_t>(LEDC_CHANNEL_0 + i), duty);
        ledc_update_duty(LEDC_LOW_SPEED_MODE, static_cast<ledc_channel_t>(LEDC_CHANNEL_0 + i));
        pwm_now[i] = pct;
    }
}

std::string diag_json() {
    char b[520];
    std::snprintf(b, sizeof(b),
                  "{\"expander\":%s,\"exp_inputs\":%u,\"exp_outputs\":%u,\"ads\":%s,\"ads_v\":[%.3f,%.3f,%.3f,%.3f],"
                  "\"ds18b20\":%d,\"temps\":[%.2f,%.2f],\"hx711\":%s,\"hx_raw\":%ld,\"pwm\":[%.1f,%.1f],"
                  "\"pulses\":{\"wheel\":%lu,\"fan\":%lu,\"meter1\":%lu,\"meter2\":%lu},"
                  "\"has\":{\"motor\":%s,\"scale\":%s,\"temp\":%s}}",
                  exp_ok.load() ? "true" : "false", static_cast<unsigned>(exp_inputs.load()),
                  static_cast<unsigned>(exp_out_wanted.load()), ads_ok.load() ? "true" : "false",
                  static_cast<double>(ads_volt[0].load()), static_cast<double>(ads_volt[1].load()),
                  static_cast<double>(ads_volt[2].load()), static_cast<double>(ads_volt[3].load()),
                  sensors_found.load(), static_cast<double>(temps[0].load()), static_cast<double>(temps[1].load()),
                  scale_valid.load() ? "true" : "false", static_cast<long>(scale_raw.load()),
                  static_cast<double>(pwm_now[0].load()), static_cast<double>(pwm_now[1].load()),
                  static_cast<unsigned long>(wheel_ch.total), static_cast<unsigned long>(fan_ch.total),
                  static_cast<unsigned long>(meter_ch[0].total), static_cast<unsigned long>(meter_ch[1].total),
                  board::MOTOR_PWM_GPIO[0] >= 0 ? "true" : "false", board::HX711_DATA >= 0 ? "true" : "false",
                  board::ONEWIRE_GPIO >= 0 ? "true" : "false");
    return b;
}

} // namespace io
