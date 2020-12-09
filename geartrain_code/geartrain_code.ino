///// Libraries ////////////////////////////////////////////////////////////
#include <WiFi.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <DHT.h>
#include <Streaming.h>
#include <PriUint64.h>
#include <SPI.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <SPIFFS.h>
#include <esp32-hal-cpu.h>

///// ERROR CODES /////
#define NO_ERROR         0
#define DHT_ERROR       15
#define LCD_CONN_ERROR  64
#define SPIFFS_ERROR    27
bool ERROR_STATE = false;

///// TASKS /////
TaskHandle_t TaskConnection, TaskControl, SetupConn;
bool STATE_CONTROL =    true;
bool STATE_CONNECTION = true;
bool PROGRAM_START =    false;
bool SPIFFS_OK = true;

///// Variables ////////////////////////////////////////////////////////////
///// WiFi /////
const char* ssid = "Konditorei_5";
const char* password = "bagpulainalex1234";
const int ledConnection = 2;
///// AsyncWebServer /////
AsyncWebServer server(80);
AsyncWebSocket ws ("/ws");
AsyncEventSource events("/events");
unsigned int connectedClients = 0;

///// OLED Config /////
bool LCD_ENABLE = true;
#define SCREEN_WIDTH  128
#define SCREEN_HEIGHT  64
#define OLED_RESET     -1
#define OLED_ADDRESS 0x3C

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

///// MOTOR /////
const int inA = 25;
const int inB = 26;
const int en = 33;
const int encoder = 36;
const int chMotor = 0;
const int freqMotor = 50;
const int resPWM = 8;
char direction = 'f';
int pwmval = 0;

volatile unsigned long encoderCounter = 0;
uint64_t refEncMicros = 5000;
uint64_t currentMicros = esp_timer_get_time();
const unsigned long timeDiffEnc = 1000000;
int RPM = 0;

///// DHT /////
#define DHTTYPE DHT11
bool DHT_STATE = true;
const int dhtPin = 32;
DHT dht(dhtPin, DHTTYPE);
uint64_t refDHTMicros = 5000;
const unsigned long timeDiffDHT = 2000000;

float hum = 0;
float temp = 0;

///// LED Array /////
bool LED_STATE = false;
const int ledFA = 4;
const int ledFB = 16;
const int ledBA = 17;
const int ledBB = 5;

///// Shift Register /////
bool SHIFT_ENABLE = false;
const int dataPin = 27;
const int clk = 18;
const int latch = 19;
const int resetMaster = 15;
const int enableRegisterOutput = 0;

int digits [10][8]{
  {1,0,0,0,1,0,0,0}, // digit 0
  {1,1,1,1,1,1,0,0}, // digit 1
  {1,0,0,1,0,0,0,1}, // digit 2
  {1,0,1,1,0,0,0,0}, // digit 3
  {1,1,1,0,0,1,0,0}, // digit 4
  {1,0,1,0,0,0,1,0}, // digit 5
  {1,0,0,0,0,0,1,0}, // digit 6
  {1,1,1,1,1,0,0,0}, // digit 7
  {1,0,0,0,0,0,0,0}, // digit 8
  {1,0,1,0,0,0,0,0}  // digit 9
};

///// Buzzer /////
const int buzz = 14;
const int freqBuzz = 2000;
const int chBuzz = 1;

///// Fan Switch /////
bool FANS = false;
const int fanSwitch = 12;

///// PROGRAM //////////////////////////////////////////////////////////////////
///// Functions ////////////////////////////////////////////////////////////////
void enc_Event() { encoderCounter++; }

void buzzerSound(int code)
{
  Serial << "Buzzer: " << code << endl;

  switch(code){
    case 0:
      ledcWrite(chBuzz, 150);
      delay(300);
      ledcWrite(chBuzz, 200);
      delay(300);
      ledcWrite(chBuzz, 250);
      delay(300);
      ledcWrite(chBuzz, 0);
      break;
    case 1:
      ledcWrite(chBuzz, 150);
      delay(200);
      ledcWrite(chBuzz, 100);
      delay(200);
      ledcWrite(chBuzz, 0);
      break;
    case 2:
      break;
    case 3:
      ledcWrite(chBuzz, 255);
      delay(300);
      ledcWrite(chBuzz, 0);
      delay(300);
      break;
    case 4:
      break;
    case 666:
      break;
    default:
      ledcWrite(chBuzz, 0);
  }
}

