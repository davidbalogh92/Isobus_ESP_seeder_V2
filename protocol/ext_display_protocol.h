/*
 * ext_display_protocol.h – külső kijelző <-> gépvezérlő CAN protokoll
 *
 * HITELES PÉLDÁNY: github.com/davidbalogh92/Isobus_ExtDisplay_esp32, protocol/
 * A gépvezérlők (vetőgép V2, szóró, permetező) változtatás nélkül veszik át.
 * Specifikáció: docs/ext_display_spec.md
 *
 * Önálló, gépfüggetlen fejléc: csak <stdint.h>, <stdbool.h>, <stddef.h> kell hozzá.
 * C99 és C++11 alatt is fordul, nincs benne dinamikus memória és globális állapot.
 *
 * Átvitel: ISOBUS Proprietary A (PGN 61184 / 0xEF00), célzottan a másik fél
 * címére, mindig 8 bájt. A 0. bájt az üzenet azonosítója, a több bájtos mezők
 * little-endian sorrendűek.
 *
 * Használat:
 *   uint8_t f[EXTDISP_FRAME_LEN];
 *   extdisp_heartbeat_t hb = { counter, EXTDISP_HB_FLAG_STOP, screen };
 *   if (extdisp_encode_heartbeat(&hb, f) == EXTDISP_OK) { küldés(f, 8); }
 *
 *   extdisp_machine_status_t st;
 *   if (extdisp_decode_machine_status(data, len, &st) == EXTDISP_OK) { ... }
 *
 * Dekódolásnál EXTDISP_ERR_VALUE esetén a kimeneti struktúra a nyers mezőkkel
 * ki van töltve (pl. hogy a vezérlő „hibás adat" nyugtát tudjon küldeni, vagy a
 * kijelző ismeretlen protokoll verziót tudjon jelezni). A többi hibánál a
 * kimenet érintetlen.
 */
#ifndef EXT_DISPLAY_PROTOCOL_H
#define EXT_DISPLAY_PROTOCOL_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ------------------------------------------------------------------------ */
/* Alapok                                                                    */
/* ------------------------------------------------------------------------ */

/* Minden nem visszafelé kompatibilis változásnál növelendő. */
#define EXTDISP_PROTOCOL_VERSION 1u

#define EXTDISP_PGN 0xEF00u /* Proprietary A, 61184 */
#define EXTDISP_FRAME_LEN 8u

/* A nem használt / tartalék bájtok értéke: az ISOBUS szokás szerint 0xFF
 * ("nem elérhető"). A dekóderek ezeket a bájtokat nem nézik. */
#define EXTDISP_RESERVED_BYTE 0xFFu

/* Időzítések (ms) és küszöbök a spec szerint */
#define EXTDISP_HEARTBEAT_PERIOD_MS 200u
#define EXTDISP_HEARTBEAT_TIMEOUT_MS 1000u
#define EXTDISP_CAPABILITIES_PERIOD_MS 5000u
#define EXTDISP_CALIB_STATUS_PERIOD_MS 200u
#define EXTDISP_TANK_STATUS_PERIOD_MS 1000u
#define EXTDISP_WIFI_STATUS_PERIOD_MS 1000u
#define EXTDISP_CALIB_MAX_SPEED_CKMH 50u /* 0,5 km/h, 0,01 km/h egységben */
#define EXTDISP_CALIB_TURN_OPTIONS 3u     /* fordulatszám-választás: 0 = 85, 1 = 42,5, 2 = 17 */
#define EXTDISP_CALIB_MAX_WEIGHTS 10u     /* legfeljebb ennyi részmérés adható össze */
#define EXTDISP_CALIB_HOLD_TIMEOUT_MS 600u /* motoros forgatás: ennyi ideig él egy "tartás" jelzés */

#define EXTDISP_MAX_TANKS 2u
#define EXTDISP_MAX_LIGHTS 2u
#define EXTDISP_MAX_FEEDERS 2u

/* ------------------------------------------------------------------------ */
/* Üzenet azonosítók és tartományok                                          */
/* ------------------------------------------------------------------------ */

/* Kijelző -> vezérlő (parancsok) */
#define EXTDISP_ID_HEARTBEAT 0x01u
#define EXTDISP_ID_CALIB_COMMAND 0x10u /* vetőgép: leforgatás */
#define EXTDISP_ID_TANK_COMMAND 0x20u
#define EXTDISP_ID_LIGHT_COMMAND 0x30u
#define EXTDISP_ID_WIFI_COMMAND 0x40u

/* Vezérlő -> kijelző (állapotok) */
#define EXTDISP_ID_CAPABILITIES 0x80u
#define EXTDISP_ID_MACHINE_STATUS 0x81u
#define EXTDISP_ID_CALIB_STATUS 0x90u /* vetőgép */
#define EXTDISP_ID_CALIB_RESULT 0x91u /* vetőgép */
#define EXTDISP_ID_SCALE_STATUS 0x92u /* vetőgép: leforgatási mérleg */
#define EXTDISP_ID_CALIB_WEIGHT_ITEM 0x93u /* vetőgép: a részmérések listájának egy tétele */
#define EXTDISP_ID_CALIB_SAVED 0x94u       /* vetőgép: a jelenleg mentett kalibráció */
#define EXTDISP_ID_TANK_STATUS 0xA0u
#define EXTDISP_ID_WIFI_STATUS 0xC0u
#define EXTDISP_ID_ACK 0xF0u

/* Tartományok */
#define EXTDISP_RANGE_CMD_CORE1_FIRST 0x01u
#define EXTDISP_RANGE_CMD_CORE1_LAST 0x0Fu
#define EXTDISP_RANGE_CMD_MACHINE_FIRST 0x10u
#define EXTDISP_RANGE_CMD_MACHINE_LAST 0x1Fu
#define EXTDISP_RANGE_CMD_CORE2_FIRST 0x20u
#define EXTDISP_RANGE_CMD_CORE2_LAST 0x4Fu
#define EXTDISP_RANGE_STATUS_CORE1_FIRST 0x80u
#define EXTDISP_RANGE_STATUS_CORE1_LAST 0x8Fu
#define EXTDISP_RANGE_STATUS_MACHINE_FIRST 0x90u
#define EXTDISP_RANGE_STATUS_MACHINE_LAST 0x9Fu
#define EXTDISP_RANGE_STATUS_CORE2_FIRST 0xA0u
#define EXTDISP_RANGE_STATUS_CORE2_LAST 0xCFu

