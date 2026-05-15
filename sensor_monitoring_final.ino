#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <PubSubClient.h>
#include <DHT.h>
#include <ArduinoJson.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <UniversalTelegramBot.h>

// =====================================================
// WIFI
// =====================================================

const char* ssid = "xxxxxxxx";
const char* password = "xxxxxxxx";

// =====================================================
// TELEGRAM
// =====================================================

const char* BOT_TOKEN= "8995134351:AAEE3TpHbOMCvdGfrGOdXnZALlG6QUu6N-A";

String chatIDs[] = {
  "5009863699",
  "6129443081",
  "922219915"
};

int totalChatID = 3;

WiFiClientSecure telegramClient;

UniversalTelegramBot bot(
  BOT_TOKEN,
  telegramClient
);

// =====================================================
// MQTT
// =====================================================

const char* mqtt_server =
"8f6cdd34c17f453ea802a2a3e90fa3e0.s1.eu.hivemq.cloud";

const int mqtt_port = 8883;

const char* mqtt_user = "SmartCoop";
const char* mqtt_pass = "PAIoT2026";

WiFiClientSecure espClient;
PubSubClient client(espClient);

// =====================================================
// DHT11
// =====================================================

#define DHTPIN 4
#define DHTTYPE DHT11

DHT dht(DHTPIN, DHTTYPE);

// =====================================================
// MQ135
// =====================================================

#define MQ135_PIN 34

// =====================================================
// LCD
// =====================================================

LiquidCrystal_I2C lcd(0x27, 16, 2);

// =====================================================
// SENSOR GLOBAL
// =====================================================

float temperature = 0;
float humidity = 0;
int gasValue = 0;

volatile float feedWeight = 0;
float currentFeedWeight = 0;

// =====================================================
// ANTI SPAM
// =====================================================

bool feedAlert = false;
bool gasAlert = false;
bool tempAlert = false;

// =====================================================
// TIMER
// =====================================================

unsigned long lastTelegramCheck = 0;
unsigned long lastMQTT = 0;

// =====================================================
// WIFI CONNECT
// =====================================================

void setup_wifi() {

  WiFi.begin(
    ssid,
    password
  );

  Serial.print("Connecting WiFi");

  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Connecting WiFi");

  while (
    WiFi.status() != WL_CONNECTED
  ) {

    delay(500);
    Serial.print(".");
  }

  Serial.println();
  Serial.println("WiFi Connected");

  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("WiFi Connected");

  delay(1000);
}

// =====================================================
// GAS STATUS
// =====================================================

String gasStatus(int gas) {

  if (gas < 1000) {

    return "AMAN";

  } else if (gas < 1800) {

    return "WARNING";

  } else {

    return "BAHAYA";
  }
}

// =====================================================
// FEED STATUS
// =====================================================

String feedStatus(float weight) {

  if (weight > 3000) {

    return "AMAN";

  } else if (weight > 1000) {

    return "MENIPIS";

  } else {

    return "HABIS";
  }
}

// =====================================================
// MQTT CALLBACK
// =====================================================

void callback(
  char* topic,
  byte* payload,
  unsigned int length
) {

  String message = "";

  for (
    int i = 0;
    i < length;
    i++
  ) {

    message += (char)payload[i];
  }

  DynamicJsonDocument doc(256);

  DeserializationError error =
    deserializeJson(
      doc,
      message
    );

  if (!error) {

    if (
      doc.containsKey(
        "feed_weight"
      )
    ) {

      feedWeight =
        doc["feed_weight"]
        .as<float>();

      Serial.print(
        "Feed MQTT: "
      );

      Serial.println(
        feedWeight
      );
    }
  }
}

// =====================================================
// MQTT RECONNECT
// =====================================================

void reconnect() {

  while (!client.connected()) {

    Serial.print(
      "Connecting MQTT..."
    );

    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Connecting MQTT");

    String clientId =
      "Sensor-";

    clientId +=
      String(
        random(0xffff),
        HEX
      );

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

      client.subscribe(
        "kandangayam/loadcell"
      );

      lcd.setCursor(0, 1);
      lcd.print("MQTT Connected");

      delay(1000);

    } else {

      Serial.println(
        client.state()
      );

      lcd.setCursor(0, 1);
      lcd.print("Retrying...");

      delay(1000);
    }
  }
}

// =====================================================
// SEND TELEGRAM
// =====================================================

