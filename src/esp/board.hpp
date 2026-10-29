#pragma once
// Hardverprofil: Waveshare ESP32-S3-ETH-8DI-8RO-C (16 MB flash, CAN GPIO17/18, 8 optós bemenet
// GPIO4–11, 8 relé TCA9554-en, W5500 Ethernet). Itt fér el minden: motoros hajtás, HX711 mérleg,
// DS18B20 hőmérők, ADS1115 (külső modul szintillesztővel), AgOpenGPS.
//
// Minden bemenet aktív alacsony: az optocsatoló / érintkező GND-re húz.
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

// Lábkiosztás a webes felülethez és a dokumentációhoz (Rendszer → Lábkiosztás)
struct PinInfo {
    const char *pin;        ///< csatlakozó / GPIO
    const char *function;   ///< funkció
    const char *note;       ///< bekötés, megjegyzés
};
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

// Pergésszűrés: ennél rövidebb impulzusközt eldobunk
constexpr std::uint32_t WHEEL_MIN_PERIOD_US = 1000;   // max. 1 kHz
constexpr std::uint32_t FAN_MIN_PERIOD_US = 300;      // max. ~3 kHz
constexpr std::uint32_t METER_MIN_PERIOD_US = 400;    // enkóder max. 2,5 kHz
constexpr std::uint32_t SAMPLE_MS = 5;                // I/O bővítő mintavétel (reed: 3 minta = 15 ms)

} // namespace board
