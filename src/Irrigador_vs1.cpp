/*********
  - Grok inseriu 2 botoes de liga e desliga da bomba
  - Reinicio do Wifi instalado - a cada 24hs
  - Email enviado na mudança status da bomba
  - Temporizador para bomba ficar intermitente
  - Alerta de bomba travada
  - Controle individual dos sensores
  - Email com tempo de bomba ligada
  - Gravando EEProm ok
  - Envios de email de bx humidade
  - Utilzar media dos valores
  - Adicionada funcionalidade OTA
  - Adicionados botões para ligar/desligar bombas manualmente
  - Adicionado sinal de notificação ao lado dos botões liga/desliga (verde para ligado, vermelho para desligado)
*********/

#include <WiFi.h>
#include <ESPmDNS.h>
#include <WebServer.h>
#include <ESP_Mail_Client.h>
#include <json/FirebaseJson.h>
#include <ESP_Google_Sheet_Client.h>
#include <LittleFS.h>
#include <Preferences.h>
#include <Update.h>
#include <time.h>
#include "web_pages.h"
#include "app_config.h"
#include "secrets.h"

const char* ntpServer = AppConfig::NTP_SERVER;
const long gmtOffset_sec = AppConfig::GMT_OFFSET_SEC;
const int daylightOffset_sec = AppConfig::DAYLIGHT_OFFSET_SEC;
constexpr const char* HOURLY_LOG_PATH = "/hourly_log.csv";
constexpr size_t HISTORY_HOURS = 24 * 7;
constexpr size_t HISTORY_DAYS = 7;
constexpr unsigned long CLOCK_SYNC_RETRY_MS = 60000;
constexpr time_t MIN_VALID_EPOCH = 1704067200;

struct HourlyStats {
  time_t hourStart = 0;
  unsigned long irrigSec1 = 0;
  unsigned long irrigSec2 = 0;
  unsigned int cycles1 = 0;
  unsigned int cycles2 = 0;
  unsigned long sensor1Sum = 0;
  unsigned long sensor2Sum = 0;
  unsigned int sensorSamples = 0;
};

struct DailyStats {
  time_t dayStart = 0;
  unsigned long irrigSec1 = 0;
  unsigned long irrigSec2 = 0;
  unsigned int cycles1 = 0;
  unsigned int cycles2 = 0;
  unsigned long sensor1Sum = 0;
  unsigned long sensor2Sum = 0;
  unsigned int sensorSamples = 0;
  String label;
};

struct PumpCycleState {
  bool active = false;
  bool manual = false;
  time_t startedAt = 0;
  long sensorStart1 = 0;
  long sensorStart2 = 0;
  float threshold1 = 0.0f;
  float threshold2 = 0.0f;
};

long timerIntervaloBomba = AppConfig::TIMER_INTERVALO_BOMBA_MS;   // Tamanho do ciclo para ligar e desligar bomba
float indiceThreshold = AppConfig::INDICE_THRESHOLD;
long intervaloEnviarEmailTemp = AppConfig::INTERVALO_EMAIL_H;  // Valor em horas
short qtdCiclosTimer = AppConfig::QTD_CICLOS_TIMER;
long tempoFlagBombaTravada = AppConfig::TEMPO_FLAG_BOMBA_TRAVADA;

// CONFIGURACAO EMAIL (centralizada em include/secrets.h)
String mensagemEmail = "Mensagem Default";
String assuntoEmail = "Assunto Default";

// OTA CONFIG
const char* host = AppConfig::OTA_HOST;


Preferences preferences;

// Define the SMTP Session object which used for SMTP transport
SMTPSession smtp;

// Define the session config data which used to store the TCP session configuration
ESP_Mail_Session session;

// Callback function to get the Email sending status
void smtpCallback(SMTP_Status status);
bool connectWiFiWithTimeout(unsigned long timeoutMs);
void maintainWiFi();
bool ensureWiFiReadyForEmail();
bool initFileSystem();
void syncClockIfNeeded(bool forceAttempt = false);
void initGoogleSheetsClientIfNeeded();
void maintainGoogleSheets();
bool ensureGoogleSheetsSheetTitle();
bool getCurrentEpoch(time_t& epochNow);
time_t getHourStart(time_t epochNow);
time_t getDayStart(time_t epochNow);
String formatTimestamp(time_t timestamp);
String formatDayLabel(time_t timestamp);
String formatDuration(unsigned long totalSeconds);
String buildGoogleSheetsRange(const char* a1Range);
bool ensureHourlyStatsCurrent();
void addSensorSnapshotToHourlyStats(long currentSensor1, long currentSensor2);
void addPumpRuntimeToHourlyStats(uint8_t pumpNumber, unsigned long seconds);
void addCycleToHourlyStats(uint8_t pumpNumber);
void persistHourlyStats(const HourlyStats& stats);
bool parseHourlyStatsLine(const String& line, HourlyStats& stats);
void accumulateDailyStats(DailyStats* days, const HourlyStats& stats);
String buildLast7DaysTableHtml();
String buildStartupEmailBody(const String& ipAddress);
String buildCycleSummaryEmailBody(uint8_t pumpNumber, const PumpCycleState& cycle, unsigned long durationSeconds, const String& reason);
bool ensureGoogleSheetsHeader();
bool appendGoogleSheetsSnapshot();
void googleSheetsTokenStatusCallback(TokenInfo info);
void startPumpCycle(PumpCycleState& cycle, bool manualMode);
String getCycleEndReason(const PumpCycleState& cycle, bool travada, bool manualRequestedOn);
void finishPumpCycle(uint8_t pumpNumber, PumpCycleState& cycle, bool travada, bool manualRequestedOn, long& durationSeconds);
void handlePumpCycleTransition(uint8_t pumpNumber, bool isOn, bool& previousState, PumpCycleState& cycle, bool manualMode, bool travada, bool manualRequestedOn, long& durationSeconds);

const int output1 = AppConfig::OUTPUT1_PIN;  // GPIO comando rele 1
const int output2 = AppConfig::OUTPUT2_PIN;   // GPIO comando rele 2
const int ledPlaca = AppConfig::LED_PLACA_PIN;  // GPIO comando led
const int humiditySensor1 = AppConfig::HUMIDITY_SENSOR1_PIN;
const int humiditySensor2 = AppConfig::HUMIDITY_SENSOR2_PIN;

// Default Threshold sensor1 Value
String lastSensor1 = "0";
String lastSensor2 = "0";

String inputMessage1;
String inputMessage2;
long intervaloEnviarEmail = 3600000;

long sensor1 = 0;
long sensor2 = 0;
short flagBomba1Travada = 0;
short flagBomba2Travada = 0;
bool flagBxHumidade1 = false;
bool flagBxHumidade2 = false;
bool histereseArmada1 = true;
bool histereseArmada2 = true;
bool flagAlertaBaixaUmidadePronto = true;

// New variables for manual pump control
bool manualPump1State = false; // Tracks manual state of Pump 1 (true = ON, false = OFF)
bool manualPump2State = false; // Tracks manual state of Pump 2 (true = ON, false = OFF)
bool manualOverride1 = false;  // Indicates if Pump 1 is manually controlled
bool manualOverride2 = false;  // Indicates if Pump 2 is manually controlled

