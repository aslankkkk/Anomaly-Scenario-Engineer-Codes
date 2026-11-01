#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>

// --- KONFIGURASI JARINGAN MIKROTIK ---
const char* ssid = "IoT-Traffic";    // Ganti dengan SSID MikroTik Anda
const char* password = "IoT-Ragoons"; // Ganti dengan Password MikroTik Anda

// --- KONFIGURASI ENDPOINT RASPBERRY PI ---
// Ganti IP_RASPI dengan IP Address Raspberry Pi Anda yang terhubung ke MikroTik
const char* serverName = "http://192.168.10.199:80/api/sensor/http"; 

// --- KONFIGURASI PIN & THRESHOLD ---
const int mq2Pin = 34;    
const int buzzerPin = 25; 
const int gasThreshold = 2500; // Batas nilai anomali gas

void setup() {
  Serial.begin(115200);
  
  pinMode(buzzerPin, OUTPUT);
  digitalWrite(buzzerPin, LOW); // Pastikan buzzer mati di awal
  
  // Memulai koneksi WiFi
  Serial.print("\nMenghubungkan ke WiFi ");
  Serial.print(ssid);
  WiFi.begin(ssid, password);
  
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  
  Serial.println("\nWiFi terhubung!");
  Serial.print("IP Address ESP32: ");
  Serial.println(WiFi.localIP());
}

void loop() {
  // 1. Baca Sensor Gas MQ-2
  int gasLevel = analogRead(mq2Pin);
  Serial.print("Nilai Gas: ");
  Serial.print(gasLevel);

  // 2. Deteksi Alarm Gas (fungsi fisik device) & Aktuasi Lokal
  // Catatan: ini status alarm GAS, bukan status "anomali jaringan".
  // Anomali jaringan (connection burst, dll) datang dari skrip attacker terpisah,
  // bukan dari nilai sensor ini - lihat diskusi labeling dataset.
  bool gasAlert = false;
  if (gasLevel > gasThreshold) {
    digitalWrite(buzzerPin, HIGH); // Buzzer Nyala
    gasAlert = true;
    Serial.println(" | ALARM! Gas Bocor -> Buzzer ON");
  } else {
    digitalWrite(buzzerPin, LOW);  // Buzzer Mati
    Serial.println(" | Normal -> Buzzer OFF");
  }

  // 3. Kirim data ke Raspberry Pi via HTTP POST
  if(WiFi.status() == WL_CONNECTED) {
    HTTPClient http;
    http.begin(serverName);
    http.addHeader("Content-Type", "application/json");

    // Format payload JSON
    StaticJsonDocument<200> doc;
    doc["device_id"] = "node_02_http";
    doc["protocol"] = "HTTP";
    doc["sensor"] = "MQ2";
    doc["value"] = gasLevel;
    doc["gas_alert"] = gasAlert; // status alarm fisik, terpisah dari label anomali jaringan

    String requestBody;
    serializeJson(doc, requestBody);

    // Eksekusi HTTP POST
    int httpResponseCode = http.POST(requestBody);

    if (httpResponseCode > 0) {
      Serial.print("   Berhasil kirim HTTP (Code: ");
      Serial.print(httpResponseCode);
      Serial.println(")");
    } else {
      Serial.print("   Gagal kirim HTTP (Error: ");
      Serial.print(httpResponseCode);
      Serial.println(")");
    }
    
    http.end(); 
  } else {
    Serial.println("   Koneksi WiFi terputus, mencoba menyambung ulang...");
    WiFi.disconnect();
    WiFi.begin(ssid, password);
  }

  // Jeda 5 detik
  delay(5000); 
}