/* ------------------------------------------------------------------------ */
/* Kódok                                                                     */
/* ------------------------------------------------------------------------ */

typedef enum {
    EXTDISP_OK = 0,
    EXTDISP_ERR_NULL = 1,   /* NULL mutató */
    EXTDISP_ERR_LENGTH = 2, /* nem 8 bájt */
    EXTDISP_ERR_ID = 3,     /* a 0. bájt nem a várt üzenet azonosító */
    EXTDISP_ERR_VALUE = 4   /* egy mező a megengedett tartományon kívül */
} extdisp_result_t;

/* Géptípus (képességek, 1. bájt) */
#define EXTDISP_MACHINE_SEEDER 1u
#define EXTDISP_MACHINE_SPREADER 2u
#define EXTDISP_MACHINE_SPRAYER 3u

/* Képességek 4. bájt: bit0 = tartály mód, bit4-5 = adagolók száma (0..2),
 * a többi bit tartalék (0-ként küldjük, a dekóder nem nézi). */
#define EXTDISP_TANK_MODE_SHARED 0u
#define EXTDISP_TANK_MODE_SEPARATE 1u
#define EXTDISP_CAP_TANK_MODE_MASK 0x01u
#define EXTDISP_CAP_FEEDER_COUNT_SHIFT 4u
#define EXTDISP_CAP_FEEDER_COUNT_MASK 0x30u

/* Aktív képernyő (szívverés, 3. bájt) */
#define EXTDISP_SCREEN_MAIN 0u
#define EXTDISP_SCREEN_CALIB 1u
#define EXTDISP_SCREEN_TANK 2u
#define EXTDISP_SCREEN_SETTINGS 3u

/* Funkció bitek (képességek, 6. bájt) */
#define EXTDISP_FEATURE_TANK 0x01u
#define EXTDISP_FEATURE_LIGHTS 0x02u
#define EXTDISP_FEATURE_WIFI_SERVICE 0x04u
#define EXTDISP_FEATURE_MACHINE_CALIB 0x08u
#define EXTDISP_FEATURE_FAN 0x10u   /* turbina fordulatszám a gépállapotban (0x81) */
#define EXTDISP_FEATURE_SCALE 0x20u /* leforgatási mérleg (0x92) */

/* Kijelző szívverés jelzők (0x01, 2. bájt) */
#define EXTDISP_HB_FLAG_STOP 0x01u
#define EXTDISP_HB_FLAG_START 0x02u

/* Gépállapot jelzők (0x81, 2. bájt) */
#define EXTDISP_ST_FLAG_WORK_SWITCH 0x01u
#define EXTDISP_ST_FLAG_MOVING 0x02u
#define EXTDISP_ST_FLAG_METERING 0x04u
#define EXTDISP_ST_FLAG_WIFI_ON 0x08u
#define EXTDISP_ST_FLAG_LIGHT1 0x10u
#define EXTDISP_ST_FLAG_LIGHT2 0x20u
#define EXTDISP_ST_FLAG_FAN_ALARM 0x40u /* a turbina fordulatszáma a határon kívül van */

/* Turbina fordulatszám (0x81, 5-6. bájt): ennyi, ha nincs adat. A bővítés előtti vezérlő a
 * tartalék bájtokban éppen ezt küldi, ezért a mező visszafelé kompatibilis. */
#define EXTDISP_FAN_RPM_UNKNOWN 0xFFFFu

/* Mérleg jelzők (0x92, 1. bájt) */
#define EXTDISP_SCALE_FLAG_VALID 0x01u  /* a mérleg működik, az érték érvényes */
#define EXTDISP_SCALE_FLAG_STABLE 0x02u /* az érték megállt (nem ingadozik) */

/*
 * Leforgatás művelet (0x10, 1. bájt). A leforgatást a vezérlő vezeti (kézi és motoros hajtásnál
 * is a saját szabályaival), a kijelző csak kér; minden parancsra nyugta jön.
 * A motoros forgatás NEM parancs: a kijelző szívverésének START bitje a "tartás" (a fizikai START
 * gomb vagy a képernyő Forgat gombja nyomva), amíg az aktív képernyő a leforgatás. A vezérlő a
 * bit megszűnésekor, a STOP bitre vagy EXTDISP_CALIB_HOLD_TIMEOUT_MS kimaradás után megáll.
 */
#define EXTDISP_CALIB_ACT_NEW 1u            /* új leforgatás: számláló nullázása, nullpont keresése */
#define EXTDISP_CALIB_ACT_SELECT_FEEDER 2u  /* adagoló választása; paraméter: 1 vagy 2 */
#define EXTDISP_CALIB_ACT_SELECT_TURNS 3u   /* fordulatszám-választás; paraméter: 0..2 */
#define EXTDISP_CALIB_ACT_ADD_WEIGHT 4u     /* részmérés hozzáadása; érték: tömeg grammban */
#define EXTDISP_CALIB_ACT_TAKE_SCALE 5u     /* a mérleg nettó értéke új részmérésként */
#define EXTDISP_CALIB_ACT_DELETE_WEIGHT 6u  /* részmérés törlése; paraméter: sorszám 0..9, 0xFF = mind */
#define EXTDISP_CALIB_ACT_SAVE 7u           /* az új kg/fordulat mentése a kiválasztott adagolóhoz */
#define EXTDISP_CALIB_ACT_COPY_TO_OTHER 8u  /* a kiválasztott adagoló kalibrációja a másikra */
#define EXTDISP_CALIB_ACT_TARE_SCALE 9u     /* a mérleg tárázása */
#define EXTDISP_CALIB_ACT_MAX 9u
#define EXTDISP_CALIB_DELETE_ALL 0xFFu

/* Adagoló / tartály választó */
#define EXTDISP_FEEDER_ALL 0u
#define EXTDISP_TANK_SHARED 0u

/* Tartály művelet (0x20, 2. bájt) */
#define EXTDISP_TANK_ACT_SET 1u
#define EXTDISP_TANK_ACT_ADD 2u
#define EXTDISP_TANK_ACT_ZERO 3u

/* Munkalámpa állapot (0x30, 2. bájt) */
#define EXTDISP_LIGHT_OFF 0u
#define EXTDISP_LIGHT_ON 1u
#define EXTDISP_LIGHT_TOGGLE 2u