const int SENSOR_SAMPLE_SIZE = AppConfig::SENSOR_SAMPLE_SIZE;
int medidasArray1[SENSOR_SAMPLE_SIZE] = {0};
int medidasArray2[SENSOR_SAMPLE_SIZE] = {0};
int sampleIndex = 0;
int sampleCount = 0;

short tempoBombaLigada = 0;
float limiteCorrigidoMais1 = 1300;
float limiteCorrigidoMenos1 = 1200;
float limiteCorrigidoMais2 = 1300;
float limiteCorrigidoMenos2 = 1200;

long previousMillis = 0;
long previousMillis1 = 0;
long previousMillis2 = 0;
long previousMillis3 = 0;
long previousMillis4 = 0;
long previousMillis5 = 0;
unsigned long lastWiFiReconnectAttempt = 0;
unsigned long lastClockSyncAttempt = 0;
unsigned long lastGoogleSheetsAttempt = 0;

long interval = AppConfig::INTERVALO_SENSOR_MS;    // tempo entre medidas do sensor
long tempoB1Ligada = 0;  // tempo da B1 ligada no ciclo de irrigação
long tempoB2Ligada = 0;  // tempo da B2 ligada no ciclo de irrigação
bool clockSynchronized = false;
bool currentHourLoaded = false;
bool lastPump1OutputState = false;
bool lastPump2OutputState = false;
bool googleSheetsClientStarted = false;
bool googleSheetsHeaderEnsured = false;
String googleSheetsSheetTitle;
HourlyStats currentHourlyStats;
PumpCycleState pump1CycleState;
PumpCycleState pump2CycleState;


// WebServer instance
WebServer server(80);

// Replaces placeholder with sensor values
String processor(const String& var) {
  if (var == "NOME_IRRIGADOR") {
    return String(AppConfig::NOME_IRRIGADOR);
  } else if (var == "HUMIDITY_1") {
    return lastSensor1;
  } else if (var == "THRESHOLD1") {
    return inputMessage1;
  } else if (var == "HUMIDITY_2") {
    return lastSensor2;
  } else if (var == "THRESHOLD2") {
    return inputMessage2;
  } else if (var == "PUMP1_INDICATOR") {
    return (digitalRead(output1) == LOW) ? "pump-on" : "pump-off";
  } else if (var == "PUMP2_INDICATOR") {
    return (digitalRead(output2) == LOW) ? "pump-on" : "pump-off";
  }
  return String();
}

String renderHomePage() {
  String html = String(INDEX_HTML);
  html.replace("%NOME_IRRIGADOR%", processor("NOME_IRRIGADOR"));
  html.replace("%HUMIDITY_1%", processor("HUMIDITY_1"));
  html.replace("%THRESHOLD1%", processor("THRESHOLD1"));
  html.replace("%HUMIDITY_2%", processor("HUMIDITY_2"));
  html.replace("%THRESHOLD2%", processor("THRESHOLD2"));
  html.replace("%PUMP1_INDICATOR%", processor("PUMP1_INDICATOR"));
  html.replace("%PUMP2_INDICATOR%", processor("PUMP2_INDICATOR"));
  return html;
}

void redirectToHome() {
  server.sendHeader("Location", "/", true);
  server.send(303, "text/plain", "");
}

const char* PARAM_INPUT_1_TH = "threshold_input1";
const char* PARAM_INPUT_2_TH = "threshold_input2";

// Forward declaration of envioemail
void envioemail(String htmlMsg, String assuntoEmail);
bool tryParseThreshold(const String& rawValue, float& parsedValue);
void saveThresholds();

void setup() {
  Serial.begin(115200);
  Serial.println(__FILE__);
  
  pinMode(ledPlaca, OUTPUT);
  pinMode(output1, OUTPUT);
  pinMode(output2, OUTPUT);
  pinMode(humiditySensor1, INPUT);
  pinMode(humiditySensor2, INPUT);
  digitalWrite(output1, HIGH);
  digitalWrite(output2, HIGH);
  digitalWrite(ledPlaca, HIGH);
  lastPump1OutputState = false;
  lastPump2OutputState = false;
  Serial.println("Saida 1 e 2 colocada em HIGH");
  intervaloEnviarEmail = intervaloEnviarEmailTemp * 3600000;

  // Leitura da EEPROM para atualizar a variável inputMessage
  preferences.begin("MemEProm1", false);
  inputMessage1 = preferences.getString("Limite1", "0");
  inputMessage2 = preferences.getString("Limite2", "0");
  preferences.end();

  initFileSystem();

  // Conectar ao WiFi
  WiFi.mode(WIFI_STA);
  WiFi.setAutoReconnect(true);
  WiFi.persistent(false);

  connectWiFiWithTimeout(AppConfig::WIFI_CONNECT_TIMEOUT_MS);
  if (WiFi.status() == WL_CONNECTED) {
    String wifiConnectedMsg = F("WiFi connected - Rede -> ");
    wifiConnectedMsg += Secrets::WIFI_SSID;
    Serial.println(wifiConnectedMsg);
    Serial.print("IP -> ");
    Serial.println(WiFi.localIP());
    syncClockIfNeeded(true);
    initGoogleSheetsClientIfNeeded();
  } else {
    Serial.println("WiFi indisponivel no boot. O sistema continuara sem bloqueio.");
  }
  Serial.println(__FILE__);

  // Configurar MDNS para OTA
  if (WiFi.status() == WL_CONNECTED) {
    if (!MDNS.begin(host)) {
      Serial.println("Error setting up MDNS responder!");
    } else {
      Serial.println("mDNS responder started");
    }
  } else {
    Serial.println("mDNS nao iniciado (sem WiFi).");
  }

  // Configurar rotas do servidor
  server.on("/", HTTP_GET, []() {
    server.send(200, "text/html", renderHomePage());
  });

  server.on("/get", HTTP_GET, []() {
    bool updated = false;

    if (server.hasArg(PARAM_INPUT_1_TH)) {
      float parsedValue = 0.0f;
      String candidate = server.arg(PARAM_INPUT_1_TH);
      if (tryParseThreshold(candidate, parsedValue)) {
        inputMessage1 = String(parsedValue, 0);
        updated = true;
      }
    }
    if (server.hasArg(PARAM_INPUT_2_TH)) {
      float parsedValue = 0.0f;
      String candidate = server.arg(PARAM_INPUT_2_TH);
      if (tryParseThreshold(candidate, parsedValue)) {
        inputMessage2 = String(parsedValue, 0);
        updated = true;
      }
    }

    if (updated) {
      saveThresholds();
    }
    redirectToHome();
  });

  // OTA Routes
  server.on("/ota", HTTP_GET, []() {
    if (!server.authenticate(Secrets::OTA_USERNAME, Secrets::OTA_PASSWORD)) {
      return server.requestAuthentication();
    }
    server.sendHeader("Connection", "close");
    server.send(200, "text/html", OTA_SERVER_INDEX);
  });

  server.on("/serverIndex", HTTP_GET, []() {
    if (!server.authenticate(Secrets::OTA_USERNAME, Secrets::OTA_PASSWORD)) {
      return server.requestAuthentication();
    }
    server.sendHeader("Connection", "close");
    server.send(200, "text/html", OTA_SERVER_INDEX);
  });

  server.on("/update", HTTP_POST, []() {
    if (!server.authenticate(Secrets::OTA_USERNAME, Secrets::OTA_PASSWORD)) {
      return server.requestAuthentication();
    }
    server.sendHeader("Connection", "close");
    server.send(200, "text/plain", (Update.hasError()) ? "FAIL" : "OK");
    ESP.restart();
  }, []() {
    if (!server.authenticate(Secrets::OTA_USERNAME, Secrets::OTA_PASSWORD)) {
      return server.requestAuthentication();
    }
    HTTPUpload& upload = server.upload();
    if (upload.status == UPLOAD_FILE_START) {
      Serial.printf("Update: %s\n", upload.filename.c_str());
      if (!Update.begin(UPDATE_SIZE_UNKNOWN)) {
        Update.printError(Serial);
      }
    } else if (upload.status == UPLOAD_FILE_WRITE) {
      if (Update.write(upload.buf, upload.currentSize) != upload.currentSize) {
        Update.printError(Serial);
      }
    } else if (upload.status == UPLOAD_FILE_END) {
      if (Update.end(true)) {
        Serial.printf("Update Success: %u\nRebooting...\n", upload.totalSize);
      } else {
        Update.printError(Serial);
      }
    }
  });

  // New routes for pump control
  server.on("/pump1_on", HTTP_GET, []() {
    manualPump1State = true;
    manualOverride1 = true;
    digitalWrite(output1, LOW); // Turn Pump 1 ON
   //String message = "Pump 1 turned ON manually.";
   // String subject = "Manual Control: Pump 1 ON - " + String(NOME_IRRIGADOR);
   // envioemail(message, subject);
    redirectToHome();
  });

  server.on("/pump1_off", HTTP_GET, []() {
    manualPump1State = false;
    manualOverride1 = false;
    digitalWrite(output1, HIGH); // Turn Pump 1 OFF
  //  String message = "Pump 1 turned OFF manually.";
  //  String subject = "Manual Control: Pump 1 OFF - " + String(NOME_IRRIGADOR);
  //  envioemail(message, subject);
    redirectToHome();
  });

  server.on("/pump2_on", HTTP_GET, []() {
    manualPump2State = true;
    manualOverride2 = true;
    digitalWrite(output2, LOW); // Turn Pump 2 ON
    //String message = "Pump 2 turned ON manually.";
    //String subject = "Manual Control: Pump 2 ON - " + String(NOME_IRRIGADOR);
    //envioemail(message, subject);
    redirectToHome();
  });

  server.on("/pump2_off", HTTP_GET, []() {
    manualPump2State = false;
    manualOverride2 = false;
    digitalWrite(output2, HIGH); // Turn Pump 2 OFF
   // String message = "Pump 2 turned OFF manually.";
   // String subject = "Manual Control: Pump 2 OFF - " + String(NOME_IRRIGADOR);
   // envioemail(message, subject);
    redirectToHome();
  });

  server.onNotFound([]() {
    server.send(404, "text/plain", "Not found");
  });

  server.begin();
  Serial.println("Servidor iniciado");

  // Enviar e-mail no power-up
  if (WiFi.status() == WL_CONNECTED) {
    String ip = WiFi.localIP().toString();
    String html = buildStartupEmailBody(ip);
    String assuntoparaemail = F("Startup ");
    assuntoparaemail += AppConfig::NOME_IRRIGADOR;
    assuntoparaemail += F(" - historico 7 dias");
    envioemail(html, assuntoparaemail);
  } else {
    Serial.println("Email de power-up nao enviado (sem WiFi).");
  }
}

