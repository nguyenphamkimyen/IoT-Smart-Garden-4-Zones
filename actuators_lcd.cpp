#include "actuators_lcd.h"
#include "config.h"
#include "app_state.h"
#include "sensors.h"
#include <ESP32Servo.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>

static LiquidCrystal_I2C lcd(LCD_ADDRESS, 16, 2);
static Servo roofServo;

static Button nutMode = {BUTTON_MODE, HIGH, HIGH, 0};
static Button nutRoof = {BUTTON_ROOF, HIGH, HIGH, 0};
static Button nutPump = {BUTTON_PUMP, HIGH, HIGH, 0};

static String lastLine0 = "";
static String lastLine1 = "";
static int trangLCD = 0;

static unsigned long lanQuayServoCuoi = 0;
static unsigned long thoiDiemBatDauServo = 0;
static bool servoDangKhoiDong = false;

void khoiTaoChapHanhVaLCD() {
  setPump(false);
  pinMode(BUTTON_MODE, INPUT_PULLUP);
  pinMode(BUTTON_ROOF, INPUT_PULLUP);
  pinMode(BUTTON_PUMP, INPUT_PULLUP);

  Wire.begin(LCD_SDA, LCD_SCL);
  lcd.init();
  lcd.backlight();

  hienThiDongLCD(0, "HE THONG TUOI CAY");
  hienThiDongLCD(1, "FW: " + String(FW_VERSION));

  digitalWrite(BUZZER_PIN, HIGH);
  delay(80);
  digitalWrite(BUZZER_PIN, LOW);

  roofServo.setPeriodHertz(50);
  roofServo.attach(SERVO_PIN, 500, 2400);
  roofServo.write(GOC_DONG);
  thoiDiemBatDauServo = millis();
  servoDangKhoiDong = true;
}

void yeuCauCapNhatLCD(uint32_t bit) {
  if (TaskLCDHandle != NULL) xTaskNotify(TaskLCDHandle, bit, eSetBits);
}

void hienThiDongLCD(int dong, String noiDung) {
  if (dong == 0) {
    if (noiDung == lastLine0) return;
    lastLine0 = noiDung;
  } else {
    if (noiDung == lastLine1) return;
    lastLine1 = noiDung;
  }
  lcd.setCursor(0, dong);
  lcd.print(noiDung);
  for (int i = noiDung.length(); i < 16; i++) lcd.print(" ");
}

void setPump(bool state) {
  if (kiemTraHetNuoc() && state == true) {
    state = false;
    yeuCauCapNhatLCD(LCD_PAGE1_BIT);
  }
  if (xMutex != NULL) xSemaphoreTake(xMutex, portMAX_DELAY);
  if (dangBom != state) {
    dangBom = state;
    guiNgayLenWeb = true;
  }
  if (xMutex != NULL) xSemaphoreGive(xMutex);
}

void capNhatMaiChe() {
  if (!cheDoAuto) return;
  bool yeuCauMoMai = muaLon || nhietDoCao;

  if (yeuCauMoMai && !trangThaiMai) {
    gocMucTieu = GOC_MO;
    if (xMutex != NULL) xSemaphoreTake(xMutex, portMAX_DELAY);
    trangThaiMai = true;
    if (xMutex != NULL) xSemaphoreGive(xMutex);
    guiNgayLenWeb = true;
  } else if (!yeuCauMoMai && trangThaiMai) {
    gocMucTieu = GOC_DONG;
    if (xMutex != NULL) xSemaphoreTake(xMutex, portMAX_DELAY);
    trangThaiMai = false;
    if (xMutex != NULL) xSemaphoreGive(xMutex);
    guiNgayLenWeb = true;
  }
}

void capNhatServo() {
  if (servoDangKhoiDong) {
    if (millis() - thoiDiemBatDauServo < 300) return;
    roofServo.detach();
    servoDangKhoiDong = false;
    return;
  }
  if (gocHienTai == gocMucTieu) {
    if (roofServo.attached()) roofServo.detach();
    return;
  }
  if (!roofServo.attached()) roofServo.attach(SERVO_PIN, 500, 2400);

  if (millis() - lanQuayServoCuoi >= TOC_DO_SERVO) {
    lanQuayServoCuoi = millis();
    if (gocHienTai < gocMucTieu) gocHienTai += BUOC_SERVO;
    else gocHienTai -= BUOC_SERVO;
    roofServo.write(gocHienTai);
  }
}

static bool vuaNhanNut(Button &nut) {
  int reading = digitalRead(nut.pin);
  if (reading != nut.lastReading) {
    nut.lastDebounce = millis();
    nut.lastReading = reading;
  }
  if (millis() - nut.lastDebounce >= DEBOUNCE) {
    if (reading != nut.stableState) {
      nut.stableState = reading;
      if (nut.stableState == BUTTON_ON) return true;
    }
  }
  return false;
}