/* WiFi parancs (0x40, 1. bájt) */
#define EXTDISP_WIFI_CMD_OFF 0u
#define EXTDISP_WIFI_CMD_ON 1u

/* Leforgatás állapot jelzők (0x90, 1. bájt) */
#define EXTDISP_CALIB_FLAG_SESSION 0x01u        /* leforgatás folyamatban (nullázástól mentésig) */
#define EXTDISP_CALIB_FLAG_ZERO_FOUND 0x02u     /* megvan a nullpont, a számlálás él (0 = tekerj, amíg megjön) */
#define EXTDISP_CALIB_FLAG_TARGET_REACHED 0x04u /* a megszámolt fordulat elérte a célt */
#define EXTDISP_CALIB_FLAG_MOTOR_RUNNING 0x08u  /* a motor éppen forgat */
#define EXTDISP_CALIB_FLAG_ELECTRIC 0x10u       /* motoros hajtás (0 = talajhajtás: kézzel kell tekerni) */
#define EXTDISP_CALIB_FLAG_HALF_BY_HAND 0x20u   /* a cél fél fordulatra végződik, a felet kézzel kell rátenni */

/* Tiltási okok (0x90 7. bájt, 0xF0 3. bájt). A 0 = nincs tiltás. */
#define EXTDISP_INHIBIT_NONE 0u
#define EXTDISP_INHIBIT_MOVING 1u
#define EXTDISP_INHIBIT_WORK_SWITCH 2u
#define EXTDISP_INHIBIT_METERING 3u
#define EXTDISP_INHIBIT_DISPLAY_LOST 4u
#define EXTDISP_INHIBIT_ALREADY_RUNNING 5u
#define EXTDISP_INHIBIT_BAD_PARAM 6u
#define EXTDISP_INHIBIT_NO_SUCH_UNIT 7u
#define EXTDISP_INHIBIT_NOT_SUPPORTED 8u
#define EXTDISP_INHIBIT_NO_SCALE_DATA 9u   /* nincs érvényes mérlegadat */
#define EXTDISP_INHIBIT_SCALE_EMPTY 10u    /* a mérleg üres (előbb tára, aztán a vödör) */
#define EXTDISP_INHIBIT_NO_WEIGHT 11u      /* nincs lemért tömeg, nincs mit menteni */
#define EXTDISP_INHIBIT_LIST_FULL 12u      /* a részmérések listája megtelt */
#define EXTDISP_INHIBIT_NOT_ELECTRIC 13u   /* motoros forgatás csak elektromos hajtással */
#define EXTDISP_INHIBIT_TUNING 14u         /* önhangolás fut */
#define EXTDISP_INHIBIT_TARGET_REACHED 15u /* a célfordulat megvan: mérj, vagy kezdj új leforgatást */
#define EXTDISP_INHIBIT_MAX 15u

/* Telítettség (0xA0, 6. bájt) */
#define EXTDISP_FILL_UNKNOWN 0xFFu

/* WiFi állapot (0xC0, 1. bájt) */
#define EXTDISP_WIFI_OFF 0u
#define EXTDISP_WIFI_ON 1u
#define EXTDISP_WIFI_CLIENT 2u
#define EXTDISP_WIFI_OTA 3u

/* Nyugta eredmény (0xF0, 2. bájt) */
#define EXTDISP_ACK_OK 0u
#define EXTDISP_ACK_INHIBITED 1u
#define EXTDISP_ACK_BAD_DATA 2u
#define EXTDISP_ACK_BUSY 3u

/* ------------------------------------------------------------------------ */
/* Üzenet struktúrák                                                         */
/* ------------------------------------------------------------------------ */

/* 0x80 Képességek (vezérlő -> kijelző) */
typedef struct {
    uint8_t machine_type;     /* EXTDISP_MACHINE_* */
    uint8_t protocol_version; /* a küldő EXTDISP_PROTOCOL_VERSION értéke */
    uint8_t tank_count;       /* 0..2 */
    uint8_t tank_mode;        /* EXTDISP_TANK_MODE_* */
    uint8_t light_count;      /* 0..2 */
    uint8_t features;         /* EXTDISP_FEATURE_* bitek */
    uint8_t fw_major;         /* vezérlő firmware fő verzió */
    uint8_t feeder_count;     /* adagolók száma, 0..2 (a 4. bájt bit4-5-ben megy) */
} extdisp_capabilities_t;

/* 0x01 Kijelző szívverés (kijelző -> vezérlő, 200 ms) */
typedef struct {
    uint8_t counter;       /* minden küldésnél +1, 255 után 0 */
    uint8_t flags;         /* EXTDISP_HB_FLAG_* */
    uint8_t active_screen; /* EXTDISP_SCREEN_* */
} extdisp_heartbeat_t;

/* 0x10 Leforgatás parancs (vetőgép) */
typedef struct {
    uint8_t action; /* EXTDISP_CALIB_ACT_* */
    uint8_t param;  /* a művelettől függ (adagoló, fordulatszám-választás, sorszám), különben 0 */
    uint32_t value; /* ADD_WEIGHT: tömeg grammban, különben 0 */
} extdisp_calib_command_t;

/* 0x20 Tartály művelet */
typedef struct {
    uint8_t tank;       /* 0 = közös, 1, 2 */
    uint8_t action;     /* EXTDISP_TANK_ACT_* */
    uint32_t amount_hg; /* 0,1 kg egységben; nullázásnál nem számít */
} extdisp_tank_command_t;

/* 0x30 Munkalámpa */
typedef struct {
    uint8_t light; /* 1, 2 */
    uint8_t state; /* EXTDISP_LIGHT_* */
} extdisp_light_command_t;

/* 0x40 WiFi szerviz mód */
typedef struct {
    uint8_t on; /* EXTDISP_WIFI_CMD_* */
} extdisp_wifi_command_t;

/* 0x81 Gépállapot / szívverés (vezérlő -> kijelző, 200 ms) */
typedef struct {
    uint8_t counter;
    uint8_t flags;       /* EXTDISP_ST_FLAG_* */
    uint16_t speed_ckmh; /* 0,01 km/h egységben */
    uint16_t fan_rpm;    /* turbina fordulat 1/perc, EXTDISP_FAN_RPM_UNKNOWN = nincs adat */
} extdisp_machine_status_t;