void loop() {
  maintainWiFi();
  syncClockIfNeeded();
  maintainGoogleSheets();
  server.handleClient();

  // TIMER DE INTERVALO DOS CICLOS DA BOMBA
  unsigned long currentMillis2 = millis();
  if (currentMillis2 - previousMillis2 >= timerIntervaloBomba) {
    previousMillis2 = currentMillis2;
    String timerMsg = String(timerIntervaloBomba / 1000);
    timerMsg += F(" segundos - flag -> ");
    timerMsg += tempoBombaLigada;
    Serial.println(timerMsg);
    tempoBombaLigada = tempoBombaLigada + 1;
    if (tempoBombaLigada == qtdCiclosTimer) {
      tempoBombaLigada = 0;
    }
  }

  // ROTINA PARA COLETA DE DADOS DOS SENSORES E GRAVAÇÃO DE EEPROM
  unsigned long currentMillis = millis();
  if (currentMillis - previousMillis >= interval) {
    previousMillis = currentMillis;
    // LEITURA DO SENSOR 1
    medidasArray1[sampleIndex] = analogRead(humiditySensor1);
    medidasArray2[sampleIndex] = analogRead(humiditySensor2);

    sampleIndex = (sampleIndex + 1) % SENSOR_SAMPLE_SIZE;
    if (sampleCount < SENSOR_SAMPLE_SIZE) {
      sampleCount++;
    }

    long soma1 = 0;
    long soma2 = 0;
    for (int i = 0; i < sampleCount; i++) {
      soma1 += medidasArray1[i];
      soma2 += medidasArray2[i];
    }
    sensor1 = soma1 / sampleCount;
    sensor2 = soma2 / sampleCount;

    lastSensor1 = String(sensor1);
    lastSensor2 = String(sensor2);
    limiteCorrigidoMais1 = float(inputMessage1.toFloat() + indiceThreshold);
    limiteCorrigidoMenos1 = float(inputMessage1.toFloat() - indiceThreshold);
    limiteCorrigidoMais2 = float(inputMessage2.toFloat() + indiceThreshold);
    limiteCorrigidoMenos2 = float(inputMessage2.toFloat() - indiceThreshold);

    String sensorLog = F("S1-> ");
    sensorLog += sensor1;
    sensorLog += F(" / S2-> ");
    sensorLog += sensor2;
    sensorLog += F(" Inputmsg ");
    sensorLog += inputMessage1;
    sensorLog += '/';
    sensorLog += inputMessage2;
    sensorLog += F(" / BXHum-> ");
    sensorLog += String(flagBxHumidade1);
    sensorLog += F(" / tmp int ciclo bomba ");
    sensorLog += String(timerIntervaloBomba / 1000);
    sensorLog += F(" seg - flag da bomba-> ");
    sensorLog += tempoBombaLigada;
    Serial.println(sensorLog);
    addSensorSnapshotToHourlyStats(sensor1, sensor2);
  }

  // DEFINIR FLAG DE BAIXA UMIDADE
  if (sensor1 > limiteCorrigidoMais1 && histereseArmada1) {
    flagBxHumidade1 = true;
    histereseArmada1 = false;
  }
  if (sensor1 < limiteCorrigidoMenos1 && !histereseArmada1) {
    flagBxHumidade1 = false;
    histereseArmada1 = true;
  }
  if (sensor2 > limiteCorrigidoMais2 && histereseArmada2) {
    flagBxHumidade2 = true;
    histereseArmada2 = false;
  }
  if (sensor2 < limiteCorrigidoMenos2 && !histereseArmada2) {
    flagBxHumidade2 = false;
    histereseArmada2 = true;
  }

  // LIGAR BOMBA 1 E CONTAR
  if (manualOverride1) {
    digitalWrite(output1, manualPump1State ? LOW : HIGH);
    if (manualPump1State && flagBomba1Travada == 0) {
      unsigned long currentMillis3 = millis();
      if (currentMillis3 - previousMillis3 >= 1000) {
        previousMillis3 = currentMillis3;
        tempoB1Ligada = tempoB1Ligada + 1;
        addPumpRuntimeToHourlyStats(1, 1);
        if (tempoB1Ligada > tempoFlagBombaTravada) {
          flagBomba1Travada = 1;
          manualOverride1 = false;
          manualPump1State = false;
          digitalWrite(output1, HIGH);
          String assuntoparaemail = F("Bomba 1 foi travada. Funcionou por ");
          assuntoparaemail += tempoB1Ligada;
          assuntoparaemail += F(" segundos");
          String textoparaemail = assuntoparaemail;
          envioemail(textoparaemail, assuntoparaemail);
        }
      }
    }
  } else if (flagBxHumidade1 && tempoBombaLigada == 1 && flagBomba1Travada == 0) {
    digitalWrite(output1, LOW);
    unsigned long currentMillis3 = millis();
    if (currentMillis3 - previousMillis3 >= 1000) {
      previousMillis3 = currentMillis3;
      tempoB1Ligada = tempoB1Ligada + 1;
      addPumpRuntimeToHourlyStats(1, 1);
      if (tempoB1Ligada > tempoFlagBombaTravada) {
        flagBomba1Travada = 1;
        String assuntoparaemail = F("Bomba 1 foi travada. Funcionou por ");
        assuntoparaemail += tempoB1Ligada;
        assuntoparaemail += F(" segundos");
        String textoparaemail = assuntoparaemail;
        envioemail(textoparaemail, assuntoparaemail);
      }
    }
  } else {
    digitalWrite(output1, HIGH);
  }

  // LIGAR BOMBA 2 E CONTAR
  if (manualOverride2) {
    digitalWrite(output2, manualPump2State ? LOW : HIGH);
    if (manualPump2State && flagBomba2Travada == 0) {
      unsigned long currentMillis4 = millis();
      if (currentMillis4 - previousMillis4 >= 1000) {
        previousMillis4 = currentMillis4;
        tempoB2Ligada = tempoB2Ligada + 1;
        addPumpRuntimeToHourlyStats(2, 1);
        if (tempoB2Ligada > tempoFlagBombaTravada) {
          flagBomba2Travada = 1;
          manualOverride2 = false;
          manualPump2State = false;
          digitalWrite(output2, HIGH);
          String assuntoparaemail = F("Bomba 2 foi travada. Funcionou por ");
          assuntoparaemail += tempoB2Ligada;
          assuntoparaemail += F(" segundos");
          String textoparaemail = assuntoparaemail;
          envioemail(textoparaemail, assuntoparaemail);
        }
      }
    }
  } else if (flagBxHumidade2 && tempoBombaLigada == 2 && flagBomba2Travada == 0) {
    digitalWrite(output2, LOW);
    unsigned long currentMillis4 = millis();
    if (currentMillis4 - previousMillis4 >= 1000) {
      previousMillis4 = currentMillis4;
      tempoB2Ligada = tempoB2Ligada + 1;
      addPumpRuntimeToHourlyStats(2, 1);
      if (tempoB2Ligada > tempoFlagBombaTravada) {
        flagBomba2Travada = 1;
        String assuntoparaemail = F("Bomba 2 foi travada. Funcionou por ");
        assuntoparaemail += tempoB2Ligada;
        assuntoparaemail += F(" segundos");
        String textoparaemail = assuntoparaemail;
        envioemail(textoparaemail, assuntoparaemail);
      }
    }
  } else {
    digitalWrite(output2, HIGH);
  }

  bool pump1IsOn = digitalRead(output1) == LOW;
  bool pump2IsOn = digitalRead(output2) == LOW;
  handlePumpCycleTransition(1, pump1IsOn, lastPump1OutputState, pump1CycleState, manualOverride1, flagBomba1Travada == 1, manualPump1State, tempoB1Ligada);
  handlePumpCycleTransition(2, pump2IsOn, lastPump2OutputState, pump2CycleState, manualOverride2, flagBomba2Travada == 1, manualPump2State, tempoB2Ligada);

  // Temporizador para enviar email com info do valor do sensor
  unsigned long currentMillis1 = millis();
  if (currentMillis1 - previousMillis1 >= intervaloEnviarEmail) {
    bool wifiReadyForEmail = true;
    if (WiFi.status() != WL_CONNECTED) {
      Serial.println("WiFi desconectado. Tentando reconectar com timeout...");
      if (!connectWiFiWithTimeout(AppConfig::WIFI_CONNECT_TIMEOUT_MS)) {
        Serial.println("Falha na reconexao WiFi. Email periodico nao enviado.");
        wifiReadyForEmail = false;
      }
    }

    if (wifiReadyForEmail) {
      String ip = WiFi.localIP().toString();
      String textoparaemail = F("Sens1> ");
      textoparaemail += sensor1;
      textoparaemail += F(" / Sens2> ");
      textoparaemail += sensor2;
      textoparaemail += F(" \n - InputMessage1 -> ");
      textoparaemail += inputMessage1;
      textoparaemail += F(" \n - InputMessage2 -> ");
      textoparaemail += inputMessage2;
      textoparaemail += F(" - ");
      textoparaemail += __FILE__;
      String assuntoparaemail = F("Update regular do ");
      assuntoparaemail += AppConfig::NOME_IRRIGADOR;
      assuntoparaemail += F(" -- estou conectado no ");
      assuntoparaemail += WiFi.localIP().toString();
      String emailLog = textoparaemail;
      emailLog += F(" WiFi connected - Rede -> ");
      emailLog += ip;
      emailLog += F(" - ");
      emailLog += __FILE__;
      Serial.println(emailLog);
      envioemail(textoparaemail, assuntoparaemail);
    }

    previousMillis1 = currentMillis1;
  }

  // ENVIO DE EMAIL QUANDO BAIXA UMIDADE
  if (!flagAlertaBaixaUmidadePronto) {
    unsigned long currentMillis5 = millis();
    if (currentMillis5 - previousMillis5 >= intervaloEnviarEmail) {
      previousMillis5 = currentMillis5;
      flagAlertaBaixaUmidadePronto = true;
    }
  }

  float sens1 = limiteCorrigidoMais1 + 150;
  float sens2 = limiteCorrigidoMais2 + 150;
  if ((sensor1 > sens1 || sensor2 > sens2) && flagAlertaBaixaUmidadePronto && tempoBombaLigada == 8) {
    flagAlertaBaixaUmidadePronto = false;
    previousMillis5 = millis();
    String textoparaemail = F(" ALERTA DE BAIXA UMIDADE - Sens1> ");
    textoparaemail += sensor1;
    textoparaemail += F(" / Sens2> ");
    textoparaemail += sensor2;
    textoparaemail += F(" \n - InputMessage1 -> ");
    textoparaemail += inputMessage1;
    textoparaemail += F(" \n - InputMessage2 -> ");
    textoparaemail += inputMessage2;
    textoparaemail += F(" - ");
    textoparaemail += __FILE__;
    String assuntoparaemail = F("ALERTA DE BAIXA UMIDADE - Mensagem do ");
    assuntoparaemail += AppConfig::NOME_IRRIGADOR;
    assuntoparaemail += F(" IP ");
    assuntoparaemail += WiFi.localIP().toString();
    envioemail(textoparaemail, assuntoparaemail);
  }
}

