/*
   10/17/2020 Ambrose Campbell
   Project to display MQTT data on a OLED screen
*/
#include <WiFi.h>
#include <PubSubClient.h>
#include "certs.h"
#include "sdkconfig.h"
#include "esp_system.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/timers.h"
#include "freertos/event_groups.h"
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1351.h>
#include "time.h"
#include <driver/adc.h>
//
//
hw_timer_t * timer = NULL;
EventGroupHandle_t eg;
#define evtDoDisplay             ( 1 << 1 )
#define evtCollectHistory        ( 1 << 2 )
#define evtParseMQTT             ( 1 << 3 )
#define evtDoTheAirParticleThing ( 1 << 4 )
// the events in this event groups tasks have been given a delay to avoid a possible race condition.
#define evtGroupBits ( evtCollectHistory | evtDoDisplay | evtDoTheAirParticleThing )
//                                                 CS GPIO_NUM_27, DC GPIO_NUM_12, DIN GPIO_NUM_13, CLK GPIO_NUM_14, RST GPIO_NUM_26
Adafruit_SSD1351 tft = Adafruit_SSD1351( 128, 128, GPIO_NUM_27,    GPIO_NUM_12,    GPIO_NUM_13,     GPIO_NUM_14,     GPIO_NUM_26 );
////
WiFiClient   wifiClient;
PubSubClient MQTTclient( mqtt_server, mqtt_port, wifiClient );
//////
// black, blue, red, green, cyan, magenta, yellow, white, lightyellow, brown
const int DataCells = 60;
int    ColorPalette[10] = { 0x0000, 0x001F, 0xF800, 0x07E0, 0x07FF, 0xF81F, 0xFFE0, 0xFFFF, 0xFFFFD8, 0xFF8040 };
int    arrayCellPtr = 0;
float  oPressureHistory[DataCells] = {0.0f};
String str_eTopic;
char   strPayload [300] = {NULL};
float  oTemperature = 0.0f;
float  oHumidity = 0.0f;
float  oIAQ = 0.0f; // Indexed Air Quality
float  oPressure = 0.0f;
byte mac[6];
////
////
SemaphoreHandle_t sema_HistoryCompleted;
SemaphoreHandle_t sema_MQTT_Parser;;
SemaphoreHandle_t sema_MQTT_KeepAlive;
////
volatile int iDoTheThing = 0;
////
void IRAM_ATTR onTimer()
{
  BaseType_t xHigherPriorityTaskWoken;
  iDoTheThing++;
  if ( iDoTheThing == 60000 )
  {
    xEventGroupSetBitsFromISR( eg, evtGroupBits, &xHigherPriorityTaskWoken );
    iDoTheThing = 0;
  }
}
////
void setup()
{
  /* Use 4th timer of 4.
    1 tick 1/(80MHZ/80) = 1us set divider 80 and count up.
    Attach onTimer function to timer
    Set alarm to call timer ISR, every 1000uS and repeat / reset ISR (true) after each alarm
    Start an timer alarm
  */
  timer = timerBegin( 3, 80, true );
  timerAttachInterrupt( timer, &onTimer, true );
  timerAlarmWrite(timer, 1000, true);
  timerAlarmEnable(timer);
  //
  str_eTopic.reserve(300);
  //
  eg = xEventGroupCreate();
  //
  tft.begin();
  tft.fillScreen(0x0000);
  //
  sema_MQTT_Parser      = xSemaphoreCreateBinary();
  sema_HistoryCompleted = xSemaphoreCreateBinary();
  sema_MQTT_KeepAlive   = xSemaphoreCreateBinary();
  xSemaphoreGive( sema_HistoryCompleted );
  xSemaphoreGive( sema_MQTT_KeepAlive ); // found keep alive can mess with a publish, stop keep alive during publish
  ////
  xTaskCreatePinnedToCore( fparseMQTT,      "fparseMQTT",      7000,  NULL, 5, NULL, 1 ); // assign all to core 1, WiFi in use.
  xTaskCreatePinnedToCore( fCollectHistory, "fCollectHistory", 10000, NULL, 4, NULL, 1 );
  xTaskCreatePinnedToCore( MQTTkeepalive,   "MQTTkeepalive",   7000,  NULL, 2, NULL, 1 ); //this task makes a WiFi and MQTT connection.
  xTaskCreatePinnedToCore( fUpdateDisplay,  "fUpdateDisplay",  50000, NULL, 5, NULL, 1 );
  ////
} //setup() END
////
////
void GetTheTime()
{
  char* ntpServer = "2.us.pool.ntp.org";
  int gmtOffset_sec = -(3600 * 7 );
  int daylightOffset_sec = 3600;
  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
}
////
// http://www.cplusplus.com/reference/ctime/strftime/
////
int getHour()
{
  struct tm timeinfo;
  getLocalTime(&timeinfo);
  char _time[ 5 ];
  strftime( _time, 80, "%T", &timeinfo );
  return String(_time).toInt();
}
////
void printLocalTime()
{
  struct tm timeinfo;
  getLocalTime(&timeinfo);
  char _time[ 80 ];
  strftime( _time, 80, "%T", &timeinfo );
  log_i( "%s", _time);
}
////
/*
   Collect history information
   task triggered by hardware timer once a minute
*/
void fCollectHistory( void * parameter )
{
  int StorageTriggerCount = 119;
  bool Tick = true;
  int maxStorageTriggerCount = 120;
  for (;;)
  {
    xEventGroupWaitBits (eg, evtCollectHistory, pdTRUE, pdTRUE, portMAX_DELAY );
    xSemaphoreTake( sema_HistoryCompleted, portMAX_DELAY );
    oPressureHistory[arrayCellPtr] = oPressure;
    StorageTriggerCount++; //triggered by the timer isr once a minute
    log_i( "storage trigger count %d", StorageTriggerCount );
    if ( StorageTriggerCount == maxStorageTriggerCount )
    {
      arrayCellPtr++;
      log_i( "arrayCellPtr %d", arrayCellPtr );
      if ( arrayCellPtr == DataCells )
      {
        arrayCellPtr = 0;
      }
      StorageTriggerCount = 0;
    }
    xSemaphoreGive( sema_HistoryCompleted );
    // log_i( " high watermark %d",  uxTaskGetStackHighWaterMark( NULL ) );
  }
  vTaskDelete( NULL );
} //void fCollectHistory( void * parameter )
////
////
void fUpdateDisplay( void * parameter )
{
  // Color definitions
  // http://www.barth-dev.de/online/rgb565-color-picker/
  /*
    ColorPalette[0] = 0x0000; //BLACK
    ColorPalette[1] = 0x001F; //BLUE
    ColorPalette[2] = 0xF800; //RED
    ColorPalette[3] = 0x07E0; //GREEN
    ColorPalette[4] = 0x07FF; //CYAN
    ColorPalette[5] = 0xF81F; //MAGENTA
    ColorPalette[6] = 0xFFE0; //YELLOW
    ColorPalette[7] = 0xFFFF; //WHITE
    ColorPalette[8] = 0xFFFFD8; //LIGHTYELLOW
    ColorPalette[9] = 0xFF8040; //BROWN
  */
  int rowRef = 110;
  int hRef = int( oPressureHistory[0] );
  const int nextPoint = 2;
  int nextCol = 0;
  for (;;)
  {
    xEventGroupWaitBits (eg, evtDoDisplay, pdTRUE, pdTRUE, portMAX_DELAY ); //
    vTaskDelay( 1 );
    tft.fillScreen( ColorPalette[0] );
    tft.setTextColor( ColorPalette[6] );
    tft.setCursor( 0, 25 );
    tft.print( "Temperature: " + String(oTemperature) + "F" );
    tft.setCursor( 0, 35 );
    tft.print( "Humidity " + String(oHumidity) + "%" );
    //
    //set the color of the value to be displayed
    if ( oIAQ < 51.0f )
    {
      tft.setTextColor( ColorPalette[3] );
    }
    if ( (oIAQ >= 50.0f) && (oIAQ <= 100.0f) )
    {
      tft.setTextColor( ColorPalette[6] );
    }
    if ( (oIAQ >= 100.0f) && (oIAQ <= 150.0f) )
    {
      tft.setTextColor( ColorPalette[9] );
    }
    if ( (oIAQ >= 150.0f) && (oIAQ <= 200.0f) )
    {
      tft.setTextColor( ColorPalette[2] );
    }
    if ( (oIAQ >= 200.00f) && (oIAQ <= 300.0f) )
    {
      tft.setTextColor( ColorPalette[5] );
    }
    if ( (oIAQ > 300.0f) )
    {
      tft.setTextColor( ColorPalette[7] );
    }
    // display AQI
    tft.setCursor( 0, 45 );
    tft.print( "AQ Index " + String(oIAQ) );
    //
    tft.setTextColor( ColorPalette[1] ); //set graph line color
    tft.setCursor( 0, 85 );
    tft.print( "Pres: " + String(oPressure) + "mmHg" );
    hRef = int( oPressureHistory[0] );
    nextCol = 0;
    xSemaphoreTake( sema_HistoryCompleted, portMAX_DELAY );
    for (int i = 0; i <= DataCells; i++)
    {
      int hDisplacement = hRef - int( oPressureHistory[i] ); // cell height displacement from baseline
      tft.setCursor( nextCol , (rowRef + hDisplacement) );
      tft.print( "_" );
      nextCol += nextPoint;
      //place vertical bar representing a 24 hour time period.
      if ( (nextCol % 24) == 0 )
      {
        //tft.drawLine( col, row, width, height, color );
        tft.drawLine( uint16_t(nextCol), uint16_t(120), uint16_t(nextCol), uint16_t(115), ColorPalette[7] );
      }
    }
    // advance cursor to place a begin of graph marker. each letter is 5 spaces wide but graph position marker only advances 2 spaces
    // to keep cursor at begining place cursor +2 the nextCol position.
    tft.setCursor( (arrayCellPtr * nextPoint), (rowRef + 3) );
    tft.print( "I" );
    xSemaphoreGive( sema_HistoryCompleted );
    //     log_i( "fUpdateDisplay MEMORY WATERMARK %d", uxTaskGetStackHighWaterMark(NULL) );
  }
  vTaskDelete( NULL );
} // void fUpdateDisplay( void * parameter )
/*
    Important to not set vTaskDelay to less then 10. Errors begin to develop with the MQTT and network connection.
    makes the initial wifi/mqtt connection and works to keeps those connections open.
*/
void MQTTkeepalive( void *pvParameters )
{
  // setting must be made before a mqtt connection is made
  MQTTclient.setKeepAlive( 90 ); // setting keep alive to 90 seconds makes for a very reliable connection, must be set before the 1st connection is made.
  for (;;)
  {
    //check for a is connected and if the WiFi thinks its connected, found checking on both is more realible than just a single check
    if ( (wifiClient.connected()) && (WiFi.status() == WL_CONNECTED) )
    {
      xSemaphoreTake( sema_MQTT_KeepAlive, portMAX_DELAY ); // whiles loop() is running no other mqtt operations should be in process
      MQTTclient.loop();
      xSemaphoreGive( sema_MQTT_KeepAlive );
    }
    else {
      log_i( "MQTT keep alive found MQTT status %s WiFi status %s", String(wifiClient.connected()), String(WiFi.status()) );
      if ( !(WiFi.status() == WL_CONNECTED) )
      {
        connectToWiFi();
      }
      connectToMQTT();
    }
    vTaskDelay( 250 ); //task runs approx every 250 mS
  }
  vTaskDelete ( NULL );
}
////
void connectToMQTT()
{

  String clientID = String(mac[0]) + String(mac[4]) ;
  log_i( "connect to mqtt clientID %s", clientID );
  while ( !MQTTclient.connected() )
  {
    MQTTclient.connect( clientID.c_str(), mqtt_username, mqtt_password );
    log_i( "connecting to MQTT" );
    vTaskDelay( 250 );
  }
  log_i("MQTT Connected");
  MQTTclient.setCallback( mqttCallback );
  MQTTclient.subscribe( mqtt_topic );
}
//
void connectToWiFi()
{
  log_i( "connect to wifi" );
  while ( WiFi.status() != WL_CONNECTED )
  {
    WiFi.disconnect();
    WiFi.begin( SSID, PASSWORD );
    log_i(" waiting on wifi connection" );
    vTaskDelay( 4000 );
  }
  log_i( "Connected to WiFi" );
  WiFi.macAddress(mac);
  log_i( "mac address %d.%d.%d.%d.%d", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]  );
  WiFi.onEvent( WiFiEvent );
  GetTheTime();
  printLocalTime();
}
////
void fparseMQTT( void *pvParameters )
{
  xSemaphoreGive ( sema_MQTT_Parser );
  for (;;)
  {
    xEventGroupWaitBits (eg, evtParseMQTT, pdTRUE, pdTRUE, portMAX_DELAY ); //
    xSemaphoreTake( sema_MQTT_Parser, portMAX_DELAY );
    if ( str_eTopic == topicOutsideTemperature )
    {
      oTemperature = String(strPayload).toFloat();
    }
    if ( (String)str_eTopic == topicOutsideHumidity )
    {
      oHumidity = String(strPayload).toFloat();
    }
    if ( (String)str_eTopic == topicAQIndex )
    {
      oIAQ = String(strPayload).toFloat();
    }
    if ( String(str_eTopic) == topicOutsidePressure )
    {
      oPressure = String(strPayload).toFloat();
    }
    // clear pointer locations
    memset( strPayload, NULL, 300 );
    str_eTopic = ""; //clear string buffer
    xSemaphoreGive( sema_MQTT_Parser );
  }
} // void fparseMQTT( void *pvParameters )
////
static void mqttCallback(char* topic, byte * payload, unsigned int length)
{
  xSemaphoreTake( sema_MQTT_Parser, portMAX_DELAY);
  str_eTopic = topic;
  int i = 0;
  for ( i; i < length; i++)
  {
    strPayload[i] = ((char)payload[i]);
  }
  strPayload[i] = NULL;
  //log_i( "topic %s payload %s" ,str_eTopicPtr, strPayloadPtr );
  xSemaphoreGive ( sema_MQTT_Parser );
  xEventGroupSetBits( eg, evtParseMQTT ); // trigger tasks
} // void mqttCallback(char* topic, byte* payload, unsigned int length)
////
void WiFiEvent(WiFiEvent_t event)
{
  switch (event) {
    case SYSTEM_EVENT_STA_CONNECTED:
      log_i("Connected to access point");
      break;
    case SYSTEM_EVENT_STA_DISCONNECTED:
      log_i("Disconnected from WiFi access point");
      break;
    case SYSTEM_EVENT_AP_STADISCONNECTED:
      log_i("WiFi client disconnected");
      break;
    default: break;
  }
}
////
void loop() { }