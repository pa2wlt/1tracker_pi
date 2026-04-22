# 1tracker_pi — Installatie-instructies (bèta-testers)

Bedankt voor het testen van **1tracker_pi**, een OpenCPN-plugin die je bootpositie
(en optioneel winddata) naar HTTP-endpoints verstuurt — zoals NoForeignLand of een
eigen server.

---

## Vereisten

- OpenCPN **5.6 of nieuwer**
- Besturingssysteem: Linux, macOS of Windows
- Internetverbinding tijdens installatie

---

## Stap 1 — Alpha-catalogus toevoegen

Omdat de plugin nog niet officieel is opgenomen in de OpenCPN-catalogus, moet je
eenmalig een extra catalogus-URL toevoegen.

1. Open OpenCPN
2. Ga naar **Opties** → tabblad **Plugins**
3. Klik op de knop **Catalogus bijwerken** (of *Update catalog*)
4. Zoek de instelling voor een **aangepaste catalogus-URL** (*Advanced* of *Custom catalog URL*)
5. Voeg de volgende URL toe:

```
https://dl.cloudsmith.io/public/pa2wlt/1tracker-alpha/raw/files/ocpn-plugins.xml
```

6. Klik op **OK** en daarna opnieuw op **Catalogus bijwerken**

---

## Stap 2 — Plugin installeren

1. Zoek in de pluginlijst naar **1tracker**
2. Klik op **Installeren**
3. Herstart OpenCPN wanneer hierom gevraagd wordt

---

## Stap 3 — Plugin activeren

1. Ga opnieuw naar **Opties** → **Plugins**
2. Zoek **1tracker** in de lijst en klik op **Inschakelen** (*Enable*)
3. Klik **OK** en herstart OpenCPN

---

## Stap 4 — Configureren

Na het herstarten verschijnt een nieuw icoon in de werkbalk.

1. Klik op het **1tracker-icoon** in de werkbalk om het configuratievenster te openen
2. Klik op **Endpoint toevoegen** en vul in:

| Veld | Omschrijving |
|---|---|
| **Naam** | Vrij te kiezen, bv. `mijn-nfl-tracker` |
| **Type** | `JSON` (generiek HTTP) of `NoForeignLand` |
| **URL** | De endpoint-URL van jouw tracker-dienst |
| **API-sleutel / Header** | Indien vereist door de dienst |
| **Interval** | Hoe vaak de positie wordt verstuurd (in minuten) |

3. Zet de schakelaar op **Aan**
4. Klik **Opslaan**

### Werkbalk-icoon

| Kleur | Betekenis |
|---|---|
| 🟢 Groen | Laatste verzending geslaagd |
| 🔴 Rood | Fout bij laatste verzending |
| ⚪ Grijs | Inactief of nog geen verzending |

---

## Problemen of feedback?

Meld bugs en opmerkingen via GitHub:
[github.com/pa2wlt/1tracker_pi/issues](https://github.com/pa2wlt/1tracker_pi/issues)

Of neem direct contact op met de ontwikkelaar.

---

*Versie 0.1.0 — april 2026*