/* 0x90 Leforgatás állapot (vetőgép; 200 ms, amíg a kijelző aktív képernyője a leforgatás) */
typedef struct {
    uint8_t flags;            /* EXTDISP_CALIB_FLAG_* */
    uint8_t feeder;           /* a kiválasztott adagoló: 1 vagy 2 */
    uint8_t turn_option;      /* fordulatszám-választás: 0..2 */
    uint16_t counted_centirev; /* megszámolt fordulat, 0,01 fordulatban */
    uint16_t target_decirev;  /* cél fordulat, 0,1 fordulatban (850, 425, 170) */
    uint8_t weight_count;     /* a részmérések száma, 0..10 */
} extdisp_calib_status_t;

/* 0x91 Leforgatás eredmény (vetőgép; a 0x90-nel együtt) */
typedef struct {
    uint8_t feeder;       /* 1 vagy 2 */
    uint32_t total_g;     /* a részmérések összege grammban (24 bit) */
    uint32_t mg_per_rev;  /* az új érték: g/fordulat x 1000 (24 bit); 0 = még nincs eredmény */
} extdisp_calib_result_t;

/* 0x92 Mérleg állapot (vetőgép; 200 ms, amíg a kijelző aktív képernyője a leforgatás) */
typedef struct {
    uint8_t flags;     /* EXTDISP_SCALE_FLAG_* */
    int32_t weight_dg; /* tömeg 0,1 g egységben, előjeles (tárázás után negatív is lehet) */
} extdisp_scale_status_t;

/* 0x93 Részmérés tétel (vetőgép; a 0x90-nel együtt, ciklusonként egy tétel körbejárva) */
typedef struct {
    uint8_t index;     /* a tétel sorszáma, 0..count-1 (ha count = 0, nem számít) */
    uint8_t count;     /* a részmérések száma, 0..10 */
    uint32_t weight_g; /* a tétel tömege grammban (24 bit) */
} extdisp_calib_weight_item_t;

/* 0x94 Mentett kalibráció (vetőgép; 1 s, amíg a kijelző aktív képernyője a leforgatás) */
typedef struct {
    uint8_t feeder;      /* 1 vagy 2 */
    uint32_t mg_per_rev; /* a jelenleg mentett érték: g/fordulat x 1000 (24 bit); 0 = nincs */
} extdisp_calib_saved_t;

/* 0xA0 Tartály állapot (1 s, tartályonként) */
typedef struct {
    uint8_t tank;          /* 0 = közös, 1, 2 */
    uint32_t remaining_hg; /* 0,1 kg egységben */
    uint8_t fill_percent;  /* 0..100, vagy EXTDISP_FILL_UNKNOWN */
} extdisp_tank_status_t;

/* 0xC0 WiFi állapot (1 s, ha be van) */
typedef struct {
    uint8_t state;          /* EXTDISP_WIFI_* */
    uint16_t minutes_left;  /* hátralévő perc */
} extdisp_wifi_status_t;

/* 0xF0 Nyugta */
typedef struct {
    uint8_t command_id;     /* a nyugtázott parancs azonosítója */
    uint8_t result;         /* EXTDISP_ACK_* */
    uint8_t inhibit_reason; /* EXTDISP_INHIBIT_* */
} extdisp_ack_t;

/* ------------------------------------------------------------------------ */
/* Segédfüggvények                                                           */
/* ------------------------------------------------------------------------ */

static inline void extdisp_put_u16(uint8_t *p, uint16_t v)
{
    p[0] = (uint8_t)(v & 0xFFu);
    p[1] = (uint8_t)((v >> 8) & 0xFFu);
}

static inline void extdisp_put_u32(uint8_t *p, uint32_t v)
{
    p[0] = (uint8_t)(v & 0xFFu);
    p[1] = (uint8_t)((v >> 8) & 0xFFu);
    p[2] = (uint8_t)((v >> 16) & 0xFFu);
    p[3] = (uint8_t)((v >> 24) & 0xFFu);
}

static inline void extdisp_put_u24(uint8_t *p, uint32_t v)
{
    p[0] = (uint8_t)(v & 0xFFu);
    p[1] = (uint8_t)((v >> 8) & 0xFFu);
    p[2] = (uint8_t)((v >> 16) & 0xFFu);
}

static inline uint32_t extdisp_get_u24(const uint8_t *p)
{
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16);
}

/* A leforgatás fordulatszám-választásának értéke tizedfordulatban (0 = érvénytelen választás). */
static inline uint16_t extdisp_calib_turns_x10(uint8_t option)
{
    return (uint16_t)(option == 0u ? 850u : (option == 1u ? 425u : (option == 2u ? 170u : 0u)));
}

static inline uint16_t extdisp_get_u16(const uint8_t *p)
{
    return (uint16_t)((uint16_t)p[0] | ((uint16_t)p[1] << 8));
}

static inline uint32_t extdisp_get_u32(const uint8_t *p)
{
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) |
           ((uint32_t)p[3] << 24);
}

/* A keretet tartalék bájtokkal tölti fel, és beírja az azonosítót. */
static inline void extdisp_frame_init(uint8_t *frame, uint8_t id)
{
    size_t i;
    frame[0] = id;
    for (i = 1; i < EXTDISP_FRAME_LEN; i++) {
        frame[i] = EXTDISP_RESERVED_BYTE;
    }
}

/* Közös ellenőrzés a dekóderek elején. */
static inline extdisp_result_t extdisp_check_frame(const uint8_t *data, size_t len,
                                                   const void *out, uint8_t id)
{
    if (data == NULL || out == NULL) {
        return EXTDISP_ERR_NULL;
    }
    if (len != EXTDISP_FRAME_LEN) {
        return EXTDISP_ERR_LENGTH;
    }
    if (data[0] != id) {
        return EXTDISP_ERR_ID;
    }
    return EXTDISP_OK;
}

/* Tartomány-osztályozás -------------------------------------------------- */

/* Kijelző -> vezérlő irányú azonosító (mag vagy gépspecifikus). */
static inline bool extdisp_is_command_id(uint8_t id)
{
    return id >= EXTDISP_RANGE_CMD_CORE1_FIRST && id <= EXTDISP_RANGE_CMD_CORE2_LAST;
}