void getRPM()
{
  Serial << "GET RPM" << endl;

  RPM = encoderCounter/11*2;
  refEncMicros = currentMicros;
  encoderCounter = 0;
  events.send(String(RPM).c_str(), "rpm");
}

void getDHT()
{
  Serial << "GET DHT" << endl;

  hum = dht.readHumidity();
  temp = dht.readTemperature();

  refDHTMicros = currentMicros;

  if(isnan(hum) || isnan(temp))
  {
    DHT_STATE = false;
    displayError(15);
  }

  events.send(String(hum).c_str(), "humidity");
  events.send(String(temp).c_str(), "temperature");
}

void DisplayDigit(int Digit)
{
    if(!SHIFT_ENABLE){
      events.send(String("FANS not Enabled").c_str(), "error");
      buzzerSound(1);
      return;
    }

    digitalWrite(latch, LOW);
    for (int i = 7; i>=0; i--)
   {
    digitalWrite(clk,LOW);
    if (digits[Digit][i]==1) digitalWrite(dataPin, HIGH);
    if (digits[Digit][i]==0) digitalWrite(dataPin, LOW);
    digitalWrite(clk,HIGH);
   }
   digitalWrite(latch, HIGH);
}

void displayError(int error_code)
{
  switch (error_code)
  {
    case 0:
      ERROR_STATE = false;
      DisplayDigit(0);
      DisplayDigit(0);
      ledcWrite(chBuzz, 0);
      display.clearDisplay();
      display.display();
      break;
    case 15:
      Serial << "DHT ERROR" << endl;
      ERROR_STATE = true;
      DisplayDigit(5);
      DisplayDigit(1);
      if(!LCD_ENABLE) { break; }
      //display.clearDisplay();
      //display.setCursor(0, 0);
      //display.setTextSize(4);
      //display.print("ERROR DHT");
      //display.display();
      buzzerSound(2);
      break;
    case 64:
      ERROR_STATE = true;
      DisplayDigit(4);
      DisplayDigit(6);
      ledcWrite(chMotor, 0);
      buzzerSound(5);
      break;
    default:
      DisplayDigit(8);
      DisplayDigit(8);
      buzzerSound(666);
  }
}


void onEvent(AsyncWebSocket *server, AsyncWebSocketClient *client, AwsEventType type, void *arg, uint8_t *data, size_t len)
{
    digitalWrite(ledConnection, HIGH);
    delay(80);
    switch (type)
    {
      case WS_EVT_CONNECT:
        Serial.printf("WebSocket client #%u connected from %s\n", client->id(), client->remoteIP().toString().c_str());
        connectedClients++;
        break;
      case WS_EVT_DISCONNECT:
        Serial.println("Disconnected");
        connectedClients--;
        break;
      case WS_EVT_DATA:
        Serial << "Data received" << endl;
        handleWebSocketMessage(arg, data, len);
        break;
      case WS_EVT_PONG:
        break;
      case WS_EVT_ERROR:
         Serial.printf("ws[%s][%u] error(%u): %s\n", server->url(), client->id(), *((uint16_t*)arg), (char*)data);
        break;
    }
    digitalWrite(ledConnection, LOW);
}

