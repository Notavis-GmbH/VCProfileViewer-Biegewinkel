VC3DProfileViewer v2.0
======================
Vision Components VC nano 3D Z – Profil-Viewer mit ROI und Winkelanzeige

ORDNERSTRUKTUR
--------------
VC3DProfileViewer\
  VC3DProfileViewer.exe   – Hauptprogramm (doppelklick zum Starten)
  Qt6*.dll                – Qt-Laufzeitbibliotheken
  platforms\              – Qt-Windows-Plugin
  Data\                   – JSON-Aufzeichnungen hier ablegen
  Devices\default.ini     – Einstellungen (IP, Port, ROI) – wird automatisch gespeichert

BEDIENUNG – LIVE SENSOR
------------------------
1. Datenquelle: "Live Sensor" auswählen
2. IP-Adresse und Port eingeben (Standard: 192.168.3.15 : 1096)
3. "Verbinden" klicken
4. ROI 1 und ROI 2 per Spinbox einstellen ODER
   per Maus-Drag direkt im Profildiagramm aufziehen
   – ROI 1 (blau): linke Linie
   – ROI 2 (orange): rechte Linie
5. Biegewinkel wird automatisch berechnet und groß angezeigt
6. "Trennen" zum Beenden der Verbindung

BEDIENUNG – JSON WIEDERGABE
-----------------------------
1. Datenquelle: "JSON Wiedergabe" auswählen
2. Auf "..." klicken und Ordner mit JSON-Dateien wählen
   (Format: LaserLineData_YYYYMMDD_HHMMSSxxxxxx.json)
3. Play / Stop / Schritt vor / Schritt zurück
4. Geschwindigkeit per Schieberegler anpassen
5. ROI-Linien können auch im Wiedergabemodus verändert werden

EINSTELLUNGEN
-------------
Alle Einstellungen (IP, Port, ROI, letzter JSON-Ordner, Geschwindigkeit)
werden beim Beenden automatisch in Devices\default.ini gespeichert
und beim nächsten Start wiederhergestellt.

HINWEIS SENSOR-RESET
--------------------
Falls der Sensor zuvor mit 3DSmartShape verwendet wurde, bitte
einmal Stromversorgung trennen, bevor dieser Viewer verbindet.
Alternativ Port 1097 verwenden.

(c) 2026 – erstellt mit Qt 6 / MSVC2022
