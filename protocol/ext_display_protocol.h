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
#define EXTDISP_CALIB_DEFAULT_TARGET_REVS 85u

#define EXTDISP_MAX_TANKS 2u
#define EXTDISP_MAX_LIGHTS 2u
#define EXTDISP_MAX_FEEDERS 2u

/* ------------------------------------------------------------------------ */
/* Üzenet azonosítók és tartományok                                          */
/* ------------------------------------------------------------------------ */

/* Kijelző -> vezérlő (parancsok) */
#define EXTDISP_ID_HEARTBEAT 0x01u
#define EXTDISP_ID_CALIB_COMMAND 0x10u  /* vetőgép */
#define EXTDISP_ID_CALIB_WEIGHT 0x11u   /* vetőgép */
#define EXTDISP_ID_CALIB_DECISION 0x12u /* vetőgép */
#define EXTDISP_ID_TANK_COMMAND 0x20u
#define EXTDISP_ID_LIGHT_COMMAND 0x30u
#define EXTDISP_ID_WIFI_COMMAND 0x40u

/* Vezérlő -> kijelző (állapotok) */
#define EXTDISP_ID_CAPABILITIES 0x80u
#define EXTDISP_ID_MACHINE_STATUS 0x81u
#define EXTDISP_ID_CALIB_STATUS 0x90u /* vetőgép */
#define EXTDISP_ID_CALIB_RESULT 0x91u /* vetőgép */
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

/* Leforgatás művelet (0x10, 1. bájt) */
#define EXTDISP_CALIB_ACT_PREPARE 1u
#define EXTDISP_CALIB_ACT_START 2u
#define EXTDISP_CALIB_ACT_STOP 3u

/* Adagoló / tartály választó */
#define EXTDISP_FEEDER_ALL 0u
#define EXTDISP_TANK_SHARED 0u

/* Eredmény döntés (0x12, 2. bájt) */
#define EXTDISP_DECISION_REJECT 0u
#define EXTDISP_DECISION_ACCEPT 1u

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

/* Leforgatás állapot (0x90, 1. bájt) */
#define EXTDISP_CALIB_IDLE 0u
#define EXTDISP_CALIB_PREPARED 1u
#define EXTDISP_CALIB_RUNNING 2u
#define EXTDISP_CALIB_WAIT_WEIGHT 3u
#define EXTDISP_CALIB_RESULT_READY 4u
#define EXTDISP_CALIB_ABORTED 5u
#define EXTDISP_CALIB_INHIBITED 6u
#define EXTDISP_CALIB_STATE_MAX 6u

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
#define EXTDISP_INHIBIT_MAX 8u

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
    uint8_t action;       /* EXTDISP_CALIB_ACT_* */
    uint8_t feeder;       /* 0 = mind, 1, 2 */
    uint16_t target_revs; /* fordulatszám-cél, egész fordulat */
} extdisp_calib_command_t;

/* 0x11 Mért tömeg (vetőgép) */
typedef struct {
    uint8_t feeder;     /* 0 = mind, 1, 2 */
    uint32_t weight_dg; /* 0,1 g egységben */
} extdisp_calib_weight_t;

/* 0x12 Eredmény döntés (vetőgép) */
typedef struct {
    uint8_t feeder;   /* 0 = mind, 1, 2 */
    uint8_t decision; /* EXTDISP_DECISION_* */
} extdisp_calib_decision_t;

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
} extdisp_machine_status_t;

/* 0x90 Leforgatás állapot (vetőgép) */
typedef struct {
    uint8_t state;          /* EXTDISP_CALIB_* */
    uint8_t feeder;         /* 0 = mind, 1, 2 */
    uint16_t done_revs;     /* kész fordulat */
    uint16_t target_revs;   /* cél fordulat */
    uint8_t inhibit_reason; /* EXTDISP_INHIBIT_* */
} extdisp_calib_status_t;

