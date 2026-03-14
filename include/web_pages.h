#ifndef WEB_PAGES_H
#define WEB_PAGES_H

const char INDEX_HTML[] PROGMEM = R"rawliteral(
<!DOCTYPE HTML>
<html>
<head>
  <title>%NOME_IRRIGADOR%</title>
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <style>
    body {
      font-family: 'Open Sans', sans-serif;
      background:
        repeating-linear-gradient(135deg, rgba(255,255,255,0.6) 0, rgba(255,255,255,0.6) 8px, rgba(236,239,241,0.9) 8px, rgba(236,239,241,0.9) 16px),
        repeating-linear-gradient(45deg, rgba(255,255,255,0.35) 0, rgba(255,255,255,0.35) 6px, rgba(230,234,238,0.9) 6px, rgba(230,234,238,0.9) 12px);
      color: #37474f;
      margin: 0;
      padding: 10px;
      display: flex;
      flex-direction: column;
      align-items: center;
      font-size: 13.5px;
    }
    h2 {
      margin: 6px 0 12px;
      font-size: 1.15em;
      font-weight: 300;
      text-align: center;
      color: #263238;
    }
    h4 {
      margin: 6px 0 4px;
      font-size: 0.9em;
      text-align: center;
      color: #546e7a;
      font-weight: 500;
    }
    .section {
      background: rgba(255,255,255,0.96);
      border-radius: 10px;
      padding: 12px;
      margin: 8px 0;
      max-width: 520px;
      width: 100%;
      box-shadow: 0 5px 14px rgba(0,0,0,0.08);
      border-top: 4px solid #2196f3;
    }
    .section:nth-child(2) { border-top-color: #4caf50; }
    .irrigator-title {
      font-size: 1.1em;
      font-weight: 700;
      margin-bottom: 6px;
      text-align: center;
      color: #2196f3;
    }
    .section:nth-child(2) .irrigator-title { color: #4caf50; }
    .sensor-display {
      background: #f5f5f5;
      padding: 8px;
      border-radius: 8px;
      margin-bottom: 6px;
      text-align: center;
    }
    .sensor-display strong { font-size: 0.95em; }
    .button-group {
      display: flex;
      justify-content: center;
      align-items: center;
      gap: 6px;
      margin-top: 6px;
    }
    .button {
      padding: 7px 12px;
      border-radius: 18px;
      font-size: 0.8em;
      font-weight: 600;
      text-transform: uppercase;
      letter-spacing: 0.4px;
      color: #fff;
      text-decoration: none;
    }
    .button-on { background: #4caf50; }
    .button-off { background: #f44336; }
    .indicator {
      width: 18px;
      height: 18px;
      border-radius: 50%;
      margin: 0 4px;
      border: 2px solid #fff;
      position: relative;
    }
    .indicator::after {
      content: "Inativa";
      font-size: 0.65em;
      font-weight: 700;
      top: 20px;
      left: 50%;
      transform: translateX(-50%);
      position: absolute;
      color: #f44336;
    }
    .pump-on::after { content: "Ativa"; color: #16a34a; }
    .pump-on { background-color: #22c55e; }
    .pump-off { background-color: #ef4444; }
    input[type="number"] {
      padding: 7px;
      margin: 5px;
      border: 1px solid #90a4ae;
      border-radius: 6px;
      font-size: 0.85em;
      width: 105px;
      text-align: center;
    }
    input[type="submit"] {
      padding: 7px 12px;
      background: #607d8b;
      color: white;
      border: none;
      border-radius: 6px;
      cursor: pointer;
      font-weight: 600;
      font-size: 0.8em;
    }
  </style>
</head>
<body>
  <h2>%NOME_IRRIGADOR%</h2>
  <div class="section">
    <div class="irrigator-title">Irrigador 1</div>
    <div class="sensor-display">
      <strong>Sensor 1: %HUMIDITY_1%</strong>
    </div>
    <p style="text-align: center;">
      <input type="number" name="threshold_input1" value="%THRESHOLD1%" placeholder="Definir limite">
      <input type="submit" value="Aplicar" onclick="location.href='/get?threshold_input1=' + document.getElementsByName('threshold_input1')[0].value">
    </p>
    <h4 style="text-align: center;">Controle da Bomba 1</h4>
    <div class="button-group">
      <a href="/pump1_on" class="button button-on">LIGAR</a>
      <span class="indicator %PUMP1_INDICATOR%"></span>
      <a href="/pump1_off" class="button button-off">DESLIGAR</a>
    </div>
  </div>
  <div class="section">
    <div class="irrigator-title">Irrigador 2</div>
    <div class="sensor-display">
      <strong>Sensor 2: %HUMIDITY_2%</strong>
    </div>
    <p style="text-align: center;">
      <input type="number" name="threshold_input2" value="%THRESHOLD2%" placeholder="Definir limite">
      <input type="submit" value="Aplicar" onclick="location.href='/get?threshold_input2=' + document.getElementsByName('threshold_input2')[0].value">
    </p>
    <h4 style="text-align: center;">Controle da Bomba 2</h4>
    <div class="button-group">
      <a href="/pump2_on" class="button button-on">LIGAR</a>
      <span class="indicator %PUMP2_INDICATOR%"></span>
      <a href="/pump2_off" class="button button-off">DESLIGAR</a>
    </div>
  </div>
</body>
</html>
)rawliteral";

const char OTA_SERVER_INDEX[] PROGMEM = R"rawliteral(
<html>
<head>
<title>ESP32 OTA Update</title>
</head>
<body>
<h1>ESP32 OTA Update</h1>
<form method="POST" action="/update" enctype="multipart/form-data">
<input type="file" name="update">
<input type="submit" value="Update">
</form>
<div id="prg">progress: 0%</div>
</body>
</html>
)rawliteral";

#endif

