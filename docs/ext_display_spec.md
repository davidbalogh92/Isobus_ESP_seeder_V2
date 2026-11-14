# Külső kijelző – specifikáció (vetőgép, szóró, permetező)

Nov 10, 2026 · @Dávid

## Cél és hatókör

A gépre szerelt 5"-os kijelző a gép mellől teszi kezelhetővé azt, amit nem a fülkéből csinálsz. Egy firmware fut minden gépen (vetőgép V2, szóró, permetező): a vezérlő bekapcsoláskor megmondja, milyen gép és mit tud, a kijelző ebből rakja össze a képernyőit. Kábellel, CAN-en kapcsolódik, és nem helyettesíti a traktor VT-jét.

**Amit tud:**

- Gépspecifikus funkciók: vetőgépnél a leforgatás (indítás, követés, tömeg beírása, elfogadás); szórónál és permetezőnél később.
- Tartálytartalom kijelzése, töltés, hozzátankolás és nullázás megadása, közös vagy külön tartállyal.
- Munkalámpák (legfeljebb 2) ki- és bekapcsolása.
- A gép WiFi szerviz módjának be- és kikapcsolása.
- Kapcsolat- és gépállapot (sebesség, munkakapcsoló, WiFi) kijelzése.

**Amit szándékosan nem tud:** vetési normát, szakaszvezérlést, gépprofilt, riasztás-nyugtázást, beállításokat. Ezek a VT-n és a webes felületen maradnak.

## Hardver

