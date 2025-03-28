#define FIRMWARE_VERSION "v25.3.2"

#include "secrets.h"
#include <WiFi.h>
#include <HTTPClient.h>
#include <WebServer.h>
#include <WiFiClientSecure.h>
#include <time.h>
#include <Preferences.h>
#include <ArduinoOTA.h>

// Telegram-Daten
const String telegramUrl = "https://api.telegram.org/bot" + botToken + "/sendMessage";
String chatId = defaultChatId;

// Definition der Eingangs-Pins für die Kontakte
const int CONTACT_PIN1 = 12;
const int CONTACT_PIN2 = 14;

// Variablen zum Überwachen der physischen Kontakte
int lastContactState1 = HIGH;
int lastContactState2 = HIGH;

// Status-Strings
String contactStatus1 = "Unbekannt";
String contactStatus2 = "Unbekannt";

// Log-Daten
String errorLog = "";

// Benutzerdefinierte Nachrichtentexte
String customText1 = "Mühle ist durchgelaufen!";
String customText2 = "Kontakt 2 wurde betätigt!";

Preferences prefs;

void loadCustomTexts() {
  prefs.begin("config", false);
  customText1 = prefs.getString("ct1", customText1);
  customText2 = prefs.getString("ct2", customText2);
  chatId = prefs.getString("chat_id", chatId);
  prefs.end();
}

void saveCustomTexts() {
  prefs.begin("config", false);
  prefs.putString("ct1", customText1);
  prefs.putString("ct2", customText2);
  prefs.putString("chat_id", chatId);
  prefs.end();
}

const unsigned long debounceInterval = 1000;
unsigned long lastSendTime1 = 0;
unsigned long lastSendTime2 = 0;

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

String getCurrentTimeString() {
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) return "Keine Zeit";
  char timeString[64];
  strftime(timeString, sizeof(timeString), "%d.%m.%Y %H:%M:%S", &timeinfo);
  return String(timeString);
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
  json += "\"errorLog\":\"" + escapeJSONString(errorLog) + "\"";
  json += "}";
  server.send(200, "application/json", json);
}

