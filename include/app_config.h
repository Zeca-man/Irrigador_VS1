#pragma once

#include <Arduino.h>

namespace AppConfig {
  constexpr const char* NOME_IRRIGADOR = "IRRIGADOR_VS1_Excel1";

  constexpr const char* NTP_SERVER = "pool.ntp.org";
  constexpr long GMT_OFFSET_SEC = -10800;
  constexpr int DAYLIGHT_OFFSET_SEC = 0;

  constexpr unsigned long WIFI_CONNECT_TIMEOUT_MS = 20000;
  constexpr unsigned long WIFI_RECONNECT_INTERVAL_MS = 10000;

  constexpr long TIMER_INTERVALO_BOMBA_MS = 10000;
  constexpr long INTERVALO_SENSOR_MS = 5000;
  constexpr float INDICE_THRESHOLD = 75.0f;
  constexpr long INTERVALO_EMAIL_H = 24;
  constexpr short QTD_CICLOS_TIMER = 12;
  constexpr long TEMPO_FLAG_BOMBA_TRAVADA = 250;

  constexpr int OUTPUT1_PIN = 19;
  constexpr int OUTPUT2_PIN = 5;
  constexpr int LED_PLACA_PIN = 2;
  constexpr int HUMIDITY_SENSOR1_PIN = 32;
  constexpr int HUMIDITY_SENSOR2_PIN = 34;
  constexpr int SENSOR_SAMPLE_SIZE = 5;

  constexpr const char* OTA_HOST = "esp32_ota";
  constexpr unsigned long GOOGLE_SHEETS_UPDATE_INTERVAL_MS = 60000;
  constexpr const char* GOOGLE_SHEETS_DATA_A1_RANGE = "A2";
  constexpr const char* GOOGLE_SHEETS_HEADER_A1_RANGE = "A1:O1";

  constexpr int SMTP_PORT = 587;
}
