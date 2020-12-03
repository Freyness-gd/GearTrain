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

///// ERROR CODES /////
#define NO_ERROR         0
#define DHT_ERROR       15
#define LCD_CONN_ERROR  64
#define SPIFFS_ERROR    27
bool ERROR_STATE = false;

///// TASKS /////
TaskHandle_t SetupConnection, SetupControl, TaskConnection, TaskControl;
bool STATE_CONTROL =    true;
bool STATE_CONNECTION = true;
bool PROGRAM_START =    false;

///// Variables ////////////////////////////////////////////////////////////
///// WiFi /////
const char* ssid = "A1-AP1";
const char* password = "WireLeSS3865@!";
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
int pwmval = 0;

volatile unsigned long encoderCounter = 0;
uint64_t refMillis = 5000;
uint64_t currentMillis = esp_timer_get_time();
const unsigned long timediff = 1000000;
int RPM = 0;

///// DHT /////
#define DHTTYPE DHT11
bool DHT_STATE = true;
const int dhtPin = 32;
DHT dht(dhtPin, DHTTYPE);
uint64_t dhtMillis = 5000;
const unsigned long dhtTime = 2000000;

float hum = 0;
float temp = 0;

///// LED Array /////
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

void enc_Event() { encoderCounter++; }

void buzzerSound(int code)
{

}

void DisplayDigit(int Digit)
{
    if(!SHIFT_ENABLE) { return; }

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
      break;
    case 15:
      ERROR_STATE = true;
      ledcWrite(chMotor, 0);
      ledcWrite(chBuzz, 100);
      DisplayDigit(5);
      DisplayDigit(1);
      if(!LCD_ENABLE) { break; }
      display.clearDisplay();
      display.setCursor(0, 0);
      display.setTextSize(4);
      display.print("ERROR DHT");
      display.display();
      buzzerSound(1);
      break;
    case 64:
      ERROR_STATE = true;
      DisplayDigit(4);
      DisplayDigit(6);
      ledcWrite(chMotor, 0);
      ledcWrite(chBuzz, 100);
      buzzerSound(2);
      break;
    default:
      DisplayDigit(8);
      DisplayDigit(8);
      buzzerSound(666);
  }
}

int testDHT()
{
  hum = dht.readHumidity();
  temp = dht.readTemperature();

  if(isnan(hum) || isnan(temp))
  {
    return 0;
  }

  else
    return 1;
}

void onEvent(AsyncWebSocket *server, AsyncWebSocketClient *client, AwsEventType type, void *arg, uint8_t *data, size_t len)
{
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
        handleWebSocketMessage(arg, data, len);
        break;
      case WS_EVT_PONG:
        break;
      case WS_EVT_ERROR:
         Serial.printf("ws[%s][%u] error(%u): %s\n", server->url(), client->id(), *((uint16_t*)arg), (char*)data);
        break;
    }
}

void handleWebSocketMessage(void *arg, uint8_t *data, size_t len)
{
  AwsFrameInfo *info = (AwsFrameInfo*)arg;
  if (info->final && info->index == 0 && info->len == len && info->opcode == WS_TEXT)
  {
    data[len] = 0;
///// Start Program
    if (strcmp((char*)data, "prgStart") == 0) { PROGRAM_START = true; }
///// Skip Error
    else if(strcmp((char*)data, "skipErr") == 0) { ERROR_STATE = false; }
///// Add PWM
    else if(strcmp((char*)data, "pwmadd") == 0){
      if (pwmval > 240) { return; }
      pwmval = pwmval+15;
      Serial << "PWM: " << pwmval << endl;
      events.send(String(pwmval).c_str(), "updatePWM");
      ledcWrite(chMotor, pwmval);
    }
///// Substract PWM
    else if(strcmp((char*)data, "pwmsub") == 0){
      if (pwmval < 15) { return; }
      pwmval = pwmval-15;
      Serial << "PWM: " << pwmval << endl;
      events.send(String(pwmval).c_str(), "updatePWM");
      ledcWrite(chMotor, pwmval);
    }
  }
}

