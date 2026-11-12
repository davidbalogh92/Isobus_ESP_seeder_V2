# Következő nagyobb átalakítás – összegyűjtött feladatok

Állapot: **terv, még nincs elkezdve.** Utolsó frissítés: 2026-11-12.
Minden pont a felhasználó kérése alapján; a megvalósítás egyben, a gépprofillal együtt történik.

---

## 1. Hajtás szerkezete

```
Hajtás típusa
├── Talajhajtás (kerék → kuplungok)                    – a mostani
└── Elektromos
    ├── Motorvezérlés
    │   ├── PWM (H-híd)
    │   │   ├── PWM + DIR   (pl. Cytron MD10C / MD20A)
    │   │   └── PWM + PWM   (pl. IBT-2 / BTS7960: RPWM + LPWM)
    │   └── Keya CAN-motor (KY40ACS…, saját szabályzás)
    └── Motorok száma
        ├── 1 motor → hajtja mindkét adagolót, adagolónként KUPLUNG marad
        └── 2 motor → adagolónként saját motor (mostani elektromos mód)
```

### 1.1 Egy motor, két adagoló (kuplungokkal)
- A motor a talajkereket „helyettesíti”: fordulata a haladási sebességgel arányos
  (gépállandó: adagoló-fordulat / ha, ugyanaz, mint a leforgatás „fordulat 1/10 ha”).
- A kg/ha-t adagolónként a **skála** (térfogat-állítás) adja, mint talajhajtásnál;
  a kuplungok adagolónként kapcsolnak (vetés BE/KI, szakaszolás, kiemelés).
- Leforgatás: a motor forgat, a kiválasztott adagoló kuplungja zár, a másiké nyit.
- Előírt +/− itt nem a motort, hanem a javasolt skálaértéket mutatja (mint talajhajtásnál).

### 1.2 PWM-vezérlő típusok
| Mód | Kimenetek motoronként | Előre | Hátra (visszafúvás / kiszabadítás) |
|---|---|---|---|
| PWM + DIR | 1 PWM + 1 digitális | DIR=0, PWM=x | DIR=1, PWM=x |
| PWM + PWM (IBT-2) | 2 PWM (EN-ek fixen 5 V-on) | RPWM=x, LPWM=0 | RPWM=0, LPWM=x |
- Irányváltásnál holtidő (pl. 100 ms 0%), hogy a híd ne kapjon keresztáramot.
- **Lábkiosztás (a panel tüskesorának felirata alapján):** kivezetett GPIO-k: **0, 1, 2, 3, 21, 45, 47, 48**,
  valamint Tx/Rx (GPIO43/44, UART0) és DP/DN (USB). Most foglalt: 47/48 motor-PWM, 1/2 HX711, 3 DS18B20.
  - **Szabadon használható: GPIO21.**
  - GPIO45: indításkori „strapping” láb (flash-feszültség) – kimenetnek jó, de indításkor nem húzhatja fel semmi.
  - GPIO0: BOOT láb – ha induláskor alacsonyra húzza a meghajtó, a panel letöltő módba indul → kerülendő.
  - GPIO43/44: a soros konzol – indításkor a ROM üzeneteket ír rájuk (motor rándulhat) → kerülendő.
  - Közvetlenül tehát 4 PWM: 47, 48, 21, 45 – megoldható, de a 45-ös láb miatt óvatosan.
- **Lábtakarékos megoldás (javasolt):** a motoronkénti PWM marad a 47/48-on, az **irányjel egy olcsó I2C bővítőről**
  (PCF8574 modul a meglévő I2C buszon, 0x21–0x27) jön – az irány ritkán vált, ehhez elég az I2C.
  - Cytron (PWM+DIR): PWM → PWM, DIR ← bővítő.
  - IBT-2: R_EN + L_EN összekötve ← PWM (GPIO), RPWM / LPWM ← bővítő (egyik magas, másik alacsony).
  Így 2 motorhoz sem kell újabb GPIO, és a vissza irány is megvan.
