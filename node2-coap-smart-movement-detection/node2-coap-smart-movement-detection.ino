#include <WiFi.h>
#include <WiFiUdp.h>
#include <coap-simple.h>

// --- KONFIGURASI JARINGAN MIKROTIK ---
const char* ssid = "IoT-Traffic";    // Ganti dengan SSID MikroTik Anda
const char* password = "IoT-Ragoons"; // Ganti dengan Password MikroTik Anda

// --- KONFIGURASI IP RASPBERRY PI ---
// Harus sama dengan IP Raspi yang dipakai node MQTT & HTTP (192.168.10.199)
IPAddress serverIP(192, 168, 10, 199); 

// --- KONFIGURASI PIN ---
#define PIR_PIN 13
#define LED_PIN 27

WiFiUDP udp;
Coap coap(udp);

// Fungsi callback saat ada balasan dari server CoAP
void callback_response(CoapPacket &packet, IPAddress ip, int port) {
  Serial.println("Response CoAP diterima dari server Raspberry Pi");
}

void setup() {
  Serial.begin(115200);
  
  pinMode(PIR_PIN, INPUT);
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, LOW); // Pastikan LED mati di awal
  
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

  // Memulai layanan CoAP
  coap.response(callback_response);
  coap.start();
}

void loop() {
  coap.loop(); // Wajib dipanggil agar CoAP bisa berjalan di background

  // 1. Baca Sensor PIR
  int motion = digitalRead(PIR_PIN);

  // 2. Aktuasi Lokal & Siapkan Payload
  String motionStatus;
  if (motion == HIGH) {
    digitalWrite(LED_PIN, HIGH);
    motionStatus = "MOTION_DETECTED";
    Serial.println("Gerakan Terdeteksi -> LED Nyala, mengirim CoAP...");
  } else {
    digitalWrite(LED_PIN, LOW);
    motionStatus = "NO_MOTION";
    Serial.println("Aman -> LED Mati, mengirim CoAP...");
  }

  // Payload sederhana berisi metadata, selaras dengan format node MQTT & HTTP
  String payload = "{\"device_id\":\"node_03_coap\",\"protocol\":\"CoAP\",\"motion_status\":\"" + motionStatus + "\"}";

  // 3. Kirim data ke Raspberry Pi via CoAP PUT
  if (WiFi.status() == WL_CONNECTED) {
    coap.put(serverIP, 5683, "motion", payload.c_str());
  } else {
    Serial.println("Koneksi WiFi terputus, mencoba menyambung ulang...");
    WiFi.disconnect();
    WiFi.begin(ssid, password);
  }

  // Jeda 5 detik (baseline normal pengiriman data)
  delay(5000); 
}