void handleRoot() {
  String html = "<html><head><title>MuehlenBuddy Dashboard</title><meta charset='UTF-8'>";
  html += "<style>";
  html += "body { font-family: sans-serif; background:#f4f4f4; margin:0; padding:0; }";
  html += ".container { max-width: 800px; margin: 30px auto; background: #fff; padding: 20px; box-shadow: 0 0 12px rgba(0,0,0,0.1); border-radius: 10px; }";
  html += "h1, h2 { color: #333; }";
  html += "input[type='text'], input[type='password'] { width: 100%; padding: 10px; margin: 5px 0 15px; border: 1px solid #ccc; border-radius: 5px; }";
  html += "input[type='submit'], input[type='button'], button { padding: 10px 20px; background: #007bff; color: white; border: none; border-radius: 5px; cursor: pointer; margin-top: 5px; }";
  html += "input[type='submit']:hover, button:hover { background: #0056b3; }";
  html += "form { margin-bottom: 20px; }";
  html += "#log { background:#eee; padding:10px; max-height:200px; overflow:auto; font-family: monospace; white-space: pre-wrap; }";
  html += "#passwordModal, #modalBackdrop { display:none; }";
  html += "</style>";
  html += "<script>function updateStatus() {";
  html += "var xhttp = new XMLHttpRequest();";
  html += "xhttp.onreadystatechange = function() {";
  html += "if (this.readyState == 4 && this.status == 200) {";
  html += "var data = JSON.parse(this.responseText);";
  html += "document.getElementById('status1').innerHTML = data.contact1;";
  html += "document.getElementById('status2').innerHTML = data.contact2;";
  html += "document.getElementById('log').innerHTML = data.errorLog.replace(/\\n/g, '<br>');";
  html += "}};xhttp.open('GET', '/status', true);xhttp.send();}";
  html += "setInterval(updateStatus, 3000);window.onload = updateStatus;</script></head><body>";

  html += "<div class='container'>";
  html += "<h1>MuehlenBuddy Dashboard</h1><h2>Kontaktstatus</h2>";
  html += "<p>Kontakt 1: <strong><span id='status1'>" + contactStatus1 + "</span></strong></p>";
  html += "<p>Kontakt 2: <strong><span id='status2'>" + contactStatus2 + "</span></strong></p>";

  html += "<h2>Nachrichtentexte anpassen</h2>";
  html += "<form action='/setMessages' method='GET'>";
  html += "Kontakt 1: <input type='text' name='msg1' value='" + customText1 + "'>";
  html += "Kontakt 2: <input type='text' name='msg2' value='" + customText2 + "'>";
  html += "<input type='submit' value='Nachrichten aktualisieren'></form>";

  html += "<h2>Telegram Chat-ID</h2>";
  html += "<form id='chatForm' action='/setChatId' method='POST'>";
  html += "Chat-ID: <input type='text' name='chatid' value='" + chatId + "'>";
  html += "<input type='hidden' name='password' id='passwordField'>";
  html += "<input type='button' value='Chat-ID speichern' onclick='showPasswordModal()'>";
  html += "</form>";

  html += "<h2>Simulation</h2>";
  html += "<button onclick=\"location.href='/simulateContact1'\">Simuliere Kontakt 1</button> ";
  html += "<button onclick=\"location.href='/simulateContact2'\">Simuliere Kontakt 2</button>";

  html += "<h2>Error-Log</h2><div id='log'></div>";
  html += "<form id='logForm' action='/clearLog' method='POST'>";
  html += "<input type='hidden' name='password' id='logPasswordField'>";
  html += "<input type='button' value='Error-Log löschen' onclick='showLogPasswordModal()'>";
  html += "</form>"; 
  html += "<p style='text-align:right; color:#888;'>Firmware: " + String(FIRMWARE_VERSION) + "</p>";
  html += "</div>";


  // Modal für Passwort-Eingabe Chat-ID ändern
  html += "<div id='passwordModal' style='display:none; position:fixed; top:30%; left:50%; transform:translate(-50%, -50%); background:#fff; ";
      html += "padding:20px; border:1px solid #ccc; box-shadow:0 0 10px rgba(0,0,0,0.3); z-index:1000; border-radius:10px;'>";
  html += "<h3>Passwort eingeben</h3>";
  html += "<div style='display:flex; align-items:center;'>";
  html += "<input type='password' id='passwordInput' style='flex:1;'>";
  html += "<button type='button' onclick='togglePassword()' style='background:transparent; border:none; font-size:1.2em; cursor:pointer; padding:0 6px; margin-left:6px;'>👁️</button>";
  html += "</div><br>";
  html += "<button onclick='submitChatForm()'>OK</button> ";
  html += "<button onclick='hidePasswordModal()'>Abbrechen</button>";
  html += "</div>";
  html += "<div id='modalBackdrop' onclick='hidePasswordModal()' style='display:none; position:fixed; top:0; left:0; width:100%; height:100%; background:rgba(0,0,0,0.3); z-index:999;'></div>";
  //Modal für Passworteingabe Error-Log löschen
  html += "<div id='logPasswordModal' style='display:none; position:fixed; top:40%; left:50%; transform:translate(-50%, -50%); background:#fff; ";
  html += "padding:20px; border:1px solid #ccc; box-shadow:0 0 10px rgba(0,0,0,0.3); z-index:1000; border-radius:10px;'>";
  html += "<h3>Passwort für Log-Löschung</h3>";
  html += "<div style='display:flex; align-items:center;'>";
  html += "<input type='password' id='logPasswordInput' style='flex:1;'>";
  html += "<button type='button' onclick='toggleLogPassword()' style='background:transparent; border:none; font-size:1.2em; cursor:pointer; padding:0 6px; margin-left:6px;'>👁️</button>";
  html += "</div><br>";
  html += "<button onclick='submitLogForm()'>OK</button> ";
  html += "<button onclick='hideLogPasswordModal()'>Abbrechen</button>";
  html += "</div>";
  //Java-Script
  html += "<script>";
  html += "function showPasswordModal() { document.getElementById('passwordModal').style.display = 'block'; document.getElementById('modalBackdrop').style.display = 'block'; document.getElementById('passwordInput').value = ''; document.getElementById('passwordInput').focus(); }";
  html += "function hidePasswordModal() { document.getElementById('passwordModal').style.display = 'none'; document.getElementById('modalBackdrop').style.display = 'none'; }";
  html += "function submitChatForm() { var pw = document.getElementById('passwordInput').value; if (!pw) return; document.getElementById('passwordField').value = pw; document.getElementById('chatForm').submit(); }";
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
  html += "function toggleLogPassword() {";
  html += "var input = document.getElementById('logPasswordInput');";
  html += "input.type = (input.type === 'password') ? 'text' : 'password';";
  html += "}";
  html += "</script>";

  html += "</body></html>";
  server.send(200, "text/html", html);
}

void handleSetChatId() {
  if (server.hasArg("chatid") && server.hasArg("password")) {
    String pw = server.arg("password");
    if (pw == configPassword) {
      String newId = server.arg("chatid");
      newId.trim();
      chatId = newId;
      prefs.begin("config", false);
      prefs.putString("chat_id", chatId);
      prefs.end();
      logEvent("chat_id aktualisiert: " + chatId);
      Serial.println("chat_id aktualisiert: " + chatId);
    } else {
      logEvent("Falsches Passwort f\u00fcr Chat-ID-Update!");
      Serial.println("Falsches Passwort f\u00fcr Chat-ID-Update!");
    }
  }
  server.sendHeader("Location", "/");
  server.send(303);
}

