#include <WiFi.h>
#include <WebServer.h>
#include <SPI.h>
#include <SD.h>
#include "PRESENT80.h"

const char* WIFI_SSID = "Secure_HID_Lab";
const char* WIFI_PASSWORD = "LabOnly_2026!";
WebServer server(443);

static const int SD_CS_PIN = 5;
static const int SD_SCK_PIN = 18;
static const int SD_MISO_PIN = 19;
static const int SD_MOSI_PIN = 23;

static const int UART_RX_PIN = 16;
static const int UART_TX_PIN = 17;
static const uint32_t UART_BAUD = 9600;

const bool USE_ACK = true;

uint8_t payloadKey[10] = { 0x12, 0x34, 0x56, 0x78, 0x9A, 0xBC, 0xDE, 0xF0, 0x13, 0x57 };
PRESENT80 present(payloadKey);

bool sdOK = false;
bool presentOK = false;
String lastStatus = "System Ready";

String u64ToHex(uint64_t value) {
char out[17];
snprintf(out, sizeof(out), "%08lX%08lX", (unsigned long)(uint32_t)(value >> 32), (unsigned long)(uint32_t)value);
return String(out);
}

int hexValue(char c) {
if (c >= '0' && c <= '9') return c - '0';
if (c >= 'A' && c <= 'F') return c - 'A' + 10;
if (c >= 'a' && c <= 'f') return c - 'a' + 10;
return -1;
}

bool hexToU64(const String& hex, uint64_t& result) {
if (hex.length() != 16) return false;
result = 0;
for (int i = 0; i < 16; i++) {
const int v = hexValue(hex[i]);
if (v < 0) return false;
result = (result << 4) | (uint64_t)v;
}
return true;
}

bool runPresentSelfTest() {
uint8_t zeroKey[10] = {0,0,0,0,0,0,0,0,0,0};
PRESENT80 tester(zeroKey);
const uint64_t pt = 0x0000000000000000ULL;
const uint64_t expected = 0x5579C1387B228445ULL;
const uint64_t ct = tester.encryptBlock(pt);
const uint64_t recovered = tester.decryptBlock(ct);
return (ct == expected && recovered == pt);
}

String encryptCommandLine(const String& plain) {
const size_t len = plain.length();
uint8_t pad = 8 - (len % 8);
if (pad == 0) pad = 8;
const size_t total = len + pad;
String result = "ENC:";

for (size_t offset = 0; offset < total; offset += 8) {
uint64_t block = 0;
for (size_t j = 0; j < 8; j++) {
const size_t idx = offset + j;
uint8_t b = (idx < len) ? (uint8_t)plain[idx] : pad;
block = (block << 8) | b;
}
result += u64ToHex(present.encryptBlock(block));
}
return result;
}

bool decryptCommandLine(const String& encoded, String& plainOut) {
plainOut = "";
if (!encoded.startsWith("ENC:")) return false;
const String hex = encoded.substring(4);
if (hex.length() == 0 || (hex.length() % 16) != 0) return false;

const size_t blockCount = hex.length() / 16;
uint8_t buffer[256];
size_t outLen = 0;

for (size_t b = 0; b < blockCount; b++) {
uint64_t cipherBlock = 0;
if (!hexToU64(hex.substring(b * 16, b * 16 + 16), cipherBlock)) return false;
const uint64_t plainBlock = present.decryptBlock(cipherBlock);
for (int j = 7; j >= 0; j--) {
buffer[outLen++] = (uint8_t)((plainBlock >> (j * 8)) & 0xFF);
}
}

const uint8_t pad = buffer[outLen - 1];
if (pad < 1 || pad > 8 || pad > outLen) return false;
outLen -= pad;

for (size_t i = 0; i < outLen; i++) plainOut += (char)buffer[i];
return true;
}

void appendToSDHistory(const String& encPayload) {
if (!sdOK) return;
File f = SD.open("/history.enc", FILE_APPEND);
if (f) {
f.println(encPayload);
f.close();
}
}

