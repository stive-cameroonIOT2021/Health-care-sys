#include <Wire.h>
#include "MAX30100_PulseOximeter.h"
#include <Adafruit_MLX90614.h>
#include <WiFi.h>
#include <WebServer.h>

// WiFi credentials
const char *ssid = "ESP32-AP";
const char *password = "12345678";

// Create a hardware serial instance
HardwareSerial mySerial(2); // Using UART1

Adafruit_MLX90614 mlx = Adafruit_MLX90614();
PulseOximeter pox;
WebServer server(80);

constexpr uint32_t REPORTING_PERIOD_MS = 1000;
constexpr auto LED_CURRENT = MAX30100_LED_CURR_7_6MA;

uint32_t tsLastReport = 0;
float lastValidTempAmb = NAN;
float lastValidTempObj = NAN;

String stepCount = "0"; // Default step count

void onBeatDetected() {
    Serial.println("♥ Beat!");
}

void initializeSensors() {
    Serial.print("Initializing pulse oximeter...");
    if (!pox.begin()) {
        Serial.println("FAILED");
        while (true);
    }
    Serial.println("SUCCESS");
    pox.setIRLedCurrent(LED_CURRENT);
    pox.setOnBeatDetectedCallback(onBeatDetected);

    Serial.print("Initializing MLX90614...");
    if (!mlx.begin()) {
        Serial.println("FAILED");
        while (true);
    }
    Serial.println("SUCCESS");
}

void reportReadings() {
    float bpm = pox.getHeartRate();
    float spo2 = pox.getSpO2();
    float tempAmb = mlx.readAmbientTempC();
    float tempObj = mlx.readObjectTempC();

    if (!isnan(tempAmb) && !isnan(tempObj)) {
        lastValidTempAmb = tempAmb;
        lastValidTempObj = tempObj;
    }

    // Combine sensor readings into a string
    char dataString[50];
    snprintf(dataString, sizeof(dataString), "%.1f,%.1f,%.1f,%.1f", 
             !isnan(tempAmb) ? tempAmb : lastValidTempAmb, !isnan(tempObj) ? tempObj : lastValidTempObj, bpm, spo2);
    mySerial.println(dataString); // Send the data to Arduino Nano via HardwareSerial

    // Debugging output
    Serial.println("Sent data:");
    Serial.println(dataString);
}

void receiveStepCount() {
    while (mySerial.available()) {
        stepCount = mySerial.readStringUntil('\n');
        stepCount.trim();
        Serial.println("Received step count: " + stepCount);
    }
}

void handleWebRequest() {
    String html = R"rawliteral(
        <!DOCTYPE html>
        <html>
        <head>
            <title>ESP32 Sensor Data</title>
            <script>
                function updateData() {
                    fetch('/data')
                        .then(response => response.json())
                        .then(data => {
                            document.getElementById('ambientTemp').innerText = data.ambientTemp + " °C";
                            document.getElementById('objectTemp').innerText = data.objectTemp + " °C";
                            document.getElementById('heartRate').innerText = data.heartRate + " bpm";
                            document.getElementById('spo2').innerText = data.spo2 + " %";
                            document.getElementById('stepCount').innerText = data.stepCount;
                        });
                }
                setInterval(updateData, 1000); // Update data every second
            </script>
        </head>
        <body>
            <h1>ESP32 Sensor Data</h1>
            <p>Ambient Temp: <span id="ambientTemp">Loading...</span></p>
            <p>Object Temp: <span id="objectTemp">Loading...</span></p>
            <p>Heart Rate: <span id="heartRate">Loading...</span></p>
            <p>SpO2: <span id="spo2">Loading...</span></p>
            <p>Step Count: <span id="stepCount">Loading...</span></p>
        </body>
        </html>
    )rawliteral";
    server.send(200, "text/html", html);
}

void handleDataRequest() {
    String jsonData = "{";
    jsonData += "\"ambientTemp\":" + String(lastValidTempAmb) + ",";
    jsonData += "\"objectTemp\":" + String(lastValidTempObj) + ",";
    jsonData += "\"heartRate\":" + String(pox.getHeartRate()) + ",";
    jsonData += "\"spo2\":" + String(pox.getSpO2()) + ",";
    jsonData += "\"stepCount\":\"" + stepCount + "\"";
    jsonData += "}";
    server.send(200, "application/json", jsonData);
}

void setupWiFi() {
    WiFi.softAP(ssid, password);
    Serial.println("ESP32 in Access Point Mode");
    Serial.print("IP address: ");
    Serial.println(WiFi.softAPIP());
}

void setup() {
    Serial.begin(115200);
    mySerial.begin(9600, SERIAL_8N1, 16, 17); // RX = 16, TX = 17

    initializeSensors();
    setupWiFi();

    server.on("/", handleWebRequest);
    server.on("/data", handleDataRequest); // Serve data as JSON
    server.begin();
    Serial.println("Web server started.");
}

void loop() {
    pox.update();
    if (millis() - tsLastReport > REPORTING_PERIOD_MS) {
        reportReadings();
        tsLastReport = millis();
    }

    receiveStepCount();
    server.handleClient();
}
