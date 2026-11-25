#include "sensors.h"
#include "config.h"
#include "app_state.h"
#include "actuators_lcd.h"
#include <DHT.h>
#include <string.h>

static DHT dht(DHT_PIN, DHT_TYPE);
static uint8_t demLoiNhietDo = 0;
static uint8_t demTotNhietDo = 0;
static GiamSatADC giamSatMua;
static GiamSatADC giamSatDat;

static unsigned long lanKeuCoiCuoi = 0;
static unsigned long thoiDiemBatDauKeu = 0;
static bool coiDangKeu = false;

void khoiTaoCamBien() {
  pinMode(RAIN_PIN, INPUT);
  pinMode(SOIL_AO, INPUT);
  pinMode(PHAO_PIN, INPUT_PULLUP);
  pinMode(BUZZER_PIN, OUTPUT);
  digitalWrite(BUZZER_PIN, LOW);
  dht.begin();
}

static bool giamSatGiaTriADC(int adc, GiamSatADC &giamSat, bool &coLoi, const char *tenCamBien) {
  bool loiCu = giamSat.dangLoi;
  bool chapNhanMau = false;

  if (!giamSat.daCoMau) {
    giamSat.giaTriTruoc = adc;
    giamSat.daCoMau = true;
    chapNhanMau = true;
  } else if (giamSat.dangLoi) {
    if (giamSat.demOnDinh == 0) {
      giamSat.giaTriUngVien = adc;
      giamSat.demOnDinh = 1;
    } else if (abs(adc - giamSat.giaTriUngVien) <= ADC_ON_DINH_CHO_PHEP) {
      giamSat.giaTriUngVien = adc;
      if (giamSat.demOnDinh < SO_LAN_ON_DINH_KHOI_PHUC) giamSat.demOnDinh++;
    } else {
      giamSat.giaTriUngVien = adc;
      giamSat.demOnDinh = 1;
    }

    if (giamSat.demOnDinh >= SO_LAN_ON_DINH_KHOI_PHUC) {
      giamSat.dangLoi = false;
      giamSat.demBatThuong = 0;
      giamSat.demOnDinh = 0;
      giamSat.giaTriTruoc = adc;
      chapNhanMau = true;
    }
  } else {
    bool batThuong = (abs(adc - giamSat.giaTriTruoc) > ADC_NHAY_BAT_THUONG);
    if (batThuong) {
      if (giamSat.demBatThuong < SO_LAN_NHAY_LOI) giamSat.demBatThuong++;
      if (giamSat.demBatThuong >= SO_LAN_NHAY_LOI) {
        giamSat.dangLoi = true;
        giamSat.demOnDinh = 0;
      }
    } else {
      giamSat.demBatThuong = 0;
      giamSat.giaTriTruoc = adc;
      chapNhanMau = true;
    }
  }

  bool loiMoi = giamSat.dangLoi;
  if (loiMoi != loiCu) {
    if (xMutex != NULL) xSemaphoreTake(xMutex, portMAX_DELAY);
    coLoi = loiMoi;
    if (strcmp(tenCamBien, "MUA") == 0) loiCamBienMua = loiMoi;
    else loiCamBienDat = loiMoi;
    if (xMutex != NULL) xSemaphoreGive(xMutex);

    yeuCauCapNhatLCD(LCD_SENSOR_ERROR_BIT);
    guiNgayLenWeb = true;
  }
  return chapNhanMau && !giamSat.dangLoi;
}

static void capNhatLoiDHT(bool loiHienTai) {
  bool thayDoi = false;
  if (xMutex != NULL) xSemaphoreTake(xMutex, portMAX_DELAY);
  if (loiHienTai) {
    demTotNhietDo = 0;
    if (demLoiNhietDo < 3) demLoiNhietDo++;
    if (demLoiNhietDo >= 3 && !loiCamBienNhietDo) { loiCamBienNhietDo = true; thayDoi = true; }
  } else {
    demLoiNhietDo = 0;
    if (demTotNhietDo < 3) demTotNhietDo++;
    if (demTotNhietDo >= 3 && loiCamBienNhietDo) { loiCamBienNhietDo = false; thayDoi = true; }
  }
  if (xMutex != NULL) xSemaphoreGive(xMutex);

  if (thayDoi) {
    yeuCauCapNhatLCD(LCD_SENSOR_ERROR_BIT);
    guiNgayLenWeb = true;
  }
}