void kirimTelegram(
  String pesan
) {

  if (
    WiFi.status() != WL_CONNECTED
  ) {

    return;
  }

  for (
    int i = 0;
    i < totalChatID;
    i++
  ) {

    bool sukses =
      bot.sendMessage(
        chatIDs[i],
        pesan,
        ""
      );

    if (sukses) {

      Serial.print(
        "Telegram OK: "
      );

      Serial.println(
        chatIDs[i]
      );

    } else {

      Serial.print(
        "Telegram FAIL: "
      );

      Serial.println(
        chatIDs[i]
      );
    }

    delay(100);
  }
}

// =====================================================
// TELEGRAM COMMAND
// =====================================================

void handleTelegram() {

  int jumlahPesan =
    bot.getUpdates(
      bot.last_message_received + 1
    );

  while (jumlahPesan) {

    for (
      int i = 0;
      i < jumlahPesan;
      i++
    ) {

      String text =
        bot.messages[i].text;

      String chat_id =
        bot.messages[i].chat_id;

      // ======================

      if (text == "/start") {

        String pesan =
          "🐔 SmartCoop Bot\n\n";

        pesan += "/status\n";
        pesan += "/suhu\n";
        pesan += "/gas\n";
        pesan += "/pakan";

        bot.sendMessage(
          chat_id,
          pesan,
          ""
        );
      }

      // ======================

      if (text == "/suhu") {

        String pesan =
          "🌡 Suhu : "
          + String(temperature)
          + "°C\n"
          + "💧 Humidity : "
          + String(humidity)
          + "%";

        bot.sendMessage(
          chat_id,
          pesan,
          ""
        );
      }

      // ======================

      if (text == "/gas") {

        String pesan =
          "☣️ MQ135 : "
          + String(gasValue)
          + "\nStatus : "
          + gasStatus(gasValue);

        bot.sendMessage(
          chat_id,
          pesan,
          ""
        );
      }

      // ======================

      if (text == "/pakan") {

        String pesan =
          "🌾 Pakan : "
          + String(currentFeedWeight)
          + " gram\n"
          + "Status : "
          + feedStatus(
              currentFeedWeight
            );

        bot.sendMessage(
          chat_id,
          pesan,
          ""
        );
      }

      // ======================

      if (text == "/status") {

        String pesan =
          "📊 STATUS KANDANG\n\n";

        pesan +=
          "🌡 Suhu : "
          + String(temperature)
          + "°C\n";

        pesan +=
          "💧 Humidity : "
          + String(humidity)
          + "%\n";

        pesan +=
          "☣️ Gas : "
          + String(gasValue)
          + "\n";

        pesan +=
          "🌾 Pakan : "
          + String(currentFeedWeight)
          + " gram\n\n";

        pesan +=
          "Gas : "
          + gasStatus(gasValue)
          + "\n";

        pesan +=
          "Pakan : "
          + feedStatus(
              currentFeedWeight
            );

        bot.sendMessage(
          chat_id,
          pesan,
          ""
        );
      }
    }

    jumlahPesan =
      bot.getUpdates(
        bot.last_message_received + 1
      );
  }
}

// =====================================================
// SETUP
// =====================================================

void setup() {

  Serial.begin(115200);

  analogReadResolution(12);

  dht.begin();

  Wire.begin(21, 22);

  lcd.init();
  lcd.backlight();

  lcd.setCursor(0, 0);
  lcd.print("SMART COOP");

  lcd.setCursor(0, 1);
  lcd.print("Starting...");

  setup_wifi();

  espClient.setInsecure();

  telegramClient.setInsecure();

  telegramClient.setTimeout(3000);

  client.setServer(
    mqtt_server,
    mqtt_port
  );

  client.setCallback(callback);

  randomSeed(micros());

  Serial.println(
    "SYSTEM READY"
  );

  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("System Ready");

  delay(1000);
}

// =====================================================
// LOOP
// =====================================================