bool initFileSystem() {
  if (!LittleFS.begin(true)) {
    Serial.println("Falha ao montar LittleFS.");
    return false;
  }

  Serial.println("LittleFS pronto.");
  return true;
}

bool ensureWiFiReadyForEmail() {
  if (WiFi.status() == WL_CONNECTED) {
    return true;
  }

  Serial.println("WiFi desconectado. Tentando reconectar para envio de email...");
  if (!connectWiFiWithTimeout(AppConfig::WIFI_CONNECT_TIMEOUT_MS)) {
    Serial.println("Falha na reconexao WiFi. Email nao enviado.");
    return false;
  }

  syncClockIfNeeded(true);
  return true;
}

void syncClockIfNeeded(bool forceAttempt) {
  if (WiFi.status() != WL_CONNECTED) {
    return;
  }

  time_t epochNow = time(nullptr);
  if (epochNow >= MIN_VALID_EPOCH) {
    clockSynchronized = true;
    return;
  }

  unsigned long currentMillis = millis();
  if (!forceAttempt && (currentMillis - lastClockSyncAttempt) < CLOCK_SYNC_RETRY_MS) {
    return;
  }

  lastClockSyncAttempt = currentMillis;
  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);

  struct tm timeinfo;
  if (getLocalTime(&timeinfo, 1000)) {
    clockSynchronized = true;
    char timeBuffer[24];
    strftime(timeBuffer, sizeof(timeBuffer), "%d/%m/%Y %H:%M:%S", &timeinfo);
    Serial.print("Relogio sincronizado em ");
    Serial.println(timeBuffer);
  } else {
    Serial.println("Relogio ainda nao sincronizado.");
  }
}

