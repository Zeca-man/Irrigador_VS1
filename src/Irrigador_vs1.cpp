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
#include <Preferences.h>
#include <Update.h>
#include <NTPClient.h>
#include <WiFiUdp.h>

const char* ntpServer = "pool.ntp.org";
const long gmtOffset_sec = -10800;
const int daylightOffset_sec = 0;

long timerIntervaloBomba = 10000;   // Tamanho do ciclo para ligar e desligar bomba
#define NOME_IRRIGADOR "IRRIGADOR_VS1"  // Nome do irrigador
float indiceThreshold = 40;
long intervaloEnviarEmailTemp = 24;  // Valor em horas
short qtdCiclosTimer = 12;
long tempoFlagBombaTravada = 250;

// CONFIGURACAO EMAIL
#define WIFI_SSID "Boituva_2"
#define WIFI_PASSWORD "zecalindo2023"
#define SMTP_HOST "smtp.gmail.com"
#define SMTP_PORT esp_mail_smtp_port_587
#define AUTHOR_EMAIL "esp32teste1234@gmail.com"
#define AUTHOR_PASSWORD "siqh lxfx fqvl xvdt"
#define RECIPIENT_EMAIL "jstagni@gmail.com"
String mensagemEmail = "Mensagem Default";
String assuntoEmail = "Assunto Default";

// OTA CONFIG
const char* host = "esp32_ota";


Preferences preferences;

// Define the SMTP Session object which used for SMTP transport
SMTPSession smtp;

// Define the session config data which used to store the TCP session configuration
ESP_Mail_Session session;

// Callback function to get the Email sending status
void smtpCallback(SMTP_Status status);

const int output1 = 19;  // GPIO comando rele 1
const int output2 = 5;   // GPIO comando rele 2
const int ledPlaca = 2;  // GPIO comando led
const int humiditySensor1 = 32;
const int humiditySensor2 = 34;

// Default Threshold sensor1 Value
String lastSensor1 = "0";
String lastSensor2 = "0";
String enableArmChecked1 = "checked";
String enableArmChecked2 = "checked";

String inputMessage1;
String inputMessage2;
String flagmudancaestadobomba1 = "true";
String flagmudancaestadobomba2 = "true";
long intervaloEnviarEmail = 3600000;

long sensor1 = 0;
long sensor2 = 0;
short flagBomba2 = 0;
short flagBomba1Travada = 0;
short flagBomba2Travada = 0;
short flagBxHumidade1 = 0;
short flagBxHumidade2 = 0;
short flag5 = 1;
short flag6 = 1;
short flag7 = 1;
short flag8 = 1;

// New variables for manual pump control
bool manualPump1State = false; // Tracks manual state of Pump 1 (true = ON, false = OFF)
bool manualPump2State = false; // Tracks manual state of Pump 2 (true = ON, false = OFF)
bool manualOverride1 = false;  // Indicates if Pump 1 is manually controlled
bool manualOverride2 = false;  // Indicates if Pump 2 is manually controlled

int medidasArray1[7];
int medidasArray2[7];
int sss;

short flagEmail1 = 0; // flag para comandar o envio de email
short flagEmail2 = 0; // flag para comandar o envio de email

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

long interval = 5000;    // tempo entre medidas do sensor
long tempoB1Ligada = 0;  // tempo da B1 ligada no ciclo de irrigação
long tempoB2Ligada = 0;  // tempo da B2 ligada no ciclo de irrigação