void handleWebSocketMessage(void *arg, uint8_t *data, size_t len)
{
  digitalWrite(ledConnection, HIGH);
  delay(80);
  AwsFrameInfo *info = (AwsFrameInfo*)arg;
  if (info->final && info->index == 0 && info->len == len && info->opcode == WS_TEXT)
  {
    data[len] = 0;
///// Start Program
    if (strcmp((char*)data, "prgStart") == 0) { PROGRAM_START = true; Serial << "START START START" << endl;}
///// Skip Error
    else if(strcmp((char*)data, "skipErr") == 0) { ERROR_STATE = false; }
///// Add PWM
    else if(strcmp((char*)data, "pwmadd") == 0){
      if (!FANS){
        events.send(String("Fans Not Enabled").c_str(), "error");
        buzzerSound(1);
        return;
      }
      else if (pwmval > 240) { return; }
      pwmval = pwmval+15;
      Serial << "PWM: " << pwmval << endl;
      events.send(String(pwmval).c_str(), "updatePWM");
      ledcWrite(chMotor, pwmval);
    }
///// Substract PWM
    else if(strcmp((char*)data, "pwmsub") == 0){
      if (!FANS){
        events.send(String("Fans Not Enabled").c_str(), "error");
        buzzerSound(1);
        return;
      }
      else if (pwmval < 15) { return; }
      pwmval = pwmval-15;
      Serial << "PWM: " << pwmval << endl;
      events.send(String(pwmval).c_str(), "updatePWM");
      ledcWrite(chMotor, pwmval);
    }
///// FANS
    else if(strcmp((char*)data, "fans") == 0){
      if(FANS){
        Serial << "Fans Disabled" << endl;

        FANS = false;
        SHIFT_ENABLE = false;
        digitalWrite(fanSwitch, LOW);
        digitalWrite(resetMaster, LOW);
        digitalWrite(enableRegisterOutput, HIGH);
        pwmval = 0;
      }
      else {
        Serial << "Fans Enabled" << endl;

        FANS = true;
        SHIFT_ENABLE = true;
        digitalWrite(fanSwitch, HIGH);
        digitalWrite(resetMaster, HIGH);
        digitalWrite(enableRegisterOutput, LOW);
        displayError(0);
      }
    }
///// Enable Motor
    else if(strcmp((char*)data, "enmotor") == 0){
      if (!FANS){
        events.send(String("Fans Not Enabled").c_str(), "error");
        buzzerSound(1);
        return;
      }
    }
///// Change Direction
    else if(strcmp((char*)data, "dirmotor") == 0){
      if(direction == 'f'){
        Serial << "Motor backward" << endl;

        direction = 'b';
        digitalWrite(inA, LOW);
        digitalWrite(inB, HIGH);
      }
      else if(direction == 'b'){
        Serial << "Motor forward" << endl;

        direction = 'f';
        digitalWrite(inA, HIGH);
        digitalWrite(inB, LOW);
      }
    }
///// LED Control
    else if(strcmp((char*)data, "led") == 0){
      if (!FANS){
        events.send(String("Fans Not Enabled").c_str(), "error");
        buzzerSound(1);
        return;
      }
      else if(LED_STATE){
        Serial << "LED OFF" << endl;

        LED_STATE = false;
        digitalWrite(ledFA, LOW);
        digitalWrite(ledFB, LOW);
        digitalWrite(ledBA, LOW);
        digitalWrite(ledBB, LOW);
      }
      else{
        Serial << "LED ON" << endl;

        LED_STATE = true;
        digitalWrite(ledFA, HIGH);
        digitalWrite(ledFB, HIGH);
        digitalWrite(ledBA, HIGH);
        digitalWrite(ledBB, HIGH);
      }
    }
  }
  digitalWrite(ledConnection, LOW);
}