- **PWM közvetlenül a Waveshare GPIO-ról** (3,3 V-os logika). Cytron: 3,3 V-ot elfogad. IBT-2: a modul 74HC244
  puffere 5 V-ról megy, a 3,3 V a határon van (sokaknál működik) → biztonságosabb egy szintillesztő
  (pl. 74HCT245 vagy kétirányú szintillesztő modul) a PWM és az irányjelek elé.
- Beállítás: PWM-frekvencia (pl. 1–20 kHz), irány megfordítása.

### 1.4 Terhelés becslése (első körben PWM-alapú, árammérés nélkül)
- Önhangolás egyszer (motor- és hajtásjellemzők), magfajtától függetlenül.
- Terhelési mutató: az aktuális PWM-hez tartozó várt fordulat (önhangolási görbe) és a mért fordulat viszonya.
- Alapszint növényenként automatikusan tanulva (egyenletes vetés közben), a kiválasztott növényhez mentve;
  100 % = az adott magnál szokásos terhelés.
- Beszorulás-felismerés változásalapú (hirtelen eltérés az utolsó ~30 s átlagától, vagy nagy PWM mellett nincs impulzus).
- Később (nem most): valódi árammérés – IBT-2 R_IS/L_IS → ADS1115, INA226, vagy Keya CAN; tápfeszültség-mérés.

### 1.5 Beszorulás-kezelés (visszafordítás)
- Felismerés: nagy PWM mellett a fordulat leesik / nincs impulzus (pl. 0,5 s), opcionálisan motoráram
  (ADS1115 A1–A2) vagy Keya motoráram.
- Teendő: motor áll → **visszafordítás néhány fokkal** (pl. 1/16 fordulat, a jeladó impulzusaiból számolva –
  a vezérlő tudja az irányt, mert ő adja) → újra előre. Legfeljebb 3 próbálkozás; ha nem old, riasztás
  „Adagoló beszorult”. Minden próbálkozás naplózva.
- **Visszafordítás kikapcsolva:** beszorulásnál a motor **azonnal megáll** és **riaszt** („Adagoló beszorult”),
  újraindítás a kezelő nyugtázása / vetés BE után.
- Beállítás (Hajtás csoport): **Visszafordítás be/ki** (választható tulajdonság), **mértéke** (fok vagy
  fordulat-tört, pl. 10–90°), próbálkozások száma, felismerési idő.
- **Tesztfelület** (VT + web, álló gép, vetés KI): adagoló kiválasztása, „Visszafordítás próba” gomb –
  a vezérlő lefuttat egy teljes ciklust (előre forgás → megállás → visszafordítás a beállított mértékkel → előre),
  és kiírja: mért visszafordulás (impulzus / fok), időtartam, PWM, motoráram (ha van mérés). Napló a webről.
- Keya motornál: pozíció-paranccsal (célpozíció −x fordulat) és a motoráram alapján még egyszerűbb.
- Egy motor + két adagoló (kuplungos) esetben a visszafordítás mindkét adagolót érinti.

### 1.3 Keya CAN-motor
Lásd 4. pont. Fordulatszám-üzemmód: a vezérlő célfordulatot küld, a motor maga szabályoz,
ezért a PWM-szabályzó (előrecsatolás, PI, önhangolás) itt nem kell.

---

## 2. Menü átszervezése (a permetező mintájára)

A permetező két szintű menüt használ: **Beállítások → csoportok (softkey-ek) → csoporton belüli oldalak**
(`VT_GROUPS` a params_def.py-ban). Javasolt csoportok a vetőgéphez:

