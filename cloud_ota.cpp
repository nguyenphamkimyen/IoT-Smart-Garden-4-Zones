#include "cloud_ota.h"
#include "config.h"
#include "app_state.h"
#include "sensors.h"
#include "actuators_lcd.h"
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <MQTT.h>
#include <ArduinoJson.h>
#include <Update.h>
#include <esp_now.h>

static WiFiClientSecure tlsClient;
static MQTTClient mqttClient(4608);

static String targetFwTitle   = "";
static String targetFwVersion = "";
static size_t targetFwSize    = 0;
static size_t otaBytesNhan    = 0;
static int    otaChunkIdx     = 0;
static const size_t OTA_CHUNK_SIZE = 4096;
static unsigned long thoiDiemVuaKetNoiCloud = 0;

// Cau truc goi tin ESP-NOW nhan tu Node Khu_B
typedef struct __attribute__((packed)) {
  uint8_t zoneId;
  float   temperature;
  int16_t soilMoisture;
  bool    rainStatus;
  bool    waterEmpty;
  bool    roofState;
  bool    pumpState;
  bool    buzzerState;
  bool    modeAuto;
} GoiTinESPNOW;

static volatile bool coDuLieuMoiKhuB = false;
static unsigned long lanNhanKhuBCuoi = 0;
static bool khuBDangOnline = false;

// Callback nhan du lieu khong day ESP-NOW tu Node Khu_B
static void onRecvESPNOW(const uint8_t *mac, const uint8_t *incomingData, int len) {
  if (len == sizeof(GoiTinESPNOW)) {
    GoiTinESPNOW dataB;
    memcpy(&dataB, incomingData, sizeof(dataB));
    if (dataB.zoneId == 1) {
      khuCon[0].nhietDo  = dataB.temperature;
      khuCon[0].doAmDat  = dataB.soilMoisture;
      khuCon[0].mua      = dataB.rainStatus;
      khuCon[0].hetNuoc  = dataB.waterEmpty;
      khuCon[0].maiMo    = dataB.roofState;
      khuCon[0].bomBat   = dataB.pumpState;
      khuCon[0].modeAuto = dataB.modeAuto;
      lanNhanKhuBCuoi    = millis();
      coDuLieuMoiKhuB    = true;
    }
  }
}

static void guiTrangThaiOTA(const char* trangThai, const char* loi = "") {
  StaticJsonDocument<256> doc;
  doc["current_fw_title"]   = FW_TITLE;
  doc["current_fw_version"] = FW_VERSION;
  doc["fwVersion"]          = FW_VERSION;
  doc["fw_state"]           = trangThai;
  if (strlen(loi) > 0) doc["fw_error"] = loi;

  String payload;
  serializeJson(doc, payload);
  // Gui voi QoS = 1 (retained = false, qos = 1)
  mqttClient.publish("v1/devices/me/telemetry", payload, false, 1);
}

static void yeuCauManhOTA(int chunkIdx) {
  String topic = "v2/fw/request/1/chunk/" + String(chunkIdx);
  mqttClient.publish(topic, String(OTA_CHUNK_SIZE), false, 1);
}

static void kiemTraKichHoatOTA(JsonObject obj) {
  if (!obj.containsKey("fw_title") || !obj.containsKey("fw_version") || !obj.containsKey("fw_size")) return;

  targetFwTitle   = obj["fw_title"].as<String>();
  targetFwVersion = obj["fw_version"].as<String>();
  targetFwSize    = obj["fw_size"].as<size_t>();

  if (targetFwTitle == FW_TITLE && targetFwVersion == FW_VERSION) {
    guiTrangThaiOTA("UPDATED");
    return;
  }

  if (!Update.begin(targetFwSize)) {
    guiTrangThaiOTA("FAILED", "Not enough Flash");
    return;
  }

  dangCapNhatOTA = true;
  otaBytesNhan   = 0;
  otaChunkIdx    = 0;

  hienThiDongLCD(0, "OTA: " + targetFwVersion);
  hienThiDongLCD(1, "DANG TAI: 0%");
  guiTrangThaiOTA("DOWNLOADING");
  yeuCauManhOTA(0);
}