void initGoogleSheetsClientIfNeeded() {
  if (googleSheetsClientStarted || WiFi.status() != WL_CONNECTED) {
    return;
  }

  GSheet.setTokenCallback(googleSheetsTokenStatusCallback);
  GSheet.setPrerefreshSeconds(10 * 60);
  GSheet.begin(Secrets::GOOGLE_CLIENT_EMAIL, Secrets::GOOGLE_PROJECT_ID, Secrets::GOOGLE_PRIVATE_KEY);
  googleSheetsClientStarted = true;
  lastGoogleSheetsAttempt = millis();
  Serial.println("Cliente Google Sheets inicializado.");
}

void maintainGoogleSheets() {
  if (WiFi.status() != WL_CONNECTED) {
    return;
  }

  initGoogleSheetsClientIfNeeded();
  if (!googleSheetsClientStarted || !clockSynchronized || !GSheet.ready()) {
    return;
  }

  if (!ensureGoogleSheetsSheetTitle()) {
    return;
  }

  if (!ensureGoogleSheetsHeader()) {
    return;
  }

  unsigned long currentMillis = millis();
  if (currentMillis - lastGoogleSheetsAttempt < AppConfig::GOOGLE_SHEETS_UPDATE_INTERVAL_MS) {
    return;
  }

  lastGoogleSheetsAttempt = currentMillis;
  appendGoogleSheetsSnapshot();
}

bool ensureGoogleSheetsSheetTitle() {
  if (googleSheetsSheetTitle.length() > 0) {
    return true;
  }

  FirebaseJson response;
  if (!GSheet.get(&response, Secrets::GOOGLE_SPREADSHEET_ID)) {
    Serial.print("Falha ao consultar metadados da planilha Google Sheets: ");
    Serial.println(GSheet.errorReason());
    return false;
  }

  FirebaseJsonData result;
  response.get(result, "sheets/[0]/properties/title");
  if (!result.success) {
    Serial.println("Nao foi possivel identificar o nome da primeira aba da planilha.");
    return false;
  }

  googleSheetsSheetTitle = result.to<String>();
  googleSheetsSheetTitle.trim();
  if (googleSheetsSheetTitle.length() == 0) {
    Serial.println("A planilha retornou um nome de aba vazio.");
    return false;
  }

  String logMessage = F("Aba Google Sheets detectada: ");
  logMessage += googleSheetsSheetTitle;
  Serial.println(logMessage);
  return true;
}

String buildGoogleSheetsRange(const char* a1Range) {
  if (googleSheetsSheetTitle.length() == 0) {
    return String(a1Range);
  }

  String range = googleSheetsSheetTitle;
  range += '!';
  range += a1Range;
  return range;
}

bool ensureGoogleSheetsHeader() {
  if (googleSheetsHeaderEnsured) {
    return true;
  }

  FirebaseJson response;
  FirebaseJson valueRange;
  valueRange.add("majorDimension", "ROWS");
  valueRange.set("values/[0]/[0]", "timestamp_local");
  valueRange.set("values/[0]/[1]", "epoch");
  valueRange.set("values/[0]/[2]", "irrigador");
  valueRange.set("values/[0]/[3]", "sensor1");
  valueRange.set("values/[0]/[4]", "sensor2");
  valueRange.set("values/[0]/[5]", "limite1");
  valueRange.set("values/[0]/[6]", "limite2");
  valueRange.set("values/[0]/[7]", "baixa_umidade1");
  valueRange.set("values/[0]/[8]", "baixa_umidade2");
  valueRange.set("values/[0]/[9]", "bomba1_ligada");
  valueRange.set("values/[0]/[10]", "bomba2_ligada");
  valueRange.set("values/[0]/[11]", "manual1");
  valueRange.set("values/[0]/[12]", "manual2");
  valueRange.set("values/[0]/[13]", "travada1");
  valueRange.set("values/[0]/[14]", "travada2");
  String headerRange = buildGoogleSheetsRange(AppConfig::GOOGLE_SHEETS_HEADER_A1_RANGE);

  bool success = GSheet.values.update(
    &response,
    Secrets::GOOGLE_SPREADSHEET_ID,
    headerRange,
    &valueRange
  );

  if (!success) {
    Serial.print("Falha ao atualizar cabecalho Google Sheets: ");
    Serial.println(GSheet.errorReason());
    Serial.print("Range usado: ");
    Serial.println(headerRange);
    return false;
  }

  googleSheetsHeaderEnsured = true;
  Serial.println("Cabecalho Google Sheets atualizado.");
  return true;
}