void capNhatCoi() {
  bool localHetNuoc;
  if (xMutex != NULL) xSemaphoreTake(xMutex, portMAX_DELAY);
  localHetNuoc = hetNuoc;
  if (xMutex != NULL) xSemaphoreGive(xMutex);

  if (!localHetNuoc) {
    digitalWrite(BUZZER_PIN, LOW);
    coiDangKeu = false;
    return;
  }

  unsigned long hienTai = millis();
  if (coiDangKeu) {
    if (hienTai - thoiDiemBatDauKeu >= THOI_GIAN_KEU) {
      digitalWrite(BUZZER_PIN, LOW);
      coiDangKeu = false;
    }
  } else if (hienTai - lanKeuCoiCuoi >= CHU_KY_COI) {
    digitalWrite(BUZZER_PIN, HIGH);
    coiDangKeu = true;
    thoiDiemBatDauKeu = hienTai;
    lanKeuCoiCuoi = hienTai;
  }
}

bool kiemTraHetNuoc() {
  static uint8_t demCan = 0;
  static uint8_t demDu = 0;
  bool docLow = (digitalRead(PHAO_PIN) == LOW);

  if (docLow) { demDu = 0; if (demCan < 5) demCan++; }
  else { demCan = 0; if (demDu < 5) demDu++; }

  bool trangThaiPhao = hetNuoc;
  if (demCan >= 5) trangThaiPhao = true;
  else if (demDu >= 5) trangThaiPhao = false;

  bool coThayDoi = false;
  if (xMutex != NULL) xSemaphoreTake(xMutex, portMAX_DELAY);
  if (hetNuoc != trangThaiPhao) { hetNuoc = trangThaiPhao; coThayDoi = true; }
  if (xMutex != NULL) xSemaphoreGive(xMutex);

  if (coThayDoi) {
    guiNgayLenWeb = true;
    if (trangThaiPhao) {
      yeuCauCapNhatLCD(LCD_PAGE1_BIT);
      lanKeuCoiCuoi = millis() - CHU_KY_COI;
    }
  }
  return trangThaiPhao;
}

int docCamBienMua() {
  long tong = 0;
  for (int i = 0; i < SO_MAU_MUA; i++) tong += analogRead(RAIN_PIN);
  return tong / SO_MAU_MUA;
}

int docCamBienDat() {
  long tong = 0;
  for (int i = 0; i < SO_MAU_DAT; i++) tong += analogRead(SOIL_AO);
  return tong / SO_MAU_DAT;
}

void xuLyMua(int adc) {
  if (!giamSatGiaTriADC(adc, giamSatMua, loiCamBienMua, "MUA")) return;

  muaLon = false;
  if (adc > NGUONG_KHONG_MUA) trangThaiMua = "KHONG MUA";
  else if (adc > NGUONG_MUA_NHO) trangThaiMua = "MUA NHO";
  else if (adc > NGUONG_MUA_VUA) trangThaiMua = "MUA VUA";
  else { trangThaiMua = "MUA LON"; muaLon = true; }
}

void xuLyDat(int adc) {
  if (!giamSatGiaTriADC(adc, giamSatDat, loiCamBienDat, "DO AM DAT")) return;

  int phanTramTam = constrain(map(adc, SOIL_KHO, SOIL_UOT, 0, 100), 0, 100);
  if (xMutex != NULL) xSemaphoreTake(xMutex, portMAX_DELAY);
  doAmDatPhanTram = phanTramTam;
  daDocDoAmDat = true;
  if (xMutex != NULL) xSemaphoreGive(xMutex);

  if (adc >= SOIL_DRY_ON) { if (cheDoAuto) setPump(true); }
  else if (adc <= SOIL_WET_OFF) { if (cheDoAuto) setPump(false); }
}

void xuLyNhietDo() {
  float giaTriNhietDo = dht.readTemperature();
  if (isnan(giaTriNhietDo)) {
    capNhatLoiDHT(true);
    return;
  }
  capNhatLoiDHT(false);

  if (xMutex != NULL) xSemaphoreTake(xMutex, portMAX_DELAY);
  nhietDo = giaTriNhietDo;
  daDocNhietDo = true;
  if (xMutex != NULL) xSemaphoreGive(xMutex);

  nhietDoCao = (giaTriNhietDo > NGUONG_NHIET_DO);
}