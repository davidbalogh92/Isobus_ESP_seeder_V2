#pragma once
// Hardverprofilok. A fordítási környezet (platformio.ini) választja ki:
//   SEEDER_BOARD_AGISOAUX – AgIsoAuxInputs ESP32-S3R8N8 panel (8 MB flash, beépített CAN GPIO10/11,
//                           PCA9555 I/O bővítő + ADS1115 az I2C-n). Ugyanaz a bekötés, mint az ESC
//                           monitoron (a sérült BTN0 helyett a bal tartály a GPIO7-en).
//   (alapértelmezett)      – Waveshare ESP32-S3-ETH-8DI-8RO-C (16 MB flash, CAN GPIO17/18, 8 optós
//                           bemenet GPIO4–11, 8 relé TCA9554-en). Itt fér el minden: motoros hajtás,
//                           HX711 mérleg, DS18B20 hőmérők, ADS1115 (külső modul szintillesztővel).
//
// Minden bemenet aktív alacsony: az optocsatoló / érintkező GND-re húz (belső felhúzó ellenállás).
// Az AgIsoAux bemeneteire 5 V vagy 12 V TILOS (csak open collector optókimenet).
#include <cstdint>

namespace board {

enum class PinKind : std::uint8_t { None, Gpio, Expander };
struct Pin {
    PinKind kind = PinKind::None;
    int num = -1;
    constexpr bool used() const { return kind != PinKind::None; }
};
constexpr Pin NONE{};
constexpr Pin G(int n) { return Pin{PinKind::Gpio, n}; }
constexpr Pin X(int bit) { return Pin{PinKind::Expander, bit}; }

enum class Expander : std::uint8_t { None, PCA9555, TCA9554 };

#ifdef SEEDER_BOARD_AGISOAUX
constexpr const char *NAME = "AgIsoAuxInputs ESP32-S3 (8 MB)";
constexpr int CAN_TX_GPIO = 10;
constexpr int CAN_RX_GPIO = 11;
constexpr int I2C_SDA = 39;
constexpr int I2C_SCL = 38;
constexpr Expander EXPANDER = Expander::PCA9555;   // BTN0..BTN7 = P0_0..P0_7
constexpr std::uint8_t EXPANDER_ADDR = 0x20;
constexpr std::uint8_t ADS1115_ADDR = 0x48;        // a panelen lévő ADS1115

// bemenetek
constexpr Pin WHEEL = G(9);                         // H7: kerékjeladó
constexpr Pin FAN = G(8);                           // H7: turbina
constexpr Pin METER[2] = {X(2), X(3)};              // BTN2 / BTN3: adagolótengely reed
constexpr Pin WORK = X(4);                          // BTN4: munkahelyzet
constexpr Pin HOPPER[2] = {G(7), X(5)};             // GPIO7 (H7) bal, BTN5 jobb tartály
// kimenetek
constexpr Pin CLUTCH[2] = {X(6), X(1)};             // BTN6: 1. adagoló, BTN1: 2. adagoló kuplung
constexpr Pin VALVE = X(7);                         // BTN7: szétnyitás / nyomjelző terelőszelep
constexpr Pin LAMP[2] = {G(48), G(47)};             // H5 / H6 "LED" láb → relémodul
constexpr Pin COOLER = G(18);                       // optós kapcsolókimenet → hűtőventilátor relé
constexpr Pin COOLER_REV = NONE;                    // nincs szabad kimenet: visszafúvás nincs
constexpr Pin CRANK_BTN = NONE;                     // nincs szabad bemenet: leforgató gomb nincs
constexpr int BUZZER_GPIO = -1;
constexpr bool HAS_ETH = false;
constexpr int ETH_SCLK = -1, ETH_MOSI = -1, ETH_MISO = -1, ETH_CS = -1, ETH_INT = -1;
constexpr int MOTOR_PWM_GPIO[2] = {-1, -1};         // nincs szabad láb: elektromos hajtás nincs
constexpr int HX711_DATA = -1, HX711_CLOCK = -1;    // nincs szabad láb: mérleg nincs
constexpr int ONEWIRE_GPIO = -1;                    // nincs szabad láb: DS18B20 nincs
#else
constexpr const char *NAME = "Waveshare ESP32-S3-ETH-8DI-8RO-C (16 MB)";
constexpr int CAN_TX_GPIO = 17;
constexpr int CAN_RX_GPIO = 18;
constexpr int I2C_SDA = 42;
constexpr int I2C_SCL = 41;
constexpr Expander EXPANDER = Expander::TCA9554;   // a 8 relé
constexpr std::uint8_t EXPANDER_ADDR = 0x20;
constexpr std::uint8_t ADS1115_ADDR = 0x48;        // külső modul (5 V, I2C szintillesztővel)

constexpr Pin WHEEL = G(4);                         // IN1
constexpr Pin FAN = G(5);                           // IN2
constexpr Pin METER[2] = {G(6), G(7)};              // IN3 / IN4: tengelyjeladó vagy motorenkóder
constexpr Pin WORK = G(8);                          // IN5
constexpr Pin HOPPER[2] = {G(9), G(10)};            // IN6 / IN7
constexpr Pin CRANK_BTN = G(11);                    // IN8: gépre szerelt leforgató nyomógomb
constexpr Pin CLUTCH[2] = {X(0), X(1)};             // relé 1 / 2
constexpr Pin VALVE = X(2);                         // relé 3
constexpr Pin LAMP[2] = {X(3), X(4)};               // relé 4 / 5
constexpr Pin COOLER = X(5);                        // relé 6
constexpr Pin COOLER_REV = X(6);                    // relé 7: hűtőventilátor irányváltó (visszafúvás), 8 tartalék
constexpr int BUZZER_GPIO = 46;                     // a panel hangjelzője (passzív, LEDC 2,7 kHz)
// beépített W5500 Ethernet (SPI) – AgOpenGPS / AgIO hálózat
constexpr bool HAS_ETH = true;
constexpr int ETH_SCLK = 15, ETH_MOSI = 13, ETH_MISO = 14, ETH_CS = 16, ETH_INT = 12;
constexpr int MOTOR_PWM_GPIO[2] = {47, 48};         // motorvezérlő PWM (Cytron MD13S / IBT-2, egy irány)
constexpr int HX711_DATA = 1, HX711_CLOCK = 2;
constexpr int ONEWIRE_GPIO = 3;                     // 2 db DS18B20, 4,7 kΩ felhúzó a 3,3 V-ra
#endif

// Lábkiosztás a webes felülethez és a dokumentációhoz (Rendszer → Lábkiosztás)
struct PinInfo {
    const char *pin;        ///< csatlakozó / GPIO
    const char *function;   ///< funkció
    const char *note;       ///< bekötés, megjegyzés
};
#ifdef SEEDER_BOARD_AGISOAUX
constexpr PinInfo PINMAP[] = {
    {"CAN TX / RX – GPIO10 / GPIO11", "ISOBUS (250 kbit/s)", "beépített adó-vevő"},
    {"GPIO9 (H7)", "Kerékjeladó", "impulzus, GND felé zár (open collector)"},
    {"GPIO8 (H7)", "Turbina fordulat", "impulzus, GND felé zár"},
    {"BTN2 / BTN3 (PCA9555)", "1. / 2. adagoló tengelyjeladó", "reed / induktív, GND felé zár"},
    {"BTN4 (PCA9555)", "Munkakapcsoló (gép lent)", "NO vagy NC (beállítás)"},
    {"GPIO7 (H7) / BTN5", "1. / 2. tartály leürülésgátló", "NO vagy NC (beállítás)"},
    {"BTN6 / BTN1", "1. / 2. adagoló kuplung", "relémodul bemenet"},
    {"BTN7", "Nyitás / nyomjelző terelőszelep", "relémodul bemenet"},
    {"GPIO48 / GPIO47 (H5 / H6)", "1. / 2. munkalámpa", "relémodul bemenet"},
    {"GPIO18", "Hűtőventilátor", "optós kimenet → relé"},
    {"I2C SDA39 / SCL38", "PCA9555 (0x20), ADS1115 (0x48)", "a panelen"},
    {"ADS1115 A0 (A0–A3 választható)", "Lenyomatási nyomás", "0–5 V szenzor, szükség esetén osztóval"},
    {"– (nincs szabad láb)", "Elektromos adagolómotor, HX711 mérleg, DS18B20, Ethernet", "csak Waveshare panelen"},
};
#else
constexpr PinInfo PINMAP[] = {
    {"CAN TX / RX – GPIO17 / GPIO18", "ISOBUS (250 kbit/s)", "beépített adó-vevő (-C változat)"},
    {"IN1 – GPIO4", "Kerékjeladó", "optós bemenet, 5–36 V jel / zárás"},
    {"IN2 – GPIO5", "Turbina fordulat", "optós bemenet"},
    {"IN3 – GPIO6", "1. adagoló: tengelyjeladó vagy motorenkóder", "optós bemenet"},
    {"IN4 – GPIO7", "2. adagoló: tengelyjeladó vagy motorenkóder", "optós bemenet"},
    {"IN5 – GPIO8", "Munkakapcsoló (gép lent)", "NO vagy NC (beállítás)"},
    {"IN6 – GPIO9", "1. tartály leürülésgátló", "NO vagy NC (beállítás)"},
    {"IN7 – GPIO10", "2. tartály leürülésgátló", "NO vagy NC (beállítás)"},
    {"IN8 – GPIO11", "Leforgató nyomógomb a gépen", "NO gomb (beállítás: Kapcsolók)"},
    {"Relé 1", "1. adagoló kuplung", "TCA9554 P0"},
    {"Relé 2", "2. adagoló kuplung", "TCA9554 P1"},
    {"Relé 3", "Nyitás / nyomjelző terelőszelep", "TCA9554 P2"},
    {"Relé 4", "1. munkalámpa", "TCA9554 P3"},
    {"Relé 5", "2. munkalámpa", "TCA9554 P4"},
    {"Relé 6", "Hűtőventilátor", "TCA9554 P5"},
    {"Relé 7", "Hűtőventilátor irányváltó (visszafúvás)", "TCA9554 P6 – motor polaritásváltó / hidraulika"},
    {"Relé 8", "Tartalék", "TCA9554 P7"},
    {"GPIO47", "1. adagolómotor PWM", "motorvezérlő (Cytron MD13S / IBT-2), egy irány"},
    {"GPIO48", "2. adagolómotor PWM", "motorvezérlő, egy irány"},
    {"GPIO1 / GPIO2", "HX711 DATA / CLOCK (leforgatási mérleg)", "3,3 V táp"},
    {"GPIO3", "DS18B20 hőmérők (1-Wire)", "4,7 kΩ felhúzó a 3,3 V-ra"},
    {"I2C SDA42 / SCL41", "TCA9554 relék (0x20), ADS1115 (0x48)", "ADS1115: külső modul szintillesztővel"},
    {"ADS1115 A0 / A1–A2 / A3", "Nyomás / motoráram / tartalék", "0–5 V"},
    {"GPIO12–16 (belső)", "W5500 Ethernet (INT, MOSI, MISO, SCLK, CS)", "AgOpenGPS / AgIO hálózat"},
    {"GPIO46", "Hangjelző (a panelen)", "leforgatás: fordulatonként rövid, célnál hosszú"},
};
#endif

// Pergésszűrés: ennél rövidebb impulzusközt eldobunk
constexpr std::uint32_t WHEEL_MIN_PERIOD_US = 1000;   // max. 1 kHz
constexpr std::uint32_t FAN_MIN_PERIOD_US = 300;      // max. ~3 kHz
constexpr std::uint32_t METER_MIN_PERIOD_US = 400;    // enkóder max. 2,5 kHz
constexpr std::uint32_t SAMPLE_MS = 5;                // I/O bővítő mintavétel (reed: 3 minta = 15 ms)

} // namespace board
