#include <WiFi.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>

// --- 1. KONFIGURASI WIFI (WAJIB SAMA DENGAN JARINGAN RASPI) ---
const char* ssid = "IoT-Traffic";       // Sesuaikan dengan SSID MikroTik/Router
const char* password = "IoT-Ragoons";   // Sesuaikan dengan Password MikroTik/Router

// --- 2. KONFIGURASI MQTT BROKER (RASPBERRY PI) ---
const char* mqtt_server = "192.168.10.199"; // IP Address Raspberry Pi Anda
const int mqtt_port = 1883;

// --- 3. KONFIGURASI TOPIK ---
const char* topic_sensor = "sensor/node1/fc28";         // Topik kirim data sensor
const char* topic_control = "control/node1/pump";       // Topik terima perintah pompa

// --- 4. KONFIGURASI PIN ---
const int fc28Pin = 32;   // Pin Analog untuk Sensor Kelembapan (ADC1)
const int relayPin = 27;  // Pin Digital untuk Relay (Pompa)

// Relay aktif-LOW (umum untuk modul 1-channel 5V): LOW = ON, HIGH = OFF
// Kalau logika relay kamu terbalik saat dites, tukar saja nilai 2 baris ini
const int RELAY_ON  = LOW;
const int RELAY_OFF = HIGH;

// Threshold ADC FC-28: makin BESAR = makin KERING (WAJIB dikalibrasi sendiri!)
const int THRESHOLD_KERING = 2800;

bool manualOverride = false; // false = mode otomatis, true = dikendalikan manual dari Raspi
bool manualPumpOn = false;

WiFiClient espClient;
PubSubClient client(espClient);

unsigned long lastMsg = 0;
const long interval = 5000; // Kirim data tiap 5 detik

// --- 5. FUNGSI PENERIMA PERINTAH KONTROL ---
void callback(char* topic, byte* payload, unsigned int length) {
  String message = "";
  for (int i = 0; i < length; i++) {
    message += (char)payload[i];
  }
  
  Serial.print("Pesan MQTT masuk dari Raspi: [");
  Serial.print(message);
  Serial.println("]");

  if (message == "ON") {
    manualOverride = true;
    manualPumpOn = true;
    digitalWrite(relayPin, RELAY_ON);
    Serial.println(">>> AKSI: Pompa Dinyalakan (manual) <<<");
  }
  else if (message == "OFF") {
    manualOverride = true;
    manualPumpOn = false;
    digitalWrite(relayPin, RELAY_OFF);
    Serial.println(">>> AKSI: Pompa Dimatikan (manual) <<<");
  }
  else if (message == "AUTO") {
    manualOverride = false;
    Serial.println(">>> AKSI: Kembali ke mode OTOMATIS <<<");
  }
}

// --- 6. SETUP WIFI ---
void setup_wifi() {
  delay(10);
  Serial.print("\nMenghubungkan ke WiFi: ");
  Serial.println(ssid);
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWiFi terhubung!");
  Serial.print("IP Address ESP32: ");
  Serial.println(WiFi.localIP());
}

// --- 7. RECONNECT MQTT ---
void reconnect() {
  while (!client.connected()) {
    Serial.print("Menghubungkan ke MQTT Raspi (");
    Serial.print(mqtt_server);
    Serial.print(")...");
    String clientId = "ESP32_Node1_";
    clientId += String(random(0xffff), HEX);
    
    // Connect ke Raspi
    if (client.connect(clientId.c_str())) {
      Serial.println("Berhasil!");
      client.subscribe(topic_control); // Wajib subscribe agar bisa dikontrol Raspi
    } else {
      Serial.print("Gagal, status=");
      Serial.print(client.state());
      Serial.println(" Coba lagi 5 detik...");
      delay(5000);
    }
  }
}

// --- 8. SETUP UTAMA ---
void setup() {
  Serial.begin(115200);
  
  // Relay selalu OUTPUT, default mati saat pertama kali dicolok
  pinMode(relayPin, OUTPUT);
  digitalWrite(relayPin, RELAY_OFF);

  setup_wifi();
  client.setServer(mqtt_server, mqtt_port);
  client.setCallback(callback); 
}

// --- 9. LOOP UTAMA ---
void loop() {
  if (!client.connected()) {
    reconnect();
  }
  client.loop(); 

  unsigned long now = millis();
  if (now - lastMsg >= interval) {
    lastMsg = now;
    
    // Baca Sensor
    int moistureValue = analogRead(fc28Pin);
    bool isDry = moistureValue > THRESHOLD_KERING;

    // Kontrol pompa: otomatis, kecuali sedang di-override manual dari Raspi
    if (manualOverride) {
      digitalWrite(relayPin, manualPumpOn ? RELAY_ON : RELAY_OFF);
    } else {
      digitalWrite(relayPin, isDry ? RELAY_ON : RELAY_OFF);
    }

    // Format JSON
    StaticJsonDocument<200> doc;
    doc["device_id"] = "node_01_mqtt";
    doc["protocol"] = "MQTT";
    doc["sensor_value"] = moistureValue;
    doc["is_dry"] = isDry;
    doc["pump_mode"] = manualOverride ? "manual" : "auto";

    String payload;
    serializeJson(doc, payload);
    
    // Publish ke Raspi
    Serial.print("Mengirim data ke Raspi: ");
    Serial.println(payload);
    client.publish(topic_sensor, payload.c_str());
  }
}