| Csoport | Oldalak |
|---|---|
| Adagolás | növény / előírt, feltöltés (csőfeltöltés), tartálytöltet |
| Gép | gép, gépprofil (táblázat, fordulat/ha), geometria |
| Hajtás | hajtás típusa, motorok, PWM-vezérlő / Keya, szabályzó, **önhangolás** |
| Turbina, hűtés | turbina, olajhőmérséklet, visszafúvás |
| Mérés, kalibrálás | sebesség, leforgatás, mérleg, nyomásmérés |
| Kapcsolók, kiegészítők | kapcsolók (NO/NC), kiegészítők, lámpák |
| Szakaszolás, TC | TC, AgOpenGPS |
| Szerviz | diagnosztika, szimuláció/teszt, I/O, Keya motor beállítás |

- A web és a Windows-program ugyanezt a csoportosítást kövesse.
- Csak a releváns oldalak látszanak (pl. Keya oldal csak Keya hajtásnál, PWM-oldal csak PWM-nél).

---

## 3. Motor önhangolás – leírás a kezelőnek

Indítás előtt a VT-n (és a weben) **tájékoztató lap**, „Indítás” csak ezután:

> **Mi fog történni?** A vezérlő kb. 30 másodpercig forgatja a kiválasztott adagolót:
> lassan felfuttatja, amíg elindul; visszaveszi, amíg megáll; majd 25 / 50 / 75 / 100 % PWM-en méri a fordulatot.
> Ebből javasolja: fordulat 100 % PWM-nél, PWM minimum, szabályzási állandó.
>
> **Mit kell tenni?** Álló gép, vetés KI · a tartályban legyen vetőmag (terhelés alatt pontos) ·
> a kiömlő alá tégy edényt (kb. … kg mag jöhet ki) · ne nyúlj a hajtáshoz.
> Megszakítás: Stop gomb, vetés BE vagy a lap elhagyása.
>
> **Utána:** a lapon a javaslat és a mostani érték egymás mellett; Mentés → beállításokba kerül, kézzel állítható.

- Becsült kiömlő mennyiség: kg/fordulat × várható fordulatszám → kiírható a tájékoztatóban.
- Csak PWM-es hajtásnál (Keya motornál nem kell).

---

## 4. Keya motor (KY40ACS01005-31-PX40-49) – protokoll összefoglaló

**Címke:** motor 3110 1/min, hajtómű 1:49 → kimenet ≈ 63,5 1/min. Névleges: 12 V, 4 Nm, 63 1/min, IP66.
Táp 10–36 V (túlfeszültség-védelem gyárilag 18 V!), 6 A folyamatos, 12 A csúcs. Csatlakozó DT04-4P:
1 = DC+, 2 = DC−, 3 = CAN-H, 4 = CAN-L.

**CAN:** 11 bites standard keret, gyárilag 500 kbit/s, node ID 1. **ID = (node << 6) | parancs.**
Adatok little-endian int32 / uint32 / float32.

| Parancs | Index | ID (node 1) | Küldött adat | Válasz |
|---|---|---|---|---|
| Letiltás | 0x00 | 0x040 | – | int32 0 = OK |
| Engedélyezés | 0x01 | 0x041 | – | int32 0 = OK |
| Célnyomaték | 0x02 | 0x042 | float Nm | nincs |
| **Célfordulat** | 0x03 | 0x043 | float fordulat/s | nincs |
| Célpozíció | 0x04 | 0x044 | float fordulat | nincs |
| Szinkron | 0x05 | 0x045 | – | nincs |
| Nullpont | 0x0C | 0x04C | – | int32 |
| Hibatörlés | 0x0D | 0x04D | – | int32 |
| Állapot lekérése | 0x0E | 0x04E | – | 8 bájt: állapot + hiba |
| Állapotjelentés (magától) | 0x0F | 0x04F | – | 8 bájt: állapot + hiba |
| Nyomaték | 0x10 | 0x050 | – | float Nm |
| **Fordulat** | 0x11 | 0x051 | – | float fordulat/s |
| **Pozíció** | 0x12 | 0x052 | – | float fordulat |
| Motoráram | 0x13 | 0x053 | – | float A |
| Tápfeszültség | 0x14 | 0x054 | – | float V |
| Buszáram | 0x15 | 0x055 | – | float A |
| Teljesítmény | 0x16 | 0x056 | – | float W |
| Paraméter írása | 0x17 | 0x057 | int32 sorszám + int32/float érték | visszaküldi |
| Mentés | 0x19 | 0x059 | – | int32 0 = OK (utána táp le-fel) |
| Szívverés | 0x3F | 0x07F | – | – |

