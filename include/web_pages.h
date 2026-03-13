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
      background: #eceff1; 
      color: #37474f; 
      margin: 0; 
      padding: 20px; 
      display: flex; 
      flex-direction: column; 
      align-items: center; 
    }
    h2 { 
      color: #263238; 
      margin-bottom: 40px; 
      font-weight: 300; 
      text-align: center; 
    }
    h4 { 
      text-align: center; 
      margin-top: 20px; 
      color: #546e7a; 
      font-weight: 500; 
    }
    .section { 
      background: #ffffff; 
      border-radius: 15px; 
      padding: 30px; 
      margin: 20px 0; 
      max-width: 700px; 
      width: 100%; 
      box-shadow: 0 10px 30px rgba(0,0,0,0.1); 
      border-top: 5px solid #2196f3; 
    }
    .section:nth-child(2) { 
      border-top-color: #4caf50; 
    }
    .irrigator-title { 
      font-size: 1.8em; 
      font-weight: bold; 
      color: #2196f3; 
      margin-bottom: 25px; 
      text-align: center; 
      position: relative; 
    }
    .irrigator-title::after { 
      content: '🌿'; 
      position: absolute; 
      right: 10px; 
      top: 0; 
    }
    .section:nth-child(2) .irrigator-title { 
      color: #4caf50; 
    }
    .section:nth-child(2) .irrigator-title::after { 
      content: '💧'; 
    }
    .sensor-display { 
      background: #f5f5f5; 
      padding: 20px; 
      border-radius: 10px; 
      margin-bottom: 20px; 
      text-align: center; 
    }
    .sensor-display strong { 
      font-size: 1.2em; 
      color: #546e7a; 
    }
    .button-group { 
      display: flex; 
      justify-content: center; 
      align-items: center; 
      margin-top: 20px; 
    }
    .button { 
      padding: 15px 30px; 
      margin: 0 10px; 
      text-decoration: none; 
      color: white; 
      border-radius: 30px; 
      font-weight: 600; 
      text-transform: uppercase; 
      letter-spacing: 1px; 
      transition: all 0.3s ease; 
      box-shadow: 0 4px 15px rgba(0,0,0,0.2); 
      position: relative; 
    }
    .button:hover { 
      transform: scale(1.05); 
    }
    .button-on { background: #4caf50; }
    .button-off { background: #f44336; }
    .indicator { 
      width: 20px; 
      height: 20px; 
      border-radius: 50%; 
      display: inline-block; 
      margin: 0 15px; 
      border: 3px solid #ffffff; 
      box-shadow: 0 0 10px rgba(0,0,0,0.3); 
      position: relative; 
    }
    .indicator::after { 
      content: "Inativa"; 
      font-size: 0.7em; 
      position: absolute; 
      top: 25px; 
      left: 50%; 
      transform: translateX(-50%); 
      color: #f44336; 
    }
    .pump-on::after { 
      content: "Ativa"; 
      color: #4caf50; 
    }
    .pump-on { background-color: #4caf50; }
    .pump-off { background-color: #f44336; }
    input[type="number"] { 
      padding: 12px; 
      margin: 10px; 
      border: 2px solid #90a4ae; 
      border-radius: 8px; 
      font-size: 1em; 
      width: 140px; 
      text-align: center; 
    }
    input[type="submit"] { 
      padding: 12px 24px; 
      background: #607d8b; 
      color: white; 
      border: none; 
      border-radius: 8px; 
      cursor: pointer; 
      font-weight: 600; 
      transition: background 0.3s ease; 
    }
    input[type="submit"]:hover { 
      background: #546e7a; 
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