/* Vezérlő -> kijelző irányú azonosító (mag, gépspecifikus vagy nyugta). */
static inline bool extdisp_is_status_id(uint8_t id)
{
    return (id >= EXTDISP_RANGE_STATUS_CORE1_FIRST && id <= EXTDISP_RANGE_STATUS_CORE2_LAST) ||
           id == EXTDISP_ID_ACK;
}

/* Gépspecifikus azonosító (jelentése a géptípustól függ). */
static inline bool extdisp_is_machine_specific_id(uint8_t id)
{
    return (id >= EXTDISP_RANGE_CMD_MACHINE_FIRST && id <= EXTDISP_RANGE_CMD_MACHINE_LAST) ||
           (id >= EXTDISP_RANGE_STATUS_MACHINE_FIRST && id <= EXTDISP_RANGE_STATUS_MACHINE_LAST);
}

/* A mag (minden gépen azonos) része-e az azonosító. */
static inline bool extdisp_is_core_id(uint8_t id)
{
    return (extdisp_is_command_id(id) || extdisp_is_status_id(id)) &&
           !extdisp_is_machine_specific_id(id);
}

/* Kell-e nyugtát (0xF0) küldeni a parancsra. A szívverésre (0x01) nincs nyugta:
 * a vezérlő 200 ms-os gépállapot üzenete (0x81) jelzi, hogy él. */
static inline bool extdisp_command_needs_ack(uint8_t id)
{
    return extdisp_is_command_id(id) && id != EXTDISP_ID_HEARTBEAT;
}

/* A kijelző csak akkor küldhet parancsot, ha a vezérlő protokoll verzióját
 * ismeri ("Ismeretlen protokoll verzió" sor a spec biztonsági táblájában). */
static inline bool extdisp_version_supported(uint8_t version)
{
    return version == EXTDISP_PROTOCOL_VERSION;
}

/* Mező-ellenőrzések ------------------------------------------------------ */

static inline bool extdisp_valid_feeder(uint8_t feeder)
{
    return feeder <= EXTDISP_MAX_FEEDERS;
}

static inline bool extdisp_valid_tank(uint8_t tank)
{
    return tank <= EXTDISP_MAX_TANKS;
}

static inline bool extdisp_valid_light(uint8_t light)
{
    return light >= 1u && light <= EXTDISP_MAX_LIGHTS;
}

static inline bool extdisp_valid_capabilities(const extdisp_capabilities_t *m)
{
    return m->machine_type >= EXTDISP_MACHINE_SEEDER &&
           m->machine_type <= EXTDISP_MACHINE_SPRAYER &&
           m->tank_count <= EXTDISP_MAX_TANKS &&
           m->tank_mode <= EXTDISP_TANK_MODE_SEPARATE &&
           m->light_count <= EXTDISP_MAX_LIGHTS &&
           m->feeder_count <= EXTDISP_MAX_FEEDERS;
}

static inline bool extdisp_valid_calib_feeder(uint8_t feeder)
{
    return feeder >= 1u && feeder <= EXTDISP_MAX_FEEDERS;
}

static inline bool extdisp_valid_calib_command(const extdisp_calib_command_t *m)
{
    if (m->action < EXTDISP_CALIB_ACT_NEW || m->action > EXTDISP_CALIB_ACT_MAX) {
        return false;
    }
    switch (m->action) {
    case EXTDISP_CALIB_ACT_SELECT_FEEDER:
        return extdisp_valid_calib_feeder(m->param);
    case EXTDISP_CALIB_ACT_SELECT_TURNS:
        return m->param < EXTDISP_CALIB_TURN_OPTIONS;
    case EXTDISP_CALIB_ACT_ADD_WEIGHT:
        return m->value > 0u;
    case EXTDISP_CALIB_ACT_DELETE_WEIGHT:
        return m->param < EXTDISP_CALIB_MAX_WEIGHTS || m->param == EXTDISP_CALIB_DELETE_ALL;
    default:
        return true;
    }
}

static inline bool extdisp_valid_tank_command(const extdisp_tank_command_t *m)
{
    return extdisp_valid_tank(m->tank) && m->action >= EXTDISP_TANK_ACT_SET &&
           m->action <= EXTDISP_TANK_ACT_ZERO;
}

static inline bool extdisp_valid_light_command(const extdisp_light_command_t *m)
{
    return extdisp_valid_light(m->light) && m->state <= EXTDISP_LIGHT_TOGGLE;
}

static inline bool extdisp_valid_calib_status(const extdisp_calib_status_t *m)
{
    return extdisp_valid_calib_feeder(m->feeder) && m->turn_option < EXTDISP_CALIB_TURN_OPTIONS &&
           m->weight_count <= EXTDISP_CALIB_MAX_WEIGHTS;
}

static inline bool extdisp_valid_calib_result(const extdisp_calib_result_t *m)
{
    return extdisp_valid_calib_feeder(m->feeder) && m->total_g <= 0xFFFFFFu && m->mg_per_rev <= 0xFFFFFFu;
}

static inline bool extdisp_valid_calib_weight_item(const extdisp_calib_weight_item_t *m)
{
    return m->count <= EXTDISP_CALIB_MAX_WEIGHTS && (m->count == 0u || m->index < m->count) &&
           m->weight_g <= 0xFFFFFFu;
}

static inline bool extdisp_valid_calib_saved(const extdisp_calib_saved_t *m)
{
    return extdisp_valid_calib_feeder(m->feeder) && m->mg_per_rev <= 0xFFFFFFu;
}

static inline bool extdisp_valid_tank_status(const extdisp_tank_status_t *m)
{
    return extdisp_valid_tank(m->tank) &&
           (m->fill_percent <= 100u || m->fill_percent == EXTDISP_FILL_UNKNOWN);
}

static inline bool extdisp_valid_ack(const extdisp_ack_t *m)
{
    return m->result <= EXTDISP_ACK_BUSY && m->inhibit_reason <= EXTDISP_INHIBIT_MAX;
}

/* ------------------------------------------------------------------------ */
/* Kódolók és dekódolók                                                      */
/*                                                                           */
/* encode: struktúra -> 8 bájtos keret. Hibás mezőnél EXTDISP_ERR_VALUE, és   */
/*         a keret érintetlen marad.                                         */
/* decode: keret -> struktúra. len-nek pontosan 8-nak kell lennie.           */
/* ------------------------------------------------------------------------ */

/* 0x80 Képességek -------------------------------------------------------- */

