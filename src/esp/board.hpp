#pragma once
// Hardverprofilok: alapból Waveshare (lent), SEEDER_BOARD_RCV17-tel az ESP32S3 RCv1.7 panel.
// Waveshare ESP32-S3-ETH-8DI-8RO-C (16 MB flash, CAN GPIO17/18, 8 optós bemenet
// GPIO4–11, 8 relé TCA9554-en, W5500 Ethernet). Itt fér el minden: motoros hajtás, HX711 mérleg,
// DS18B20 hőmérők, ADS1115 (külső modul szintillesztővel), AgOpenGPS.
//
// Minden bemenet aktív alacsony: az optocsatoló / érintkező GND-re húz.
#include <cstddef>
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

enum class Expander : std::uint8_t { None, PCA9555, TCA9554, PCA9685 };

// ---------------------------------------------------------------------------------------------
// ESP32S3 RCv1.7 (AgOpenGPS Rate Controller panel, Vági F. / Gunics B.) – „kisebb tudású” vetőgép-panel:
// két motor enkóderrel (PWM + DIR a panel motorvezérlőjén), két tartályszenzor, munkakapcsoló,
// kerék- és turbina-jeladó, leforgató gomb, munkalámpák és kuplungok a PCA9685 H-híd kimenetein,
// nyomás az ADS1115-ön, Ethernet (W5500); mérleg (HX711) és hőmérők (DS18B20) a H26 szabad lábain
// (GPIO10–12, 3,3 V!). Nincs: hangjelző.
// Lábkiosztás: Rate_control_ESPRC_01_V1.4 kapcsolási rajz (6. lap) + RC_ESP32 firmware. A két szabályzó
// kimenet a panelen TB67H451 H-híd (IN1 = „Dir”, IN2 = „PWM”, mindkettő PWM): 1. csatorna GPIO4 / 5 → MOT9,
// 2. csatorna GPIO7 / 15 → ZONE1 sorkapocs; a H2 tüskesoron külső Cytronhoz is kijönnek.
// A flow / bin / work bemenetek optocsatolósak (felhúzó feszültség jumperrel).
// FONTOS: a GPIO45 (strapping) a W5500 INT-re kötött → a flash VDD_SPI eFuse-t 3,3 V-ra kell égetni
// az első feltöltés előtt (docs/HARDVER_RCV17_HU.md).
#if defined(SEEDER_BOARD_RCV17)
constexpr const char *NAME = "ESP32S3 RCv1.7 (16 MB, PSRAM nélkül)";
constexpr std::size_t LOG_LINES = 48;                // webes napló: PSRAM nélkül kevesebb sor (≈ 8 kB)
constexpr int CAN_TX_GPIO = 17;
constexpr int CAN_RX_GPIO = 16;                     // MCP2562, 120 Ω lezárás jumperrel
constexpr int I2C_SDA = 8;
constexpr int I2C_SCL = 18;
constexpr Expander EXPANDER = Expander::PCA9685;    // 8 H-híd kimenet: k. kimenet = 2k / 2k+1 csatorna
constexpr std::uint8_t EXPANDER_ADDR = 0x40;
constexpr std::uint8_t ADS1115_ADDR = 0x48;         // a panelen (0–5 V / 4–20 mA)

