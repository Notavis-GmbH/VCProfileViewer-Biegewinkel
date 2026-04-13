# VC 3D Profile Viewer – Biegewinkel

**Profil-Viewer für die Vision Components VC nano 3D Z** mit ROI-Einstellung, Biegewinkelberechnung (OLS / RANSAC / Hough / Auto), Messwert-Log und JSON-Aufnahme.

Entwickelt von [NOTAVIS GmbH](https://www.notavis.com) · Member of the Vision Components Group

---

## Features

- **Live-Profil-Anzeige** direkt vom VC nano 3D Z Sensor
- **Zwei unabhängige ROIs** (blau / orange) für präzise Biegewinkelberechnung
- **4 Linienfinder-Methoden**: OLS, RANSAC, Hough, Auto
- **JSON-Wiedergabe** für Offline-Tests ohne physischen Sensor
- **Messwert-Log (CSV)** mit Zeitstempel: Phi1, Phi2, Biegewinkel, RMS, Methode
- **Preset-Verwaltung** zum Speichern und Laden von Messkonfigurationen
- **Lock-Funktion** gegen versehentliche Änderungen im laufenden Betrieb

---

## Voraussetzungen

- Windows 10 / 11 (x64)
- Vision Components VC nano 3D Z (Ethernet, im gleichen Subnetz erreichbar)
- Keine zusätzliche Software oder Installation erforderlich

---

## Schnellstart

1. ZIP entpacken in einen Ordner (z. B. `C:\Tools\VC3DProfileViewer\`)
2. `VC3DProfileViewer.exe` per Doppelklick starten
3. Beim ersten Start wird `Devices\Presets.ini` mit dem Preset **TestTyp_1** angelegt
4. Datenquelle auf **JSON Wiedergabe** setzen und `TestData\` öffnen → sofort betriebsbereit ohne Sensor

> Ausführliche Anleitung: [`docs/QuickStartGuide_VCProfileViewer_Biegewinkel.pdf`](docs/QuickStartGuide_VCProfileViewer_Biegewinkel.pdf)

---

## Sensor verbinden (Live-Betrieb)

| Feld | Standard | Ort |
|---|---|---|
| IP-Adresse | `192.168.3.15` | Linkes Panel → **Sensor (Live)** |
| Port | `1096` | Linkes Panel → **Sensor (Live)** |

1. Datenquelle oben auf **Live Sensor** setzen
2. IP und Port anpassen
3. **Verbinden** klicken → Status wechselt auf grün „Verbunden"

> Falls der Sensor zuvor mit 3DSmartShape verwendet wurde, Stromversorgung kurz trennen oder Port `1097` probieren.

---

## ROI & Winkelberechnung

Die zwei Auswertefenster definieren, wo die Fit-Geraden berechnet werden.

**Per Maus (empfohlen):** Linke Maustaste gedrückt halten und im Chart ziehen.

**Linienfinder-Methoden:**

| Methode | Empfohlen für |
|---|---|
| **OLS** | Saubere Profile ohne Reflexionen |
| **RANSAC** | Profile mit einzelnen Ausreißern |
| **Hough** | Fragmentierte Profile / Lücken |
| **Auto** | Automatische Wahl *(Standard)* |

**Winkel-Quadrant:** ◤ ◥ ◣ ◢ wählen, welcher der vier Winkel am Schnittpunkt gemessen wird. Der gelbe Bogen im Chart zeigt die aktive Messrichtung.

---

## JSON-Wiedergabe (Offline-Modus)

Ideal zum Testen und Einstellen der ROIs ohne physischen Sensor.

1. Datenquelle auf **JSON Wiedergabe** setzen
2. **„..."** → Ordner mit JSON-Dateien wählen (`TestData\` oder eigene Aufnahmen)
3. **Play** drücken oder mit `◀◀` / `▶▶` schrittweise navigieren

---

## Ordnerstruktur

```
VC3DProfileViewer\
  VC3DProfileViewer.exe     – Hauptprogramm
  Qt6*.dll                  – Qt-Laufzeitbibliotheken
  TestData\                 – 20 mitgelieferte JSON-Testprofile
  Recordings\               – Eigene Sensor-Aufnahmen (automatisch angelegt)
  Logs\                     – Messwert-Logs CSV (automatisch angelegt)
  Devices\
    default.ini             – Letzte Einstellungen (automatisch gespeichert)
    Presets.ini             – Gespeicherte Typen
  resources\
    logo_notavis.svg        – NOTAVIS Logo
```

---

## Tastatur & Maus

| Aktion | Funktion |
|---|---|
| Mausrad | Zoom um Cursorposition |
| Rechte Maustaste + Ziehen | Pan (Bild verschieben) |
| Doppelklick | Zoom auf alle Messdaten zurücksetzen |
| Linke Maustaste + Ziehen | ROI aufziehen |
| **? Hilfe** (Button unten rechts) | Dokumentation einblenden |

---

## macOS (Apple Silicon)

Für Mac-Nutzer steht ein separates Repository bereit:
[PatrikDrexel/VC3DProfileViewer-macOS](https://github.com/PatrikDrexel/VC3DProfileViewer-macOS)

Installation: ZIP entpacken → `VC3DProfileViewer.app` in Programme ziehen → Doppelklick

---

## Dokumentation

| Dokument | Beschreibung |
|---|---|
| [Quick Start Guide](docs/QuickStartGuide_VCProfileViewer_Biegewinkel.pdf) | Einstieg in 5 Minuten |
| [Ausführliche Dokumentation](docs/Dokumentation_VCProfileViewer_Biegewinkel.pdf) | Vollständige Funktionsreferenz |

---

## Fehlerbehebung

| Problem | Lösung |
|---|---|
| Kein Profil sichtbar nach Verbinden | ROI-Bereiche prüfen – müssen im X-Bereich des Profils liegen |
| Sensor verbindet nicht | IP/Port prüfen, Sensor neu starten, Port `1097` probieren |
| App reagiert langsam | Linienfinder-Methode auf OLS wechseln |
| Wasserzeichen fehlt | `resources\logo_notavis.svg` neben der EXE vorhanden? |

Bei Problemen: `Logs\AppLog_*.txt` beilegen und an den Support senden.

---

## Lizenz

Proprietäre Software – © 2026 NOTAVIS GmbH. Alle Rechte vorbehalten.
Nutzung nur gemäß beiliegender `LICENSE.txt` gestattet.

---

## Kontakt

**NOTAVIS GmbH** · Member of the Vision Components Group  
Web: [www.notavis.com](https://www.notavis.com)

---

*Erstellt mit Qt 6.8 / MSVC2022*
