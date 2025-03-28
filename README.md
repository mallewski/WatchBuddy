# MuehlenBuddy

ESP32-basiertes Projekt zur Überwachung von Kontakten mit Telegram-Benachrichtigung, Web-Dashboard und OTA-Update-Funktion.

## Funktionen

- Senden von Telegram-Nachrichten bei Kontaktbetätigung
- Lokales Webinterface zur Anzeige, Konfiguration und Fehlersuche
- Automatische Verbindung mit mehreren bekannten WLANs
- Anpassbare Nachrichtentexte über das Dashboard
- Passwortgeschützte Konfiguration (OTA + Chat-ID)
- Automatische Chat-ID-Aktualisierung bei Telegram-Gruppenmigration
- OTA-Updates direkt über Netzwerk
- Loganzeige im Webinterface
- Automatische Verbindung mit mehreren bekannten WLANs

## Dateien

- `MuehlenBuddy.ino` – Hauptsketch
- `secrets.h` – sensible Daten (nicht in Git speichern!)
- `.gitignore` – schützt `secrets.h` vor versehentlichem Upload
- `README.md` – dieses Dokument

## Webinterface (Dashboard)

Hier siehst du:

- Aktuelle Zustände der Kontakte (offen / geschlossen)
- Fehlerprotokoll mit Zeitstempel
- Formular zur Anpassung der Nachrichtentexte
- Telegram Chat-ID ändern (passwortgeschützt)
- Kontakt-Simulation mit direkter Telegram-Auslösung
- Firmware-Versionsanzeige (`v25.3.x`)

## Telegram-Nachrichten

Der ESP sendet Nachrichten direkt an deinen Telegram-Bot.

Hinweis: Wenn der Gruppenchat zu einer „Supergroup“ migriert wird, wird die `chat_id` automatisch aktualisiert.

## Sicherheit

- Keine Klartext-Passwörter im Hauptcode (`secrets.h`)
- Dashboard-Änderungen nur mit Passwort
- OTA-Updates nur nach Authentifizierung
- Telegram-Zugang ist auf deinen Bot & deine Chat-ID begrenzt

## Erweiterungsideen

- Push via MQTT oder E-Mail
- Zustandsanzeige per LED oder OLED
- Sensoren/Trigger erweitern
- Benachrichtigungen mit Standort oder Batterielevel

## Version

Aktuelle Firmware: `v25.3.2`

Änderungen siehst du im Error-Log oder per Versionshinweis im Dashboard.

Made with Liebe und ESP32.