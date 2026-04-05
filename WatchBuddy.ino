#define FIRMWARE_VERSION "v25.4.1"

#include "secrets.h"
#include <WiFi.h>
#include <HTTPClient.h>
#include <WebServer.h>
#include <WiFiClientSecure.h>
#include <time.h>
#include <Preferences.h>
#include <ArduinoOTA.h>

//Name
const String NAME = "WatchBuddy";

// Telegram-Daten
const String telegramUrl = "https://api.telegram.org/bot" + botToken + "/sendMessage";
String chatId = defaultChatId;

// Definition der Eingangs-Pins für die Kontakte
const int CONTACT_PIN1 = 14; // bei nächster Gelegenheit zu Pin 14 wechseln (Du hast an GPIO12 einen Taster, der nach GND zieht (normal für INPUT_PULLUP) → beim Einschalten wird GPIO12 LOW → ESP32 bootet nicht mehr korrekt!)
const int CONTACT_PIN2 = 27;
const int CONTACT_PIN3 = 26; 
const int CONTACT_PIN4 = 25;

// Variablen zum Überwachen der physischen Kontakte
int lastContactState1 = HIGH;
int lastContactState2 = HIGH;
int lastContactState3 = HIGH;
int lastContactState4 = HIGH;

// Status-Strings
String contactStatus1 = "Unbekannt";
String contactStatus2 = "Unbekannt";
String contactStatus3 = "Unbekannt";
String contactStatus4 = "Unbekannt";

// Log-Daten
String errorLog = "";

// Benutzerdefinierte Nachrichtentexte
String customText1 = "Kontakt 1 wurde bestätigt";
String customText2 = "Kontakt 2 wurde betätigt!";
String customText3 = "Kontakt 3 wurde betätigt!";
String customText4 = "Kontakt 4 wurde ausgelöst!";


Preferences prefs;

void loadCustomTexts() {
  prefs.begin("config", false);
  customText1 = prefs.getString("ct1", customText1);
  customText2 = prefs.getString("ct2", customText2);
  customText3 = prefs.getString("ct3", customText3);
  customText4 = prefs.getString("ct4", customText4);
  chatId = prefs.getString("chat_id", chatId);
  prefs.end();
}

void saveCustomTexts() {
  prefs.begin("config", false);
  prefs.putString("ct1", customText1);
  prefs.putString("ct2", customText2);
  prefs.putString("ct3", customText3);
  prefs.putString("ct4", customText4);
  prefs.putString("chat_id", chatId);
  prefs.end();
}

const unsigned long debounceInterval = 1000;
unsigned long lastSendTime1 = 0;
unsigned long lastSendTime2 = 0;
unsigned long lastSendTime3 = 0;
unsigned long lastSendTime4 = 0;

WebServer server(80);

String urlEncode(const String &str) {
  String encoded = "";
  for (size_t i = 0; i < str.length(); i++) {
    char c = str.charAt(i);
    if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9')) {
      encoded += c;
    } else {
      encoded += '%';
      char code0 = (c >> 4) & 0xF;
      char code1 = c & 0xF;
      encoded += (code0 > 9) ? char(code0 - 10 + 'A') : char(code0 + '0');
      encoded += (code1 > 9) ? char(code1 - 10 + 'A') : char(code1 + '0');
    }
  }
  return encoded;
}
//Zeitausgabe für Log und Nachrichten
String getCurrentTimeString() {
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) return "Keine Zeit";
  char buf[30];
  strftime(buf, sizeof(buf), "%d.%m.%Y %H:%M:%S", &timeinfo);
  return String(buf);
}
//Zeitausgabe für Dashboard
String getCurrentTimeISO() {
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) return "1970-01-01T00:00:00";
  char buf[30];
  strftime(buf, sizeof(buf), "%Y-%m-%dT%H:%M:%S", &timeinfo);
  return String(buf);
}

void logEvent(String eventMessage) {
  String timestamp = getCurrentTimeString();
  String logEntry = timestamp + " - " + eventMessage + "\n";
  errorLog += logEntry;

  prefs.begin("config", false);
  prefs.putString("elog", errorLog);
  prefs.end();
}

void loadErrorLog() {
  prefs.begin("config", true);
  errorLog = prefs.getString("elog", "");
  prefs.end();
}

