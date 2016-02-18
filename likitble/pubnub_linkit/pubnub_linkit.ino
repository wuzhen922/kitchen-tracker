/*
 *SMART KITCHEN TRACKER                                        *
 */
//Importing the BLE and Wifi Library for the Bluetooth and WiFi Communication 
#include <LGATT.h>
#include <LWiFi.h>
#include <LGATTUUID.h>
#include <LGATTClient.h>
#include <LWiFiClient.h>
#include "SPI.h"
#include "./PubNub.h"

const char *g_container1 = "001";
const char *g_container2 = "002";

//Setting up the Linkit to Work with the Local WiFi Router 
#define WIFI_AP "radio" // provide your WIFI_AP name
#define WIFI_PASSWORD "radio@123" //provide your WIFI password
#define WIFI_AUTH LWIFI_WPA

//Setting up the PubNub Publish and Subscribe Channels with Channel Name
char pubkey[] = "pub-c-913ab39c-d613-44b3-8622-2e56b8f5ea6d";
char subkey[] = "sub-c-8ad89b4e-a95e-11e5-a65d-02ee2ddab7fe";
char channel[] = "kitchenDevice-resp";

//Function Prototypes
void pubnubPublish(char *p_data);
void prepare_json_data(const char *p_containerId,const char* p_weight);

//Test UUID to begin the BLE communication
static LGATTUUID test_uuid("B4B4B4B4-B4B4-B4B4-B4B4-B4B4B4B4B4B5");    
LGATTClient bleClient;
LGATTDeviceInfo info = {0};

//Number of Devices Scanned by the Linkit BLE 
volatile unsigned int g_numOfDevices = 0;
char g_jsonResponse[26] = "";
float g_previous_weight_1 = 5.55,g_previous_weight_2 = 5.55;

/*******************************************************************************************************
 Function Name            : prepare_json_data
 Description              : Prepares the json data to be published to the pubnub channel
 Parameters               : p_containerId,p_weight
          p_containerId   : The Unique ID for the container
          p_weight        : Present weight in the specfic container
 *******************************************************************************************************/
void prepare_json_data(const char *p_containerId,const char* p_weight)
{
  strcat(g_jsonResponse,"{\"containerID\":\"");
  strcat(g_jsonResponse,p_containerId);
  strcat(g_jsonResponse,"\",\"weight\":");
  strcat(g_jsonResponse,p_weight);
  strcat(g_jsonResponse,"}");
}
/*******************************************************************************************************
 Function Name            : setup
 Description              : Initialize the Bluetooth and Connection WiFi to the local AP
                            and scanning the available bluetooth connections
 Parameters               : None
 *******************************************************************************************************/
void setup(void) {
  Serial.begin(9600);
  delay(3000);
  //Connecting to the local AP using the SSID and Password
  while (0 == LWiFi.connect(WIFI_AP, LWiFiLoginInfo(WIFI_AUTH, WIFI_PASSWORD)))
  {
      Serial.println(" . ");
      delay(1000);
  }
  //Initialize PubNub PUB SUB keys
  PubNub.begin(pubkey, subkey);
  //Connecting to the test UUID to establish the ble connection
  if (bleClient.begin(test_uuid))
  {
    Serial.println("BLE Successfully Started");
  }
  else
  {
    Serial.println("Failed To Start BLE");
    delay(0xffffffff);
  }
  //Scan's the available ble advertisers 
  g_numOfDevices = bleClient.scan(10);
}
/*******************************************************************************************************
 Function Name            : loop
 Description              : Start the Conncection between the nodes and get the data and publish on 
                            pubnub.
 Parameters               : None
 *******************************************************************************************************/
void loop(void) {
  while (1)
  {
    if(g_numOfDevices == 0){
      g_numOfDevices = bleClient.scan(10);
      Serial.printf("Scanning Number of the Devices [%d]", g_numOfDevices);
    }
    else{
      for (int i = 0; i < g_numOfDevices; i++)
      {
        bleClient.getScanResult(i, info);
        if (!bleClient.connect(info.bd_addr)) // search all services till timeout or searching done.
        {
            Serial.println("Failed to connect to the device");
            delay(0xff);
        }
        else
            Serial.println("Successfully Connection Esatablished");
        long int l_starttime = millis();
        while (1)
        {
          //Primary Service UUID for the GATT Service
          LGATTUUID serviceUUID = 0XFFE0;  
          //Charterstic UUID to write the data to the client
          LGATTUUID characteristicUUID = 0xFFE1;
          LGATTAttributeValue attrValue;
          boolean isPrimary;
          if(bleClient.setupNotification(1,serviceUUID, isPrimary, characteristicUUID));
          if(bleClient.queryNotification(serviceUUID, isPrimary, characteristicUUID,
          attrValue)){
            Serial.println((char *)attrValue.value);
            //attValue.value contains the data that is received from the client ble
            if(attrValue.len > 0){
              float l_weight_1,l_weight_2;
              char l_buf[10];
              char l_buf1[10];
              for(int i = 0; i<=3;i++){
                l_buf[i] = (char)attrValue.value[i+2];
                l_buf1[i] = (char)attrValue.value[i+9];
              }
              l_buf[4] = '\0';
              l_buf1[4] = '\0';
              l_weight_1 = atof(l_buf);
              l_weight_2 = atof(l_buf1);
              if((int)(g_previous_weight_1*100) != (int)(l_weight_1*100)){
                g_previous_weight_1 = l_weight_1;
                prepare_json_data(g_container1,l_buf);
                pubnubPublish(g_jsonResponse);
                memset(g_jsonResponse, 0, sizeof(g_jsonResponse));  
              }
              delay(500);
              if((int)(g_previous_weight_2*100) != (int)(l_weight_2*100)){
                g_previous_weight_2 = l_weight_2;
                prepare_json_data(g_container2,l_buf1);
                pubnubPublish(g_jsonResponse);
                memset(g_jsonResponse, 0, sizeof(g_jsonResponse));  
              }
            }
            Serial.println("Disconnect BLE Communication");
            bleClient.disconnect(info.bd_addr);//Once the data is published disconnecting from the ble 
            delay(5000);
            break;
          }
          long int l_endtime = millis();
          if(l_endtime - l_starttime > 1000){
            break;
          }
        }
      }
    }
    delay(5000);
  }
}
/*******************************************************************************************************
 Function Name            : pubnubPublish
 Description              : Publish the data to the specfied channel 
 Parameters               : p_data
                p_data    : data to be published in the given channel
 *******************************************************************************************************/
void pubnubPublish(char *p_data){
  LWiFiClient *client;
  client = PubNub.publish(channel,p_data);
  if (!client) {
      Serial.println("publishing error");
      delay(1000);
      return;
  }
  while (client->connected()) {
      while (client->connected() && !client->available()); // wait
      char c = client->read();
      Serial.print(c);
  }
  client->stop();
}
//End of the Program
/*******************************************************************************************************************************/