constexpr Pin WHEEL = G(1);                         // H4 szabad láb: jelillesztés kell (3,3 V!)
constexpr Pin FAN = G(2);                           // H4 szabad láb: jelillesztés kell (3,3 V!)
constexpr Pin METER[2] = {G(21), G(47)};            // FLOW1 / FLOW2 (optós): motorenkóder vagy tengelyjeladó
constexpr Pin WORK = G(40);                         // WORK1 (optós)
constexpr Pin HOPPER[2] = {G(42), G(41)};           // BIN1 / BIN2 (optós) – a rajz szerint BIN1 = GPIO42
constexpr Pin CRANK_BTN = G(9);                     // H26 szabad láb (nem strapping): leforgató gomb, GND-re zár
// kimenetek a PCA9685-ön – a számozás a Waveshare reléivel azonos (k. kimenet = „k. relé”)
constexpr Pin CLUTCH[2] = {X(0), X(1)};             // kimenet 1 / 2
constexpr Pin VALVE = X(2);                         // kimenet 3
constexpr Pin LAMP[2] = {X(3), X(4)};               // kimenet 4 / 5
constexpr Pin COOLER = X(5);                        // kimenet 6: hűtőventilátor
constexpr Pin COOLER_REV = X(6);                    // kimenet 7
constexpr int BUZZER_GPIO = -1;
constexpr bool HAS_ETH = true;
constexpr int ETH_SCLK = 36, ETH_MOSI = 35, ETH_MISO = 37, ETH_CS = 38, ETH_INT = 45, ETH_RST = 48;
// előre = IN2 („PWM”), hátra = IN1 („Dir”). A panel TB67H451-éhez „PWM + PWM” mód kell (IN1 / IN2 felváltva);
// a H2-re kötött külső Cytronhoz „PWM + DIR” (IN1 = DIR, IN2 = PWM).
constexpr int MOTOR_PWM_GPIO[2] = {5, 15};          // 1. csatorna IN2 (MOT9) / 2. csatorna IN2 (ZONE1)
constexpr int MOTOR_AUX_GPIO[2] = {4, 7};           // 1. csatorna IN1 / 2. csatorna IN1
constexpr int DRIVER_EN_GPIO = 13;                  // régebbi paneleken Cytron-engedélyező (újakon nincs bekötve)
constexpr bool DIR_DEFAULT = true;                  // „csak PWM” módban is IN1 = alacsony kimenet (ne lebegjen)
constexpr int HX711_DATA = 11, HX711_CLOCK = 12;   // H26: a HX711 modul VCC/VDD 3,3 V-ról!
constexpr int ONEWIRE_GPIO = 10;                    // H26: 2 db DS18B20, 4,7 kΩ felhúzó a 3,3 V-ra

