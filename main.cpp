#include <Arduino.h>
#include "config.h"
#include "app_state.h"
#include "sensors.h"
#include "actuators_lcd.h"
#include "cloud_ota.h"

// ================== KHOI TAO BIEN TOAN CUC ==================
bool cheDoAuto = false, trangThaiMai = false, dangBom = false, hetNuoc = false;
bool muaLon = false, nhietDoCao = false, daDocNhietDo = false, daDocDoAmDat = false;
float nhietDo = 0;
int doAmDatPhanTram = 0, gocHienTai = GOC_DONG, gocMucTieu = GOC_DONG;
String trangThaiMua = "KHONG MUA";

bool loiCamBienNhietDo = false, loiCamBienMua = false, loiCamBienDat = false;
int chiSoLoiLCD = 0;

volatile bool dangCapNhatOTA = false, daKetNoiCloud = false, guiNgayLenWeb = false;
unsigned long thoiDiemHienSyncLCD = 0;
String dongSyncLCD = "";

KhuCon khuCon[3] = {
  {"Khu_B", 30.5, 58, false, false, false, false, false, 0},
  {"Khu_C", 26.5, 74, false, false, false, false, false, 1},
  {"Khu_D", 33.5, 35, false, false, false, false, false, 2}
};

TaskHandle_t TaskChinhHandle = NULL, TaskLCDHandle = NULL, TaskTBHandle = NULL;
SemaphoreHandle_t xMutex = NULL;
static TimerHandle_t TimerCamBien = NULL, TimerLCD = NULL;

// ================== TIMER & TASK DIEU KHIEN CHINH ==================
void callbackTimerCamBien(TimerHandle_t xTimer) {
  if (TaskChinhHandle != NULL) xTaskNotify(TaskChinhHandle, 0x01, eSetBits);
}

void callbackTimerLCD(TimerHandle_t xTimer) {
  if (TaskLCDHandle != NULL) xTaskNotify(TaskLCDHandle, LCD_TIMER_BIT, eSetBits);
}

void TaskChinh(void *pvParameters) {
  uint32_t thongBao;
  unsigned long lanDocDHTCuoi = 0, lanDocDatCuoi = 0;

  for (;;) {
    if (dangCapNhatOTA) { vTaskDelay(pdMS_TO_TICKS(50)); continue; }

    xuLyNutNhan();
    capNhatServo();

    bool phaoHetNuoc = kiemTraHetNuoc();
    capNhatCoi();
    if (dangBom && phaoHetNuoc) setPump(false);

    if (xTaskNotifyWait(0x00, 0xFFFFFFFF, &thongBao, 0) == pdTRUE) {
      if (thongBao & 0x01) {
        kiemTraHetNuoc();
        xuLyMua(docCamBienMua());

        unsigned long chuKyDat = dangBom ? CHU_KY_DOC : CHU_KY_DAT_NGHI;
        if (millis() - lanDocDatCuoi >= chuKyDat || !daDocDoAmDat) {
          lanDocDatCuoi = millis();
          xuLyDat(docCamBienDat());
        }
        if (millis() - lanDocDHTCuoi >= CHU_KY_DHT) {
          lanDocDHTCuoi = millis();
          xuLyNhietDo();
        }
        capNhatMaiChe();
      }
    }
    vTaskDelay(pdMS_TO_TICKS(10));
  }
}

void setup() {
  Serial.begin(115200);
  analogReadResolution(12);
  xMutex = xSemaphoreCreateMutex();

  khoiTaoCamBien();
  khoiTaoChapHanhVaLCD();

  xTaskCreate(TaskChinh, "TaskChinh", 8192, NULL, 2, &TaskChinhHandle);
  xTaskCreate(TaskLCD, "TaskLCD", 4096, NULL, 1, &TaskLCDHandle);
  xTaskCreatePinnedToCore(TaskThingsBoard, "TaskTB", 12288, NULL, 1, &TaskTBHandle, 0);

  TimerCamBien = xTimerCreate("TimerCamBien", pdMS_TO_TICKS(CHU_KY_DOC), pdTRUE, NULL, callbackTimerCamBien);
  TimerLCD     = xTimerCreate("TimerLCD", pdMS_TO_TICKS(CHU_KY_LCD), pdTRUE, NULL, callbackTimerLCD);

  if (TimerCamBien != NULL) xTimerStart(TimerCamBien, 0);
  if (TimerLCD != NULL) xTimerStart(TimerLCD, 0);
}

void loop() {
  vTaskDelay(pdMS_TO_TICKS(1000));
}