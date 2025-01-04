#include <WiFi.h>
#include <PubSubClient.h>
#include "certs.h"
#include "sdkconfig.h"
#include "esp_system.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/timers.h"
#include "freertos/event_groups.h"
//#include <Wire.h>
#include <SPI.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_BME280.h>
////
Adafruit_BME280 bme280( 5 ); // hardware SPI
////
WiFiClient   wifiClient;
PubSubClient MQTTclient( mqtt_server, mqtt_port, wifiClient );
////
SemaphoreHandle_t sema_MQTT_KeepAlive;
////
byte mac[6];
////
void setup()
{
  sema_MQTT_KeepAlive   = xSemaphoreCreateBinary();
  xSemaphoreGive( sema_MQTT_KeepAlive ); // found keep alive can mess with a publish, stop keep alive during publish
  ////
  SPI.begin();
  bme280.begin();
  xTaskCreatePinnedToCore( MQTTkeepalive, "MQTTkeepalive", 20000, NULL, 3, NULL, 1 );
  xTaskCreatePinnedToCore( DoTheBME280Thing, "DoTheBME280Thing", 20000, NULL, 4, NULL, 1);
} //void setup()
//
void DoTheBME280Thing( void *pvParameters )
{
  float temperature = 0.0f;
  float pressure    = 0.0f;
  float humidity    = 0.f;
  //wait for a mqtt connection
  while ( !MQTTclient.connected() )
  {
    vTaskDelay( 250 );
  }
  for (;;)
  {
    temperature = ( 1.8f * bme280.readTemperature() ) +32.0f;
    pressure    = bme280.readPressure() / 133.3223684f; // mmHg
    humidity    = bme280.readHumidity();
    log_i( " temperature %f, Pressure %f, Humidity %f", temperature, pressure, humidity);
    xSemaphoreTake( sema_MQTT_KeepAlive, portMAX_DELAY );
    if ( MQTTclient.connected() )
    {
      MQTTclient.publish( topicInsideTemp, String(temperature).c_str() );
      vTaskDelay( 2 ); // gives the Raspberry Pi 4 time to receive the message and process
      MQTTclient.publish( topicInsideHumidity, String(humidity).c_str() );
      vTaskDelay( 2 ); // no delay and RPi is still processing previous message
      MQTTclient.publish( topicInsidePressure, String(pressure).c_str() );
    }
    xSemaphoreGive( sema_MQTT_KeepAlive );
    vTaskDelay( 1000 * 15 );
    log_i( "DoTheBME280Thing high watermark %d",  uxTaskGetStackHighWaterMark( NULL ) );
  }
  vTaskDelete ( NULL );
}
////
/*
    Important to not set vTaskDelay to less then 10. Errors begin to develop with the MQTT and network connection.
    makes the initial wifi/mqtt connection and works to keeps those connections open.
*/
void MQTTkeepalive( void *pvParameters )
{
  // setting must be set before a mqtt connection is made
  MQTTclient.setKeepAlive( 90 ); // setting keep alive to 90 seconds makes for a very reliable connection, must be set before the 1st connection is made.
  for (;;)
  {
    //check for a is-connected and if the WiFi 'thinks' its connected, found checking on both is more realible than just a single check
    if ( (wifiClient.connected()) && (WiFi.status() == WL_CONNECTED) )
    {
      xSemaphoreTake( sema_MQTT_KeepAlive, portMAX_DELAY ); // whiles MQTTlient.loop() is running no other mqtt operations should be in process
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
  // create client ID from mac address
  String clientID = String(mac[0]) + String(mac[5]) ;
  log_i( "connect to mqtt as client %s", clientID );
  while ( !MQTTclient.connected() )
  {
    MQTTclient.connect( clientID.c_str(), mqtt_username, mqtt_password );
    log_i( "connecting to MQTT" );
    vTaskDelay( 250 );
  }
  log_i("MQTT Connected");
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
}
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