**Állapotbitek:** 0 engedélyezve, 1 cél elérve, 2 áramkorlát.
**Hibabitek:** 0 önteszthiba*, 1 jeladó kiesett*, 17 túlfeszültség, 18 alacsony feszültség, 19 túláram
(*csak újraindítással törölhető).

**Fontos paraméterek (sorszám):** 1 irány megfordítása · 7 áramkorlát · 8 fordulatkorlát [ford/s] ·
11 üzemmód (fordulatszám-felfutás kell) · 13/14 fordulatszabályzó P/I · 18 fordulat-elérési ablak ·
21 fordulat-felfutás [(ford/s)/s] · 26 alacsony fesz. küszöb · 27 túlfesz. küszöb · 28 túláram küszöb ·
29 max. buszáram · **31 node ID** · **32 baud (0=250k, 1=500k, 2=800k, 3=1M)** · 33 szívverés-fogadás időkorlát [ms] ·
34 szívverés-küldés időköz [ms].

**Nyitott kérdések (próbával tisztázandó, `tools/keya/keya_test.py`):**
- A fordulat/s a **motortengelyre** (max ≈ 51,8 ford/s) vagy a **kimenetre** (max ≈ 1,06 ford/s) vonatkozik? → `test 1 10`
- A paraméter-írás float formátuma (a leírás csak egész példát ad).
- Paraméter visszaolvasás (nincs leírva).
- A tényleges fordulat csak lekérdezésre jön (kb. 50–100 ms-onként kérdezni).

**Integráció terve:**
- Közös ISOBUS-busz 250k-n (a motorokat 250k-ra állítani), a 11 bites kereteket a CAN-meghajtó szintjén
  leválasztani az AgIsoStack elől; később opcionálisan külön CAN (MCP2515) – a meghajtó buszfüggetlen.
- Indításkor a vezérlő beírja: üzemmód, fordulatkorlát, felfutás, szívverés-időkorlát (pl. 500 ms), és küldi a szívverést.
- Riasztások: a hibabitekből; diagnosztika: áram, feszültség, teljesítmény.
- Leforgatás a pozícióból (tört fordulat is).
- Szerviz → „Keya motor beállítás” mód (webről / Windowsból indítva): ideiglenesen 500k, node ID + 250k + mentés,
  időkorlát után automatikus visszaállás 250k-ra. Közben az ISOBUS nem érhető el (géppel lekapcsolva a traktorról).

---

## 5. Korábban feljegyzett, ugyanebbe a körbe tartozó pontok

- **Gépprofil:** feltölthető saját növény/skála táblázat (JSON, webes/Windows feltöltés, CSV-ből Excelhez),
  beépített Accord profil marad gyárinak; VT-n futás közben átírható növénynevek.
- **Leforgatás fordulatszáma állítható** („fordulat 1/10 ha-ra”, alap 85; 1/20, 1/50 ebből; szabad cél), a gépprofil része.
- **Cserebetétes gépek** előkészítése: betétek listája (cm³/ford), betétválasztás adagolónként,
  kg/ford memória betétenként, betét-tanácsadó (motor fordulattartomány alapján).
- **Tartálytöltet:** tankolás megadása és hozzátankolás, a főlapon a tartály ikonokban; két tartálynál
  közös (egy mennyiség) vagy külön; fogyás az elszámolt kg-ból (talaj- és elektromos hajtásnál is);
  csak tájékoztatás, riasztás nincs.
- (Opcionális) **Pool-méret csökkentése:** geometria rajzok (~24 KB), több méretben tárolt ikonok (~8 KB),
  „1/2” jelvényes softkey-ek (~5 KB).
