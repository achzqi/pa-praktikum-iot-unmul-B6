#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <PubSubClient.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include "HX711.h"
#include "time.h"

// =================================================
// WIFI
// =================================================

const char* ssid = "Zidan Zaida";
const char* password = "PerkedelJagung1122";

// =================================================
// FIREBASE
// =================================================

String firebaseURL =
    "https://smart-coop-pa-default-rtdb.asia-southeast1.firebasedatabase.app/jadwal_pakan.json";

// =================================================
// MQTT
// =================================================

const char* mqtt_server =
    "8f6cdd34c17f453ea802a2a3e90fa3e0.s1.eu.hivemq.cloud";

const int mqtt_port = 8883;

const char* mqtt_user = "SmartCoop";
const char* mqtt_pass = "PAIoT2026";

WiFiClientSecure espClient;
PubSubClient client(espClient);

// =================================================
// TIMER
// =================================================

unsigned long lastFirebase = 0;
unsigned long lastPublish = 0;

// =================================================
// NTP
// =================================================

const char* ntpServer = "pool.ntp.org";

const long gmtOffset_sec = 8 * 3600;
const int daylightOffset_sec = 0;

// =================================================
// HX711
// =================================================

#define DT_PIN 15
#define SCK_PIN 16

HX711 scale;

float calibration_factor = -215.71;
float containerWeight = 1288.58;

// =================================================
// MOTOR
// =================================================

// Motor keluar pakan
#define M1_IN1 17
#define M1_IN2 18

// Motor distribusi
#define M2_IN1 7
#define M2_IN2 8

// =================================================
// JADWAL
// =================================================

String jadwalJam1 = "";
String jadwalJam2 = "";

int jadwalGram1 = 0;
int jadwalGram2 = 0;

bool sudahJalan1 = false;
bool sudahJalan2 = false;

// =================================================
// MOTOR CONTROL
// =================================================

void motor1Maju() {

    digitalWrite(M1_IN1, HIGH);
    digitalWrite(M1_IN2, LOW);
}

void motor1Stop() {

    digitalWrite(M1_IN1, LOW);
    digitalWrite(M1_IN2, LOW);
}

void motor2Maju() {

    digitalWrite(M2_IN1, HIGH);
    digitalWrite(M2_IN2, LOW);
}

void motor2Mundur() {

    digitalWrite(M2_IN1, LOW);
    digitalWrite(M2_IN2, HIGH);
}

void motor2Stop() {

    digitalWrite(M2_IN1, LOW);
    digitalWrite(M2_IN2, LOW);
}

// =================================================
// BACA BERAT
// =================================================

float bacaBerat() {

    float total =
        scale.get_units(10);

    float berat =
        total - containerWeight;

    if (berat < 20) {
        berat = 0;
    }

    return berat;
}

// =================================================
// MQTT
// =================================================

void reconnectMQTT() {

    while (!client.connected()) {

        String clientId =
            "ESP32-" +
            String(random(0xffff), HEX);

        if (
            client.connect(
                clientId.c_str(),
                mqtt_user,
                mqtt_pass
            )
        ) {

            Serial.println(
                "MQTT Connected"
            );

        } else {

            Serial.println(
                "MQTT Retry..."
            );

            delay(3000);
        }
    }
}

void kirimLoadcell(float feedWeight) {

    StaticJsonDocument<128> doc;

    doc["feed_weight"] =
        feedWeight;

    char buffer[128];

    serializeJson(
        doc,
        buffer
    );

    bool result =
        client.publish(
            "kandangayam/loadcell",
            buffer
        );

    Serial.print("Publish: ");
    Serial.println(buffer);

    Serial.println(
        result
            ? "BERHASIL"
            : "GAGAL"
    );
}

// =================================================
// FIREBASE
// =================================================

void ambilFirebase() {

    HTTPClient http;

    http.begin(firebaseURL);

    int httpCode =
        http.GET();

    if (httpCode == 200) {

        String payload =
            http.getString();

        DynamicJsonDocument doc(1024);

        deserializeJson(
            doc,
            payload
        );

        jadwalJam1 =
            doc["jadwal1"]["jam"]
                .as<String>();

        jadwalGram1 =
            doc["jadwal1"]["gram"];

        jadwalJam2 =
            doc["jadwal2"]["jam"]
                .as<String>();

        jadwalGram2 =
            doc["jadwal2"]["gram"];

        Serial.println(
            "Firebase Updated"
        );
    }

    http.end();
}