///// SETUP CORES //////////////////////////////////////////////////////////////

void setupControl( void * parameter )
{
  ///// SET PIN MODES /////
  const int pinArrayOut[14] = {25,26,4,16,17,5,18,19,27,2,14,12,15,0};
  for (int i=0; i<14; i++) { pinMode(pinArrayOut[i], OUTPUT); }

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
  { displayError(LCD_CONN_ERROR); ERROR_STATE = true; }

  while(ERROR_STATE) { LCD_ENABLE = false; }

  dht.begin();
  if(!testDHT())
  { displayError(DHT_ERROR); ERROR_STATE = true; }

  while(ERROR_STATE) { DHT_STATE = false; }


  display.clearDisplay();
  display.setTextSize(2);
  display.setTextColor(SSD1306_WHITE);
  display.cp437(true);

  display.println("GearTrain");
  display.print("v5.3-beta");
  display.display();

  delay(2000);

  display.clearDisplay();
  display.println("Loading Setup...");

  delay(2000);

  while(!PROGRAM_START);

  xTaskCreatePinnedToCore(
    loopControl,
    "LoopControl",
    1000,
    NULL,
    2,
    &TaskControl,
    1);

  Serial << "Finishing Control Setup" << endl;
  vTaskDelete(NULL);
}

void setupConnection ( void * parameter )
{
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

  delay(4000);

  /*while(!PROGRAM_START)
  {
    Serial.println("HELLO");

    if (LCD_ENABLE)
    {
      //display.clearDisplay();
      //display.print("IP: ");
      //display.println(WiFi.localIP());
      //display.print("Connections: ");
      //display.println(connectedClients);
    }
  }*/

  xTaskCreatePinnedToCore(
    loopConnection,
    "LoopConnection",
    1000,
    NULL,
    2,
    &TaskConnection,
    0);

  /*if(!SPIFFS.begin())
  {
    displayError(SPIFFS_ERROR);
    ERROR_STATE = true;
    exit(-1);
  }  */

  Serial << "Finishing Connection Setup!!" << endl;
  vTaskDelete(NULL);
}


///////// TASK LOOPS ///////////////////////////////////////////////////////////

void loopControl( void * parameter )
{
  while(STATE_CONTROL)
  {

  }
}

void loopConnection( void * parameter )
{
  for(;;)
  {

  }
}


///////////START SETUP//////////////////////////////////////////////////////////

void setup()
{
  Serial.begin(115200);

  /*xTaskCreatePinnedToCore(
    setupControl,
    "SetupControl",
    1000,
    NULL,
    1,
    &SetupControl,
    1);

  attachInterrupt(digitalPinToInterrupt(encoder), enc_Event, RISING);*/


  WiFi.begin(ssid, password);
  xTaskCreatePinnedToCore(
    setupConnection,
    "SetupConnection",
    1000,
    NULL,
    1,
    &SetupConnection,
    0);
}

///// DEFAULT LOOP /////
void loop(){}
////////////////////////


/*currentMillis = esp_timer_get_time();

if(currentMillis-refMillis>=timediff)
{
  RPM = encoderCounter/11*2;
  refMillis = currentMillis;
  encoderCounter = 0;
  events.send(String(RPM).c_str(), "rpm");
}

if(currentMillis-dhtMillis>=dhtTime)
{
  dhtMillis = currentMillis;

  hum = dht.readHumidity();
  temp = dht.readTemperature();

  if(isnan(hum) || isnan(temp))
  {
    displayError(DHT_ERROR);
    return;
  }

  if(ERROR_STATE==true) { displayError(0); }
  events.send(String(hum).c_str(), "humidity");
  events.send(String(temp).c_str(), "temperature");

  display.setTextSize(2);
  display.clearDisplay();
  display.setCursor(0, 0);
  display.print("Temp:");
  display.println(temp);
  display.print("Hum:");
  display.println(hum);
  display.print("RPM:");
  display.println(RPM);

  display.display();
}
*/