bool appendGoogleSheetsSnapshot() {
  time_t epochNow = 0;
  if (!getCurrentEpoch(epochNow)) {
    Serial.println("Google Sheets aguardando relogio sincronizado.");
    return false;
  }

  const bool pump1IsOn = digitalRead(output1) == LOW;
  const bool pump2IsOn = digitalRead(output2) == LOW;
  FirebaseJson response;
  FirebaseJson valueRange;
  valueRange.add("majorDimension", "ROWS");
  valueRange.set("values/[0]/[0]", formatTimestamp(epochNow));
  valueRange.set("values/[0]/[1]", String(static_cast<long long>(epochNow)));
  valueRange.set("values/[0]/[2]", AppConfig::NOME_IRRIGADOR);
  valueRange.set("values/[0]/[3]", String(sensor1));
  valueRange.set("values/[0]/[4]", String(sensor2));
  valueRange.set("values/[0]/[5]", inputMessage1);
  valueRange.set("values/[0]/[6]", inputMessage2);
  valueRange.set("values/[0]/[7]", flagBxHumidade1 ? "1" : "0");
  valueRange.set("values/[0]/[8]", flagBxHumidade2 ? "1" : "0");
  valueRange.set("values/[0]/[9]", pump1IsOn ? "1" : "0");
  valueRange.set("values/[0]/[10]", pump2IsOn ? "1" : "0");
  valueRange.set("values/[0]/[11]", manualOverride1 ? "1" : "0");
  valueRange.set("values/[0]/[12]", manualOverride2 ? "1" : "0");
  valueRange.set("values/[0]/[13]", flagBomba1Travada == 1 ? "1" : "0");
  valueRange.set("values/[0]/[14]", flagBomba2Travada == 1 ? "1" : "0");
  String dataRange = buildGoogleSheetsRange(AppConfig::GOOGLE_SHEETS_DATA_A1_RANGE);

  bool success = GSheet.values.append(
    &response,
    Secrets::GOOGLE_SPREADSHEET_ID,
    dataRange,
    &valueRange
  );

  if (!success) {
    Serial.print("Falha ao enviar linha para Google Sheets: ");
    Serial.println(GSheet.errorReason());
    Serial.print("Range usado: ");
    Serial.println(dataRange);
    return false;
  }

  String logMessage = F("Google Sheets atualizado em ");
  logMessage += formatTimestamp(epochNow);
  logMessage += F(" | S1 ");
  logMessage += sensor1;
  logMessage += F(" | S2 ");
  logMessage += sensor2;
  Serial.println(logMessage);
  return true;
}

bool getCurrentEpoch(time_t& epochNow) {
  epochNow = time(nullptr);
  if (epochNow < MIN_VALID_EPOCH) {
    return false;
  }

  return true;
}

time_t getHourStart(time_t epochNow) {
  struct tm timeinfo;
  localtime_r(&epochNow, &timeinfo);
  timeinfo.tm_min = 0;
  timeinfo.tm_sec = 0;
  return mktime(&timeinfo);
}

time_t getDayStart(time_t epochNow) {
  struct tm timeinfo;
  localtime_r(&epochNow, &timeinfo);
  timeinfo.tm_hour = 0;
  timeinfo.tm_min = 0;
  timeinfo.tm_sec = 0;
  return mktime(&timeinfo);
}

String formatTimestamp(time_t timestamp) {
  if (timestamp < MIN_VALID_EPOCH) {
    return F("sem horario sincronizado");
  }

  struct tm timeinfo;
  localtime_r(&timestamp, &timeinfo);
  char buffer[24];
  strftime(buffer, sizeof(buffer), "%d/%m/%Y %H:%M", &timeinfo);
  return String(buffer);
}

String formatDayLabel(time_t timestamp) {
  if (timestamp < MIN_VALID_EPOCH) {
    return F("--/--");
  }

  struct tm timeinfo;
  localtime_r(&timestamp, &timeinfo);
  char buffer[12];
  strftime(buffer, sizeof(buffer), "%d/%m", &timeinfo);
  return String(buffer);
}

String formatDuration(unsigned long totalSeconds) {
  unsigned long hours = totalSeconds / 3600;
  unsigned long minutes = (totalSeconds % 3600) / 60;
  unsigned long seconds = totalSeconds % 60;

  String result;
  result.reserve(24);
  result += hours;
  result += F("h ");
  if (minutes < 10) {
    result += '0';
  }
  result += minutes;
  result += F("m ");
  if (seconds < 10) {
    result += '0';
  }
  result += seconds;
  result += 's';
  return result;
}

bool ensureHourlyStatsCurrent() {
  time_t epochNow = 0;
  if (!getCurrentEpoch(epochNow)) {
    return false;
  }

  time_t hourStart = getHourStart(epochNow);
  if (!currentHourLoaded) {
    currentHourlyStats = HourlyStats();
    currentHourlyStats.hourStart = hourStart;
    currentHourLoaded = true;
    return true;
  }

  if (currentHourlyStats.hourStart != hourStart) {
    persistHourlyStats(currentHourlyStats);
    currentHourlyStats = HourlyStats();
    currentHourlyStats.hourStart = hourStart;
  }

  return true;
}

void addSensorSnapshotToHourlyStats(long currentSensor1, long currentSensor2) {
  if (!ensureHourlyStatsCurrent()) {
    return;
  }

  currentHourlyStats.sensor1Sum += static_cast<unsigned long>(currentSensor1);
  currentHourlyStats.sensor2Sum += static_cast<unsigned long>(currentSensor2);
  currentHourlyStats.sensorSamples++;
}

void addPumpRuntimeToHourlyStats(uint8_t pumpNumber, unsigned long seconds) {
  if (!ensureHourlyStatsCurrent()) {
    return;
  }

  if (pumpNumber == 1) {
    currentHourlyStats.irrigSec1 += seconds;
  } else if (pumpNumber == 2) {
    currentHourlyStats.irrigSec2 += seconds;
  }
}

void addCycleToHourlyStats(uint8_t pumpNumber) {
  if (!ensureHourlyStatsCurrent()) {
    return;
  }

  if (pumpNumber == 1) {
    currentHourlyStats.cycles1++;
  } else if (pumpNumber == 2) {
    currentHourlyStats.cycles2++;
  }
}

void persistHourlyStats(const HourlyStats& stats) {
  if (stats.hourStart < MIN_VALID_EPOCH) {
    return;
  }

  char newLine[160];
  snprintf(
    newLine,
    sizeof(newLine),
    "%lld,%lu,%lu,%u,%u,%lu,%lu,%u\n",
    static_cast<long long>(stats.hourStart),
    stats.irrigSec1,
    stats.irrigSec2,
    stats.cycles1,
    stats.cycles2,
    stats.sensor1Sum,
    stats.sensor2Sum,
    stats.sensorSamples
  );

  String fileContent;
  File file = LittleFS.open(HOURLY_LOG_PATH, "r");
  if (file) {
    fileContent = file.readString();
    file.close();
  }

  if (fileContent.length() > 0 && !fileContent.endsWith("\n")) {
    fileContent += '\n';
  }
  fileContent += newLine;

  size_t lineCount = 0;
  for (size_t i = 0; i < fileContent.length(); i++) {
    if (fileContent[i] == '\n') {
      lineCount++;
    }
  }

  while (lineCount > HISTORY_HOURS) {
    int newlinePos = fileContent.indexOf('\n');
    if (newlinePos < 0) {
      break;
    }
    fileContent.remove(0, newlinePos + 1);
    lineCount--;
  }

  file = LittleFS.open(HOURLY_LOG_PATH, "w");
  if (!file) {
    Serial.println("Falha ao gravar historico horario.");
    return;
  }

  file.print(fileContent);
  file.close();
}