///// SETUPS ///////////////////////////////////////////////////////////////////
void setupControl()
{
  ///// SET PIN MODES /////
  const int pinArrayOut[14] = {25,26,4,16,17,5,18,19,27,2,14,12,15,0};
  for (int i=0; i<14; i++) { pinMode(pinArrayOut[i], OUTPUT); }

  digitalWrite(inA, HIGH);
  digitalWrite(inB, LOW);
  digitalWrite(resetMaster, LOW);
  digitalWrite(enableRegisterOutput, HIGH);
  digitalWrite(fanSwitch, LOW);
  digitalWrite(ledFA, LOW);
  digitalWrite(ledFB, LOW);
  digitalWrite(ledBA, LOW);
  digitalWrite(ledBB, LOW);

  ledcSetup(chMotor, freqMotor, resPWM);
  ledcAttachPin(en, chMotor);
  ledcSetup(chBuzz, freqBuzz, resPWM);
  ledcAttachPin(buzz, chBuzz);

  pinMode(encoder, INPUT);

  ///// START OLED AND DHT /////
  if(!display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDRESS))
  {
    Serial.println("ERROR OLED");
    digitalWrite(fanSwitch, HIGH);
    SHIFT_ENABLE = true;
    displayError(LCD_CONN_ERROR);
    ERROR_STATE = true;
    LCD_ENABLE = false;
  }
  display.display();

  if(LCD_ENABLE)
  {
    display.setTextSize(2);
    display.setTextColor(SSD1306_WHITE);
    display.cp437(true);
    display.clearDisplay();
    display.println("Loading...");
    display.display();
  }

  dht.begin();

  if(!SPIFFS.begin())
  {
    Serial.println("ERROR SPIFFS");
    digitalWrite(fanSwitch, HIGH);
    SHIFT_ENABLE = true;
    displayError(SPIFFS_ERROR);
    ERROR_STATE = true;
    SPIFFS_OK = false;
  }

  delay(1000);
  Serial << "Finishing Control Setup" << endl;
}

void setupConnection ()
{
  if(LCD_ENABLE)
  {
    display.clearDisplay();
    display.println("Loading...");
  }

  while(WiFi.status() != WL_CONNECTED)
  {
    delay(1000);
    Serial << "Connecting to WiFi: " << ssid << endl;
  }

  buzzerSound(0);

  ws.onEvent(onEvent);
  server.addHandler(&ws);
  server.addHandler(&events);

  Serial << WiFi.localIP() << endl;

  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send(SPIFFS, "/index.html", "text/html");
  });

  server.on("/script.js", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send(SPIFFS, "/script.js", "text/javascript");
  });
  server.begin();

  delay(1000);
  Serial << "Finishing Connection Setup!!" << endl;
}
////////////////////////////////////////////////////////////////////////////////
///// CORE LOOPS ///////////////////////////////////////////////////////////////

void loopControl( void * parameter )
{
  for(;STATE_CONTROL;)
  {
    while(!PROGRAM_START){
      display.setTextSize(2);
      display.setCursor(0, 0);
      display.clearDisplay();
      display.println("IP: ");
      display.println(WiFi.localIP());
      display.display();
    }

    display.clearDisplay();
    display.display();

    currentMicros = esp_timer_get_time();
    if(currentMicros-refEncMicros>=timeDiffEnc) { getRPM(); }
    if((currentMicros-refDHTMicros>=timeDiffDHT) && DHT_STATE) { getDHT(); }

  }

  vTaskDelete(NULL);
}

void loopConnection( void * parameter )
{
  for(;STATE_CONNECTION;)
  {
    while(!PROGRAM_START)
    {
      delay(300);

      Serial << "Connections: " << connectedClients << endl;
      if(connectedClients == 0){ buzzerSound(3); }

      Serial << "ErrorState: " << ERROR_STATE << endl;
    }

    delay(200);
    if(connectedClients == 0){ buzzerSound(3); }


  }

  vTaskDelete(NULL);
}

///// StartUP //////////////////////////////////////////////////////////////////
void setup()
{
  Serial.begin(115200);
  Serial << "Starting ESP32" << endl;
  Serial << "CPU Frequency: " << getCpuFrequencyMhz() << "MHz" << endl;

  attachInterrupt(digitalPinToInterrupt(encoder), enc_Event, RISING);
  setupControl();

  WiFi.begin(ssid, password);
  setupConnection();

  while(!SPIFFS_OK);

  delay(2000);

  Serial << "Starting Tasks" << endl;

  xTaskCreatePinnedToCore(
    loopControl,
    "Loop Control",
    5000,
    NULL,
    1,
    &TaskControl,
    1);

  xTaskCreatePinnedToCore(
    loopConnection,
    "Loop Connection",
    5000,
    NULL,
    1,
    &TaskConnection,
    0);
}

void loop()
{ vTaskDelay(portMAX_DELAY); }
