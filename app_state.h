#pragma once
#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/timers.h>
#include <freertos/semphr.h>

struct Button {
  uint8_t pin;
  int lastReading;
  int stableState;
  unsigned long lastDebounce;
};

struct KhuCon {
  const char* ten;
  float nhietDo;
  int doAmDat;
  bool mua;
  bool hetNuoc;
  bool modeAuto;
  bool maiMo;
  bool bomBat;
  int buocKichBan;
};

struct GiamSatADC {
  int giaTriTruoc = 0;
  int giaTriUngVien = 0;
  uint8_t demBatThuong = 0;
  uint8_t demOnDinh = 0;
  bool daCoMau = false;
  bool dangLoi = false;
};

// Bien trang thai he thong
extern bool cheDoAuto;
extern bool trangThaiMai;
extern bool dangBom;
extern bool hetNuoc;
extern bool muaLon;
extern bool nhietDoCao;
extern bool daDocNhietDo;
extern bool daDocDoAmDat;

extern float nhietDo;
extern int doAmDatPhanTram;
extern String trangThaiMua;
extern int gocHienTai;
extern int gocMucTieu;

// Loi cam bien
extern bool loiCamBienNhietDo;
extern bool loiCamBienMua;
extern bool loiCamBienDat;
extern int chiSoLoiLCD;

// Mang, Cloud & OTA
extern volatile bool dangCapNhatOTA;
extern volatile bool daKetNoiCloud;
extern volatile bool guiNgayLenWeb;
extern unsigned long thoiDiemHienSyncLCD;
extern String dongSyncLCD;
extern KhuCon khuCon[3];

// FreeRTOS Handles
extern TaskHandle_t TaskChinhHandle;
extern TaskHandle_t TaskLCDHandle;
extern TaskHandle_t TaskTBHandle;
extern SemaphoreHandle_t xMutex;