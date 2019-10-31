//Includes
#include <GxEPD2_BW.h>
#include <U8g2_for_Adafruit_GFX.h>
#include <NTPtimeESP.h>
#include <HTTPClient.h>
#include <credentials.h>

//Defines
#define TIMEOUT   5000  //5 sec

//Global variables
const char* host = "http://192.168.178.53/";
char dateChar[13];
char timeChar[11];
float transData;
strDateTime dateTime;

GxEPD2_BW<GxEPD2_213_B73, GxEPD2_213_B73::HEIGHT> display(GxEPD2_213_B73(/*CS=*/ 5, /*DC=*/ 17, /*RST=*/ 16, /*BUSY=*/ 4)); // GDEH0213B73
U8G2_FOR_ADAFRUIT_GFX u8g2Fonts;
NTPtime NTPhu("hu.pool.ntp.org");   // Choose server pool as required

void setup()
{
  unsigned long wifitimeout = millis();

  Serial.begin(115200);
  esp_sleep_enable_ext0_wakeup(GPIO_NUM_39, 0); //1 = High, 0 = Low
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
  ReadTransmitter();
  DrawText();
  display.hibernate();
  esp_deep_sleep_start();
}

void loop()
{
}

void DrawText()
{
  char displayData[8];

  dtostrf(transData, 4, 1, displayData);
  strcat(displayData, "°C");
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
    u8g2Fonts.print(displayData);
    u8g2Fonts.setFont(u8g2_font_logisoso16_tf);
    u8g2Fonts.setCursor(10, 26); // start writing at this position
    u8g2Fonts.print(dateChar);
    u8g2Fonts.setCursor(120, 26); // start writing at this position
    u8g2Fonts.print(timeChar);
    u8g2Fonts.setCursor(215, 26); // start writing at this position
    u8g2Fonts.print("97%");
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
  bool validDate = 0;
  int nroftry = 0;
  while (!validDate)
  {
    nroftry++;
    validDate = RefreshDateTime();
    if (nroftry > 1000)
    {
      break;
    }
  }
  sprintf(dateChar, "%i-%02i-%02i", dateTime.year, dateTime.month, dateTime.day);
  sprintf(timeChar, "%02i:%02i:%02i", dateTime.hour, dateTime.minute, dateTime.second);
}

void ReadTransmitter()
{
  HTTPClient webclient;
  webclient.begin(host);
  webclient.setConnectTimeout(500);
  if (HTTP_CODE_OK == webclient.GET())
  {
    transData = webclient.getString().toFloat();
  }
  else
  {
    transData = 0.0f;
  }
  webclient.end();
  if ((transData < 10.0) || transData > 84.0)
  {
    transData = 0.0f;
  }
}