// HTML do controle de humidade - pagina principal
const char index_html[] PROGMEM = R"rawliteral(
<!DOCTYPE HTML>
<html>
<head>
  <title>%NOME_IRRIGADOR%</title>
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <style>
    body { font-family: Arial; text-align: center; }
    .button { padding: 10px 20px; margin: 5px; text-decoration: none; color: white; border-radius: 5px; }
    .button-on { background-color: #4CAF50; }
    .button-off { background-color: #f44336; }
    .indicator { display: inline-block; width: 15px; height: 15px; border-radius: 50%; margin-left: 10px; }
    .pump-on { background-color: #4CAF50; }
    .pump-off { background-color: #f44336; }
  </style>
</head>
<body>
  <h2>%NOME_IRRIGADOR%</h2>
  
  <p>Sensor 1 - %HUMIDITY_1%</p>
  <p>
    Sensor 1 
    <input type="number" name="threshold_input1" value="%THRESHOLD1%">
    <input type="submit" value="Set Threshold" onclick="location.href='/get?threshold_input1=' + document.getElementsByName('threshold_input1')[0].value">
  </p>

  <h3>Pump 1 Control</h3>
  <p>
    <a href="/pump1_on" class="button button-on">ON</a>
    <span class="indicator %PUMP1_INDICATOR%"></span>
    <a href="/pump1_off" class="button button-off">OFF</a>
    <span class="indicator %PUMP1_INDICATOR%"></span>
  </p>

  <p>Sensor 2 - %HUMIDITY_2%</p>
  <p>
    Sensor 2 
    <input type="number" name="threshold_input2" value="%THRESHOLD2%">
    <input type="submit" value="Set Threshold" onclick="location.href='/get?threshold_input2=' + document.getElementsByName('threshold_input2')[0].value">
  </p>

  <h3>Pump 2 Control</h3>
  <p>
    <a href="/pump2_on" class="button button-on">ON</a>
    <span class="indicator %PUMP2_INDICATOR%"></span>
    <a href="/pump2_off" class="button button-off">OFF</a>
    <span class="indicator %PUMP2_INDICATOR%"></span>
  </p>
</body>
</html>
)rawliteral";

// Página de login para OTA
const char* loginIndex = 
 "<html>"
 "<head>"
 "<title>ESP32 OTA Login Page</title>"
 "</head>"
 "<body>"
 "<h1>ESP32 OTA - <<NOME_IRRIGADOR>> -Login Page</h1>"
 "<form name='loginForm' action='/serverIndex' method='POST'>"
 "<label>Username:</label>"
 "<input type='text' name='username'><br>"
 "<label>Password:</label>"
 "<input type='password' name='password'><br>"
 "<input type='submit' value='Login'>"
 "</form>"
 "</body>"
 "</html>";

// Página de atualização de firmware
const char* serverIndex = 
 "<html>"
 "<head>"
 "<title>ESP32 OTA Update</title>"
 "</head>"
 "<body>"
 "<h1>ESP32 OTA Update</h1>"
 "<form method='POST' action='/update' enctype='multipart/form-data'>"
 "<input type='file' name='update'>"
 "<input type='submit' value='Update'>"
 "</form>"
 "<div id='prg'>progress: 0%</div>"
 "</body>"
 "</html>";

// WebServer instance
WebServer server(80);

// Replaces placeholder with sensor values
String processor(const String& var) {
  if (var == "NOME_IRRIGADOR") {
    return String(NOME_IRRIGADOR);
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

const char* PARAM_INPUT_1_TH = "threshold_input1";
const char* PARAM_INPUT_2_TH = "threshold_input2";

// Forward declaration of envioemail
void envioemail(String htmlMsg, String assuntoEmail);

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
  Serial.println("Saida 1 e 2 colocada em HIGH");
  intervaloEnviarEmail = intervaloEnviarEmailTemp * 3600000;

  // Leitura da EEPROM para atualizar a variável inputMessage
  preferences.begin("MemEProm1", false);
  inputMessage1 = preferences.getString("Limite1", "0");
  inputMessage2 = preferences.getString("Limite2", "0");
  preferences.end();

  // Conectar ao WiFi
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  while (WiFi.status() != WL_CONNECTED) {
    Serial.print(".");
    delay(500);
  }
  Serial.println();
  Serial.println("WiFi connected - Rede -> " + String(WIFI_SSID));
  Serial.print("IP -> ");
  Serial.println(WiFi.localIP());
  Serial.println(__FILE__);

  // Configurar MDNS para OTA
  if (!MDNS.begin(host)) {
    Serial.println("Error setting up MDNS responder!");
    while (1) {
      delay(1000);
    }
  }
  Serial.println("mDNS responder started");

  // Configurar rotas do servidor
  server.on("/", HTTP_GET, []() {
    String html = String(index_html);
    html.replace("%NOME_IRRIGADOR%", processor("NOME_IRRIGADOR"));
    html.replace("%HUMIDITY_1%", processor("HUMIDITY_1"));
    html.replace("%THRESHOLD1%", processor("THRESHOLD1"));
    html.replace("%HUMIDITY_2%", processor("HUMIDITY_2"));
    html.replace("%THRESHOLD2%", processor("THRESHOLD2"));
    html.replace("%PUMP1_INDICATOR%", processor("PUMP1_INDICATOR"));
    html.replace("%PUMP2_INDICATOR%", processor("PUMP2_INDICATOR"));
    server.send(200, "text/html", html);
  });

  server.on("/get", HTTP_GET, []() {
    if (server.hasArg(PARAM_INPUT_1_TH)) {
      inputMessage1 = server.arg(PARAM_INPUT_1_TH);
    }
    if (server.hasArg(PARAM_INPUT_2_TH)) {
      inputMessage2 = server.arg(PARAM_INPUT_2_TH);
    }
    server.send(200, "text/html", "HTTP GET request sent to your ESP.<br><a href='/'>Return to Home Page</a>");
  });

  // OTA Routes
  server.on("/ota", HTTP_GET, []() {
    server.sendHeader("Connection", "close");
    server.send(200, "text/html", loginIndex);
  });

  server.on("/serverIndex", HTTP_GET, []() {
    server.sendHeader("Connection", "close");
    server.send(200, "text/html", serverIndex);
  });

  server.on("/update", HTTP_POST, []() {
    server.sendHeader("Connection", "close");
    server.send(200, "text/plain", (Update.hasError()) ? "FAIL" : "OK");
    ESP.restart();
  }, []() {
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
    server.send(200, "text/html", "Pump 1 turned ON.<br><a href='/'>Return to Home Page</a>");
  });

  server.on("/pump1_off", HTTP_GET, []() {
    manualPump1State = false;
    manualOverride1 = false;
    digitalWrite(output1, HIGH); // Turn Pump 1 OFF
  //  String message = "Pump 1 turned OFF manually.";
  //  String subject = "Manual Control: Pump 1 OFF - " + String(NOME_IRRIGADOR);
  //  envioemail(message, subject);
    server.send(200, "text/html", "Pump 1 turned OFF.<br><a href='/'>Return to Home Page</a>");
  });

  server.on("/pump2_on", HTTP_GET, []() {
    manualPump2State = true;
    manualOverride2 = true;
    digitalWrite(output2, LOW); // Turn Pump 2 ON
    //String message = "Pump 2 turned ON manually.";
    //String subject = "Manual Control: Pump 2 ON - " + String(NOME_IRRIGADOR);
    //envioemail(message, subject);
    server.send(200, "text/html", "Pump 2 turned ON.<br><a href='/'>Return to Home Page</a>");
  });

  server.on("/pump2_off", HTTP_GET, []() {
    manualPump2State = false;
    manualOverride2 = false;
    digitalWrite(output2, HIGH); // Turn Pump 2 OFF
   // String message = "Pump 2 turned OFF manually.";
   // String subject = "Manual Control: Pump 2 OFF - " + String(NOME_IRRIGADOR);
   // envioemail(message, subject);
    server.send(200, "text/html", "Pump 2 turned OFF.<br><a href='/'>Return to Home Page</a>");
  });

  server.onNotFound([]() {
    server.send(404, "text/plain", "Not found");
  });

  server.begin();
  Serial.println("Servidor iniciado");

  // Enviar e-mail no power-up
  String html = String(index_html);
  html.replace("%NOME_IRRIGADOR%", processor("NOME_IRRIGADOR"));
  html.replace("%HUMIDITY_1%", processor("HUMIDITY_1"));
  html.replace("%THRESHOLD1%", processor("THRESHOLD1"));
  html.replace("%HUMIDITY_2%", processor("HUMIDITY_2"));
  html.replace("%THRESHOLD2%", processor("THRESHOLD2"));
  String ip = WiFi.localIP().toString();
  String assuntoparaemail = "Genius PowerUp! A unidade " + String(NOME_IRRIGADOR) + " acabou de ligar. Conectado no " + String(ip) + " " + String(__FILE__);
  envioemail(html, assuntoparaemail);
}

void loop() {
  server.handleClient();

  // TIMER DE INTERVALO DOS CICLOS DA BOMBA
  unsigned long currentMillis2 = millis();
  if (currentMillis2 - previousMillis2 >= timerIntervaloBomba) {
    previousMillis2 = currentMillis2;
    Serial.println(String(timerIntervaloBomba / 1000) + " segundos - flag -> " + String(tempoBombaLigada));
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
    long senstemp1 = analogRead(humiditySensor1);
    medidasArray1[sss] = senstemp1;
    sensor1 = (medidasArray1[1] + medidasArray1[2] + medidasArray1[3] + medidasArray1[4] + medidasArray1[5]) / 5;
    long senstemp2 = analogRead(humiditySensor2);
    medidasArray2[sss] = senstemp2;
    sensor2 = (medidasArray2[1] + medidasArray2[2] + medidasArray2[3] + medidasArray2[4] + medidasArray2[5]) / 5;
    sss++;
    if (sss > 5) {
      sss = 0;
    }

    lastSensor1 = String(sensor1);
    lastSensor2 = String(sensor2);
    limiteCorrigidoMais1 = float(inputMessage1.toFloat() + indiceThreshold);
    limiteCorrigidoMenos1 = float(inputMessage1.toFloat() - indiceThreshold);
    limiteCorrigidoMais2 = float(inputMessage2.toFloat() + indiceThreshold);
    limiteCorrigidoMenos2 = float(inputMessage2.toFloat() - indiceThreshold);

    // Verifica se variável mudou e grava na EEPROM
    preferences.begin("MemEProm1", false);
    String valortemp1 = preferences.getString("Limite1", "0");
    String valortemp2 = preferences.getString("Limite2", "0");
    if (valortemp1 != inputMessage1) {
      preferences.putString("Limite1", inputMessage1);
    }
    if (valortemp2 != inputMessage2) {
      preferences.putString("Limite2", inputMessage2);
    }
    preferences.end();

    Serial.println("S1-> " + String(sensor1) + " / S2-> " + String(sensor2) + " Inputmsg " + inputMessage1 + "/" + inputMessage2 + " / BXHum-> " +
                   String(flagBxHumidade1) + " / tmp int ciclo bomba " +
                   String(timerIntervaloBomba / 1000) + " seg - flag da bomba-> " + String(tempoBombaLigada));
  }

  // DEFINIR FLAG DE BAIXA UMIDADE
  if (sensor1 > limiteCorrigidoMais1 && flag5 == 1) {
    flagBxHumidade1 = 1;
    flag5 = 0;
  }
  if (sensor1 < limiteCorrigidoMenos1 && flag5 == 0) {
    flagBxHumidade1 = 0;
    flag5 = 1;
  }
  if (sensor2 > limiteCorrigidoMais2 && flag6 == 1) {
    flagBxHumidade2 = 1;
    flag6 = 0;
  }
  if (sensor2 < limiteCorrigidoMenos2 && flag6 == 0) {
    flagBxHumidade2 = 0;
    flag6 = 1;
  }

  // LIGAR BOMBA 1 E CONTAR
  if (manualOverride1) {
    digitalWrite(output1, manualPump1State ? LOW : HIGH);
    if (manualPump1State && flagBomba1Travada == 0) {
      unsigned long currentMillis3 = millis();
      if (currentMillis3 - previousMillis3 >= 1000) {
        previousMillis3 = currentMillis3;
        tempoB1Ligada = tempoB1Ligada + 1;
        if (tempoB1Ligada > tempoFlagBombaTravada) {
          flagBomba1Travada = 1;
          manualOverride1 = false;
          manualPump1State = false;
          digitalWrite(output1, HIGH);
          String assuntoparaemail = String("Bomba 1 foi travada. Funcionou por " + String(tempoB1Ligada) + " segundos");
          String textoparaemail = assuntoparaemail;
          envioemail(textoparaemail, assuntoparaemail);
        }
      }
    }
  } else if (flagBxHumidade1 == 1 && tempoBombaLigada == 1 && flagBomba1Travada == 0) {
    digitalWrite(output1, LOW);
    unsigned long currentMillis3 = millis();
    if (currentMillis3 - previousMillis3 >= 1000) {
      previousMillis3 = currentMillis3;
      tempoB1Ligada = tempoB1Ligada + 1;
      if (tempoB1Ligada > tempoFlagBombaTravada) {
        flagBomba1Travada = 1;
        String assuntoparaemail = String("Bomba 1 foi travada. Funcionou por " + String(tempoB1Ligada) + " segundos");
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
        if (tempoB2Ligada > tempoFlagBombaTravada) {
          flagBomba2Travada = 1;
          manualOverride2 = false;
          manualPump2State = false;
          digitalWrite(output2, HIGH);
          String assuntoparaemail = String("Bomba 2 foi travada. Funcionou por " + String(tempoB2Ligada) + " segundos");
          String textoparaemail = assuntoparaemail;
          envioemail(textoparaemail, assuntoparaemail);
        }
      }
    }
  } else if (flagBxHumidade2 == 1 && tempoBombaLigada == 2 && flagBomba2Travada == 0) {
    digitalWrite(output2, LOW);
    unsigned long currentMillis4 = millis();
    if (currentMillis4 - previousMillis4 >= 1000) {
      previousMillis4 = currentMillis4;
      tempoB2Ligada = tempoB2Ligada + 1;
      if (tempoB2Ligada > tempoFlagBombaTravada) {
        flagBomba2Travada = 1;
        String assuntoparaemail = String("Bomba 2 foi travada. Funcionou por " + String(tempoB2Ligada) + " segundos");
        String textoparaemail = assuntoparaemail;
        envioemail(textoparaemail, assuntoparaemail);
      }
    }
  } else {
    digitalWrite(output2, HIGH);
  }

  // ENVIO DE EMAIL QUANDO DA MUDANÇA DE ESTADO SENSOR 1
  if (flagBxHumidade1 == 1 && flagEmail1 == 0) {
    String textoparaemail = String("Bomba ligada - Sensor1 -> " + String(sensor1) + " - Sensor2 -> " + String(sensor2) + " - dado inserido ->" + String(float(inputMessage1.toFloat())));
    String assuntoparaemail = String("Ligando a Bomba 1 - Mensagem do irrigador");
    Serial.println(textoparaemail);
    flagEmail1 = 1;
    envioemail(textoparaemail, assuntoparaemail);
  }
  if (flagBxHumidade1 == 0 && flagEmail1 == 1) {
    String textoparaemail = String("Bomba 1 funcionou por " + String(tempoB1Ligada) + " segundos. Dados do sensores - Sensor1 -> " + String(sensor1) + " - Sensor2 -> " + String(sensor2));
    String assuntoparaemail = String("Resumo da ativação da bomba 1. Funcionou por " + String(tempoB1Ligada) + " segundos. Status Bomba 1 Desligada");
    Serial.println(textoparaemail);
    tempoB1Ligada = 0;
    flagEmail1 = 0;
    envioemail(textoparaemail, assuntoparaemail);
    flagmudancaestadobomba1 = String("true");
  }

  // ENVIO DE EMAIL QUANDO DA MUDANÇA DE ESTADO SENSOR 2
  if (flagBxHumidade2 == 1 && flagEmail2 == 0) {
    String textoparaemail = String("Bomba ligada - Sensor1 -> " + String(sensor1) + " - Sensor2 -> " + String(sensor2) + " - dado inserido ->" + String(float(inputMessage2.toFloat())));
    String assuntoparaemail = String("Ligando a Bomba 2 - Mensagem do irrigador");
    Serial.println(textoparaemail);
    flagEmail2 = 1;
    envioemail(textoparaemail, assuntoparaemail);
  }
  if (flagBxHumidade2 == 0 && flagEmail2 == 1) {
    String textoparaemail = String("Bomba 2 funcionou por " + String(tempoB2Ligada) + " segundos. Dados do sensores - Sensor1 -> " + String(sensor1) + " - Sensor2 -> " + String(sensor2));
    String assuntoparaemail = String("Resumo da ativação da bomba 2. Funcionou por " + String(tempoB2Ligada) + " segundos. Status Bomba 2 Desligada");
    Serial.println(textoparaemail);
    tempoB2Ligada = 0;
    flagEmail2 = 0;
    envioemail(textoparaemail, assuntoparaemail);
    flagmudancaestadobomba2 = String("true");
  }

  // Temporizador para enviar email com info do valor do sensor
  unsigned long currentMillis1 = millis();
  if (currentMillis1 - previousMillis1 >= intervaloEnviarEmail) {
    Serial.println("Reiniciar WIFI pelo temporizador");
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
    while (WiFi.status() != WL_CONNECTED) {
      Serial.print(".");
      delay(1000);
    }
    Serial.println();
    Serial.println("WiFi connected.");
    String ip = WiFi.localIP().toString();
    String textoparaemail = String("Sens1> " + String(sensor1) + " / Sens2> " + String(sensor2) + " \n - InputMessage1 -> " + inputMessage1 + " \n - InputMessage2 -> " +
                                  inputMessage2 + String(" - ") + String(__FILE__));
    String assuntoparaemail = "Update regular do " + String(NOME_IRRIGADOR) + " -- estou conectado no " + String(WiFi.localIP());
    Serial.println(textoparaemail + " WiFi connected - Rede -> " + ip + String(" - ") + String(__FILE__));
    envioemail(textoparaemail, assuntoparaemail);
    previousMillis1 = currentMillis1;
  }

  // ENVIO DE EMAIL QUANDO BAIXA UMIDADE
  float sens1 = limiteCorrigidoMais1 + 150;
  float sens2 = limiteCorrigidoMais2 + 150;
  if ((sensor1 > sens1 || sensor2 > sens2) && flag7 == 1 && tempoBombaLigada == 8) {
    flag7 = 0;
    String textoparaemail = String(" ALERTA DE BAIXA UMIDADE - Sens1> " + String(sensor1) + " / Sens2> " + String(sensor2) + " \n - InputMessage1 -> " +
                                  inputMessage1 + " \n - InputMessage2 -> " + inputMessage2 + String(" - ") + String(__FILE__));
    String assuntoparaemail = "ALERTA DE BAIXA UMIDADE - Mensagem do " + String(NOME_IRRIGADOR) + " IP " + String(WiFi.localIP());
    envioemail(textoparaemail, assuntoparaemail);
    unsigned long currentMillis5 = millis();
    if (currentMillis5 - previousMillis5 >= intervaloEnviarEmail) {
      previousMillis5 = currentMillis5;
      flag7 = 1;
    }
  }
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
  config.server.host_name = SMTP_HOST;
  config.server.port = SMTP_PORT;
  config.login.email = AUTHOR_EMAIL;
  config.login.password = AUTHOR_PASSWORD;
  config.login.user_domain = F("127.0.0.1");
  config.time.ntp_server = F("pool.ntp.org,time.nist.gov");
  config.time.gmt_offset = 3;
  config.time.day_light_offset = 0;

  SMTP_Message message;
  message.sender.name = NOME_IRRIGADOR;
  message.sender.email = AUTHOR_EMAIL;
  message.subject = String(assuntoEmail);
  message.addRecipient(F("Someone"), RECIPIENT_EMAIL);
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