void sendTelegramMessage(String message) {
  String timestamp = getCurrentTimeString();
  String fullMessage = message + " \n(" + timestamp + ")";
  String encodedMessage = urlEncode(fullMessage);

  if (WiFi.status() == WL_CONNECTED) {
    HTTPClient http;
    String url = telegramUrl + "?chat_id=" + chatId + "&text=" + encodedMessage;

    WiFiClientSecure client;
    client.setInsecure();
    http.begin(client, url);

    int httpCode = http.GET();
    if (httpCode > 0) {
      String payload = http.getString();
      Serial.printf("HTTP GET Code: %d\n", httpCode);
      Serial.println("Antwort: " + payload);

      if (payload.indexOf("migrate_to_chat_id") > -1) {
        int pos = payload.indexOf("migrate_to_chat_id");
        int start = payload.indexOf(":", pos) + 1;
        int end = payload.indexOf("}", start);
        String newChatId = payload.substring(start, end);
        newChatId.trim();
       if (newChatId != chatId) {
          logEvent("chat_id automatisch aktualisiert durch Telegram: " + newChatId);
          Serial.println("Neue chat_id erkannt: " + newChatId);
          prefs.begin("config", false);
          prefs.putString("chat_id", newChatId);
          prefs.end();
          chatId = newChatId;
       }

        // Retry mit neuer Chat-ID
        String retryUrl = telegramUrl + "?chat_id=" + chatId + "&text=" + encodedMessage;
        http.begin(client, retryUrl);
        int retryCode = http.GET();
        String retryPayload = http.getString();
        Serial.printf("Erneuter Sendeversuch: %d\n", retryCode);
        Serial.println(retryPayload);
       if (retryPayload.indexOf("ok") > -1) {
        logEvent("Telegram-Nachricht erfolgreich nach Chat-ID-Wechsel versendet.");
        } else {
        logEvent("Fehler beim Wiederholungsversuch mit neuer chat_id.");
        }
      }

      if (payload.indexOf("chat not found") > -1) {
       logEvent("Fehler: Ungültige chat_id – 'chat not found'");
      }
      if (payload.indexOf("bot was kicked") > -1) {
        logEvent("Fehler: Bot wurde aus der Gruppe entfernt!");
      }
    } else {
      String err = String("HTTP GET Fehler: ") + http.errorToString(httpCode);
      Serial.println(err);
      logEvent(err);
    }
    http.end();
  } else {
    String err = "Kein WLAN verbunden";
    Serial.println(err);
    logEvent(err);
  }
}

String escapeJSONString(String s) {
  s.replace("\\", "\\\\");
  s.replace("\"", "\\\"");
  s.replace("\n", "\\n");
  s.replace("\r", "");
  return s;
}

void handleStatus() {
  String json = "{";
  json += "\"contact1\":\"" + contactStatus1 + "\",";
  json += "\"contact2\":\"" + contactStatus2 + "\",";
  json += "\"contact3\":\"" + contactStatus3 + "\",";
  json += "\"contact4\":\"" + contactStatus4 + "\",";
  json += "\"errorLog\":\"" + escapeJSONString(errorLog) + "\",";
  json += "\"time\":\"" + getCurrentTimeString() + "\"";
  json += "}";
  server.send(200, "application/json", json);
}