static inline extdisp_result_t extdisp_encode_capabilities(const extdisp_capabilities_t *m,
                                                           uint8_t *frame)
{
    if (m == NULL || frame == NULL) {
        return EXTDISP_ERR_NULL;
    }
    if (!extdisp_valid_capabilities(m)) {
        return EXTDISP_ERR_VALUE;
    }
    extdisp_frame_init(frame, EXTDISP_ID_CAPABILITIES);
    frame[1] = m->machine_type;
    frame[2] = m->protocol_version;
    frame[3] = m->tank_count;
    frame[4] = (uint8_t)(m->tank_mode |
                         (uint8_t)(m->feeder_count << EXTDISP_CAP_FEEDER_COUNT_SHIFT));
    frame[5] = m->light_count;
    frame[6] = m->features;
    frame[7] = m->fw_major;
    return EXTDISP_OK;
}

/* Megjegyzés: ERR_VALUE esetén is ki van töltve a struktúra, így a kijelző
 * a protocol_version mezőt ismeretlen géptípus mellett is ki tudja olvasni. */
static inline extdisp_result_t extdisp_decode_capabilities(const uint8_t *data, size_t len,
                                                           extdisp_capabilities_t *m)
{
    extdisp_result_t r = extdisp_check_frame(data, len, m, EXTDISP_ID_CAPABILITIES);
    if (r != EXTDISP_OK) {
        return r;
    }
    m->machine_type = data[1];
    m->protocol_version = data[2];
    m->tank_count = data[3];
    m->tank_mode = (uint8_t)(data[4] & EXTDISP_CAP_TANK_MODE_MASK);
    m->feeder_count = (uint8_t)((data[4] & EXTDISP_CAP_FEEDER_COUNT_MASK) >>
                                EXTDISP_CAP_FEEDER_COUNT_SHIFT);
    m->light_count = data[5];
    m->features = data[6];
    m->fw_major = data[7];
    return extdisp_valid_capabilities(m) ? EXTDISP_OK : EXTDISP_ERR_VALUE;
}

/* 0x01 Kijelző szívverés ------------------------------------------------- */

static inline extdisp_result_t extdisp_encode_heartbeat(const extdisp_heartbeat_t *m,
                                                        uint8_t *frame)
{
    if (m == NULL || frame == NULL) {
        return EXTDISP_ERR_NULL;
    }
    extdisp_frame_init(frame, EXTDISP_ID_HEARTBEAT);
    frame[1] = m->counter;
    frame[2] = m->flags;
    frame[3] = m->active_screen;
    return EXTDISP_OK;
}

static inline extdisp_result_t extdisp_decode_heartbeat(const uint8_t *data, size_t len,
                                                        extdisp_heartbeat_t *m)
{
    extdisp_result_t r = extdisp_check_frame(data, len, m, EXTDISP_ID_HEARTBEAT);
    if (r != EXTDISP_OK) {
        return r;
    }
    m->counter = data[1];
    m->flags = data[2];
    m->active_screen = data[3];
    return EXTDISP_OK;
}

/* 0x10 Leforgatás parancs ------------------------------------------------ */

static inline extdisp_result_t extdisp_encode_calib_command(const extdisp_calib_command_t *m, uint8_t *frame)
{
    if (m == NULL || frame == NULL) {
        return EXTDISP_ERR_NULL;
    }
    if (!extdisp_valid_calib_command(m)) {
        return EXTDISP_ERR_VALUE;
    }
    extdisp_frame_init(frame, EXTDISP_ID_CALIB_COMMAND);
    frame[1] = m->action;
    frame[2] = m->param;
    extdisp_put_u32(&frame[3], m->value);
    return EXTDISP_OK;
}

static inline extdisp_result_t extdisp_decode_calib_command(const uint8_t *data, size_t len, extdisp_calib_command_t *m)
{
    extdisp_result_t r = extdisp_check_frame(data, len, m, EXTDISP_ID_CALIB_COMMAND);
    if (r != EXTDISP_OK) {
        return r;
    }
    m->action = data[1];
    m->param = data[2];
    m->value = extdisp_get_u32(&data[3]);
    return extdisp_valid_calib_command(m) ? EXTDISP_OK : EXTDISP_ERR_VALUE;
}

/* 0x20 Tartály művelet --------------------------------------------------- */

static inline extdisp_result_t extdisp_encode_tank_command(const extdisp_tank_command_t *m,
                                                           uint8_t *frame)
{
    if (m == NULL || frame == NULL) {
        return EXTDISP_ERR_NULL;
    }
    if (!extdisp_valid_tank_command(m)) {
        return EXTDISP_ERR_VALUE;
    }
    extdisp_frame_init(frame, EXTDISP_ID_TANK_COMMAND);
    frame[1] = m->tank;
    frame[2] = m->action;
    extdisp_put_u32(&frame[3], m->amount_hg);
    return EXTDISP_OK;
}

static inline extdisp_result_t extdisp_decode_tank_command(const uint8_t *data, size_t len,
                                                           extdisp_tank_command_t *m)
{
    extdisp_result_t r = extdisp_check_frame(data, len, m, EXTDISP_ID_TANK_COMMAND);
    if (r != EXTDISP_OK) {
        return r;
    }
    m->tank = data[1];
    m->action = data[2];
    m->amount_hg = extdisp_get_u32(&data[3]);
    return extdisp_valid_tank_command(m) ? EXTDISP_OK : EXTDISP_ERR_VALUE;
}

/* 0x30 Munkalámpa -------------------------------------------------------- */

static inline extdisp_result_t extdisp_encode_light_command(const extdisp_light_command_t *m,
                                                            uint8_t *frame)
{
    if (m == NULL || frame == NULL) {
        return EXTDISP_ERR_NULL;
    }
    if (!extdisp_valid_light_command(m)) {
        return EXTDISP_ERR_VALUE;
    }
    extdisp_frame_init(frame, EXTDISP_ID_LIGHT_COMMAND);
    frame[1] = m->light;
    frame[2] = m->state;
    return EXTDISP_OK;
}

static inline extdisp_result_t extdisp_decode_light_command(const uint8_t *data, size_t len,
                                                            extdisp_light_command_t *m)
{
    extdisp_result_t r = extdisp_check_frame(data, len, m, EXTDISP_ID_LIGHT_COMMAND);
    if (r != EXTDISP_OK) {
        return r;
    }
    m->light = data[1];
    m->state = data[2];
    return extdisp_valid_light_command(m) ? EXTDISP_OK : EXTDISP_ERR_VALUE;
}