void xuLyNutNhan() {
  bool nhanNut1 = vuaNhanNut(nutMode);
  bool nhanNut2 = vuaNhanNut(nutRoof);
  bool nhanNut3 = vuaNhanNut(nutPump);

  if (nhanNut1) {
    if (xMutex != NULL) xSemaphoreTake(xMutex, portMAX_DELAY);
    cheDoAuto = !cheDoAuto;
    bool autoMoi = cheDoAuto;
    if (xMutex != NULL) xSemaphoreGive(xMutex);

    guiNgayLenWeb = true;
    if (autoMoi) {
      capNhatMaiChe();
      int soilADC = docCamBienDat();
      if (soilADC < SOIL_DRY_ON) setPump(false);
      xuLyDat(soilADC);
    }
    yeuCauCapNhatLCD(LCD_PAGE1_BIT);
  }

  if (!cheDoAuto) {
    if (nhanNut2) {
      if (xMutex != NULL) xSemaphoreTake(xMutex, portMAX_DELAY);
      trangThaiMai = !trangThaiMai;
      bool maiMoi = trangThaiMai;
      if (xMutex != NULL) xSemaphoreGive(xMutex);

      gocMucTieu = maiMoi ? GOC_MO : GOC_DONG;
      guiNgayLenWeb = true;
      yeuCauCapNhatLCD(LCD_PAGE2_BIT);
    }
    if (nhanNut3) {
      setPump(!dangBom);
      guiNgayLenWeb = true;
      yeuCauCapNhatLCD(LCD_PAGE3_BIT);
    }
  }
}

static void veManHinhLCD() {
  if (dangCapNhatOTA) return;

  if (thoiDiemHienSyncLCD > 0 && (millis() - thoiDiemHienSyncLCD < 3500)) {
    hienThiDongLCD(0, "DONG BO TU WEB");
    hienThiDongLCD(1, dongSyncLCD);
    return;
  }

  bool localCheDoAuto, localHetNuoc, localDaDocNhietDo, localTrangThaiMai, localDaDocDoAmDat, localDangBom;
  bool localLoiNhietDo, localLoiMua, localLoiDat;
  float localNhietDo;
  int localDoAmDatPhanTram;

  if (xMutex != NULL) xSemaphoreTake(xMutex, portMAX_DELAY);
  localCheDoAuto = cheDoAuto; localHetNuoc = hetNuoc;
  localDaDocNhietDo = daDocNhietDo; localNhietDo = nhietDo;
  localTrangThaiMai = trangThaiMai; localDaDocDoAmDat = daDocDoAmDat;
  localDoAmDatPhanTram = doAmDatPhanTram; localDangBom = dangBom;
  localLoiNhietDo = loiCamBienNhietDo; localLoiMua = loiCamBienMua; localLoiDat = loiCamBienDat;
  if (xMutex != NULL) xSemaphoreGive(xMutex);

  if (localLoiNhietDo || localLoiMua || localLoiDat) {
    hienThiDongLCD(0, "LOI CAM BIEN");
    int soLoi = (localLoiNhietDo ? 1 : 0) + (localLoiMua ? 1 : 0) + (localLoiDat ? 1 : 0);
    if (chiSoLoiLCD >= soLoi) chiSoLoiLCD = 0;
    int viTri = chiSoLoiLCD;

    if (localLoiNhietDo) { if (viTri == 0) { hienThiDongLCD(1, "NHIET DO!"); return; } viTri--; }
    if (localLoiMua)      { if (viTri == 0) { hienThiDongLCD(1, "MUA!"); return; } viTri--; }
    if (localLoiDat && viTri == 0) { hienThiDongLCD(1, "DO AM DAT!"); return; }
    return;
  }

  if (trangLCD == 0) {
    String header = (localCheDoAuto ? "AUTO " : "MANUAL ") + String(FW_VERSION);
    if (!daKetNoiCloud) header = (localCheDoAuto ? "AUTO" : "MANUAL") + String(" [OFFLINE]");
    hienThiDongLCD(0, header);
    hienThiDongLCD(1, localHetNuoc ? "CAN THEM NUOC!" : "");
  } else if (trangLCD == 1) {
    hienThiDongLCD(0, localDaDocNhietDo ? ("NHIET DO: " + String(localNhietDo, 0) + " DO C") : "NHIET DO: --");
    hienThiDongLCD(1, localTrangThaiMai ? "MAI CHE: MO" : "MAI CHE: DONG");
  } else if (trangLCD == 2) {
    hienThiDongLCD(0, localDaDocDoAmDat ? ("DO AM DAT: " + String(localDoAmDatPhanTram) + "%") : "DO AM DAT: --%");
    hienThiDongLCD(1, localDangBom ? "MAY BOM: BAT" : "MAY BOM: TAT");
  }
}

void TaskLCD(void *pvParameters) {
  uint32_t thongBao;
  trangLCD = 0;
  veManHinhLCD();

  for (;;) {
    if (xTaskNotifyWait(0x00, 0xFFFFFFFF, &thongBao, portMAX_DELAY) == pdTRUE) {
      if (dangCapNhatOTA) continue;

      bool coLoi;
      if (xMutex != NULL) xSemaphoreTake(xMutex, portMAX_DELAY);
      coLoi = loiCamBienNhietDo || loiCamBienMua || loiCamBienDat;
      if (xMutex != NULL) xSemaphoreGive(xMutex);

      if (thongBao & LCD_SYNC_BIT) { veManHinhLCD(); continue; }

      if (coLoi) {
        if (thongBao & LCD_SENSOR_ERROR_BIT) chiSoLoiLCD = 0;
        else if (thongBao & LCD_TIMER_BIT) chiSoLoiLCD++;
        trangLCD = 3;
      } else {
        if (thongBao & LCD_SENSOR_ERROR_BIT) { chiSoLoiLCD = 0; trangLCD = 0; }
        else if (thongBao & LCD_PAGE1_BIT) trangLCD = 0;
        else if (thongBao & LCD_PAGE2_BIT) trangLCD = 1;
        else if (thongBao & LCD_PAGE3_BIT) trangLCD = 2;
        else if (thongBao & LCD_TIMER_BIT) {
          trangLCD++;
          if (trangLCD > 2) trangLCD = 0;
        }
      }
      veManHinhLCD();
    }
  }
}