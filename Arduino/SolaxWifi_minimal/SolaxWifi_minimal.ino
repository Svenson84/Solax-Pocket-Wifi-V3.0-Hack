// This software provides an alternative firmware for the original Solax Pocket Wifi V3.0 dongle.
// This is a minimalistic example which can be used as basis for your own application.
// This example provides a local webinterface via a wifi connection.
// The hardware of the Solax Pocket Wifi V3.0 uses an ESP32-S2-WROOM.
// The physical connection to the Solax X1 mini is given by a USB3.0 plug, but it does not use USB protocoll. Instead an UART protocol is used.
// Due to this any compatible or similar ESP might also work (not tested), minor adaptions might be necessary. 
// The UART protocoll is similar - but not identical to the RS485 protocol.
//
// It does NOT use the Solax cloud connection or Solax API! 
// It does NOT require any internet connection!
// 
// This code uses, and/or is partly based on, and/or is inspired by the following projects:
// https://github.com/xdubx/Solax-Pocket-USB-reverse-engineering
// https://github.com/JensJordan/solaXd
// https://github.com/chartjs/Chart.js
//
// Disclaimer:
// Feel free to use this code on your own risk ;)
//
// Referenced documents:
// REF{1} https://github.com/syssi/esphome-modbus-solax-x1/blob/main/docs/SolaxPower_Single_Phase_External_Communication_Protocol_-_X1_Series_V1.2.pdf
// REF{2} https://www.solaxpower.com/wp-content/uploads/2017/01/X1-Mini-Install-Manual.pdf

#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <WiFiManager.h>
#include <ArduinoOTA.h>

#include "Solax_USB.h"

#define LED_PIN                       3     // ESP32-S2 IO3  --> Green LED
#define USB_UART_RX_PIN               18    // ESP32-S2 IO18 --> UART 1, RX-Pin
#define USB_UART_TX_PIN               17    // ESP32-S2 IO17 --> UART 1, TX-Pin
#define WIFI_RESET_PIN                0     // ESP32-S2 IO0  --> (optional) add jumper to GND to reset wifi credentials

#define WIFI_HOSTNAME                 "ESP32S2"
#define WIFI_CONFIG_SSID              "ESP32S2-Setup"

Solax_USB Inv_USB(USB_UART_RX_PIN, USB_UART_TX_PIN);
WiFiManager wifiManager;
WebServer WebServer(80);
WiFiClient WifiClient;

bool WifiReset = false;
bool EspReboot = false;

uint32_t Now = 0;
uint32_t UptimeSeconds = 0;

int32_t WiFiRSSI = 0;
int32_t WiFiReconnects = 0;


void StatusLED(uint8_t on) {
  if(on) {
    digitalWrite(LED_PIN, LOW);
  }
  else {
    digitalWrite(LED_PIN, HIGH);
  }
}


void UptimeUpdate() {
  static uint16_t DeltaMillis = 0;
  static uint32_t LastUpdate = 0;

  Now = millis();
  DeltaMillis += (Now - LastUpdate);
  LastUpdate = Now;
  
  while(DeltaMillis >= 1000) {
    UptimeSeconds++;
    DeltaMillis -= 1000;
  }
}


void CheckWifiStatus() {
  bool res;

  // Check if reset of wifi data is requested by pulling WIFI_RESET_PIN to GND
  if(digitalRead(WIFI_RESET_PIN) == 0) {
    WifiReset = true;
  }
  else if (WiFi.status() != WL_CONNECTED) {
    // WiFi connection lost... try to reconnect!
    res = wifiManager.autoConnect(WIFI_CONFIG_SSID);
    if(!res) {
      StatusLED(false);
    } 
    else {
      StatusLED(true);
      WiFiReconnects++;
    }
  }
  else {
	  WiFiRSSI = WiFi.RSSI();
  }
}