/* 0x40 WiFi szerviz mód -------------------------------------------------- */

static inline extdisp_result_t extdisp_encode_wifi_command(const extdisp_wifi_command_t *m,
                                                           uint8_t *frame)
{
    if (m == NULL || frame == NULL) {
        return EXTDISP_ERR_NULL;
    }
    if (m->on > EXTDISP_WIFI_CMD_ON) {
        return EXTDISP_ERR_VALUE;
    }
    extdisp_frame_init(frame, EXTDISP_ID_WIFI_COMMAND);
    frame[1] = m->on;
    return EXTDISP_OK;
}

static inline extdisp_result_t extdisp_decode_wifi_command(const uint8_t *data, size_t len,
                                                           extdisp_wifi_command_t *m)
{
    extdisp_result_t r = extdisp_check_frame(data, len, m, EXTDISP_ID_WIFI_COMMAND);
    if (r != EXTDISP_OK) {
        return r;
    }
    m->on = data[1];
    return (m->on <= EXTDISP_WIFI_CMD_ON) ? EXTDISP_OK : EXTDISP_ERR_VALUE;
}

/* 0x81 Gépállapot / szívverés -------------------------------------------- */

static inline extdisp_result_t extdisp_encode_machine_status(const extdisp_machine_status_t *m,
                                                             uint8_t *frame)
{
    if (m == NULL || frame == NULL) {
        return EXTDISP_ERR_NULL;
    }
    extdisp_frame_init(frame, EXTDISP_ID_MACHINE_STATUS);
    frame[1] = m->counter;
    frame[2] = m->flags;
    extdisp_put_u16(&frame[3], m->speed_ckmh);
    extdisp_put_u16(&frame[5], m->fan_rpm);
    return EXTDISP_OK;
}

static inline extdisp_result_t extdisp_decode_machine_status(const uint8_t *data, size_t len,
                                                             extdisp_machine_status_t *m)
{
    extdisp_result_t r = extdisp_check_frame(data, len, m, EXTDISP_ID_MACHINE_STATUS);
    if (r != EXTDISP_OK) {
        return r;
    }
    m->counter = data[1];
    m->flags = data[2];
    m->speed_ckmh = extdisp_get_u16(&data[3]);
    m->fan_rpm = extdisp_get_u16(&data[5]);
    return EXTDISP_OK;
}

/* 0x90 Leforgatás állapot ------------------------------------------------ */

static inline extdisp_result_t extdisp_encode_calib_status(const extdisp_calib_status_t *m, uint8_t *frame)
{
    if (m == NULL || frame == NULL) {
        return EXTDISP_ERR_NULL;
    }
    if (!extdisp_valid_calib_status(m)) {
        return EXTDISP_ERR_VALUE;
    }
    extdisp_frame_init(frame, EXTDISP_ID_CALIB_STATUS);
    frame[1] = m->flags;
    frame[2] = (uint8_t)(m->feeder | (uint8_t)(m->turn_option << 4));
    extdisp_put_u16(&frame[3], m->counted_centirev);
    extdisp_put_u16(&frame[5], m->target_decirev);
    frame[7] = m->weight_count;
    return EXTDISP_OK;
}

static inline extdisp_result_t extdisp_decode_calib_status(const uint8_t *data, size_t len, extdisp_calib_status_t *m)
{
    extdisp_result_t r = extdisp_check_frame(data, len, m, EXTDISP_ID_CALIB_STATUS);
    if (r != EXTDISP_OK) {
        return r;
    }
    m->flags = data[1];
    m->feeder = (uint8_t)(data[2] & 0x0Fu);
    m->turn_option = (uint8_t)(data[2] >> 4);
    m->counted_centirev = extdisp_get_u16(&data[3]);
    m->target_decirev = extdisp_get_u16(&data[5]);
    m->weight_count = data[7];
    return extdisp_valid_calib_status(m) ? EXTDISP_OK : EXTDISP_ERR_VALUE;
}

/* 0x91 Leforgatás eredmény ----------------------------------------------- */

static inline extdisp_result_t extdisp_encode_calib_result(const extdisp_calib_result_t *m, uint8_t *frame)
{
    if (m == NULL || frame == NULL) {
        return EXTDISP_ERR_NULL;
    }
    if (!extdisp_valid_calib_result(m)) {
        return EXTDISP_ERR_VALUE;
    }
    extdisp_frame_init(frame, EXTDISP_ID_CALIB_RESULT);
    frame[1] = m->feeder;
    extdisp_put_u24(&frame[2], m->total_g);
    extdisp_put_u24(&frame[5], m->mg_per_rev);
    return EXTDISP_OK;
}

static inline extdisp_result_t extdisp_decode_calib_result(const uint8_t *data, size_t len, extdisp_calib_result_t *m)
{
    extdisp_result_t r = extdisp_check_frame(data, len, m, EXTDISP_ID_CALIB_RESULT);
    if (r != EXTDISP_OK) {
        return r;
    }
    m->feeder = data[1];
    m->total_g = extdisp_get_u24(&data[2]);
    m->mg_per_rev = extdisp_get_u24(&data[5]);
    return extdisp_valid_calib_result(m) ? EXTDISP_OK : EXTDISP_ERR_VALUE;
}

/* 0x92 Mérleg állapot ---------------------------------------------------- */

static inline extdisp_result_t extdisp_encode_scale_status(const extdisp_scale_status_t *m,
                                                           uint8_t *frame)
{
    if (m == NULL || frame == NULL) {
        return EXTDISP_ERR_NULL;
    }
    extdisp_frame_init(frame, EXTDISP_ID_SCALE_STATUS);
    frame[1] = m->flags;
    extdisp_put_u32(&frame[2], (uint32_t)m->weight_dg);
    return EXTDISP_OK;
}

static inline extdisp_result_t extdisp_decode_scale_status(const uint8_t *data, size_t len,
                                                           extdisp_scale_status_t *m)
{
    uint32_t raw;
    extdisp_result_t r = extdisp_check_frame(data, len, m, EXTDISP_ID_SCALE_STATUS);
    if (r != EXTDISP_OK) {
        return r;
    }
    m->flags = data[1];
    /* kettes komplemens, hordozható módon */
    raw = extdisp_get_u32(&data[2]);
    m->weight_dg = (raw & 0x80000000u) ? (int32_t)(-(int64_t)(0x100000000ull - raw)) : (int32_t)raw;
    return EXTDISP_OK;
}