bool parseHourlyStatsLine(const String& rawLine, HourlyStats& stats) {
  String line = rawLine;
  line.trim();
  if (line.length() == 0 || line.length() >= 160) {
    return false;
  }

  char buffer[160];
  line.toCharArray(buffer, sizeof(buffer));

  char* context = nullptr;
  char* token = strtok_r(buffer, ",", &context);
  if (token == nullptr) {
    return false;
  }

  stats = HourlyStats();
  stats.hourStart = static_cast<time_t>(strtoll(token, nullptr, 10));

  token = strtok_r(nullptr, ",", &context);
  if (token == nullptr) {
    return false;
  }
  stats.irrigSec1 = strtoul(token, nullptr, 10);

  token = strtok_r(nullptr, ",", &context);
  if (token == nullptr) {
    return false;
  }
  stats.irrigSec2 = strtoul(token, nullptr, 10);

  token = strtok_r(nullptr, ",", &context);
  if (token == nullptr) {
    return false;
  }
  stats.cycles1 = static_cast<unsigned int>(strtoul(token, nullptr, 10));

  token = strtok_r(nullptr, ",", &context);
  if (token == nullptr) {
    return false;
  }
  stats.cycles2 = static_cast<unsigned int>(strtoul(token, nullptr, 10));

  token = strtok_r(nullptr, ",", &context);
  if (token == nullptr) {
    return false;
  }
  stats.sensor1Sum = strtoul(token, nullptr, 10);

  token = strtok_r(nullptr, ",", &context);
  if (token == nullptr) {
    return false;
  }
  stats.sensor2Sum = strtoul(token, nullptr, 10);

  token = strtok_r(nullptr, ",", &context);
  if (token == nullptr) {
    return false;
  }
  stats.sensorSamples = static_cast<unsigned int>(strtoul(token, nullptr, 10));

  return stats.hourStart >= MIN_VALID_EPOCH;
}

void accumulateDailyStats(DailyStats* days, const HourlyStats& stats) {
  for (size_t i = 0; i < HISTORY_DAYS; i++) {
    if (stats.hourStart >= days[i].dayStart && stats.hourStart < (days[i].dayStart + 86400)) {
      days[i].irrigSec1 += stats.irrigSec1;
      days[i].irrigSec2 += stats.irrigSec2;
      days[i].cycles1 += stats.cycles1;
      days[i].cycles2 += stats.cycles2;
      days[i].sensor1Sum += stats.sensor1Sum;
      days[i].sensor2Sum += stats.sensor2Sum;
      days[i].sensorSamples += stats.sensorSamples;
      break;
    }
  }
}

String buildLast7DaysTableHtml() {
  String html = F("<h3>Ultimos 7 dias</h3>");

  time_t epochNow = 0;
  if (!getCurrentEpoch(epochNow)) {
    html += F("<p>Historico indisponivel porque o relogio ainda nao foi sincronizado.</p>");
    return html;
  }

  DailyStats days[HISTORY_DAYS];
  time_t todayStart = getDayStart(epochNow);
  for (size_t i = 0; i < HISTORY_DAYS; i++) {
    days[i].dayStart = todayStart - static_cast<time_t>((HISTORY_DAYS - 1 - i) * 86400);
    days[i].label = formatDayLabel(days[i].dayStart);
  }

  File file = LittleFS.open(HOURLY_LOG_PATH, "r");
  if (file) {
    while (file.available()) {
      HourlyStats stats;
      if (parseHourlyStatsLine(file.readStringUntil('\n'), stats)) {
        accumulateDailyStats(days, stats);
      }
    }
    file.close();
  }

  if (currentHourLoaded) {
    accumulateDailyStats(days, currentHourlyStats);
  }

  html += F("<table border='1' cellpadding='4' cellspacing='0' style='border-collapse:collapse;'>");
  html += F("<tr><th rowspan='2'>Dia</th><th colspan='2'>Bomba 1</th><th colspan='2'>Bomba 2</th><th rowspan='2'>Media S1</th><th rowspan='2'>Media S2</th></tr>");
  html += F("<tr><th>Tempo</th><th>Ciclos</th><th>Tempo</th><th>Ciclos</th></tr>");
  for (size_t i = 0; i < HISTORY_DAYS; i++) {
    String avgSensor1 = F("-");
    String avgSensor2 = F("-");
    if (days[i].sensorSamples > 0) {
      avgSensor1 = String(days[i].sensor1Sum / days[i].sensorSamples);
      avgSensor2 = String(days[i].sensor2Sum / days[i].sensorSamples);
    }

    html += F("<tr><td>");
    html += days[i].label;
    html += F("</td><td>");
    html += formatDuration(days[i].irrigSec1);
    html += F("</td><td>");
    html += days[i].cycles1;
    html += F("</td><td>");
    html += formatDuration(days[i].irrigSec2);
    html += F("</td><td>");
    html += days[i].cycles2;
    html += F("</td><td>");
    html += avgSensor1;
    html += F("</td><td>");
    html += avgSensor2;
    html += F("</td></tr>");
  }
  html += F("</table>");

  return html;
}

String buildStartupEmailBody(const String& ipAddress) {
  String html = F("<html><body style='font-family:Arial,sans-serif;'>");
  html += buildLast7DaysTableHtml();
  html += F("<p style='margin-top:16px;font-size:12px;color:#5b6b68;'>");
  html += __FILE__;
  html += F(" | IP ");
  html += ipAddress;
  html += F("</p></body></html>");
  return html;
}

String buildCycleSummaryEmailBody(uint8_t pumpNumber, const PumpCycleState& cycle, unsigned long durationSeconds, const String& reason) {
  time_t finishedAt = 0;
  getCurrentEpoch(finishedAt);

  String html = F("<html><body style='font-family:Arial,sans-serif;'>");
  html += F("<h2>Resumo do ciclo de irrigacao</h2>");
  html += F("<table border='1' cellpadding='4' cellspacing='0' style='border-collapse:collapse;'>");
  html += F("<tr><td>Unidade</td><td>");
  html += AppConfig::NOME_IRRIGADOR;
  html += F("</td></tr><tr><td>Bomba</td><td>");
  html += String(pumpNumber);
  html += F("</td></tr><tr><td>Modo</td><td>");
  html += cycle.manual ? F("manual") : F("automatico");
  html += F("</td></tr><tr><td>Inicio</td><td>");
  html += formatTimestamp(cycle.startedAt);
  html += F("</td></tr><tr><td>Fim</td><td>");
  html += formatTimestamp(finishedAt);
  html += F("</td></tr><tr><td>Duracao</td><td>");
  html += formatDuration(durationSeconds);
  html += F("</td></tr><tr><td>Motivo do encerramento</td><td>");
  html += reason;
  html += F("</td></tr><tr><td>Sensor 1</td><td>");
  html += cycle.sensorStart1;
  html += F(" -> ");
  html += sensor1;
  html += F("</td></tr><tr><td>Sensor 2</td><td>");
  html += cycle.sensorStart2;
  html += F(" -> ");
  html += sensor2;
  html += F("</td></tr><tr><td>Limites</td><td>S1 ");
  html += String(cycle.threshold1, 0);
  html += F(" / S2 ");
  html += String(cycle.threshold2, 0);
  html += F("</td></tr></table>");
  html += buildLast7DaysTableHtml();
  html += F("</body></html>");
  return html;
}