void SendHtml() {
  String html = "<html>\n\t<head>\n\t\t<title>Solax Pocket Wifi V3.0</title>\n\t\t<meta http-equiv=\"refresh\" content=\"2\">\n\t</head>\n\t<body>\n\t\t<table>";
  html += "<tr><td>Grid Voltage</td><td align=\"right\">" + String(Inv_USB.Data.GridVoltage / 10.0) + "</td><td>V</td></tr>";
  html += "<tr><td>Output Current</td><td align=\"right\">" + String(Inv_USB.Data.OutputCurrent / 10.0) + "</td><td>A</td></tr>";
  html += "<tr><td>Output Power</td><td align=\"right\">" + String(Inv_USB.Data.OutputPower) + "</td><td>W</td></tr>";
  html += "<tr><td>PV1 Voltage</td><td align=\"right\">" + String(Inv_USB.Data.PV1Voltage / 10.0) + "</td><td>V</td></tr>";
  html += "<tr><td>PV2 Voltage</td><td align=\"right\">" + String(Inv_USB.Data.PV2Voltage / 10.0) + "</td><td>V</td></tr>";
  html += "<tr><td>PV1 Current</td><td align=\"right\">" + String(Inv_USB.Data.PV1Current / 10.0) + "</td><td>A</td></tr>";
  html += "<tr><td>PV2 Current</td><td align=\"right\">" + String(Inv_USB.Data.PV2Current / 10.0) + "</td><td>A</td></tr>";
  html += "<tr><td>PV1 Power</td><td align=\"right\">" + String(Inv_USB.Data.PV1Power) + "</td><td>W</td></tr>";
  html += "<tr><td>PV2 Power</td><td align=\"right\">" + String(Inv_USB.Data.PV2Power) + "</td><td>W</td></tr>";
  html += "<tr><td>Grid Frequency</td><td align=\"right\">" + String(Inv_USB.Data.GridFrequency / 100.0) + "</td><td>Hz</td></tr>";
  html += "<tr><td>Mode</td><td>";
    switch(Inv_USB.Data.InverterMode) { 
    case 0: html += "(0) Waiting"; break;
    case 1: html += "(1) Checking"; break;
    case 2: html += "(2) On-Grid"; break;
    case 3: html += "(3) Fault"; break;
    default: html += "Unknown"; break;
  }
  html += "</td><td>&nbsp;</td></tr>";
  html += "<tr><td>Energy Total</td><td align=\"right\">" + String(Inv_USB.Data.EnergyTotal / 10.0) + "</td><td>kWh</td></tr>";
  html += "<tr><td>Energy Today</td><td align=\"right\">" + String(Inv_USB.Data.EnergyToday / 10.0) + "</td><td>kWh</td></tr>";
  html += "<tr><td>Temperature</td><td align=\"right\">" + String(Inv_USB.Data.Temperature) + "</td><td>&#8451;</td></tr>";
  html += "<tr><td>Runtime Total</td><td align=\"right\">" + String(Inv_USB.Data.RuntimeTotal) + "</td><td>h</td></tr>";
  html += "<tr><td>Runtime ESP</td><td align=\"right\">" + String(UptimeSeconds) + "</td><td>s</td></tr>";
  html += "<tr><td>WiFi SSID</td><td align=\"right\">" + String(WiFi.SSID()) + "</td><td>&nbsp;</td></tr>";
  html += "<tr><td>WiFi IP</td><td align=\"right\">" + WiFi.localIP().toString() + "</td><td>&nbsp;</td></tr>";
  html += "<tr><td>WiFi RSSI</td><td align=\"right\">" + String(WiFiRSSI) + "</td><td>dBm</td></tr>";
  html += "<tr><td>WiFi Reconnects</td><td align=\"right\">" + String(WiFiReconnects) + "</td><td>&nbsp;</td></tr>";
  html += "\n\t\t</table>\n\t</body>\n</html>";

  WebServer.send(200, "text/html", html);
}