// =================================================
// DISTRIBUSI PAKAN
// =================================================

void distribusiPakan() {

    Serial.println(
        "Distribusi mulai"
    );

    for (int i = 0; i < 5; i++) {

        motor2Maju();
        delay(1000);

        motor2Stop();
        delay(300);

        motor2Mundur();
        delay(1000);

        motor2Stop();
        delay(300);
    }

    Serial.println(
        "Distribusi selesai"
    );
}

// =================================================
// KELUARKAN PAKAN
// =================================================

void keluarkanPakan(int targetGram) {

    Serial.println(
        "MULAI PAKAN"
    );

    float beratAwal =
        bacaBerat();

    float targetSisa =
        beratAwal - targetGram;

    if (targetSisa < 0) {
        targetSisa = 0;
    }

    motor1Maju();

    while (true) {

        if (!client.connected()) {

            reconnectMQTT();
        }

        client.loop();

        float berat =
            bacaBerat();

        kirimLoadcell(berat);

        Serial.print("Hopper: ");
        Serial.println(berat);

        if (berat <= targetSisa) {

            motor1Stop();

            Serial.println(
                "Target tercapai"
            );

            Serial.println(
                "Tunggu 2 detik"
            );

            delay(2000);

            distribusiPakan();

            break;
        }

        delay(100);
    }
}

// =================================================
// WIFI
// =================================================

void setupWifi() {

    WiFi.begin(
        ssid,
        password
    );

    Serial.print(
        "Connecting WiFi"
    );

    while (
        WiFi.status() != WL_CONNECTED
    ) {

        delay(500);
        Serial.print(".");
    }

    Serial.println();
    Serial.println(
        "WiFi Connected"
    );
}

// =================================================
// SETUP
// =================================================

void setup() {

    Serial.begin(115200);

    pinMode(M1_IN1, OUTPUT);
    pinMode(M1_IN2, OUTPUT);

    pinMode(M2_IN1, OUTPUT);
    pinMode(M2_IN2, OUTPUT);

    motor1Stop();
    motor2Stop();

    scale.begin(
        DT_PIN,
        SCK_PIN
    );

    scale.set_scale(
        calibration_factor
    );

    setupWifi();

    espClient.setInsecure();

    client.setServer(
        mqtt_server,
        mqtt_port
    );

    configTime(
        gmtOffset_sec,
        daylightOffset_sec,
        ntpServer
    );

    randomSeed(micros());

    Serial.println(
        "SYSTEM READY"
    );
}

// =================================================
// LOOP
// =================================================

void loop() {

    if (!client.connected()) {
        reconnectMQTT();
    }

    client.loop();

    static unsigned long lastTimePrint = 0;

    // ==========================
    // Firebase tiap 30 detik
    // ==========================

    if (millis() - lastFirebase > 30000) {

        ambilFirebase();

        lastFirebase = millis();
    }

    // ==========================
    // Publish berat tiap 1 detik
    // ==========================

    if (millis() - lastPublish > 1000) {

        float berat = bacaBerat();

        kirimLoadcell(berat);

        Serial.println("====================");

        Serial.print("Berat Hopper : ");
        Serial.print(berat);
        Serial.println(" gram");

        Serial.println("====================");

        lastPublish = millis();
    }

    // ==========================
    // Print waktu tiap 5 detik
    // ==========================

    if (millis() - lastTimePrint > 5000) {

        struct tm timeinfo;

        if (getLocalTime(&timeinfo)) {

            char waktu[6];

            sprintf(
                waktu,
                "%02d:%02d",
                timeinfo.tm_hour,
                timeinfo.tm_min
            );

            Serial.print("Waktu: ");
            Serial.println(waktu);

            // JADWAL 1

            if (
                jadwalJam1 == String(waktu)
                && !sudahJalan1
            ) {

                keluarkanPakan(
                    jadwalGram1
                );

                sudahJalan1 = true;
            }

            // JADWAL 2

            if (
                jadwalJam2 == String(waktu)
                && !sudahJalan2
            ) {

                keluarkanPakan(
                    jadwalGram2
                );

                sudahJalan2 = true;
            }

            if(String(waktu)!=jadwalJam1)
                sudahJalan1=false;

            if(String(waktu)!=jadwalJam2)
                sudahJalan2=false;
        }

        lastTimePrint = millis();
    }

    delay(10);
}