# VC 3D Profile Viewer v2.2

**Vision Components VC nano 3D Z** – Profil-Viewer mit ROI-Einstellung, Winkelberechnung (OLS / RANSAC / Hough / Auto), Messwert-Log und JSON-Aufnahme.

---

## Quickstart für neue Teammitglieder

### 1 · Installation

1. ZIP entpacken in einen Ordner deiner Wahl (z. B. `C:\Tools\VC3DProfileViewer\`)
2. `VC3DProfileViewer.exe` per Doppelklick starten – keine Installation, kein Installer
3. Beim ersten Start wird `Devices\Presets.ini` mit dem Preset **TestTyp_1** angelegt

---

### 2 · Sensor-IP konfigurieren

| Feld | Standard | Wo |
|---|---|---|
| IP-Adresse | `192.168.3.15` | Linkes Panel → **Sensor (Live)** |
| Port | `1096` | Linkes Panel → **Sensor (Live)** |

**Schritte:**
1. Datenquelle oben auf **Live Sensor** setzen
2. IP und Port anpassen
3. **Verbinden** klicken → Status wechselt auf grün „Verbunden"
4. Das Profil erscheint sofort im Chart

> **Hinweis:** Falls der Sensor zuvor mit 3DSmartShape verwendet wurde, Stromversorgung kurz trennen oder Port `1097` probieren.

---

### 3 · ROI einrichten

Die zwei Auswertefenster (ROI 1 blau / ROI 2 orange) definieren wo die Fit-Geraden berechnet werden.

**Methode A – Spinboxen:**
- Linkes Panel → **ROI Einstellungen**
- Start- und Endwert in mm eintragen

**Methode B – Maus im Chart (empfohlen):**
1. Linke Maustaste gedrückt halten und im Profil-Chart ziehen
2. ROI 1 im linken Bereich aufziehen, ROI 2 im rechten Bereich
3. Werte werden automatisch in die Spinboxen übernommen

**Linienfinder-Methode** pro ROI wählbar:

| Methode | Empfohlen für |
|---|---|
| **OLS** | Saubere Profile ohne Reflexionen |
| **RANSAC** | Profile mit einzelnen Ausreißern |
| **Hough** | Fragmentierte Profile / Lücken |
| **Auto** | Automatische Wahl je nach Inlier-Verhältnis *(Standard)* |

**Winkel-Quadrant** (Messergebnis-Panel):
- ◤ ◥ ◣ ◢ wählen welcher der vier Winkel am Schnittpunkt gemessen wird
- Gelber Bogen im Chart zeigt die aktive Messrichtung

---

### 4 · JSON-Wiedergabe-Modus (Offline)

Ideal zum Testen und Einstellen der ROIs ohne physischen Sensor.

**Profiles aufzeichnen (Live-Modus):**
1. Mit Sensor verbinden
2. Linkes Panel → **Profil-Aufnahme (JSON)** → Ordner wählen
3. Optional: Max. Frames begrenzen (0 = unbegrenzt)
4. **⏺ Aufnahme starten** → Frames werden als `LaserLineData_*.json` gespeichert

**Aufnahmen abspielen:**
1. Datenquelle auf **JSON Wiedergabe** setzen
2. **„..."** → Ordner mit den JSON-Dateien wählen (`TestData\` oder eigene Recordings)
3. **Play** drücken – oder Schritt für Schritt mit `◀◀` / `▶▶`
4. Geschwindigkeit per Schieberegler anpassen

> **Tipp:** Den **TestTyp_1** Preset laden – dieser hat ROI und Ordner bereits auf die mitgelieferten 20 Testprofile voreingestellt.

---

### 5 · Typverwaltung (Presets)

Einstellungen (IP, ROI, Methode, Ordner) als benannter Typ speichern:

1. Alle Einstellungen wie gewünscht vornehmen
2. **Speichern** → Name eingeben → OK
3. Nächstes Mal: Typ aus Dropdown wählen → alle Einstellungen werden sofort übernommen (Sensor startet **nicht** automatisch)

**Lock-Funktion:** 🔒 Button sperrt alle Einstellungen gegen versehentliche Änderungen im laufenden Betrieb.

---

### 6 · Messwert-Log (CSV)

Zeichnet Phi1, Phi2, Biegewinkel, RMS und Methode mit Zeitstempel auf:

1. Linkes Panel → **Messwert-Log (CSV)**
2. Pfad wählen oder Standard verwenden (`Logs\MeasLog_*.csv`)
3. **● Aufzeichnung starten** → läuft bei jedem Frame mit
4. CSV direkt in Excel öffnen (Semikolon-Trennzeichen, UTF-8)

---

## Ordnerstruktur

```
VC3DProfileViewer\
  VC3DProfileViewer.exe     – Hauptprogramm
  Qt6*.dll                  – Qt-Laufzeitbibliotheken
  TestData\                 – 20 mitgelieferte JSON-Testprofile
  Recordings\               – Eigene Sensor-Aufnahmen (wird automatisch angelegt)
  Logs\                     – Messwert-Logs CSV (wird automatisch angelegt)
  Data\                     – Freier Ablageordner
  Devices\
    default.ini             – Letzte Einstellungen (automatisch gespeichert)
    Presets.ini             – Gespeicherte Typen
  resources\
    logo_notavis.svg        – NOTAVIS Logo (Wasserzeichen + Hilfe)
```

---

## Tastatur & Maus im Chart

| Aktion | Funktion |
|---|---|
| Mausrad | Zoom um Cursorposition |
| Rechte Maustaste + Ziehen | Pan (Bild verschieben) |
| Doppelklick | Zoom auf alle Messdaten zurücksetzen |
| Linke Maustaste + Ziehen | ROI aufziehen |
| **? Hilfe** (Button unten rechts) | Dokumentation einblenden |

---

## Technische Voraussetzungen

- Windows 10 / 11 (x64)
- Netzwerkverbindung zum Sensor (Ethernet, IP im gleichen Subnetz)
- Keine zusätzliche Software erforderlich

---

## macOS (Apple Silicon)

Für Mac-Nutzer steht ein separates Repository bereit:
[PatrikDrexel/VC3DProfileViewer-macOS](https://github.com/PatrikDrexel/VC3DProfileViewer-macOS)

Installation: ZIP entpacken → `VC3DProfileViewer.app` in Programme ziehen → Doppelklick

---

## Fehlerbehebung

| Problem | Lösung |
|---|---|
| Kein Profil sichtbar nach Verbinden | ROI-Bereiche prüfen – müssen im X-Bereich des Profils liegen |
| Sensor verbindet nicht | IP/Port prüfen, Sensor neu starten, Port 1097 probieren |
| App friert ein | Linienfinder-Methode auf OLS wechseln (RANSAC/Hough bei sehr vielen Punkten langsamer) |
| Wasserzeichen fehlt | `resources\logo_notavis.svg` neben der EXE vorhanden? |
| Versionsnummer zeigt „dev" | Nur bei lokalem Build ohne CI – normal |

Bei Problemen: `Logs\AppLog_*.txt` an die Entwicklung schicken.

---

*© 2026 NOTAVIS GmbH · Member of the Vision Components Group · erstellt mit Qt 6.8 / MSVC2022*
