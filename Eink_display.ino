//Includes
#include <GxEPD2_BW.h>
#include <U8g2_for_Adafruit_GFX.h>
#include <NTPtimeESP.h>
#include <HTTPClient.h>
#include <credentials.h>
#include <Wire.h>
#include <SparkFun_APDS9960.h>

//Defines
#define TIMEOUT   5000  // 5 sec
#define ANALOGPIN 35
#define MINVOLT 1830.0 // 3.3*4096/6.6
#define MAXVOLT 2330.0 // 4.2*4096/6.6
#define APDS9960_INT    GPIO_NUM_34  // Needs to be an interrupt pin
#define PROX_INT_HIGH   128 // Proximity level for interrupt
#define PROX_INT_LOW    0   // No far interrupt

//Global variables
const char* host = "http://192.168.178.53/";
char dateChar[13];
char timeChar[11];
strDateTime dateTime;

//Define services
GxEPD2_BW<GxEPD2_213_B73, GxEPD2_213_B73::HEIGHT> display(GxEPD2_213_B73(/*CS=*/ 5, /*DC=*/ 17, /*RST=*/ 16, /*BUSY=*/ 4)); // GDEH0213B73
U8G2_FOR_ADAFRUIT_GFX u8g2Fonts;
NTPtime NTPhu("hu.pool.ntp.org");   // Choose server pool as required
WiFiClient wclient;
HTTPClient hclient;
SparkFun_APDS9960 apds = SparkFun_APDS9960();

void setup()
{
  unsigned long wifitimeout = millis();
  esp_sleep_enable_ext0_wakeup(APDS9960_INT, 0); //1 = High, 0 = Low
  SetupProximitySensor();
  WiFi.persistent(false);
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(50);
    if (millis() - wifitimeout > TIMEOUT)
    {
      break;
    }
  }
  display.init(); // uses standard SPI pins, e.g. SCK(18), MISO(19), MOSI(23), SS(5)
  u8g2Fonts.begin(display);
  DateTime2String();
  DrawText();
  display.hibernate();
  apds.clearProximityInt();
  delay(100);
  esp_deep_sleep_start();
}

void loop()
{
}

void DrawText()
{
  char tempChar[8];
  char batteryChar[5];
  float bPercent;
  float tData;

  bPercent = BatteryLevel();
  tData = ReadTransmitter();
  dtostrf(tData, 4, 1, tempChar);
  strcat(tempChar, "°C");
  dtostrf(bPercent, 2, 0, batteryChar);
  strcat(batteryChar, "%");
  display.setRotation(1);
  u8g2Fonts.setFontMode(1);                 // use u8g2 transparent mode (this is default)
  u8g2Fonts.setFontDirection(0);            // left to right (this is default)
  u8g2Fonts.setForegroundColor(GxEPD_BLACK);         // apply Adafruit GFX color
  u8g2Fonts.setBackgroundColor(GxEPD_WHITE);         // apply Adafruit GFX color
  u8g2Fonts.setFont(u8g2_font_logisoso58_tf);
  uint16_t tempx = (display.width() - 200) / 2;
  uint16_t tempy = display.height() - 20;
  display.firstPage();
  do
  {
    display.fillScreen(GxEPD_WHITE);
    u8g2Fonts.setCursor(tempx, tempy); // start writing at this position
    u8g2Fonts.print(tempChar);
    u8g2Fonts.setFont(u8g2_font_logisoso16_tf);
    u8g2Fonts.setCursor(10, 26); // start writing at this position
    u8g2Fonts.print(dateChar);
    u8g2Fonts.setCursor(120, 26); // start writing at this position
    u8g2Fonts.print(timeChar);
    u8g2Fonts.setCursor(210, 26); // start writing at this position
    u8g2Fonts.print(batteryChar);
  }
  while (display.nextPage());
}

bool RefreshDateTime()
{
  dateTime = NTPhu.getNTPtime(1.0, 1);
  if (dateTime.year > 2035)
  {
    return 0;
  }
  return (dateTime.valid);
}

void DateTime2String()
{
  int nroftry = 0;
  while (!RefreshDateTime())
  {
    if (++nroftry > 1000)
    {
      break;
    }
  }
  sprintf(dateChar, "%i-%02i-%02i", dateTime.year, dateTime.month, dateTime.day);
  sprintf(timeChar, "%02i:%02i:%02i", dateTime.hour, dateTime.minute, dateTime.second);
}

float ReadTransmitter()
{
  float transData;
  
  hclient.begin(wclient, host);
  hclient.setConnectTimeout(500);
  if (HTTP_CODE_OK == hclient.GET())
  {
    transData = hclient.getString().toFloat();
  }
  else
  {
    transData = 0.0f;
  }
  hclient.end();
  if ((transData < 10.0) || transData > 84.0)
  {
    transData = 0.0f;
  }
  return transData;
}

float BatteryLevel()
{
  float batteryPercent;
  unsigned int batteryData = 0;

  for (int i = 0; i < 64; i++)
  {
    batteryData = batteryData + analogRead(ANALOGPIN);
  }
  batteryData = batteryData >> 6; //divide by 64
  batteryPercent = (float(batteryData) - MINVOLT) / (MAXVOLT - MINVOLT) * 100.0;
  if (batteryPercent < 0.0)
  {
    batteryPercent = 0.0;
  }
  if (batteryPercent > 100.0)
  {
    batteryPercent = 100.0;
  }
  return batteryPercent;
}

void SetupProximitySensor()
{
  apds.init();
  apds.enableProximitySensor(true);
  apds.setProximityIntLowThreshold(PROX_INT_LOW);
  apds.setProximityIntHighThreshold(PROX_INT_HIGH);
  apds.setProximityGain(PGAIN_8X);
  apds.setProximityGain(LED_DRIVE_12_5MA);
  apds.clearProximityInt();
  delay(100);
}
