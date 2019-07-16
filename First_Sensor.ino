#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <EEPROM.h>
#include <string.h>
#include <ESP8266mDNS.h>
//input sensor ID
unsigned int sensorID = 1;
unsigned int nextSensorID = 2;

//connect to next sensor
String _ssid = "sensor" + String(nextSensorID ,DEC);
const char *ssid = _ssid.c_str();//toCharStar(_ssid) ; //SSID STATION MODE
char *password = "123456789"; //Password STATION MODE

//AP for last sensor
String _myssid = "sensor" + String(sensorID );
const char* myssid = _myssid.c_str(); //SSID AP MODE
const char* mypassword = "123456789"; //Password AP MODE

IPAddress hostIP(192,168,nextSensorID,1);
IPAddress apIP(192,168,sensorID,1);

String _host = String("192.") +String("168.")+String(nextSensorID) + String(".1");
String sensorName = String(myssid);
WiFiServer server(80);
ESP8266WebServer serverAP(80);
WiFiClient clientSTA;

//myVar
char estado=0;
float u,x = 0,x_dot ,delta_x,delta_time;
int x_plus_u_lastSensor;
long int last_time = 0; 
char intToStr[6];
unsigned long timeout =0;
int last_x,last_u;
int max_time_out = 3500;
int t1,t2;
int timer1_counter;

void handleSensorData()
{
  if (serverAP.hasArg("u") && serverAP.hasArg("x"))  // this is the variable sent from the client
  { 
    last_u = serverAP.arg("u").toInt();
    last_x = serverAP.arg("x").toInt();
    serverAP.send(200, "text/html", "Data received");
    x_plus_u_lastSensor = last_x + last_u;
    Calculation();
  }
  serverAP.send(200, "text/html", "<h1>You are connected to sensor</h1>");
}

void Calculation()
{
  u = analogRead(A0) + (int)random(-50, 50);
  delta_time = ((float)(millis() - last_time))/1000.0;
  if (delta_time < 0)
    delta_time = ((float)millis())/1000.0;
  else if(delta_time > 6)     //if delta time > 6 s  ==>> data not valid
    delta_time = 0;
    
  last_time = millis();
  x_dot = u + x_plus_u_lastSensor - 3*x;
  delta_x = x_dot * delta_time;
  x = x + delta_x;
  Serial.print(u , DEC);Serial.print(" ");Serial.println(x , DEC);
}


void handleReadSensorData()
{
  String htm = "<html> <h1>";
  htm += sensorName;
  htm += " : <br> </h1>" ;
  htm += "sensor filtered value = ";
  htm += String(x);
  htm += "</html>";
  
  serverAP.send(200, "text/html", htm);
}

void setup()
{
  ESP.wdtDisable();
  Serial.begin(115200);         // Start the Serial communication to send messages to the computer
  delay(500);
  Serial.println('\n');
  
  WiFi.mode(WIFI_AP_STA);

  //AP
  WiFi.softAPConfig(apIP, apIP, IPAddress(255, 255, 255, 0));

  IPAddress myIP = WiFi.softAP(myssid, mypassword);
  serverAP.on("/data/", HTTP_GET, handleSensorData);
  serverAP.on("/read_data/", HTTP_GET, handleReadSensorData);
  serverAP.begin();
  
  //Station
  WiFi.begin(ssid, password);

  
  if (!MDNS.begin(sensorName)) {
    Serial.println("Error setting up MDNS responder!");
    while (1) {
      delay(1000);
    }
  }
  Serial.println("mDNS responder started");
  
  int i = 0;
  while (WiFi.status() != WL_CONNECTED) 
  { // Wait for the Wi-Fi to connect
    delay(1000);
    Serial.print(++i); Serial.print(' ');
  }

  Serial.println('\n');
  Serial.println("Connection established!");  
  Serial.print("IP address:\t");
  Serial.println(WiFi.localIP());         // Send the IP address of the ESP8266 to the computer

  clientSTA.connect(hostIP, 82);
  pinMode(A0 , INPUT);
  last_time = millis();
  
}

void loop()
{

  for(int i = 0; i < 20 ; i++)
  {
    serverAP.handleClient();
    Calculation();
    delay(20);
  }

  int i = 0;
  while (WiFi.status() != WL_CONNECTED) 
  { // Wait for the Wi-Fi to connect
    delay(200);
 //.   Serial.print(++i); Serial.print(' ');
  }
  
  if(estado==0)
  {
    if (WiFi.status() == WL_CONNECTED)
    {
      server.begin();
      server.setNoDelay(true);
      estado=1;
    }
  }

  if(estado==1)
  {
    timeout=millis();
    while (!clientSTA.connect(hostIP, 80) && (millis() - timeout) < 20);
    if (!clientSTA.connect(hostIP, 80)) 
    {
//.      Serial.println("connection failed");
      return;
    }
    Calculation();
    itoa(x, intToStr, 10);
    String url = "/data/";
    url += "?x=";
    url += intToStr;
    url += "&u=";
    itoa(u, intToStr, 10);
    url += intToStr;
    // This will send the request to the server
    clientSTA.flush();
    clientSTA.print(String("GET ") + url + " HTTP/1.1\r\n" +
                 "Host: " + _host + "\r\n" +
                 "Connection: close\r\n\r\n");
                 
    timeout = millis();
    while (clientSTA.available() == 0)
    {
      serverAP.handleClient();delay(20);
      Calculation();
      if (millis() - timeout > max_time_out)
      {
        Serial.println(">>> Client Timeout !");
        clientSTA.stop();
        return;
      }
    }

    delay(1);
  }


}
