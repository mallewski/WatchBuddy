# MuehlenBuddy

ESP32-basiertes Projekt zur Überwachung von Kontakten mit Telegram-Benachrichtigung, Web-Dashboard und OTA-Update-Funktion.

## Funktionen

- Senden von Telegram-Nachrichten bei Kontaktbetätigung
- Lokales Webinterface zur Anzeige, Konfiguration und Fehlersuche
- Automatische Verbindung mit mehreren bekannten WLANs
- Anpassbare Nachrichtentexte über das Dashboard
- Automatische Chat-ID-Aktualisierung bei Telegram-Gruppenmigration
- Passwortgeschützte Konfiguration
- Loganzeige im Webinterface
- OTA-Updates direkt über Netzwerk

## Dateien

- `MuehlenBuddy.ino` – Hauptsketch
- `secrets.h` – sensible Daten (nicht in Git gespeichert! Siehe `secrets_template.h`)
- `.gitignore` – schützt `secrets.h` vor versehentlichem Upload
- `README.md` – dieses Dokument

## Webinterface (Dashboard)

Hier siehst du:
- Aktuelle Uhrzeit des ESP (Wird in Nachrichten und Error-Log verwendet)
- Aktuelle Zustände der Kontakte (offen / geschlossen)
- Fehlerprotokoll mit Zeitstempel
- Formular zur Anpassung der Nachrichtentexte
- Anzeige der aktuellen Telegram Chat-ID.
- Möglichkeit aktuelle Telegram-Chat-ID zu ändern (passwortgeschützt)
- Kontakt-Simulation mit direkter Telegram-Auslösung (Troubleshooting)
- Firmware-Versionsanzeige

## Telegram-Nachrichten

Der ESP sendet Nachrichten direkt an deinen Telegram-Bot.

Hinweis: Wenn der Gruppenchat zu einer „Supergroup“ migriert wird, wird die `chat_id` automatisch aktualisiert.

## Sicherheit

- Keine Klartext-Passwörter im Hauptcode (`secrets.h`)
- Dashboard-Änderungen nur mit Passwort
- OTA-Updates nur nach Authentifizierung
- Telegram-Zugang ist auf deinen Bot & deine Chat-ID begrenzt

## Vorbedingungen

- Telegram-Bot erstellen
  -> Suche in Telegram nach @BotFather.
  -> Schreibe /newbot und folge den Anweisungen.
  -> Gib Name und Benutzername (muss auf bot enden) ein.
  -> Du erhältst ein Bot-Token → sicher speichern.
  -> Fertig! Dein Bot ist jetzt einsatzbereit.
- Chat-ID von Chat oder Gruppen-Chat ermitteln.
  -> Schreibe dem Bot eine Nachricht bei Telegram.
  -> Öffne im Browser: `https://api.telegram.org/bot<DEIN_BOT_TOKEN>/getUpdates`
  -> Suche in der Antwort nach "chat":{"id":...} → Das ist die Chat-ID.
- ESP32-Board
- Arduino IDE

## Erweiterungsideen

- Push via MQTT oder E-Mail
- Zustandsanzeige per LED oder OLED
- Sensoren/Trigger erweitern
- Benachrichtigungen mit Standort oder Batterielevel

## Version

Aktuelle Firmware: `v25.3.4`

Made with Liebe und ESP32.