void loop() {

  // ================= WIFI =================

  if (
    WiFi.status() != WL_CONNECTED
  ) {

    setup_wifi();
  }

  // ================= MQTT =================

  if (
    !client.connected()
  ) {

    reconnect();
  }

  client.loop();

  // ================= TELEGRAM =================

  if (
    millis() -
    lastTelegramCheck >
    1000
  ) {

    handleTelegram();

    lastTelegramCheck =
      millis();
  }

  // ================= DHT =================

  temperature =
    dht.readTemperature();

  humidity =
    dht.readHumidity();

  // ================= MQ135 =================

  gasValue = 0;

  for (
    int i = 0;
    i < 10;
    i++
  ) {

    gasValue +=
      analogRead(
        MQ135_PIN
      );

    delay(10);
  }

  gasValue /= 10;

  // ================= LOADCELL MQTT =================

  currentFeedWeight =
    feedWeight;

  // ================= VALIDASI =================

  if (
    isnan(temperature) ||
    isnan(humidity)
  ) {

    Serial.println(
      "DHT ERROR"
    );

    return;
  }

  // ================= STATUS =================

  String statusGas =
    gasStatus(gasValue);

  String statusFeed =
    feedStatus(
      currentFeedWeight
    );

  // =================================================
  // NOTIF PAKAN
  // =================================================

  if (
    currentFeedWeight < 1000 &&
    !feedAlert
  ) {

    kirimTelegram(
      "⚠️ Pakan hampir habis\n"
      "Sisa: "
      + String(currentFeedWeight)
      + " gram"
    );

    feedAlert = true;
  }

  if (
    currentFeedWeight > 1000
  ) {

    feedAlert = false;
  }

  // =================================================
  // NOTIF SUHU
  // =================================================

  if (
    temperature > 35 &&
    !tempAlert
  ) {

    kirimTelegram(
      "🔥 Suhu tinggi\n"
      + String(temperature)
      + "°C"
    );

    tempAlert = true;
  }

  if (
    temperature <= 35
  ) {

    tempAlert = false;
  }

  // =================================================
  // NOTIF GAS
  // =================================================

  if (
    gasValue > 1800 &&
    !gasAlert
  ) {

    kirimTelegram(
      "☣️ Udara kandang buruk\n"
      "MQ135: "
      + String(gasValue)
    );

    gasAlert = true;
  }

  if (
    gasValue < 1800
  ) {

    gasAlert = false;
  }

  // ================= MQTT JSON =================

  if (
    millis() - lastMQTT >
    1000
  ) {

    StaticJsonDocument<256> doc;

    doc["temperature"] =
      temperature;

    doc["humidity"] =
      humidity;

    doc["gas"] =
      gasValue;

    doc["gas_status"] =
      statusGas;

    doc["feed_weight"] =
      currentFeedWeight;

    doc["feed_status"] =
      statusFeed;

    char buffer[256];

    serializeJson(
      doc,
      buffer
    );

    client.publish(
      "kandangayam/data",
      buffer
    );

    lastMQTT =
      millis();
  }

  // ================= SERIAL =================

  Serial.println("================");

  Serial.print("Temp: ");
  Serial.println(temperature);

  Serial.print("Hum: ");
  Serial.println(humidity);

  Serial.print("Gas: ");
  Serial.println(gasValue);

  Serial.print("Feed: ");
  Serial.println(currentFeedWeight);

  // ================= LCD PAGE 1 =================

  lcd.clear();

  lcd.setCursor(0, 0);
  lcd.print("T:");
  lcd.print(temperature, 1);
  lcd.print("C");

  lcd.setCursor(9, 0);
  lcd.print("H:");
  lcd.print(humidity, 0);
  lcd.print("%");

  lcd.setCursor(0, 1);
  lcd.print("Gas:");
  lcd.print(gasValue);

  for (
    int i = 0;
    i < 20;
    i++
  ) {

    client.loop();
    delay(100);
  }

  // ================= LCD PAGE 2 =================

  lcd.clear();

  lcd.setCursor(0, 0);
  lcd.print("Feed Weight");

  lcd.setCursor(0, 1);
  lcd.print(currentFeedWeight, 1);
  lcd.print(" g");

  for (
    int i = 0;
    i < 20;
    i++
  ) {

    client.loop();
    delay(100);
  }

  // ================= LCD PAGE 3 =================

  lcd.clear();

  lcd.setCursor(0, 0);
  lcd.print("Feed Status");

  lcd.setCursor(0, 1);
  lcd.print(statusFeed);

  for (
    int i = 0;
    i < 20;
    i++
  ) {

    client.loop();
    delay(100);
  }

  // ================= LCD PAGE 4 =================

  lcd.clear();

  lcd.setCursor(0, 0);
  lcd.print("Air Quality");

  lcd.setCursor(0, 1);
  lcd.print(statusGas);

  for (
    int i = 0;
    i < 20;
    i++
  ) {

    client.loop();
    delay(100);
  }
}
