#pragma once
#include <Arduino.h>

void khoiTaoChapHanhVaLCD();
void setPump(bool state);
void capNhatMaiChe();
void capNhatServo();
void xuLyNutNhan();
void yeuCauCapNhatLCD(uint32_t bit);
void hienThiDongLCD(int dong, String noiDung);
void TaskLCD(void *pvParameters);