bool sendToProMicro(const String& cmd) {
Serial2.println(cmd);
Serial.print("UART Out -> "); Serial.println(cmd);

if (!USE_ACK) {
lastStatus = "Sent: " + cmd;
return true;
}

const uint32_t start = millis();
while (millis() - start < 1000) {
if (Serial2.available()) {
String reply = Serial2.readStringUntil('\n');
reply.trim();
if (reply == "ACK") {
  lastStatus = "ACK Confirmed";
  return true;
}
if (reply == "NACK") {
  lastStatus = "NACK: Rejected";
  return false;
}
}
}
lastStatus = "ACK Timeout";
return false;
}

void handleRoot() {
String html = "<!DOCTYPE html><html lang='en'><head>";
html += "<meta charset='UTF-8'><meta name='viewport' content='width=device-width,initial-scale=1.0'>";
html += "<title>PRESENT-80 Wireless HID</title>";
html += "<style>";
html += "* { box-sizing: border-box; margin: 0; padding: 0; font-family: 'Segoe UI', Tahoma, Geneva, Verdana, sans-serif; }";
html += "body { background: #0f172a; color: #f8fafc; padding: 20px; display: flex; justify-content: center; }";
html += ".container { width: 100%; max-width: 600px; }";
html += ".header { text-align: center; padding-bottom: 20px; border-bottom: 1px solid #1e293b; margin-bottom: 20px; }";
html += ".header h1 { font-size: 22px; color: #38bdf8; letter-spacing: 0.5px; }";
html += ".header p { font-size: 13px; color: #94a3b8; margin-top: 4px; }";
html += ".card { background: #1e293b; border-radius: 12px; padding: 18px; margin-bottom: 16px; border: 1px solid #334155; box-shadow: 0 4px 6px -1px rgba(0, 0, 0, 0.3); }";
html += ".card-title { font-size: 15px; font-weight: 600; color: #cbd5e1; margin-bottom: 12px; display: flex; align-items: center; justify-content: space-between; }";
html += ".status-grid { display: grid; grid-template-columns: 1fr 1fr; gap: 10px; font-size: 13px; }";
html += ".status-item { background: #0f172a; padding: 10px; border-radius: 8px; border: 1px solid #334155; }";
html += ".badge-ok { color: #4ade80; font-weight: bold; } .badge-bad { color: #f87171; font-weight: bold; }";
html += ".btn-group { display: grid; grid-template-columns: repeat(auto-fit, minmax(130px, 1fr)); gap: 10px; }";
html += ".btn { width: 100%; padding: 12px; border: none; border-radius: 8px; font-size: 14px; font-weight: 600; cursor: pointer; transition: all 0.2s ease; color: #fff; }";
html += ".btn-primary { background: #2563eb; } .btn-primary:hover { background: #1d4ed8; }";
html += ".btn-warning { background: #d97706; } .btn-warning:hover { background: #b45309; }";
html += ".btn-danger { background: #dc2626; } .btn-danger:hover { background: #b91c1c; }";
html += ".btn-success { background: #16a34a; } .btn-success:hover { background: #15803d; }";
html += "textarea { width: 100%; padding: 12px; border-radius: 8px; border: 1px solid #334155; background: #0f172a; color: #38bdf8; font-family: monospace; font-size: 14px; resize: vertical; margin-bottom: 12px; outline: none; }";
html += "textarea:focus { border-color: #38bdf8; box-shadow: 0 0 0 2px rgba(56, 189, 248, 0.2); }";
html += ".checkbox-container { display: flex; align-items: center; gap: 8px; font-size: 13px; color: #94a3b8; margin-bottom: 12px; }";
html += "input[type=checkbox] { accent-color: #38bdf8; width: 16px; height: 16px; cursor: pointer; }";
html += "</style></head><body>";

html += "<div class='container'>";
html += "<div class='header'><h1>🔐 Wireless HID Injector</h1><p>PRESENT-80 Encrypted Control Center</p></div>";

html += "<div class='card'><div class='card-title'>System Diagnostics</div>";
html += "<div class='status-grid'>";
html += "<div class='status-item'>PRESENT-80: " + String(presentOK ? "<span class='badge-ok'>PASS</span>" : "<span class='badge-bad'>FAIL</span>") + "</div>";
html += "<div class='status-item'>SD Storage: " + String(sdOK ? "<span class='badge-ok'>Mounted</span>" : "<span class='badge-bad'>N/A</span>") + "</div>";
html += "</div>";
html += "<div class='status-item' style='margin-top:10px;'>Status Log: <span style='color:#38bdf8;'>" + lastStatus + "</span></div>";
html += "</div>";

// إضافة زر Admin CMD
html += "<div class='card'><div class='card-title'>Quick Controller Actions</div>";
html += "<div class='btn-group'>";
html += "<form action='/exec_enc' method='POST'><input type='hidden' name='payload' value='" + encryptCommandLine("OPEN_RUN") + "'><button class='btn btn-warning'>⚡ Win + R</button></form>";
html += "<form action='/exec_enc' method='POST'><input type='hidden' name='payload' value='" + encryptCommandLine("OPEN_ADMIN_CMD") + "'><button class='btn btn-danger'>🛡️ Admin CMD</button></form>";
html += "<form action='/exec_enc' method='POST'><input type='hidden' name='payload' value='" + encryptCommandLine("CLEAR_TEXT") + "'><button class='btn btn-danger'>🧹 Clear Text</button></form>";
html += "<form action='/exec_enc' method='POST'><input type='hidden' name='payload' value='" + encryptCommandLine("ENTER") + "'><button class='btn btn-success'>↵ Enter Key</button></form>";
html += "</div></div>";

html += "<div class='card'><div class='card-title'>Dynamic Payload Injector</div>";
html += "<form action='/exec_custom' method='POST'>";
html += "<textarea name='custom_text' rows='3' placeholder='Type command, URL, or script here...' required></textarea>";
html += "<div class='checkbox-container'>";
html += "<input type='checkbox' id='enter_check' name='append_enter' value='true' checked>";
html += "<label for='enter_check'>Auto press Enter after sending</label>";
html += "</div>";
html += "<button class='btn btn-primary' type='submit'>🔐 Encrypt (PRESENT-80) & Inject</button>";
html += "</form></div>";

html += "</div></body></html>";
server.send(200, "text/html", html);
}