static void xuLyLenhTuCloud(JsonObject obj) {
  bool coLenh = false;

  if (obj.containsKey("cmdAuto")) {
    bool reqMode = obj["cmdAuto"].as<bool>();
    if (xMutex != NULL) xSemaphoreTake(xMutex, portMAX_DELAY);
    cheDoAuto = reqMode;
    if (xMutex != NULL) xSemaphoreGive(xMutex);
    if (reqMode) {
      capNhatMaiChe();
      xuLyDat(docCamBienDat());
    }
    dongSyncLCD = cheDoAuto ? "CHE DO: AUTO" : "CHE DO: MANUAL";
    coLenh = true;
  }

  if (obj.containsKey("cmdRoof")) {
    bool reqRoof = obj["cmdRoof"].as<bool>();
    if (xMutex != NULL) xSemaphoreTake(xMutex, portMAX_DELAY);
    cheDoAuto = false;
    trangThaiMai = reqRoof;
    if (xMutex != NULL) xSemaphoreGive(xMutex);
    gocMucTieu = reqRoof ? GOC_MO : GOC_DONG;
    dongSyncLCD = trangThaiMai ? "MAI CHE: MO" : "MAI CHE: DONG";
    coLenh = true;
  }

  if (obj.containsKey("cmdPump")) {
    bool reqPump = obj["cmdPump"].as<bool>();
    if (xMutex != NULL) xSemaphoreTake(xMutex, portMAX_DELAY);
    cheDoAuto = false;
    if (xMutex != NULL) xSemaphoreGive(xMutex);
    setPump(reqPump);
    dongSyncLCD = dangBom ? "MAY BOM: BAT" : "MAY BOM: TAT";
    coLenh = true;
  }

  if (coLenh) {
    thoiDiemHienSyncLCD = millis();
    yeuCauCapNhatLCD(LCD_SYNC_BIT);
    guiNgayLenWeb = true;
  }
  kiemTraKichHoatOTA(obj);
}

// Callback nhan tin nhan MQTT (ho tro nhan byte tho cho OTA va JSON cho dieu khien)
static void onMqttMessage(MQTTClient *client, char topic[], char bytes[], int length) {
  String topicStr = String(topic);

  if (topicStr.startsWith("v2/fw/response/1/chunk/")) {
    if (!dangCapNhatOTA) return;

    size_t written = Update.write((uint8_t*)bytes, length);
    if (written != (size_t)length) {
      Update.abort();
      dangCapNhatOTA = false;
      guiTrangThaiOTA("FAILED", "Flash write error");
      return;
    }

    otaBytesNhan += written;
    otaChunkIdx++;

    int phanTram = (otaBytesNhan * 100) / targetFwSize;
    hienThiDongLCD(1, "DANG TAI: " + String(phanTram) + "%");

    if (otaBytesNhan >= targetFwSize) {
      guiTrangThaiOTA("DOWNLOADED");
      if (Update.end(true)) {
        guiTrangThaiOTA("VERIFIED");
        hienThiDongLCD(0, "OTA THANH CONG!");
        hienThiDongLCD(1, "KHOI DONG LAI...");
        delay(1200);
        ESP.restart();
      } else {
        guiTrangThaiOTA("FAILED", "Update.end failed");
        dangCapNhatOTA = false;
      }
    } else {
      yeuCauManhOTA(otaChunkIdx);
    }
    return;
  }

  StaticJsonDocument<1024> doc;
  if (deserializeJson(doc, bytes, length)) return;

  if (topicStr == "v1/devices/me/attributes") {
    xuLyLenhTuCloud(doc.as<JsonObject>());
  } else if (topicStr.startsWith("v1/devices/me/attributes/response/")) {
    if (doc.containsKey("shared")) xuLyLenhTuCloud(doc["shared"].as<JsonObject>());
  }
}

static void guiDuLieuLenCloudQoS1() {
  if (!mqttClient.connected() || dangCapNhatOTA) return;

  // 1. Day du lieu that cua Tram Gateway Khu_A voi QoS 1
  StaticJsonDocument<512> docA;
  if (xMutex != NULL) xSemaphoreTake(xMutex, portMAX_DELAY);
  docA["temperature"]  = round(nhietDo * 10) / 10.0;
  docA["soilMoisture"] = doAmDatPhanTram;
  docA["rainStatus"]   = (trangThaiMua != "KHONG MUA");
  docA["rainText"]     = trangThaiMua;
  docA["waterEmpty"]   = hetNuoc;
  docA["roofState"]    = trangThaiMai;
  docA["pumpState"]    = dangBom;
  docA["buzzerState"]  = hetNuoc;
  docA["modeAuto"]     = cheDoAuto;
  docA["fwVersion"]    = FW_VERSION;

  if (loiCamBienNhietDo) docA["sensorError"] = "LOI CAM BIEN NHIET DO";
  else if (loiCamBienMua) docA["sensorError"] = "LOI CAM BIEN MUA";
  else if (loiCamBienDat) docA["sensorError"] = "LOI CAM BIEN DAT";
  else docA["sensorError"] = "NONE";
  if (xMutex != NULL) xSemaphoreGive(xMutex);

  String payloadA;
  serializeJson(docA, payloadA);
  mqttClient.publish("v1/devices/me/telemetry", payloadA, false, 1); // QoS = 1

  // 2. Chuyen tiep du lieu that nhan tu Node Khu_B (qua ESP-NOW) voi QoS 1
  if (coDuLieuMoiKhuB || (khuBDangOnline && (millis() - lanNhanKhuBCuoi < 6000))) {
    coDuLieuMoiKhuB = false;
    if (!khuBDangOnline) {
      khuBDangOnline = true;
      mqttClient.publish("v1/gateway/connect", "{\"device\":\"Khu_B\"}", false, 1);
    }

    DynamicJsonDocument docGW(768);
    JsonArray arr = docGW.createNestedArray("Khu_B");
    JsonObject obj = arr.createNestedObject();
    obj["temperature"]  = round(khuCon[0].nhietDo * 10) / 10.0;
    obj["soilMoisture"] = khuCon[0].doAmDat;
    obj["rainStatus"]   = khuCon[0].mua;
    obj["waterEmpty"]   = khuCon[0].hetNuoc;
    obj["roofState"]    = khuCon[0].maiMo;
    obj["pumpState"]    = khuCon[0].bomBat;
    obj["buzzerState"]  = khuCon[0].hetNuoc;
    obj["modeAuto"]     = khuCon[0].modeAuto;
    obj["fwVersion"]    = FW_VERSION;
    obj["sensorError"]  = "NONE";

    String payloadGW;
    serializeJson(docGW, payloadGW);
    mqttClient.publish("v1/gateway/telemetry", payloadGW, false, 1); // QoS = 1
  } else if (khuBDangOnline && (millis() - lanNhanKhuBCuoi >= 6000)) {
    // Neu Node Khu_B mat song / rut dien qua 6 giay -> bao Disconnect len ThingsBoard
    khuBDangOnline = false;
    mqttClient.publish("v1/gateway/disconnect", "{\"device\":\"Khu_B\"}", false, 1);
  }
}

