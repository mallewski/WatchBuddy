#pragma once

//WLAN-Zugänge
const char* knownSSIDs[] = {
  "**First WLAN SSID**", 
  "**Frist WLAN Password**"
  //add more if u wish
};
const char* knownPasswords[] = {
  "**Second WLAN SSID**", 
  "**Second WLAN Password**"
  //add more if u wish
};
const int wifiCount = sizeof(knownSSIDs) / sizeof(knownSSIDs[0]); //ermittelt dynamisch die anzahl verfügbarer WLANs

//OTA-Password
const char* otaPassword = "**OTA-Update-Password**";

//Config-Password
const String configPassword = "**Config-Password**";

//Telegram-Verbindungsdaten
const String botToken = "**Telegram-API-Token**";
const String defaultChatId = "**Telegram-ChatID**";