void SendJson() {
  String json = "{\n";
  json += "  \"GridVoltage\": " + String(Inv_USB.Data.GridVoltage / 10.0) + ",\n";
  json += "  \"OutputCurrent\": " + String(Inv_USB.Data.OutputCurrent / 10.0) + ",\n";
  json += "  \"OutputPower\": " + String(Inv_USB.Data.OutputPower) + ",\n";
  json += "  \"PV1Voltage\": " + String(Inv_USB.Data.PV1Voltage / 10.0) + ",\n";
  json += "  \"PV2Voltage\": " + String(Inv_USB.Data.PV2Voltage / 10.0) + ",\n";
  json += "  \"PV1Current\": " + String(Inv_USB.Data.PV1Current / 10.0) + ",\n";
  json += "  \"PV2Current\": " + String(Inv_USB.Data.PV2Current / 10.0) + ",\n";
  json += "  \"PV1Power\": " + String(Inv_USB.Data.PV1Power / 10.0) + ",\n";
  json += "  \"PV2Power\": " + String(Inv_USB.Data.PV2Power / 10.0) + ",\n";
  json += "  \"GridFrequency\": " + String(Inv_USB.Data.GridFrequency / 100.0) + ",\n";
  json += "  \"Mode\": " + String(Inv_USB.Data.InverterMode) + ",\n";
  json += "  \"ModeName\": \"";
  switch(Inv_USB.Data.InverterMode) { 
    case 0: json += "Waiting"; break;
    case 1: json += "Checking"; break;
    case 2: json += "On-Grid"; break;
    case 3: json += "Fault"; break;
    default: json += "Unknown"; break;
  }
  json += "\",\n";
  json += "  \"EnergyTotal\": " + String(Inv_USB.Data.EnergyTotal / 10.0) + ",\n";
  json += "  \"EnergyToday\": " + String(Inv_USB.Data.EnergyToday / 10.0) + ",\n";
  json += "  \"Temperature\": " + String(Inv_USB.Data.Temperature) + ",\n";
  json += "  \"RuntimeTotal\": " + String(Inv_USB.Data.RuntimeTotal) + ",\n";
  json += "  \"RuntimeEsp\": " + String(UptimeSeconds) + ",\n";
  json += "  \"WifiSSID\": \"" + String(WiFi.SSID()) + "\",\n";
  json += "  \"WifiIP\": \"" + WiFi.localIP().toString() + "\",\n";
  json += "  \"WifiRSSI\": " + String(WiFiRSSI) + ",\n";
  json += "  \"WifiReconnects\": " + String(WiFiReconnects) + "\n";
  json += "}";

  WebServer.send(200, "application/json", json);
}


void handleRoot(){
  WebServer.sendHeader("Location", "/index.html", true);
  WebServer.send(302, "text/plain","");
}


void handleWebRequests(){
  String uri = WebServer.uri();

  if(uri.startsWith("/ClearWiFi")) {
    WifiReset = true;
  }
  else if(uri.startsWith("/Reboot")) {
    EspReboot = true;
  }
  else if(uri.startsWith("/info.html")) {
    SendHtml();
  }
  else if(uri.startsWith("/info.json")) {
    SendJson();
  }
  else {
    String message = "<html>Please choose:<br><a href=\"info.html\">info.html</a><br><a href=\"info.json\">info.json</a></html>";
    WebServer.send(404, "text/html", message);
  }
}


void setup() { 
  // Configure GPIO pin for status LED
  pinMode(LED_PIN, OUTPUT);
  StatusLED(false);

  // Configure Debug Serial Interface (UART0)
  Serial.begin(115200, SERIAL_8N1);
  Serial.println("Booting...");

  Serial.print("Initialize Inverter... ");
  Inv_USB.begin();
  Serial.println("[OK]");
  
  // Configure GPIO pin for wifi reset input
  pinMode(WIFI_RESET_PIN, INPUT_PULLUP);

  Serial.print("Initialize WifiManager... ");
  //WiFi.mode(WIFI_STA); // explicitly set mode, esp defaults to STA+AP
  wifiManager.setDebugOutput(false);
  wifiManager.setHostname(WIFI_HOSTNAME);
	wifiManager.setEnableConfigPortal(true);
  wifiManager.setWiFiAutoReconnect(false);
	wifiManager.setCleanConnect(true);
	wifiManager.setConnectRetries(10);
	wifiManager.setConnectTimeout(6);

  bool res;
  res = wifiManager.autoConnect(WIFI_CONFIG_SSID);
  if(!res) {
    // Failed to connect... reboot!
    ESP.restart();
  }
  
  // WiFi connected!
  StatusLED(true);
  Serial.println("[OK]");
  
  Serial.print("Initialize ArduinoOTA... ");
  ArduinoOTA.setHostname(WIFI_HOSTNAME);
  ArduinoOTA.begin();
  Serial.println("[OK]");
  
  Serial.print("Initialize Webserver... ");
  WebServer.on("/", handleRoot);
  WebServer.onNotFound(handleWebRequests);
  WebServer.begin();
  Serial.println("[OK]");
  
  Serial.println("Start loop()");
} 


void loop() {
  if(WifiReset) {
    wifiManager.resetSettings();
    wifiManager.reboot();
  }

  if(EspReboot) {
    ESP.restart();
  }

  ArduinoOTA.handle();
  UptimeUpdate();
  CheckWifiStatus();
  Inv_USB.loop();
  WebServer.handleClient();
}