A Waveshare ESP32-S3-Touch-LCD-5B (SKU 28151, 1024 × 600) mindent tud, ami kell: van rajta CAN-adó-vevő, közvetlenül megy 7–36 V-ról, és van két leválasztott bemenete a fizikai gomboknak. Adatok: [Waveshare dokumentáció](https://docs.waveshare.com/ESP32-S3-Touch-LCD-5).

| Tulajdonság | Érték | Mit jelent nekünk |
| --- | --- | --- |
| CAN | TJA1051, TX = GPIO15, RX = GPIO16 | Saját lábak, nem osztozik az USB-vel |
| CAN lezárás | 120 Ω, kapcsolóval, gyárilag KI | Leágazásként KI marad |
| Táp | DC 7–36 V vagy USB-C 5 V | 12 V-ról közvetlenül, külső védelemmel |
| Fogyasztás | kb. 5 V / 450 mA | 12 V-on kb. 0,2–0,3 A |
| Fényerő | 550 cd/m² | Jó, de ellenző kell |
| Üzemi hőmérséklet | 0–65 °C | Fagyban nem garantált |
| Leválasztott bemenet | 2 db (DI0, DI1), 5–36 V, CH422G-n át | STOP és START/OK gomb |
| Leválasztott kimenet | 2 db (DO0, DO1), max. 450 mA | Berregő, visszajelző LED |
| Akku csatlakozó | MX1.25, 3,7 V, kapcsolóval | Nem használjuk, kapcsoló KI |

**Védelem és szerelés:**

- **Táp:** biztosíték (1 A), fordított polaritás elleni dióda vagy P-FET, TVS dióda a túlfeszültség (load dump) ellen. A panel bírja a 36 V-ot, de a traktoros tüskéket nem.
- **Ház:** IP65/67, polikarbonát előlap, páraszellőző (Gore-vent), UV-álló anyag. A WiFi-antenna környékére ne kerüljön fém.
- **Gombok:** 2 db IP67 nyomógomb a DI0/DI1-re (STOP piros, START/OK zöld). A STOP mindig fizikai gomb.
- **Berregő:** a DO0-ra, a leforgatás végének jelzésére. Kézi tekerésnél (talajhajtás) ez sokat segít.
- **Rögzítés:** gumibakos, Deutsch DT csatlakozóval.
- **Akku:** nincs, a panel akkukapcsolója KI állásban. Az RTC-gombelem (CR927) maradhat, de nem szükséges.

## Bekötés és CAN busz

A kijelző egy 4-eres kábellel csatlakozik a gép belső ISOBUS kábelére, a vezérlő közelében. Ugyanarról a kapcsolt tápról megy, mint a V2 vezérlő, így a gyújtással együtt kapcsol ki, és holt szezonban semmit sem merít.

| Ér | Jel | Panel kapocs |
| --- | --- | --- |
| 1 | +12 V (kapcsolt, a vezérlővel közös) | DC 7–36 V + |
| 2 | GND | DC 7–36 V − |
| 3 | CAN-H | CAN H |
| 4 | CAN-L | CAN L |

- **Busz:** ISOBUS, 250 kbit/s, 29 bites azonosítók.
- **Leágazás:** fél méter alatt, sodrott érpárral. A panel lezáró kapcsolója KI.
- **Ha a kijelző a busz végére kerül:** akkor ott BE a lezárás, és a régi végponton KI. A busz mindkét végén pontosan egy-egy 120 Ω legyen (mérve kb. 60 Ω a H és L között, áramtalanítva).
- **Gombok:** a DI közös pontja a GND-re kötve (PNP, magas szintre aktív), a gombok +12 V-ot adnak a DI0/DI1-re. A közös pont a sorkapocs külön kapcsa (a panel GND-jével nincs összekötve), bekötése nélkül a bemenetek nem működnek.
- **Terhelés:** a két irányú 200 ms-os szívverés és az állapotüzenetek együtt is jóval 1% alatti busztelítettséget jelentenek.

## Szerepek

A V2 vezérlő az egyetlen, aki dönt és kimenetet kapcsol. A VT és a külső kijelző egyenrangú kérő: ugyanazt az állapotot látják, és bármelyik leállíthat egy futó leforgatást.

&#91;embedded content: szerepek · VT, kijelző, V2 vezérlő, kimenetek\]

Így a két kezelőfelület soha nem kerül ellentmondásba, mert a gép állapota csak a vezérlőben él.

## Üzenetkészlet

Minden üzenet Proprietary A (PGN 61184, 0xEF00), célzottan a másik fél címére, 8 bájtos. Az első bájt az üzenet azonosítója, a többi little-endian. A közös definíció egy fejlécfájlba kerül (`ext_display_protocol.h`), amit mindkét firmware ugyanúgy használ.

**Címfelvétel:** a kijelző saját NAME-mel végez ISOBUS address claimet (önkonfiguráló cím). A V2 vezérlőt NAME-szűrővel találja meg (AgIsoStack partner control function), így a címek változása nem okoz gondot.

**ID-tartományok:** a mag minden gépen ugyanaz: parancsok 0x01–0x0F és 0x20–0x4F, állapotok 0x80–0x8F, 0xA0–0xCF és 0xF0. A gépspecifikus parancsok 0x10–0x1F, állapotok 0x90–0x9F; a jelentésük a géptípustól függ (vetőgépnél leforgatás). A kijelző gépspecifikus üzenetet csak akkor küld, ha a képességek üzenet szerint a gép tudja.

**Képességek (0x80, vezérlő → kijelző):** bekapcsoláskor, a kijelző megjelenésekor és 5 s-onként.

| Bájt | Tartalom |
| --- | --- |
| 1 | Géptípus: 1 = vetőgép, 2 = szóró, 3 = permetező |
| 2 | Protokoll verzió |
| 3 | Tartályok száma (0–2) |
| 4 | bit0: tartály mód (0 = közös, 1 = külön) · bit4–5: adagolók száma (0–2) · a többi bit tartalék (0) |
| 5 | Munkalámpák száma (0–2) |
| 6 | Funkció bitek: bit0 tartály, bit1 lámpák, bit2 WiFi szerviz mód, bit3 gépspecifikus kalibrálás, bit4 turbina fordulatszám, bit5 leforgatási mérleg |
| 7 | Vezérlő firmware fő verzió |

**Kijelző → vezérlő (parancsok):**

| ID | Név | Bájtok 1–7 |
| --- | --- | --- |
| 0x01 | Szívverés (200 ms) | 1: számláló · 2: jelzők (bit0 = STOP gomb nyomva, bit1 = START gomb nyomva) · 3: aktív képernyő (0 = főképernyő, 1 = leforgatás, 2 = tartály, 3 = beállítás) |
| 0x10 | Leforgatás parancs (vetőgép) | 1: művelet (lásd lent) · 2: paraméter · 3–6: érték (uint32) |
| 0x20 | Tartály művelet | 1: tartály (0 = közös, 1, 2) · 2: művelet (1 = beállít, 2 = hozzátankol, 3 = nulláz) · 3–6: mennyiség 0,1 kg-ban (uint32) |
| 0x30 | Munkalámpa | 1: lámpa (1, 2) · 2: állapot (0 = ki, 1 = be, 2 = vált) |
| 0x40 | WiFi szerviz mód | 1: 0 = ki, 1 = be |

**Vezérlő → kijelző (állapotok):**

| ID | Név | Bájtok 1–7 |
| --- | --- | --- |
| 0x81 | Gépállapot / szívverés (200 ms) | 1: számláló · 2: jelzők (bit0 munkakapcsoló, bit1 mozog, bit2 adagolás fut, bit3 WiFi be, bit4 lámpa 1, bit5 lámpa 2, bit6 turbina riasztás) · 3–4: sebesség 0,01 km/h · 5–6: turbina fordulatszám 1/perc (0xFFFF = nincs adat) · 7: tartalék |
| 0x90 | Leforgatás állapot (vetőgép; 200 ms, amíg a kijelző aktív képernyője a leforgatás) | 1: jelzők (bit0 leforgatás folyamatban, bit1 nullpont megvan, bit2 cél elérve, bit3 motor forog, bit4 motoros hajtás, bit5 a fél fordulatot kézzel kell rátenni) · 2: adagoló (alsó 4 bit: 1, 2) és fordulatszám-választás (felső 4 bit: 0 = 85, 1 = 42,5, 2 = 17) · 3–4: megszámolt fordulat 0,01-ben · 5–6: cél fordulat 0,1-ben · 7: részmérések száma (0–10) |
| 0x91 | Leforgatás eredmény (vetőgép; a 0x90-nel együtt) | 1: adagoló · 2–4: a részmérések összege grammban (24 bit) · 5–7: új érték g/fordulat × 1000 (24 bit, 0 = még nincs) |
| 0x92 | Mérleg állapot (vetőgép; 200 ms, amíg a kijelző aktív képernyője a leforgatás) | 1: jelzők (bit0 érvényes, bit1 stabil) · 2–5: tömeg 0,1 g-ban (int32, előjeles) |
| 0x93 | Részmérés tétel (vetőgép; a 0x90-nel együtt, ciklusonként egy tétel körbejárva) | 1: sorszám (0–9) · 2: részmérések száma · 3–5: a tétel tömege grammban (24 bit) |
| 0x94 | Mentett kalibráció (vetőgép; 1 s, amíg a kijelző aktív képernyője a leforgatás) | 1: adagoló · 2–4: a jelenleg mentett g/fordulat × 1000 (24 bit, 0 = nincs) |
| 0xA0 | Tartály állapot (1 s, tartályonként) | 1: tartály · 2–5: hátralévő 0,1 kg-ban (uint32) · 6: telítettség % (0xFF = ismeretlen) |
| 0xC0 | WiFi állapot (1 s, ha be van) | 1: állapot (0 = ki, 1 = be, 2 = kliens csatlakozva, 3 = OTA fut) · 2–3: hátralévő perc |
| 0xF0 | Nyugta | 1: a parancs ID-ja · 2: eredmény (0 = OK, 1 = tiltva, 2 = hibás adat, 3 = foglalt) · 3: tiltás oka |

**Nyugta és tartalék bájtok:** a vezérlő minden parancsra nyugtát (0xF0) küld, kivéve a szívverést (0x01); arra a 200 ms-os gépállapot (0x81) a válasz. A nem használt bájtok értéke 0xFF, a fogadó nem nézi őket. A protokoll verzió jelenleg 1.

**Leforgatás műveletek (0x10):** 1 új leforgatás (számláló nullázása, nullpont keresése) · 2 adagoló választása (paraméter: 1, 2) · 3 fordulatszám-választás (paraméter: 0 = 85, 1 = 42,5, 2 = 17) · 4 részmérés hozzáadása (érték: gramm) · 5 a mérleg értékének átvétele részmérésként · 6 részmérés törlése (paraméter: sorszám 0–9, 0xFF = mind) · 7 mentés · 8 másolás a másik adagolóra · 9 a mérleg tárázása.

**Motoros forgatás:** nincs rá külön parancs. Amíg a kijelző aktív képernyője a leforgatás, a szívverés START bitje a „tartás" (a fizikai START gomb vagy a képernyő Forgat gombja nyomva). A vezérlő addig forgat, amíg a bit érkezik; a bit megszűnésekor, a STOP bitre vagy 600 ms kimaradás után megáll.

**Tiltási okok:** 1 a gép mozog · 2 munkakapcsoló aktív · 3 adagolás fut · 4 kijelző-kapcsolat elveszett · 5 már fut · 6 hibás paraméter · 7 nincs ilyen tartály vagy lámpa · 8 a funkció ezen a gépen nincs · 9 nincs érvényes mérlegadat · 10 a mérleg üres · 11 nincs lemért tömeg · 12 a részmérések listája megtelt · 13 motoros forgatás csak elektromos hajtással · 14 önhangolás fut · 15 a célfordulat megvan.

## Funkciók

### Leforgatás (csak vetőgép)

A leforgatás teljes egészében kezelhető a kijelzőről, kézi (talajhajtás) és motoros hajtásnál is. A vezérlő egyetlen leforgatás-állapota vezeti, így a kijelző és a VT ugyanazt látja, és a szabályokat (mikor forgathat a motor, mikor számol a jeladó) kizárólag a vezérlő dönti el. A kijelző a V2 meglévő leforgatását kezeli: három fordulatszám közül lehet választani (85, 42,5, 17), és legfeljebb 10 részmérés adható össze.

1. **Új leforgatás:** kiválasztod az adagolót és a fordulatszámot, majd nullázod a számlálót.
2. **Nullpont (kevés impulzusú jeladónál):** addig tekersz, amíg megjön az első impulzus; a számlálás onnan indul. A kijelző jelzi, hogy a nullpont megvan-e.
3. **Forgatás:**
   - *Kézi hajtás:* te tekersz, a vezérlő a tengelyjeladóból számol. A kijelző fordulatonként rövid, a célnál hosszú hangjelzést ad a berregőn. Ha a cél fél fordulatra végződik és a jeladó ezt nem tudja mérni, a kijelző kiírja, hogy a felet kézzel kell rátenni.
   - *Motoros hajtás:* a Forgat gombot (vagy a fizikai START-ot) nyomva tartod, a motor addig forog. A célnál megáll (ha az automatikus megállás be van állítva). Csak álló gépnél és kikapcsolt vetésnél indul.
4. **Mérés:** a lemért tömeget számbillentyűzeten grammban írod be, vagy ha a gépen van leforgatási mérleg, annak értékét egy gombbal átveszed (előtte tárázható). Több vödörnyi mérés összeadható; a tételek egyenként vagy együtt törölhetők.
5. **Eredmény:** a vezérlő kiszámolja az új g/fordulat értéket; a kijelző megmutatja a jelenleg mentett értéket és az eltérést.
6. **Mentés:** az új érték a kiválasztott adagolóhoz mentődik; a másik adagolóra átmásolható.

A STOP gomb a motoros forgatást bármikor megállítja. A termény, a táblázati beállítás és a hektárra vetített ellenőrzés (leforgatási segéd) a VT-n marad.

### Tartály és hozzátankolás

Tájékoztató jellegű, riasztás nélkül, ahogy a V2 tervében szerepel. A fogyást a vezérlő számolja: villanymotornál a motorfordulatból, talajhajtásnál az enkóderből, mindkettőt a leforgatott g/fordulat értékkel.

- **Beállít:** a tartályban lévő teljes mennyiség (pl. első töltés).
- **Hozzátankol:** a meglévőhöz adja hozzá.
- **Nulláz:** kiürítés után.
- **Két tartálynál:** a gépprofil dönti el, hogy közös mennyiség (egy tér, együtt dolgoznak) vagy külön mennyiség tartályonként.

### Munkalámpák

Legfeljebb 2 lámpa, egyenként kapcsolható. A kimeneteket a vezérlő kapcsolja, a gépprofil rendeli hozzá a lámpákhoz. Nincs tiltás, munka közben is kapcsolhatók. Az állapot a VT-n és a kijelzőn is látszik, bármelyikről kapcsolható.

## WiFi szerviz mód

A vezérlő WiFi-je és webszervere alapból ki van kapcsolva, és csak igény szerint indul. Ez minden gépvezérlőre (vetőgép, szóró, permetező) egyformán vonatkozik.

- **Bekapcsolás:** a VT menüjéből, a külső kijelzőről (0x40), vagy vészmegoldásként a vezérlő egy bemenetének lenyomva tartásával bekapcsoláskor.
- **Kikapcsolás:** kézzel, vagy magától a beállított tétlenségi idő után (alapból 20 perc, a weben 5–60 perc között állítható).
- **Nem kapcsol le**, amíg kliens csatlakozik a weboldalra vagy OTA frissítés fut.
- **Tiltva**, ha a munkakapcsoló aktív. Ha munka közben aktiválódik a munkakapcsoló, a WiFi azonnal kikapcsol (OTA alatt nem, de OTA közben a munka nem indítható).
- **Állapot:** a VT-n és a kijelzőn WiFi ikon és hátralévő idő (0xC0).

## Biztonság, tiltások, kijelző kiesése

A leforgatás csak álló gépnél, inaktív munkakapcsolónál és álló adagolásnál indulhat, és bármelyik STOP azonnal megállítja. A STOP-ot a vezérlő mindig végrehajtja, bárhonnan jön és bármilyen állapotban.

| Helyzet | Vezérlő reakciója | Kijelző reakciója |
| --- | --- | --- |
| Sebesség > 0,5 km/h leforgatás közben | Motor stop; a forgatás kérésére „tiltva", ok: 1 | Kiírja a tiltás okát |
| Munkakapcsoló aktiválódik leforgatás közben | Motor stop, ok: 2 | Hibaüzenet |
| Kijelző-szívverés (a „tartás" bit) kimarad 600 ms-ig motoros forgatás közben | Motor stop | — |
| Vezérlő-szívverés kimarad 1 s-ig | — | „Nincs kapcsolat" felirat, minden gomb tiltva |
| Fizikai STOP | Motor stop | A STOP a szívverés jelzőbitjében is megy, nem csak parancsként |
| VT és kijelző egyszerre kezeli a leforgatást | Egyetlen közös állapot van: az utolsó parancs érvényes, mindkét felület ugyanazt látja | Az állapotból frissül |
| Ismeretlen protokoll verzió | Parancsok elutasítva | „Firmware frissítés szükséges" |

Két ponton szándékos a redundancia: a STOP a szívverésben is benne van, így egy elveszett parancsüzenet sem okoz gondot; és ha a VT indította a leforgatást, a kijelző kiesése nem állítja le, mert a VT-ről akkor is meg lehet állítani.

## Képernyők

Négy képernyő elég, szürke háttérrel, a V2 VT stílusához igazítva, nagy (kesztyűs kézzel is nyomható, legalább 100 × 100 px-es, azaz kb. 11 × 11 mm-es) gombokkal.

| Képernyő | Tartalom | Gombok |
| --- | --- | --- |
| Főképernyő | Tartályok töltöttséggel és kg-mal, turbina fordulatszám, lámpa 1–2 állapot, WiFi állapot, kapcsolatjelző. A sebesség, a munkakapcsoló és az adagolás állapota itt nem jelenik meg (a VT-n látszik). | Leforgatás · Tartály · Lámpa 1 · Lámpa 2 · WiFi |
| Leforgatás | Adagoló és fordulatszám választás, nagy megszámolt/cél számláló a nullpont jelzésével, részmérések listája és összege, mérleg értéke, új és mentett g/fordulat, eltérés % | Új leforgatás · Forgat (motoros hajtásnál, nyomva tartva) · Tömeg beírása · Mérleg átvétele · Tára · Törlés · Mentés · Másolás |
| Tartály | Tartályonként (vagy közösen) a jelenlegi mennyiség, számbillentyűzet | Beállít · Hozzátankol · Nulláz |
| Beállítás | Fényerő, firmware verzió, CAN diagnosztika (cím, partner látszik-e, hibaszámlálók) | Vissza |

A főképernyő gombjai a képességek üzenetből jönnek: amit a gép nem tud, az nem jelenik meg. A leforgatás gomb csak vetőgépnél látszik.

A kijelző firmware-e LVGL-lel készül; az 1024 × 600-as RGB panelhez a PSRAM-ban tartott kettős puffer elég.

## Megvalósítás Claude Code-ban

Két repó, két Claude Code munkamenet. A kijelző repó a protokoll fejléc egyetlen hiteles helye; a gépvezérlők a saját repójukban, a saját munkamenetükben veszik át. Ezt a dokumentumot Markdownba exportálva mindkét repóba tedd be: `docs/ext_display_spec.md`.

1. **Kijelző repó** (`Isobus_ExtDisplay_esp32`): protokoll fejléc unit tesztekkel, CAN és address claim, képességek fogadása, főképernyő, kapcsolatjelzés, gombok, berregő, tartály és beállítás képernyő, végül a leforgatás képernyő.
2. **V2 vezérlő, 1. kör:** a fejléc átvétele, képességek küldése, szívverés, nyugták, munkalámpák, WiFi szerviz mód.
3. **V2 vezérlő, 2. kör** (a gépprofil-átdolgozással): leforgatás állapotgép a VT-vel közösen, állítható fordulatszám, tartálykövetés.
4. **Szóró, majd permetező:** ugyanaz a mag modul; gépspecifikus üzenetek később.
5. **Asztali próba:** a V2 panel és a kijelző egy buszon, mindkét végén lezárással, CANlink hídon és Windows-on figyelve; utána a gépen.

A Claude Code-nak szóló két kérés (kijelző repó, V2 repó) a beszélgetésben van.

## Nyitott kérdések

- [ ] A V2 Arduino vagy ESP-IDF alapon fut? A kijelző firmware ugyanazt kövesse.
- [ ] Hány munkalámpa és melyik kimenet a vetőgépen?
- [ ] Elég a 0,1 g felbontás a leforgatásnál?
- [ ] Hová kerül a kijelző a gépen (napfény, sár, elérhetőség)?
- [ ] Kell-e fagy alatti használat? A panel 0 °C-ig van megadva.
- [ ] Szórónál és permetezőnél mi legyen a gépspecifikus funkció (pl. kalibrálás)?

## Források

- [ESP32-S3-Touch-LCD-5 – Waveshare dokumentáció](https://docs.waveshare.com/ESP32-S3-Touch-LCD-5)