void startPumpCycle(PumpCycleState& cycle, bool manualMode) {
  cycle = PumpCycleState();
  cycle.active = true;
  cycle.manual = manualMode;
  getCurrentEpoch(cycle.startedAt);
  cycle.sensorStart1 = sensor1;
  cycle.sensorStart2 = sensor2;
  cycle.threshold1 = inputMessage1.toFloat();
  cycle.threshold2 = inputMessage2.toFloat();
}

String getCycleEndReason(const PumpCycleState& cycle, bool travada, bool manualRequestedOn) {
  if (travada) {
    return F("travamento por tempo maximo");
  }

  if (cycle.manual && !manualRequestedOn) {
    return F("desligamento manual");
  }

  if (cycle.manual) {
    return F("ciclo manual encerrado");
  }

  return F("umidade normalizada");
}

void finishPumpCycle(uint8_t pumpNumber, PumpCycleState& cycle, bool travada, bool manualRequestedOn, long& durationSeconds) {
  if (!cycle.active) {
    durationSeconds = 0;
    return;
  }

  addCycleToHourlyStats(pumpNumber);
  String reason = getCycleEndReason(cycle, travada, manualRequestedOn);

  if (ensureWiFiReadyForEmail()) {
    String subject = F("Resumo do ciclo da bomba ");
    subject += String(pumpNumber);
    subject += F(" - ");
    subject += AppConfig::NOME_IRRIGADOR;
    String body = buildCycleSummaryEmailBody(pumpNumber, cycle, durationSeconds, reason);
    envioemail(body, subject);
  } else {
    Serial.println("Resumo do ciclo nao enviado por falta de WiFi.");
  }

  durationSeconds = 0;
  cycle = PumpCycleState();
}

void handlePumpCycleTransition(uint8_t pumpNumber, bool isOn, bool& previousState, PumpCycleState& cycle, bool manualMode, bool travada, bool manualRequestedOn, long& durationSeconds) {
  if (isOn && !previousState) {
    durationSeconds = 0;
    startPumpCycle(cycle, manualMode);
  } else if (!isOn && previousState) {
    finishPumpCycle(pumpNumber, cycle, travada, manualRequestedOn, durationSeconds);
  } else if (isOn && !cycle.active) {
    startPumpCycle(cycle, manualMode);
  }

  previousState = isOn;
}

bool connectWiFiWithTimeout(unsigned long timeoutMs) {
  WiFi.begin(Secrets::WIFI_SSID, Secrets::WIFI_PASSWORD);
  unsigned long startAttemptTime = millis();

  while (WiFi.status() != WL_CONNECTED && (millis() - startAttemptTime) < timeoutMs) {
    Serial.print(".");
    delay(500);
  }
  Serial.println();

  return WiFi.status() == WL_CONNECTED;
}

void maintainWiFi() {
  if (WiFi.status() == WL_CONNECTED) {
    return;
  }

  unsigned long now = millis();
  if (now - lastWiFiReconnectAttempt < AppConfig::WIFI_RECONNECT_INTERVAL_MS) {
    return;
  }

  lastWiFiReconnectAttempt = now;
  Serial.println("WiFi desconectado. Tentando reconectar...");
  WiFi.disconnect(false);
  WiFi.begin(Secrets::WIFI_SSID, Secrets::WIFI_PASSWORD);
}

bool tryParseThreshold(const String& rawValue, float& parsedValue) {
  if (rawValue.length() == 0) {
    return false;
  }

  char* endPtr = nullptr;
  parsedValue = strtof(rawValue.c_str(), &endPtr);
  if (endPtr == rawValue.c_str() || *endPtr != '\0') {
    return false;
  }

  return parsedValue >= 0.0f && parsedValue <= 4095.0f;
}

void saveThresholds() {
  preferences.begin("MemEProm1", false);
  preferences.putString("Limite1", inputMessage1);
  preferences.putString("Limite2", inputMessage2);
  preferences.end();
}

void googleSheetsTokenStatusCallback(TokenInfo info) {
  if (info.status == token_status_error) {
    GSheet.printf(
      "Google Sheets token erro: %s\n",
      GSheet.getTokenError(info).c_str()
    );
    return;
  }

  GSheet.printf(
    "Google Sheets token status: %s\n",
    GSheet.getTokenStatus(info).c_str()
  );
}

/* Callback function to get the Email sending status */
void smtpCallback(SMTP_Status status) {
  Serial.println(status.info());
  if (status.success()) {
    Serial.println("----------------");
    MailClient.printf("Message sent success: %d\n", status.completedCount());
    MailClient.printf("Message sent failed: %d\n", status.failedCount());
    Serial.println("----------------\n");
    for (size_t i = 0; i < smtp.sendingResult.size(); i++) {
      SMTP_Result result = smtp.sendingResult.getItem(i);
      MailClient.printf("Message No: %d\n", i + 1);
      MailClient.printf("Status: %s\n", result.completed ? "success" : "failed");
      MailClient.printf("Date/Time: %s\n", MailClient.Time.getDateTimeString(result.timestamp, "%B %d, %Y %H:%M:%S").c_str());
      MailClient.printf("Recipient: %s\n", result.recipients.c_str());
      MailClient.printf("Subject: %s\n", result.subject.c_str());
    }
    Serial.println("----------------\n");
    smtp.sendingResult.clear();
  }
}

void envioemail(String htmlMsg, String assuntoEmail) {
  MailClient.networkReconnect(true);
  smtp.debug(0);
  smtp.callback(smtpCallback);

  Session_Config config;
  config.server.host_name = Secrets::SMTP_HOST;
  config.server.port = AppConfig::SMTP_PORT;
  config.login.email = Secrets::AUTHOR_EMAIL;
  config.login.password = Secrets::AUTHOR_PASSWORD;
  config.login.user_domain = F("127.0.0.1");
  config.time.ntp_server = F("pool.ntp.org,time.nist.gov");
  config.time.gmt_offset = 3;
  config.time.day_light_offset = 0;

  SMTP_Message message;
  message.sender.name = AppConfig::NOME_IRRIGADOR;
  message.sender.email = Secrets::AUTHOR_EMAIL;
  message.subject = String(assuntoEmail);
  message.addRecipient(F("Someone"), Secrets::RECIPIENT_EMAIL);
  message.text.flowed = true;
  message.html.content = htmlMsg.c_str();
  message.text.charSet = F("us-ascii");
  message.text.transfer_encoding = Content_Transfer_Encoding::enc_7bit;
  message.priority = esp_mail_smtp_priority::esp_mail_smtp_priority_low;
  message.addHeader(F("Message-ID: "));

  if (!smtp.connect(&config)) {
    MailClient.printf("Connection error, Status Code: %d, Error Code: %d, Reason: %s\n", smtp.statusCode(), smtp.errorCode(), smtp.errorReason().c_str());
    return;
  }
  if (!smtp.isLoggedIn()) {
    Serial.println("Not yet logged in.");
  } else {
    if (smtp.isAuthenticated())
      Serial.println("Successfully logged in.");
    else
      Serial.println("Connected with no Auth.");
  }
  if (!MailClient.sendMail(&smtp, &message))
    MailClient.printf("Error, Status Code: %d, Error Code: %d, Reason: %s\n", smtp.statusCode(), smtp.errorCode(), smtp.errorReason().c_str());
  MailClient.printf("Free Heap: %d\n", MailClient.getFreeHeap());
}