struct PinInfo {
    const char *pin;
    const char *function;
    const char *note;
};
constexpr PinInfo PINMAP[] = {
    {"CAN TX / RX – GPIO17 / GPIO16", "ISOBUS (250 kbit/s), Keya motorok, külső kijelző", "P3 sorkapocs: 1 CAN_L, 2 CAN_H; 120 Ω: H3 jumper (meglévő buszon nyitva)"},
    {"FLOW1 – GPIO21", "1. adagoló: motorenkóder (vagy tengelyjeladó)", "optós bemenet"},
    {"FLOW2 – GPIO47", "2. adagoló: motorenkóder (vagy tengelyjeladó)", "optós bemenet"},
    {"MOT9 – IN2 GPIO5 / IN1 GPIO4", "1. adagolómotor (TB67H451)", "Motorvezérlő = PWM + PWM; H2-n külső vezérlő: Cytron PWM + DIR, IBT-2 PWM + PWM"},
    {"ZONE1 – IN2 GPIO15 / IN1 GPIO7", "2. adagolómotor (TB67H451)", "mint fent"},
    {"GPIO13", "Cytron-engedélyező (régebbi panelek)", "elektromos hajtásnál magas"},
    {"WORK1 – GPIO40", "Munkakapcsoló (gép lent)", "optós; NO vagy NC (beállítás)"},
    {"BIN1 / BIN2 – GPIO42 / GPIO41", "1. / 2. tartály leürülésgátló", "optós; NO vagy NC (beállítás)"},
    {"H4: GPIO1", "Kerékjeladó", "3,3 V-os láb: optocsatoló / feszültségosztó kell!"},
    {"H4: GPIO2", "Turbina fordulat", "3,3 V-os láb: optocsatoló / feszültségosztó kell!"},
    {"H26: GPIO9", "Leforgató nyomógomb (bekapcsoláskor 3 s: WiFi)", "GND-re záró gomb"},
    {"H26: GPIO10", "DS18B20 hőmérők (olajhűtés, 2 db)", "3,3 V táp, 4,7 kΩ felhúzó a 3,3 V-ra"},
    {"H26: GPIO11 / GPIO12", "HX711 DATA / CLOCK (leforgatási mérleg)", "a HX711 VCC/VDD 3,3 V-ról"},
    {"PCA9685 kimenet 1 / 2 (1A–1B / 2A–2B)", "1. / 2. adagoló kuplung", "TB67H451: A = +, B = −; csatorna 0 / 2"},
    {"PCA9685 kimenet 3", "Nyitás / nyomjelző terelőszelep", "csatorna 4"},
    {"PCA9685 kimenet 4 / 5", "1. / 2. munkalámpa (állítható)", "csatorna 6 / 8"},
    {"PCA9685 kimenet 6 / 7", "Olajhűtő ventilátor / irányváltó (visszafúvás)", "csatorna 10 / 12"},
    {"PCA9685 kimenet 8", "Tartalék", "csatorna 14"},
    {"I2C SDA8 / SCL18", "PCA9685 (0x40), ADS1115 (0x48), NAU7802 tartálymérleg (0x2A)", "400 kHz; a NAU7802 modul 3,3 V-ról"},
    {"ADS1115 A0", "Nyomás (lenyomás)", "0–5 V"},
    {"W5500: MISO37 MOSI35 SCLK36 CS38 INT45 RST48", "Ethernet", "AgOpenGPS, web, Windows-program"},
};
#else
constexpr const char *NAME = "Waveshare ESP32-S3-ETH-8DI-8RO-C (16 MB)";
constexpr std::size_t LOG_LINES = 120;               // webes napló (≈ 20 kB)
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
constexpr int ETH_SCLK = 15, ETH_MOSI = 13, ETH_MISO = 14, ETH_CS = 16, ETH_INT = 12, ETH_RST = -1;
constexpr int MOTOR_PWM_GPIO[2] = {47, 48};         // motorvezérlő PWM (előre / IBT-2 RPWM)
// második motorvezérlő-jel: PWM + DIR módban a DIR, IBT-2 módban az LPWM (hátra). Csak irányváltós
// bekötésnél kapcsoljuk be (a GPIO45 indításkori „strapping” láb: indításkor nem húzhatja fel semmi).
constexpr int MOTOR_AUX_GPIO[2] = {21, 45};
constexpr int DRIVER_EN_GPIO = -1;                 // motorvezérlő engedélyező (itt nincs)
constexpr bool DIR_DEFAULT = false;                 // alap motorvezérlő: a beállítás szerint
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
    {"GPIO47", "1. adagolómotor PWM (IBT-2: RPWM)", "Cytron / Pololu: PWM · IBT-2: RPWM, R_EN+L_EN 5 V-ra"},
    {"GPIO48", "2. adagolómotor PWM (IBT-2: RPWM)", "mint fent, 2. motor"},
    {"GPIO21", "1. motor DIR (PWM+DIR) / LPWM (IBT-2)", "csak irányváltós bekötésnél"},
    {"GPIO45", "2. motor DIR (PWM+DIR) / LPWM (IBT-2)", "indításkor nem húzhatja fel semmi (strapping láb)"},
    {"GPIO1 / GPIO2", "HX711 DATA / CLOCK (leforgatási mérleg)", "3,3 V táp"},
    {"GPIO3", "DS18B20 hőmérők (1-Wire)", "4,7 kΩ felhúzó a 3,3 V-ra"},
    {"I2C SDA42 / SCL41", "TCA9554 relék (0x20), ADS1115 (0x48), NAU7802 tartálymérleg (0x2A)", "ADS1115: külső modul szintillesztővel; NAU7802 3,3 V-ról"},
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
