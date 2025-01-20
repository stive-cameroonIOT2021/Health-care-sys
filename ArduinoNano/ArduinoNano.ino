#include <SoftwareSerial.h>
#include <U8g2lib.h>
#include <Wire.h>

unsigned long lastStepCountSendTime = 0; // To keep track of the last transmission time
const unsigned long stepCountSendInterval = 1000; // Send every 1 second

// Variables for step counting
const int MPU_ADDR = 0x68; // I2C address of MPU-6050
int16_t accelX, accelY, accelZ;
int stepCount = 0;
float accMagnitudePrev = 0;

U8G2_SSD1306_128X32_UNIVISION_F_HW_I2C oled(U8G2_R0);
bool initialized=false;

// Define SoftwareSerial RX and TX pins
#define RX_PIN 10
#define TX_PIN 11

SoftwareSerial mySerial(RX_PIN, TX_PIN);

float bpm = 0.0, spo2 = 0.0, temp_obj = 0.0, temp_amb = 0.0;
char outputBuffer[50]; // Buffer for formatted output string


void init_display() 
{
  if (not initialized) 
  {
    oled.clearBuffer();
    oled.setCursor(24,12);
    oled.setFont(u8g2_font_5x7_tr);
    oled.drawStr(6, 36, "Steps: 0");    // Initial message with step count
    oled.sendBuffer(); 
    initialized=true;
  }
}

void setup() {
  Serial.begin(115200);       // For debugging
  mySerial.begin(9600);       // For communication with ESP32
  Wire.begin();

  oled.begin();
  init_display();

    // Initialize the MPU6050
    Serial.println("Initializing MPU6050...");
    Wire.beginTransmission(MPU_ADDR);
    Wire.write(0x6B);  // PWR_MGMT_1 register
    Wire.write(0);     // set to zero (wakes up the MPU-6050)
    Wire.endTransmission(true);
    Serial.println("MPU6050 Initialized.");

  Serial.println("Arduino Uno ready to receive data.");
}

void loop() {
  // Read raw accelerometer data
  readAccelerometerData();

  // Detect step
  detectStep();

  // Check for data from ESP32
  if (mySerial.available()) {
    String receivedData = mySerial.readStringUntil('\n'); // Read until newline
    receivedData.trim(); // Remove whitespace

    if (parseData(receivedData)) {
      // Successfully parsed data
    } else {
      Serial.println("Error: Failed to parse data.");
    }
  }

  // Send step count to ESP32 periodically
  unsigned long currentTime = millis();
  if (currentTime - lastStepCountSendTime >= stepCountSendInterval) {
    sendStepCountToESP32();
    lastStepCountSendTime = currentTime;
  }

  // Update OLED display
  oled.clearBuffer();
  oled.setCursor(6, 8);
  oled.print("Spo2: "); oled.print(spo2, 1); oled.print("%");
  oled.setCursor(70, 8);
  oled.print("Bpm: "); oled.print(bpm, 1);
  oled.setCursor(6, 18);
  oled.print("Obj: "); oled.print(temp_obj, 1); oled.print("C");
  oled.setCursor(70, 18);
  oled.print("Amb: "); oled.print(temp_amb, 1); oled.print("C");
  oled.setCursor(6, 26);
  oled.print("Steps: "); oled.print(stepCount, 1);
  oled.sendBuffer();
}

void sendStepCountToESP32() {
  mySerial.print("Steps:");
  mySerial.println(stepCount);
}


void readAccelerometerData() {
  Wire.beginTransmission(MPU_ADDR);
  Wire.write(0x3B); // Starting register for accelerometer data
  Wire.endTransmission(false);
  Wire.requestFrom(MPU_ADDR, 6, true);

  accelX = Wire.read() << 8 | Wire.read();
  accelY = Wire.read() << 8 | Wire.read();
  accelZ = Wire.read() << 8 | Wire.read();
  }
void detectStep() {
  float accX = accelX / 16384.0;
  float accY = accelY / 16384.0;
  float accZ = accelZ / 16384.0;

  // Calculate the magnitude of acceleration
  // accX * accX is equivalent to pow(accX, 2)
   float accMagnitude = sqrt(accX * accX + accY * accY + accZ * accZ);
  // float accMagnitude = sqrt(accX * accX + accY * accY + accZ * accZ);: This line calculates the magnitude of the acceleration vector using the calculated acceleration values for the X, Y, and Z axes. The sqrt() function is used to calculate the square root of the sum of the squared acceleration values.
 
  // Peak detection
  if (accMagnitudePrev > accMagnitude + 0.1 && accMagnitudePrev > 1.5) {
    stepCount++;
   
   
  }
  accMagnitudePrev = accMagnitude;
}
// Function to parse the received string and convert to values
bool parseData(String data) {
  // Expected format: "x,y,z,temp_obj,temp_amb"
  int comma1 = data.indexOf(',');
  int comma2 = data.indexOf(',', comma1 + 1);
  int comma3 = data.indexOf(',', comma2 + 1);
  //int comma4 = data.indexOf(',', comma3 + 1);

  if (comma1 == -1 || comma2 == -1 || comma3 == -1 /* || comma4 == -1 */) {
    return false; // Invalid format
  }

  // Extract and convert each value
  temp_amb = data.substring(0, comma1).toFloat();
  temp_obj = data.substring(comma1 + 1, comma2).toFloat();
  bpm = data.substring(comma2 + 1, comma3).toFloat();
  spo2 = data.substring(comma3 + 1).toFloat();
  //temp_amb = data.substring(comma4 + 1).toFloat();

  return true; // Successfully parsed
}

// Function to format parsed data into a string
void formatData() {
  // Use dtostrf to convert floats to strings
  char BPM[10], SPO2[10];
  char tempObjStr[10], tempAmbStr[10];

  dtostrf(temp_amb, 6, 2, tempAmbStr);
  dtostrf(temp_obj, 6, 2, tempObjStr);
  dtostrf(bpm, 6, 2, BPM); // 6 = total width, 2 = decimal places
  dtostrf(spo2, 6, 2, SPO2);
  

  // Format the final output string
  sprintf(outputBuffer, "%s,%s,%s,%s", tempAmbStr, tempObjStr, BPM, SPO2);
}