/* 0x91 Leforgatás eredmény (vetőgép) */
typedef struct {
    uint8_t feeder;         /* 0 = mind, 1, 2 */
    uint32_t mg_per_rev;    /* g/fordulat x 1000 */
    int16_t deviation_pm;   /* eltérés az előzőtől 0,1 %-ban, előjeles */
} extdisp_calib_result_t;

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

static inline bool extdisp_valid_calib_command(const extdisp_calib_command_t *m)
{
    return m->action >= EXTDISP_CALIB_ACT_PREPARE && m->action <= EXTDISP_CALIB_ACT_STOP &&
           extdisp_valid_feeder(m->feeder);
}

static inline bool extdisp_valid_calib_decision(const extdisp_calib_decision_t *m)
{
    return extdisp_valid_feeder(m->feeder) && m->decision <= EXTDISP_DECISION_ACCEPT;
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
    return m->state <= EXTDISP_CALIB_STATE_MAX && extdisp_valid_feeder(m->feeder) &&
           m->inhibit_reason <= EXTDISP_INHIBIT_MAX;
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

static inline extdisp_result_t extdisp_encode_calib_command(const extdisp_calib_command_t *m,
                                                            uint8_t *frame)
{
    if (m == NULL || frame == NULL) {
        return EXTDISP_ERR_NULL;
    }
    if (!extdisp_valid_calib_command(m)) {
        return EXTDISP_ERR_VALUE;
    }
    extdisp_frame_init(frame, EXTDISP_ID_CALIB_COMMAND);
    frame[1] = m->action;
    frame[2] = m->feeder;
    extdisp_put_u16(&frame[3], m->target_revs);
    return EXTDISP_OK;
}

static inline extdisp_result_t extdisp_decode_calib_command(const uint8_t *data, size_t len,
                                                            extdisp_calib_command_t *m)
{
    extdisp_result_t r = extdisp_check_frame(data, len, m, EXTDISP_ID_CALIB_COMMAND);
    if (r != EXTDISP_OK) {
        return r;
    }
    m->action = data[1];
    m->feeder = data[2];
    m->target_revs = extdisp_get_u16(&data[3]);
    return extdisp_valid_calib_command(m) ? EXTDISP_OK : EXTDISP_ERR_VALUE;
}

/* 0x11 Mért tömeg -------------------------------------------------------- */

static inline extdisp_result_t extdisp_encode_calib_weight(const extdisp_calib_weight_t *m,
                                                           uint8_t *frame)
{
    if (m == NULL || frame == NULL) {
        return EXTDISP_ERR_NULL;
    }
    if (!extdisp_valid_feeder(m->feeder)) {
        return EXTDISP_ERR_VALUE;
    }
    extdisp_frame_init(frame, EXTDISP_ID_CALIB_WEIGHT);
    frame[1] = m->feeder;
    extdisp_put_u32(&frame[2], m->weight_dg);
    return EXTDISP_OK;
}

static inline extdisp_result_t extdisp_decode_calib_weight(const uint8_t *data, size_t len,
                                                           extdisp_calib_weight_t *m)
{
    extdisp_result_t r = extdisp_check_frame(data, len, m, EXTDISP_ID_CALIB_WEIGHT);
    if (r != EXTDISP_OK) {
        return r;
    }
    m->feeder = data[1];
    m->weight_dg = extdisp_get_u32(&data[2]);
    return extdisp_valid_feeder(m->feeder) ? EXTDISP_OK : EXTDISP_ERR_VALUE;
}

/* 0x12 Eredmény döntés --------------------------------------------------- */

static inline extdisp_result_t extdisp_encode_calib_decision(const extdisp_calib_decision_t *m,
                                                             uint8_t *frame)
{
    if (m == NULL || frame == NULL) {
        return EXTDISP_ERR_NULL;
    }
    if (!extdisp_valid_calib_decision(m)) {
        return EXTDISP_ERR_VALUE;
    }
    extdisp_frame_init(frame, EXTDISP_ID_CALIB_DECISION);
    frame[1] = m->feeder;
    frame[2] = m->decision;
    return EXTDISP_OK;
}

static inline extdisp_result_t extdisp_decode_calib_decision(const uint8_t *data, size_t len,
                                                             extdisp_calib_decision_t *m)
{
    extdisp_result_t r = extdisp_check_frame(data, len, m, EXTDISP_ID_CALIB_DECISION);
    if (r != EXTDISP_OK) {
        return r;
    }
    m->feeder = data[1];
    m->decision = data[2];
    return extdisp_valid_calib_decision(m) ? EXTDISP_OK : EXTDISP_ERR_VALUE;
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
    return EXTDISP_OK;
}

/* 0x90 Leforgatás állapot ------------------------------------------------ */

static inline extdisp_result_t extdisp_encode_calib_status(const extdisp_calib_status_t *m,
                                                           uint8_t *frame)
{
    if (m == NULL || frame == NULL) {
        return EXTDISP_ERR_NULL;
    }
    if (!extdisp_valid_calib_status(m)) {
        return EXTDISP_ERR_VALUE;
    }
    extdisp_frame_init(frame, EXTDISP_ID_CALIB_STATUS);
    frame[1] = m->state;
    frame[2] = m->feeder;
    extdisp_put_u16(&frame[3], m->done_revs);
    extdisp_put_u16(&frame[5], m->target_revs);
    frame[7] = m->inhibit_reason;
    return EXTDISP_OK;
}

static inline extdisp_result_t extdisp_decode_calib_status(const uint8_t *data, size_t len,
                                                           extdisp_calib_status_t *m)
{
    extdisp_result_t r = extdisp_check_frame(data, len, m, EXTDISP_ID_CALIB_STATUS);
    if (r != EXTDISP_OK) {
        return r;
    }
    m->state = data[1];
    m->feeder = data[2];
    m->done_revs = extdisp_get_u16(&data[3]);
    m->target_revs = extdisp_get_u16(&data[5]);
    m->inhibit_reason = data[7];
    return extdisp_valid_calib_status(m) ? EXTDISP_OK : EXTDISP_ERR_VALUE;
}

/* 0x91 Leforgatás eredmény ----------------------------------------------- */

static inline extdisp_result_t extdisp_encode_calib_result(const extdisp_calib_result_t *m,
                                                           uint8_t *frame)
{
    if (m == NULL || frame == NULL) {
        return EXTDISP_ERR_NULL;
    }
    if (!extdisp_valid_feeder(m->feeder)) {
        return EXTDISP_ERR_VALUE;
    }
    extdisp_frame_init(frame, EXTDISP_ID_CALIB_RESULT);
    frame[1] = m->feeder;
    extdisp_put_u32(&frame[2], m->mg_per_rev);
    extdisp_put_u16(&frame[6], (uint16_t)m->deviation_pm);
    return EXTDISP_OK;
}

static inline extdisp_result_t extdisp_decode_calib_result(const uint8_t *data, size_t len,
                                                           extdisp_calib_result_t *m)
{
    uint16_t raw;
    extdisp_result_t r = extdisp_check_frame(data, len, m, EXTDISP_ID_CALIB_RESULT);
    if (r != EXTDISP_OK) {
        return r;
    }
    m->feeder = data[1];
    m->mg_per_rev = extdisp_get_u32(&data[2]);
    /* kettes komplemens, hordozható módon (nem implementációfüggő konverzió) */
    raw = extdisp_get_u16(&data[6]);
    m->deviation_pm = (raw & 0x8000u) ? (int16_t)((int32_t)raw - 65536) : (int16_t)raw;
    return extdisp_valid_feeder(m->feeder) ? EXTDISP_OK : EXTDISP_ERR_VALUE;
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
