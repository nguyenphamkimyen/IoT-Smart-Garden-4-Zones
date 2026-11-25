#pragma once
#include <Arduino.h>

// ================== THONG TIN FIRMWARE (THINGSBOARD OTA) ==================
#define FW_TITLE   "Vuon_4_Khu_FW"
#define FW_VERSION "v1.0.0"          // Doi thanh "v2.0.0" khi xuat file .bin OTA

// ================== CAU HINH WI-FI & THINGSBOARD MQTTS 8883 ==================
#define WIFI_SSID     "YOUR_WIFI_SSID"          // Dien ten WiFi / Hotspot 2.4GHz
#define WIFI_PASSWORD "YOUR_WIFI_PASSWORD"

#define TB_SERVER     "eu.thingsboard.cloud"
#define TB_PORT       8883
#define TOKEN_KHU_A   "YOUR_THINGSBOARD_TOKEN"

#define CHU_KY_GUI_TB       2000
#define CHU_KY_DOI_KICH_BAN 240000

// ================== KHAI BAO CHAN GPIO ==================
#define RAIN_PIN       4
#define SOIL_AO        5
#define SERVO_PIN      17
#define PHAO_PIN       40
#define DHT_PIN        36
#define BUZZER_PIN     8

#define BUTTON_MODE    42
#define BUTTON_ROOF    39
#define BUTTON_PUMP    38
#define BUTTON_ON      LOW
#define BUTTON_OFF     HIGH

#define LCD_SDA        45
#define LCD_SCL        48
#define LCD_ADDRESS    0x27
#define DHT_TYPE       DHT11

// ================== NGUONG CAM BIEN & DONG CO ==================
#define NGUONG_KHONG_MUA 3000
#define NGUONG_MUA_NHO   1973
#define NGUONG_MUA_VUA   1671

#define SOIL_DRY_ON      2000
#define SOIL_WET_OFF     1600
#define SOIL_KHO         2600
#define SOIL_UOT         1700
#define NGUONG_NHIET_DO  32.0        // Ban v2.0.0 ha xuong 30.0

#define GOC_DONG         0
#define GOC_MO           90
#define TOC_DO_SERVO     40
#define BUOC_SERVO       1

// ================== CHU KY & LOC NHIEU ==================
#define SO_MAU_MUA      20
#define SO_MAU_DAT      10
#define CHU_KY_DOC      1000
#define CHU_KY_DHT      2000
#define CHU_KY_LCD      3000
#define CHU_KY_DAT_NGHI 10000

#define DEBOUNCE        50
#define CHU_KY_COI      5000
#define THOI_GIAN_KEU   200

#define ADC_NHAY_BAT_THUONG     700
#define ADC_ON_DINH_CHO_PHEP    150
#define SO_LAN_NHAY_LOI         3
#define SO_LAN_ON_DINH_KHOI_PHUC 5

// ================== CO THONG BAO LCD (TASK NOTIFY) ==================
#define LCD_TIMER_BIT        0x01
#define LCD_PAGE1_BIT        0x02
#define LCD_PAGE2_BIT        0x04
#define LCD_PAGE3_BIT        0x08
#define LCD_SENSOR_ERROR_BIT 0x10
#define LCD_SYNC_BIT         0x20