void handleSimulateContact1() {
  Serial.println("Simulation: Kontakt 1 wurde betätigt!");
  sendTelegramMessage("Simulation: " + customText1);
  logEvent("Simulation: Kontakt 1 ausgelöst");
  server.sendHeader("Location", "/");
  server.send(303);
}

void handleSimulateContact2() {
  Serial.println("Simulation: Kontakt 2 wurde betätigt!");
  sendTelegramMessage("Simulation: " + customText2);
  logEvent("Simulation: Kontakt 2 ausgelöst");
  server.sendHeader("Location", "/");
  server.send(303);
}

void handleSetMessages() {
  if (server.hasArg("msg1") && server.hasArg("msg2")) {
    customText1 = server.arg("msg1");
    customText2 = server.arg("msg2");
    saveCustomTexts();
    logEvent("Nachrichten aktualisiert: " + customText1 + " / " + customText2);
  }
  server.sendHeader("Location", "/");
  server.send(303);
}

void handleClearLog() {
  if (server.hasArg("password")) {
    String pw = server.arg("password");
    if (pw == configPassword) {
      errorLog = "";
      prefs.begin("config", false);
      prefs.remove("elog");
      prefs.end();
      logEvent("ErrorLog gelöscht");
    } else {
      logEvent("Falsches Passwort für Log-Löschung!");
    }
  }
  server.sendHeader("Location", "/");
  server.send(303);
}

bool connectToWiFi() {
  Serial.println("WLAN-Verbindung wird versucht...");
  for (int i = 0; i < wifiCount; i++) {
    Serial.print("Verbinde mit: ");
    Serial.println(knownSSIDs[i]);
    WiFi.begin(knownSSIDs[i], knownPasswords[i]);
    unsigned long startAttemptTime = millis();

    // max. 10 Sekunden versuchen
    while (WiFi.status() != WL_CONNECTED && millis() - startAttemptTime < 10000) {
      delay(500);
      Serial.print(".");
    }

    if (WiFi.status() == WL_CONNECTED) {
      Serial.println("\n✅ Verbunden mit: " + String(knownSSIDs[i]));
      return true;
    } else {
      Serial.println("\n❌ Verbindung fehlgeschlagen mit: " + String(knownSSIDs[i]));
    }
  }
  return false;
}

void setup() {
  Serial.begin(115200);
  loadCustomTexts();
  loadErrorLog();
  pinMode(CONTACT_PIN1, INPUT_PULLUP);
  pinMode(CONTACT_PIN2, INPUT_PULLUP);
  WiFi.setHostname("MuehlenBuddy");
  if (!connectToWiFi()) {
    logEvent("Keine WLAN-Verbindung möglich!");
    Serial.println("❌ WLAN-Verbindung fehlgeschlagen – Neustart in 30 Sekunden");
    delay(30000);  // oder dauerhaft warten / neuen Versuch starten
    ESP.restart();
  }
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("MuehlenBuddy Firmware-Version: " + String(FIRMWARE_VERSION));
  Serial.println("\nVerbunden, IP: " + WiFi.localIP().toString());
  configTzTime("CET-1CEST,M3.5.0,M10.5.0/3", "pool.ntp.org");

  ArduinoOTA.setHostname("MuehlenBuddyOTA");
  ArduinoOTA.setPassword(otaPassword);
  ArduinoOTA.begin();

  server.on("/", handleRoot);
  server.on("/status", handleStatus);
  server.on("/simulateContact1", handleSimulateContact1);
  server.on("/simulateContact2", handleSimulateContact2);
  server.on("/setMessages", handleSetMessages);
  server.on("/setChatId", handleSetChatId);
  server.on("/clearLog", handleClearLog);
  server.begin();
}

void loop() {
  ArduinoOTA.handle();
  server.handleClient();

  int currentState1 = digitalRead(CONTACT_PIN1);
  int currentState2 = digitalRead(CONTACT_PIN2);
  contactStatus1 = (currentState1 == HIGH) ? "Offen" : "Geschlossen";
  contactStatus2 = (currentState2 == HIGH) ? "Offen" : "Geschlossen";

  unsigned long currentMillis = millis();
  if (currentState1 == LOW && lastContactState1 == HIGH && (currentMillis - lastSendTime1 >= debounceInterval)) {
    sendTelegramMessage(customText1);
    logEvent("Kontakt 1 betätigt");
    lastSendTime1 = currentMillis;
  }
  if (currentState2 == LOW && lastContactState2 == HIGH && (currentMillis - lastSendTime2 >= debounceInterval)) {
    sendTelegramMessage(customText2);
    logEvent("Kontakt 2 betätigt");
    lastSendTime2 = currentMillis;
  }

  lastContactState1 = currentState1;
  lastContactState2 = currentState2;
  delay(100);
}