void TaskThingsBoard(void *pvParameters) {
  WiFi.mode(WIFI_STA);
  WiFi.setAutoReconnect(true);
  WiFi.persistent(false);
  WiFi.setTxPower(WIFI_POWER_11dBm);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  Serial.print("[GATEWAY KHU_A] Dia chi MAC: ");
  Serial.println(WiFi.macAddress());

  // Khoi tao ESP-NOW tren Gateway Khu_A de san sang don Node Khu_B
  if (esp_now_init() == ESP_OK) {
    esp_now_register_recv_cb(onRecvESPNOW);
    Serial.println("[ESP-NOW] Gateway Khu_A san sang nhan du lieu tu Node Khu_B!");
  }

  tlsClient.setInsecure();
  mqttClient.begin(TB_SERVER, TB_PORT, tlsClient);
  mqttClient.setKeepAlive(10);
  mqttClient.setCleanSession(false); // Giu phien MQTT de Broker luu tin nhan QoS 1 khi mat mang
  mqttClient.onMessageAdvanced(onMqttMessage);

  bool daInWiFi = false;
  unsigned long lanThuKetNoiCuoi = 0, lanQuetLaiWiFiCuoi = 0, lanGuiCuoi = 0;

  for (;;) {
    if (WiFi.status() == WL_CONNECTED) {
      if (!daInWiFi) {
        daInWiFi = true;
        Serial.printf("[WIFI] Da ket noi! IP: %s | Channel: %d\n",
                      WiFi.localIP().toString().c_str(), WiFi.channel());
      }

      if (!mqttClient.connected()) {
        daKetNoiCloud = false;
        dangCapNhatOTA = false;
        if (millis() - lanThuKetNoiCuoi >= 3000) {
          lanThuKetNoiCuoi = millis();
          if (mqttClient.connect("ESP32_Gateway_KhuA", TOKEN_KHU_A, "")) {
            daKetNoiCloud = true;
            thoiDiemVuaKetNoiCloud = millis();

            // Subscribe voi QoS = 1
            mqttClient.subscribe("v1/devices/me/attributes", 1);
            mqttClient.subscribe("v1/devices/me/attributes/response/+", 1);
            mqttClient.subscribe("v2/fw/response/+/chunk/+", 1);

            mqttClient.publish("v1/devices/me/attributes/request/1",
              "{\"sharedKeys\":\"cmdAuto,cmdRoof,cmdPump,fw_title,fw_version,fw_size\"}", false, 1);

            guiTrangThaiOTA("UPDATED");
            yeuCauCapNhatLCD(LCD_PAGE1_BIT);
          }
        }
      } else {
        daKetNoiCloud = true;
        mqttClient.loop();

        if (!dangCapNhatOTA && (millis() - thoiDiemVuaKetNoiCloud >= 2000)) {
          if (guiNgayLenWeb || (millis() - lanGuiCuoi >= CHU_KY_GUI_TB)) {
            guiNgayLenWeb = false;
            lanGuiCuoi = millis();
            guiDuLieuLenCloudQoS1();
          }
        }
      }
    } else {
      if (daInWiFi) { daInWiFi = false; yeuCauCapNhatLCD(LCD_PAGE1_BIT); }
      daKetNoiCloud = false;
      dangCapNhatOTA = false;

      if (millis() - lanQuetLaiWiFiCuoi >= 8000) {
        lanQuetLaiWiFiCuoi = millis();
        WiFi.disconnect(true);
        vTaskDelay(pdMS_TO_TICKS(100));
        WiFi.mode(WIFI_STA);
        WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
      }
      vTaskDelay(pdMS_TO_TICKS(200));
    }
    vTaskDelay(pdMS_TO_TICKS(10));
  }
}