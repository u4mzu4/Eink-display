//Includes
#include <GxEPD2_BW.h>
#include <U8g2_for_Adafruit_GFX.h>
#include <time.h>
#include <HTTPClient.h>
#include <credentials.h>
#include <InfluxDbClient.h>

//Defines
#define TIMEOUT   5000  // 5 sec
#define ANALOGPIN 35
#define MINVOLT 1830.0 // 3.3*4096/6.6
#define MAXVOLT 2330.0 // 4.2*4096/6.6
#define OFFSET 8.0
#define MAXNROFATTEMPTS 8
#define NTPSERVER "hu.pool.ntp.org"

//Global variables
char dateChar[13];
char timeChar[11];
struct tm dateTime;

//Define services
GxEPD2_BW<GxEPD2_213_B73, GxEPD2_213_B73::HEIGHT> display(GxEPD2_213_B73(/*CS=*/ 5, /*DC=*/ 17, /*RST=*/ 16, /*BUSY=*/ 4)); // GDEH0213B73
U8G2_FOR_ADAFRUIT_GFX u8g2Fonts;
WiFiClient wclient;
HTTPClient hclient;
InfluxDBClient influxclient(influxdb_URL, influxdb_ORG, influxdb_BUCKET, influxdb_TOKEN);

void setup()
{
  unsigned long wifitimeout = millis();

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
  configTime(3600, 3600, NTPSERVER);
  display.init(); // uses standard SPI pins, e.g. SCK(18), MISO(19), MOSI(23), SS(5)
  u8g2Fonts.begin(display);
  display.clearScreen(GxEPD_WHITE);
  DateTime2String();
  ReadTransmitter();
  BatteryLevel();
  DrawText();
  display.hibernate();
  esp_deep_sleep_start();
}

void loop()
{
}

void DrawText()
{
  float bPercent;
  float tData;
  char tempChar[8];
  char batteryChar[5];

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
  display.setFullWindow();
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
  display.display(false);

}

bool RefreshDateTime()
{
  bool timeIsValid = getLocalTime(&dateTime);

  if (dateTime.tm_year > 135)
  {
    return 0;
  }
  return (timeIsValid);
}

void DateTime2String()
{
  RefreshDateTime();
  sprintf(dateChar, "%i-%02i-%02i", dateTime.tm_year + 1900, dateTime.tm_mon, dateTime.tm_mday);
  sprintf(timeChar, "%02i:%02i:%02i", dateTime.tm_hour, dateTime.tm_min, dateTime.tm_sec);
}

float ReadTransmitter()
{
  float transData;
  int nrOfattempts = 0;
  bool successComm = 0;

  hclient.begin(wclient, host[0]);
  hclient.setConnectTimeout(500);
  if (HTTP_CODE_OK == hclient.GET())
  {
    transData = hclient.getString().toFloat();
    transData -= OFFSET;
    hclient.end();
  }
  else
  {
    successComm = 0;
    nrOfattempts ++;
    transData = InfluxBatchReaderInfo();
  }
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

float InfluxBatchReaderInfo() {
  float influxData;
  String query = "from(bucket: \"thermo_data\") |> range(start: -1m, stop:now()) |> filter(fn: (r) => r[\"_measurement\"] == \"thermostat\" and r[\"_field\"] == \"childRoomTemp\") |> last()";

  FluxQueryResult result = influxclient.query(query);
  while (result.next()) {
    influxData = result.getValueByName("_value").getDouble();
  }
  result.close();
  return influxData;
}