/* 0x93 Részmérés tétel --------------------------------------------------- */

static inline extdisp_result_t extdisp_encode_calib_weight_item(const extdisp_calib_weight_item_t *m, uint8_t *frame)
{
    if (m == NULL || frame == NULL) {
        return EXTDISP_ERR_NULL;
    }
    if (!extdisp_valid_calib_weight_item(m)) {
        return EXTDISP_ERR_VALUE;
    }
    extdisp_frame_init(frame, EXTDISP_ID_CALIB_WEIGHT_ITEM);
    frame[1] = m->index;
    frame[2] = m->count;
    extdisp_put_u24(&frame[3], m->weight_g);
    return EXTDISP_OK;
}

static inline extdisp_result_t extdisp_decode_calib_weight_item(const uint8_t *data, size_t len, extdisp_calib_weight_item_t *m)
{
    extdisp_result_t r = extdisp_check_frame(data, len, m, EXTDISP_ID_CALIB_WEIGHT_ITEM);
    if (r != EXTDISP_OK) {
        return r;
    }
    m->index = data[1];
    m->count = data[2];
    m->weight_g = extdisp_get_u24(&data[3]);
    return extdisp_valid_calib_weight_item(m) ? EXTDISP_OK : EXTDISP_ERR_VALUE;
}

/* 0x94 Mentett kalibráció ------------------------------------------------ */

static inline extdisp_result_t extdisp_encode_calib_saved(const extdisp_calib_saved_t *m, uint8_t *frame)
{
    if (m == NULL || frame == NULL) {
        return EXTDISP_ERR_NULL;
    }
    if (!extdisp_valid_calib_saved(m)) {
        return EXTDISP_ERR_VALUE;
    }
    extdisp_frame_init(frame, EXTDISP_ID_CALIB_SAVED);
    frame[1] = m->feeder;
    extdisp_put_u24(&frame[2], m->mg_per_rev);
    return EXTDISP_OK;
}

static inline extdisp_result_t extdisp_decode_calib_saved(const uint8_t *data, size_t len, extdisp_calib_saved_t *m)
{
    extdisp_result_t r = extdisp_check_frame(data, len, m, EXTDISP_ID_CALIB_SAVED);
    if (r != EXTDISP_OK) {
        return r;
    }
    m->feeder = data[1];
    m->mg_per_rev = extdisp_get_u24(&data[2]);
    return extdisp_valid_calib_saved(m) ? EXTDISP_OK : EXTDISP_ERR_VALUE;
}

/* 0xA0 Tartály állapot --------------------------------------------------- */

static inline extdisp_result_t extdisp_encode_tank_status(const extdisp_tank_status_t *m,
                                                          uint8_t *frame)
{
    if (m == NULL || frame == NULL) {
        return EXTDISP_ERR_NULL;
    }
    if (!extdisp_valid_tank_status(m)) {
        return EXTDISP_ERR_VALUE;
    }
    extdisp_frame_init(frame, EXTDISP_ID_TANK_STATUS);
    frame[1] = m->tank;
    extdisp_put_u32(&frame[2], m->remaining_hg);
    frame[6] = m->fill_percent;
    return EXTDISP_OK;
}

static inline extdisp_result_t extdisp_decode_tank_status(const uint8_t *data, size_t len,
                                                          extdisp_tank_status_t *m)
{
    extdisp_result_t r = extdisp_check_frame(data, len, m, EXTDISP_ID_TANK_STATUS);
    if (r != EXTDISP_OK) {
        return r;
    }
    m->tank = data[1];
    m->remaining_hg = extdisp_get_u32(&data[2]);
    m->fill_percent = data[6];
    return extdisp_valid_tank_status(m) ? EXTDISP_OK : EXTDISP_ERR_VALUE;
}

/* 0xC0 WiFi állapot ------------------------------------------------------ */

static inline extdisp_result_t extdisp_encode_wifi_status(const extdisp_wifi_status_t *m,
                                                          uint8_t *frame)
{
    if (m == NULL || frame == NULL) {
        return EXTDISP_ERR_NULL;
    }
    if (m->state > EXTDISP_WIFI_OTA) {
        return EXTDISP_ERR_VALUE;
    }
    extdisp_frame_init(frame, EXTDISP_ID_WIFI_STATUS);
    frame[1] = m->state;
    extdisp_put_u16(&frame[2], m->minutes_left);
    return EXTDISP_OK;
}

static inline extdisp_result_t extdisp_decode_wifi_status(const uint8_t *data, size_t len,
                                                          extdisp_wifi_status_t *m)
{
    extdisp_result_t r = extdisp_check_frame(data, len, m, EXTDISP_ID_WIFI_STATUS);
    if (r != EXTDISP_OK) {
        return r;
    }
    m->state = data[1];
    m->minutes_left = extdisp_get_u16(&data[2]);
    return (m->state <= EXTDISP_WIFI_OTA) ? EXTDISP_OK : EXTDISP_ERR_VALUE;
}

/* 0xF0 Nyugta ------------------------------------------------------------ */

static inline extdisp_result_t extdisp_encode_ack(const extdisp_ack_t *m, uint8_t *frame)
{
    if (m == NULL || frame == NULL) {
        return EXTDISP_ERR_NULL;
    }
    if (!extdisp_valid_ack(m)) {
        return EXTDISP_ERR_VALUE;
    }
    extdisp_frame_init(frame, EXTDISP_ID_ACK);
    frame[1] = m->command_id;
    frame[2] = m->result;
    frame[3] = m->inhibit_reason;
    return EXTDISP_OK;
}

static inline extdisp_result_t extdisp_decode_ack(const uint8_t *data, size_t len,
                                                  extdisp_ack_t *m)
{
    extdisp_result_t r = extdisp_check_frame(data, len, m, EXTDISP_ID_ACK);
    if (r != EXTDISP_OK) {
        return r;
    }
    m->command_id = data[1];
    m->result = data[2];
    m->inhibit_reason = data[3];
    return extdisp_valid_ack(m) ? EXTDISP_OK : EXTDISP_ERR_VALUE;
}

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* EXT_DISPLAY_PROTOCOL_H */