void handleRoot() {
  String html = "<html><head><title>" + String(NAME) + " Dashboard</title><meta charset='UTF-8'>";
  html += "<link rel='icon' type='image/x-icon' href='data:image/x-icon;base64,AAABAAEAEBAAAAEAIABoBAAAFgAAACgAAAAQAAAAIAAAAAEAGAAAAAAAAAMAABMLAAATCwAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAERERERERERERERERERERERERERERERERERERERERERERERERERERERERERERERERERERERERERERERERERERERERERERERERERERERERERERERERERERERERERERERERERERERERERERERERERERERERERERERERERERERERERERERERERERERERERERERERERERERERERERERERERH//wAA//8AAP//AAD//wAA//8AAP//AAD//wAA//8AAP//AAD//wAA//8AAP//AAD//wAA'>\n";
  html += "<style>";
  html += "body { font-family: sans-serif; background:#f4f4f4; margin:0; padding:0; }";
  html += ".container { max-width: 800px; margin: 30px auto; background: #fff; padding: 20px; box-shadow: 0 0 12px rgba(0,0,0,0.1); border-radius: 10px; }";
  html += "h1, h2 { color: #333; }";
  html += "input[type='text'], input[type='password'] { width: 100%; padding: 10px; margin: 5px 0 5px; border: 1px solid #ccc; border-radius: 5px; }";
  html += "input[type='submit'], input[type='button'], ";
  html += "input[type='submit']:hover";
  html += "form { margin-bottom: 20px; }";
  html += "#log { background:#eee; padding:10px; max-height:200px; overflow:auto; font-family: monospace; white-space: pre-wrap; }";
  html += "#passwordModal, #modalBackdrop { display:none; }";
  html += "button, input[type='button'], input[type='submit'] {";
  html += "  background-color: #e1ecf4;";
  html += "  border-radius: 3px;";
  html += "  border: 1px solid #7aa7c7;";
  html += "  box-shadow: rgba(255, 255, 255, .7) 0 1px 0 0 inset;";
  html += "  box-sizing: border-box;";
  html += "  color: #39739d;";
  html += "  cursor: pointer;";
  html += "  display: inline-block;";
  html += "  font-family: -apple-system,system-ui,\"Segoe UI\",\"Liberation Sans\",sans-serif;";
  html += "  font-size: 13px;";
  html += "  font-weight: 400;";
  html += "  line-height: 1.15385;";
  html += "  margin: 10px 0 0;";
  html += "  outline: none;";
  html += "  padding: 8px .8em;";
  html += "  position: relative;";
  html += "  text-align: center;";
  html += "  text-decoration: none;";
  html += "  user-select: none;";
  html += "  -webkit-user-select: none;";
  html += "  touch-action: manipulation;";
  html += "  vertical-align: baseline;";
  html += "  white-space: nowrap;";
  html += "}";

  html += "button:hover, input[type='button']:hover, input[type='submit']:hover,";
  html += "button:focus, input[type='button']:focus, input[type='submit']:focus {";
  html += "  background-color: #b3d3ea;";
  html += "  color: #2c5777;";
  html += "}";

  html += "button:focus, input[type='button']:focus, input[type='submit']:focus {";
  html += "  box-shadow: 0 0 0 4px rgba(0, 149, 255, .15);";
  html += "}";  

  html += "button:active, input[type='button']:active, input[type='submit']:active {";
  html += "  background-color: #a0c7e4;";
    html += "  box-shadow: none;";
  html += "  color: #2c5777;";
  html += "}";
  html += "button.eye-toggle {outline: none;box-shadow: none;margin: 0;}button.eye-toggle:focus {outline: none;box-shadow: none;}";
  html += "</style>";
  html += "<script>function updateStatus() {";
  html += "var xhttp = new XMLHttpRequest();";
  html += "xhttp.onreadystatechange = function() {";
  html += "if (this.readyState == 4 && this.status == 200) {";
  html += "var data = JSON.parse(this.responseText);";
  html += "document.getElementById('status1').innerHTML = data.contact1;";
  html += "document.getElementById('status2').innerHTML = data.contact2;";
  html += "document.getElementById('status3').innerHTML = data.contact3;";
  html += "document.getElementById('status4').innerHTML = data.contact4;";
  html += "document.getElementById('log').innerHTML = data.errorLog.replace(/\\n/g, '<br>');";
  html += "}};xhttp.open('GET', '/status', true);xhttp.send();}";
  html += "window.onload = function() {";
  html += "  updateStatus();";
  html += "  startLiveClock('" + getCurrentTimeISO() + "');";
  html += "  setInterval(updateStatus, 3000);";
  html += "};</script></head><body>";
  //Anfang Inhalt
  html += "<div class='container'>";
  html += "<h1>WatchBuddy Dashboard</h1>";
  html += "<p style='text-align:right; color:#888;'>Modul-Zeit: <span id='espTime'></span></p>";
  html += "<h2>Kontaktstatus</h2>";
  html += "<p>Kontakt 1 (P" + String(CONTACT_PIN1) + "): <strong><span id='status1'>" + contactStatus1 + "</span></strong></p>";
  html += "<p>Kontakt 2 (P" + String(CONTACT_PIN2) + "): <strong><span id='status2'>" + contactStatus2 + "</span></strong></p>";
  html += "<p>Kontakt 3 (P" + String(CONTACT_PIN3) + "): <strong><span id='status3'>" + contactStatus3 + "</span></strong></p>";
  html += "<p>Kontakt 4 (P" + String(CONTACT_PIN4) + "): <strong><span id='status4'>" + contactStatus4 + "</span></strong></p>";

  //Nachrichten
  html += "<h2>Nachrichtentexte anpassen</h2>";
  html += "<form action='/setMessages' method='GET'>";
  html += "Kontakt 1: <input type='text' name='msg1' value='" + customText1 + "'>";
  html += "Kontakt 2: <input type='text' name='msg2' value='" + customText2 + "'>";
  html += "Kontakt 3: <input type='text' name='msg3' value='" + customText3 + "'>";
  html += "Kontakt 4: <input type='text' name='msg4' value='" + customText4 + "'>";
  html += "<input type='submit' value='Nachrichten aktualisieren'></form>";
  //Telegram
  html += "<h2>Telegram Chat-ID</h2>";
  html += "<form id='chatForm' action='/setChatId' method='POST'>";
  html += "Chat-ID: <input type='text' name='chatid' value='" + chatId + "'>";
  html += "<input type='hidden' name='password' id='passwordField'>";
  html += "<input type='button' value='Chat-ID speichern' onclick='showPasswordModal()'>";
  html += "</form>";
  //Simulation
  html += "<h2>Simulation</h2>";
  html += "<button onclick=\"location.href='/simulateContact1'\">Simuliere Kontakt 1</button> ";
  html += "<button onclick=\"location.href='/simulateContact2'\">Simuliere Kontakt 2</button>";
  //Error-Log
  html += "<h2>Error-Log</h2><div id='log'></div>";
  html += "<form id='logForm' action='/clearLog' method='POST'>";
  html += "<input type='hidden' name='password' id='logPasswordField'>";
  html += "<input type='button' value='Error-Log löschen' onclick='showLogPasswordModal()'>";
  html += "</form>"; 
  //Firmwarebutton
  html += "<p><button type='button' onclick=\"showFirmwareModal()\">Firmware-Update</button></p>";

  html += "<p style='text-align:right; color:#888;'>Firmware: " + String(FIRMWARE_VERSION) + "</p>";
  html += "</div>";
  // Modal für Passwort-Eingabe Chat-ID ändern
  html += "<div id='passwordModal' style='display:none; position:fixed; top:30%; left:50%; transform:translate(-50%, -50%); background:#fff; ";
  html += "padding:20px; border:1px solid #ccc; box-shadow:0 0 10px rgba(0,0,0,0.3); z-index:1000; border-radius:10px;'>";
  html += "<h3>Passwort eingeben</h3>";
  html += "<div style='display:flex; align-items:center;'>";
  html += "<input type='password' id='passwordInput' style='flex:1;'>";
  html += "<button type='button' class='eye-toggle' onclick='togglePassword(this)' data-target='passwordInput' style='background:transparent; border:none; padding:0 6px; margin-left:6px; cursor:pointer;'>";
  html += "<span class='eye-show'>";
  html += "<svg viewBox='0 0 24 24' width='24' height='24' stroke='currentColor' fill='none' stroke-width='2' stroke-linecap='round' stroke-linejoin='round'>";
  html += "<path d='M1 12s4-8 11-8 11 8 11 8-4 8-11 8S1 12 1 12z'></path>";
  html += "<circle cx='12' cy='12' r='3'></circle>";
  html += "</svg></span>";
  html += "<span class='eye-hide' style='display:none;'>";
  html += "<svg viewBox='0 0 24 24' width='24' height='24' stroke='currentColor' fill='none' stroke-width='2' stroke-linecap='round' stroke-linejoin='round'>";
  html += "<path d='M1 12s4-8 11-8 11 8 11 8-4 8-11 8S1 12 1 12z'></path>";
  html += "<circle cx='12' cy='12' r='3'></circle>";
  html += "<path d='M3 3l18 18'></path>";
  html += "</svg></span>";
  html += "</button>";
  html += "</div><br>";
  html += "<button onclick='submitChatForm()'>OK</button> ";
  html += "<button onclick='hidePasswordModal()'>Abbrechen</button>";
  html += "</div>";
  //Modal für Passworteingabe Error-Log löschen
  html += "<div id='logPasswordModal' style='display:none; position:fixed; top:40%; left:50%; transform:translate(-50%, -50%); background:#fff; ";
  html += "padding:20px; border:1px solid #ccc; box-shadow:0 0 10px rgba(0,0,0,0.3); z-index:1000; border-radius:10px;'>";
  html += "<h3>Passwort für Log-Löschung</h3>";
  html += "<div style='display:flex; align-items:center;'>";
  html += "<input type='password' id='logPasswordInput' style='flex:1;'>";
  html += "<button type='button' class='eye-toggle' onclick='togglePassword(this)' data-target='logPasswordInput' style='background:transparent; border:none; padding:0 6px; margin-left:6px; cursor:pointer;'>";
  html += "<span class='eye-show'>";
  html += "<svg viewBox='0 0 24 24' width='24' height='24' stroke='currentColor' fill='none' stroke-width='2' stroke-linecap='round' stroke-linejoin='round'>";
  html += "<path d='M1 12s4-8 11-8 11 8 11 8-4 8-11 8S1 12 1 12z'></path>";
  html += "<circle cx='12' cy='12' r='3'></circle>";
  html += "</svg></span>";
  html += "<span class='eye-hide' style='display:none;'>";
  html += "<svg viewBox='0 0 24 24' width='24' height='24' stroke='currentColor' fill='none' stroke-width='2' stroke-linecap='round' stroke-linejoin='round'>";
  html += "<path d='M1 12s4-8 11-8 11 8 11 8-4 8-11 8S1 12 1 12z'></path>";
  html += "<circle cx='12' cy='12' r='3'></circle>";
  html += "<path d='M3 3l18 18'></path>";
  html += "</svg></span>";
  html += "</button>";
  html += "</div><br>";
  html += "<button onclick='submitLogForm()'>OK</button> ";
  html += "<button onclick='hideLogPasswordModal()'>Abbrechen</button>";
  html += "</div>";
  html += R"rawliteral(
    <div id="firmwareModal" style="display:none; position:fixed; top:30%; left:50%; transform:translate(-50%, -50%); background:#fff;
    padding:20px; border:1px solid #ccc; box-shadow:0 0 10px rgba(0,0,0,0.3); z-index:1000; border-radius:10px;">
      <h3>Firmware-Update durchführen</h3>
      <p>Installierte Version: )rawliteral" + String(FIRMWARE_VERSION) + R"rawliteral(</p>
      <form method="POST" action="/update" enctype="multipart/form-data" onsubmit="return confirm('Firmware wirklich aktualisieren?')">
        <input type="file" name="firmware" required><br><br>
        <input type="submit" value="Upload & Update">
        <button type="button" onclick="hideFirmwareModal()">Abbrechen</button>
      </form>
    </div>
    <div id="firmwareBackdrop" style="display:none; position:fixed; top:0; left:0; width:100%; height:100%; background:rgba(0,0,0,0.3); z-index:999;"></div>
  )rawliteral";
  //Java-Script
  html += "<script>";
  html += "function showPasswordModal() { document.getElementById('passwordModal').style.display = 'block'; document.getElementById('modalBackdrop').style.display = 'block'; document.getElementById('passwordInput').value = ''; document.getElementById('passwordInput').focus(); }";
  html += "function hidePasswordModal() { document.getElementById('passwordModal').style.display = 'none'; document.getElementById('modalBackdrop').style.display = 'none'; }";
  html += "function submitChatForm() { var pw = document.getElementById('passwordInput').value; if (!pw) return; document.getElementById('passwordField').value = pw; document.getElementById('chatForm').submit(); }";
  html += "function showFirmwareModal() { document.getElementById('firmwareModal').style.display = 'block'; document.getElementById('firmwareBackdrop').style.display = 'block'; }";
  html += "function hideFirmwareModal() { document.getElementById('firmwareModal').style.display = 'none'; document.getElementById('firmwareBackdrop').style.display = 'none'; }";
  html += "function togglePassword() { var input = document.getElementById('passwordInput'); input.type = (input.type === 'password') ? 'text' : 'password'; }";
  html += "function showLogPasswordModal() {";
  html += "document.getElementById('logPasswordModal').style.display = 'block';";
  html += "document.getElementById('modalBackdrop').style.display = 'block';";
  html += "document.getElementById('logPasswordInput').value = '';";
  html += "document.getElementById('logPasswordInput').focus();";
  html += "}";
  html += "function hideLogPasswordModal() {";
  html += "document.getElementById('logPasswordModal').style.display = 'none';";
  html += "document.getElementById('modalBackdrop').style.display = 'none';";
  html += "}";
  html += "function submitLogForm() {";
  html += "var pw = document.getElementById('logPasswordInput').value;";
  html += "if (!pw) return;";
  html += "document.getElementById('logPasswordField').value = pw;";
  html += "document.getElementById('logForm').submit();";
  html += "}";
  html += "function togglePassword(button) {";
  html += "  const inputId = button.getAttribute('data-target');";
  html += "  const input = document.getElementById(inputId);";
  html += "  const showIcon = button.querySelector('.eye-show');";
  html += "  const hideIcon = button.querySelector('.eye-hide');";
  html += "  const isHidden = input.type === 'password';";
 