void handleExecEncrypted() {
if (server.hasArg("payload")) {
String encPayload = server.arg("payload");
appendToSDHistory(encPayload);
String plainCmd;
if (decryptCommandLine(encPayload, plainCmd)) {
sendToProMicro(plainCmd);
} else {
lastStatus = "Decryption Failed";
}
}
server.sendHeader("Location", "/");
server.send(303);
}

void handleExecCustom() {
if (server.hasArg("custom_text")) {
String text = server.arg("custom_text");
bool addEnter = server.hasArg("append_enter");

text.trim();
if (text.length() > 0) {
String fullCmd = "STRING " + text;
String encPayload = encryptCommandLine(fullCmd);
appendToSDHistory(encPayload);

String plainCmd;
if (decryptCommandLine(encPayload, plainCmd)) {
  sendToProMicro(plainCmd);
  
  if (addEnter) {
    delay(1);
    sendToProMicro("ENTER");
  }
}
}
}
server.sendHeader("Location", "/");
server.send(303);
}

void setup() {
Serial.begin(115200);
Serial2.begin(UART_BAUD, SERIAL_8N1, UART_RX_PIN, UART_TX_PIN);

presentOK = runPresentSelfTest();
SPI.begin(SD_SCK_PIN, SD_MISO_PIN, SD_MOSI_PIN, SD_CS_PIN);
sdOK = SD.begin(SD_CS_PIN, SPI);

WiFi.mode(WIFI_AP);
WiFi.softAP(WIFI_SSID, WIFI_PASSWORD);

server.on("/", HTTP_GET, handleRoot);
server.on("/exec_enc", HTTP_POST, handleExecEncrypted);
server.on("/exec_custom", HTTP_POST, handleExecCustom);
server.begin();
}

void loop() {
server.handleClient();
}