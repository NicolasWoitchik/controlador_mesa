// #include <AsyncTCP.h>
// #include <ESPAsyncWebServer.h>
// // Create AsyncWebServer object on port 80

// AsyncWebServer server(80);
// AsyncWebSocket ws("/ws");

// JSONVar readings;

// int lastPackage = 0;

// void notifyClients()
// {
//   if (millis() - lastPackage > 1000)
//   {
//     readings["motorEsquerdaDirecao1Output"] = String(analogRead(motorEsquerdaDirecao1Output));
//     readings["motorEsquerdaDirecao2Output"] = String(analogRead(motorEsquerdaDirecao2Output));
//     readings["motorDireitaDirecao1Output"] = String(analogRead(motorDireitaDirecao1Output));
//     readings["motorDireitaDirecao2Output"] = String(analogRead(motorDireitaDirecao2Output));
//     readings["motorEsquerdaHALLInput"] = String(motorEsquerdaHALLInput);
//     readings["motorDireitaHALLInput"] = String(motorDireitaHALLInput);
  
//     readings["motorEsquerdaPosicaoHALL"] = String(motorEsquerdaPosicaoHALL);
//     readings["motorDireitaPosicaoHALL"] = String(motorDireitaPosicaoHALL);
  
//     readings["sobe"] = sobe;
//     readings["desce"] = desce;
  
//     String jsonString = JSON.stringify(readings);
  
//     ws.textAll(jsonString);
//     lastPackage = millis();
//   }
// }



// void handleWebSocketMessage(void *arg, uint8_t *data, size_t len)
// {
//   AwsFrameInfo *info = (AwsFrameInfo *)arg;
//   if (info->final && info->index == 0 && info->len == len && info->opcode == WS_TEXT)
//   {
//     data[len] = 0;
//     Serial.println((char *)data);
//     if (strcmp((char *)data, "toggle") == 0)
//     {
//       ledState = !ledState;
//     }
//     else if (strcmp((char *)data, "sobe") == 0)
//     {
//       subir();
//     }
//     else if (strcmp((char *)data, "desce") == 0)
//     {
//       descer();
//     }
//     else if (strcmp((char *)data, "para") == 0)
//     {
//       parar();
//     }
//     notifyClients();
//   }
// }

// void onEvent(AsyncWebSocket *server, AsyncWebSocketClient *client, AwsEventType type,
//              void *arg, uint8_t *data, size_t len)
// {
//   switch (type)
//   {
//   case WS_EVT_CONNECT:
//     Serial.printf("WebSocket client #%u connected from %s\n", client->id(), client->remoteIP().toString().c_str());
//     break;
//   case WS_EVT_DISCONNECT:
//     Serial.printf("WebSocket client #%u disconnected\n", client->id());
//     break;
//   case WS_EVT_DATA:
//     handleWebSocketMessage(arg, data, len);
//     break;
//   case WS_EVT_PONG:
//   case WS_EVT_ERROR:
//     break;
//   }
// }

// void initWebSocket()
// {
//   ws.onEvent(onEvent);
//   server.addHandler(&ws);
// }

// String processor(const String &var)
// {
//   Serial.println(var);
//   if (var == "STATE")
//   {
//     if (ledState)
//     {
//       return "ON";
//     }
//     else
//     {
//       return "OFF";
//     }
//   }
//   return String();
// }

// char index_html[] PROGMEM = R"rawliteral(
// <!DOCTYPE html>
// <html>
//   <head>
//     <title>ESP IOT DASHBOARD</title>
//     <meta name="viewport" content="width=device-width, initial-scale=1" />
//     <style>
//       html {
//         font-family: Arial, Helvetica, sans-serif;
//         display: inline-block;
//         text-align: center;
//       }
//       h1 {
//         font-size: 1.8rem;
//         color: white;
//       }
//       .topnav {
//         overflow: hidden;
//         background-color: #0a1128;
//       }
//       body {
//         margin: 0;
//       }
//       .content {
//         padding: 50px;
//       }
//       .card-grid {
//         max-width: 800px;
//         margin: 0 auto;
//         display: grid;
//         grid-gap: 2rem;
//         grid-template-columns: repeat(auto-fit, minmax(200px, 1fr));
//       }
//       .card {
//         background-color: white;
//         box-shadow: 2px 2px 12px 1px rgba(140, 140, 140, 0.5);
//       }
//       .card-title {
//         font-size: 1.2rem;
//         font-weight: bold;
//         color: #034078;
//       }
//       .reading {
//         font-size: 1.2rem;
//         color: #1282a2;
//       }
//     </style>
//   </head>
//   <body>
//     <div class="topnav">
//       <h1>INFORMAÇÕES DA MESA</h1>
//     </div>
//     <div class="content">
//       <div class="card-grid">
//         <div class="card">
//           <p class="card-title">Motor esquerdo</p>
//           <p class="reading"><span id="motorEsquerdaPosicaoHALL"></span></p>
//         </div>
//         <div class="card">
//           <p class="card-title">Motor Direito</p>
//           <p class="reading"><span id="motorDireitaPosicaoHALL"></span></p>
//         </div>
//         <div class="card">
//           <p class="card-title">Estado atual</p>
//           <p class="reading"><span id="estado"></span></p>
//         </div>
//       </div>
//       <div class="card-grid">
//         <div class="card">
//           <p class="card-title">Controles</p>
//           <p>
//             <button onclick="sobe();">&uparrow;</button>
//             <button onclick="desce();">&downarrow;;</button>
//             <button onclick="para();">X</button>
//           </p>
//         </div>
//       </div>
//     </div>
//     <script>
//       var gateway = `ws://${window.location.hostname}/ws`;
//       var websocket;
//       // Init web socket when the page loads
//       window.addEventListener("load", onload);

//       function onload(event) {
//         initWebSocket();
//       }

//       function getReadings() {
//         websocket.send("getReadings");
//       }

//       function initWebSocket() {
//         console.log("Trying to open a WebSocket connection…");
//         websocket = new WebSocket(gateway);
//         websocket.onopen = onOpen;
//         websocket.onclose = onClose;
//         websocket.onmessage = onMessage;
//       }

//       function sobe() {
//         websocket.send("sobe");
//       }

//       function desce() {
//         websocket.send("desce");
//       }

//       function para() {
//         websocket.send("para");
//       }

//       // When websocket is established, call the getReadings() function
//       function onOpen(event) {
//         console.log("Connection opened");
//         getReadings();
//       }

//       function onClose(event) {
//         console.log("Connection closed");
//         setTimeout(initWebSocket, 2000);
//       }

//       // Function that receives the message from the ESP32 with the readings
//       function onMessage(event) {
//         console.log(event.data);
//         var data = JSON.parse(event.data);
//         var keys = Object.keys(data);

//         if (data.motorEsquerdaPosicaoHALL) {
//           document.getElementById("motorEsquerdaPosicaoHALL").innerHTML =
//             data.motorEsquerdaPosicaoHALL;
//         }
//         if (data.motorDireitaPosicaoHALL) {
//           document.getElementById("motorDireitaPosicaoHALL").innerHTML =
//             data.motorDireitaPosicaoHALL;
//         }

//         if (data.sobe && !data.desce) {
//           document.getElementById("estado").innerHTML = "Subindo";
//         } else if (!data.sobe && data.desce) {
//           document.getElementById("estado").innerHTML = "Descendo";
//         } else {
//           document.getElementById("estado").innerHTML = "Parado";
//         }
//       }
//     </script>
//   </body>
// </html>

// )rawliteral";
