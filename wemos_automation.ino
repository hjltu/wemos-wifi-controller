// Project name: hjhome
// Copyright (C) 2016  hjltu@ya.ru
// https://launchpad.net/hjhome

// hjhome is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.

// hjhome is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.

// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <http://www.gnu.org/licenses/>.

    //  FOR NodeMCU V3 & Wemos D1 mini //

// code marks: TODO should be implemented, fixed or tested

// change log
// 22-aug-16 start
// 17-mar-18 disable AP mode, add: memory
// 25-mar-18 add: long press, somfy
// 19-apr-18 add anti freezing function
// 12-may-18 add client name edit
// 13-may-18 add counters
// 24-may-18 add buttons
// 28-may-18 change reconnect
// 01-jun-18 add gap
// 04-jul-18 add last will
// 08-jul-18 add default network settings
// 16-sep-18 fix mqtt server
// 18-sep-18 fix dimm delay
// 20-sep-18 add reset timer to 20 sec
// 15-oct-18 add shutters
// 08-nov-18 dimm flash (lenovo x130, arduino 1.8.5)
// 22-jan-19 add revers, change shutters: divider by seconds
// 23-jan-19 fix shutters one second step
// 25jul26 add ap mode
// 29jul26 add temp,tmp,hum simulation

// D0   GPIO16
// D1   GPIO5
// D2   GPIO4       output not work
// D3   GPIO0       input not boot if push in gnd
// D4   GPIO2       output not work, input not boot if push in gnd
// D5   GPIO14      output not work
// D6   GPIO12
// D7   GPIO13
// D8   GPIO15      rele no boot, not in use
// VU   5volt

// EEPROM Map:
// 0-19     int I/O states
// 50-53    mqtt server ip
// 60-79    long counters
// 80-99    String name
// 100-119  int opt state
// 200-219  net_ssid
// 220-239  net_pass


#include <EEPROM.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <DHT.h>
#include <ESP8266WiFi.h>
#include <PubSubClient.h>

const char* vers = "23-jul-26";

// default settings
String str_name = "wemos";
String str_ssid = "hjhome";
String str_pass = "pass1234";
String str_mac;
String str_ip;

// Access Point
String ap_ssid = "ESP8266_hjhome";
String ap_pass = "pass1234";
IPAddress ap_ip(192,168,9,1);
IPAddress ap_gateway(192,168,9,1);
IPAddress ap_subnet(255,255,255,0);

// MQTT server
WiFiClient wclient;
PubSubClient mclient(wclient);
IPAddress mqtt_server(0,0,0,0); // in EEPROM
IPAddress mqtt_default_server(192,168,0,10);
IPAddress mqtt_ap_server(192,168,9,10);
String str_topic;
String str_payload;

// web server
WiFiServer web_server(80);

//oneWire pin
OneWire oneWire(5);
DallasTemperature sensors(&oneWire);

//DHT pin
DHT dht(4, DHT22, 15);

//  check symbols
String my_base64="0123456789abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ_-";

const int BUFSIZE = 20;
//char buf[BUFSIZE];

bool bt[18],rele[18],net_default,http,term,count;
bool state,period,blinds,shutters,buttons,sensor;
bool revers,net_ap_mode;
byte ac_pwm[18],pwm[18],gap,my_mqtt[4];
byte somfy,ac_somfy,shut,ac_shut;
int temp,hum,tmp,sn,an,bcount[18],pin;

unsigned long lcount[18],rcount[18];
unsigned long stop_count,scount,shcount;
unsigned long value[4];

void setup()
{
    WiFi.softAPdisconnect(true);
    Serial.begin(115200);
    Serial.println("");
    Serial.println("setup...");
    delay(10);
    EEPROM.begin(256);
    my_eeprom_states();
    my_pin_mode();
    Serial.println(F("Pin mode OK"));
    if(net_default==false && net_ap_mode==false)
    {
        str_name = my_str_read(str_name,80);
        delay(10);
        str_ssid = my_str_read(str_ssid,200);
        delay(10);
        str_pass = my_str_read("ERR: pass",220);
        mqtt_server[0] = EEPROM.read(50);
        mqtt_server[1] = EEPROM.read(51);
        mqtt_server[2] = EEPROM.read(52);
        mqtt_server[3] = EEPROM.read(53);
        mclient.set_server(mqtt_server);
        Serial.printf("Set Saved Network: Name: %s, SSID: %s, PASS: %s, MQTT: %s\n",
            str_name, str_ssid, str_pass.c_str(), mqtt_server.toString().c_str());
    }
    if(net_default==true)
    {
        mqtt_server = mqtt_default_server;
        mclient.set_server(mqtt_server);
        Serial.printf("Set Default Network: Name: %s, SSID: %s, PASS: %s, MQTT: %s\n",
            str_name, str_ssid, str_pass.c_str(), mqtt_server.toString().c_str());
    }
    // TODO test AP mode
    if(net_ap_mode==true)
    {
        str_ssid = ap_ssid;
        str_pass = ap_pass;
        mqtt_server = mqtt_ap_server;
        mclient.set_server(mqtt_server);
        delay(0);
        Serial.printf("Set AP Network: Name: %s, SSID: %s, PASS: %s, MQTT: %s\n",
            str_name, str_ssid, str_pass.c_str(), mqtt_server.toString().c_str());
        my_ap_mode();
    }
    Serial.printf("Current Network Check: Name: %s, SSID: %s, PASS: %s, MQTT: %s\n",
        str_name, str_ssid, str_pass.c_str(), mqtt_server.toString().c_str());
    Serial.print("state: ");
    Serial.println(state);
    Serial.print("period: ");
    Serial.println(period);
    Serial.print("http: ");
    Serial.println(http);
    Serial.print("blinds: ");
    Serial.println(blinds);
    Serial.print("shutters: ");
    Serial.println(shutters);
    Serial.print("revers: ");
    Serial.println(revers);
    Serial.print("term: ");
    Serial.println(term);
    Serial.print("count: ");
    Serial.println(count);
    Serial.print("buttons: ");
    Serial.println(buttons);
    Serial.print("gap: ");
    Serial.println(gap);
    Serial.print("sensor: ");
    Serial.println(sensor);
    Serial.print("default: ");
    Serial.println(net_default);
    Serial.print("apmode: ");
    Serial.println(net_ap_mode);
    sensors.begin();            // ds18b20
    dht.begin();                // DHT22
    delay(10);

    // network
    my_connect();
    delay(10);
    web_server.begin();
    Serial.println(F("Web Server started"));
    Serial.println("start");
//  ESP.wdtEnable(WDTO_4S);
//  ESP.wdtEnable(2000);
}

void my_eeprom_states()
{
    state=EEPROM.read(100);
    period=EEPROM.read(101);
    http=EEPROM.read(102);
    blinds=EEPROM.read(103);
    term=EEPROM.read(104);
    count=EEPROM.read(105);
    buttons=EEPROM.read(106);
    gap=EEPROM.read(107);
    sensor=EEPROM.read(108);
    net_default=EEPROM.read(109);
    shutters=EEPROM.read(110);
    revers=EEPROM.read(111);
    net_ap_mode=EEPROM.read(112);
}

void my_pin_mode()
{
    // i/o
    pinMode(0,INPUT_PULLUP);    //  D3 bt   flash
    pinMode(2,INPUT_PULLUP);    //  D4 bt
    //pinMode(4);               //  D2 tmp,hum DHT 
    //pinMode(5);               //  D1 temp ds18b20
    if(buttons==0)
    {
        pinMode(12,OUTPUT);         //  D6 pwm, somfy
        pinMode(13,OUTPUT);         //  D7 pwm, somfy
        if(blinds==0)
            analogWriteRange(255);
    }
    if(buttons==1 && blinds==0)
    {
        pinMode(12,INPUT_PULLUP);   //  D6 bt   flash
        pinMode(13,INPUT_PULLUP);   //  D7 bt   flash
    }
    pinMode(14,OUTPUT);         //  D5 rele
    pinMode(16,OUTPUT);         //  D0 rele
    //pinMode(A0,INPUT);        //  A0 sn 0-5v
    //pinMode(A0);              //  A0(17) an 0-1023
    // counters
    int mem=60;
    for(int i=0; i<=3; i++)
    {
        value[i]=my_long_read(mem);
        mem+=5;
    }

    if(state==0)
    {
        if(buttons==0 && blinds==0)
        {
            ac_pwm[12]=pwm[12]=0;
            analogWrite(12,ac_pwm[12]);
            ac_pwm[13]=pwm[13]=0;
            analogWrite(13,ac_pwm[13]);
        }
        if(term==0 && shutters==0)
        {
            rele[14]=false;
            digitalWrite(14,LOW);
            rele[16]=false;
            digitalWrite(16,LOW);
        }
    }
    if(state==1)
    {
        if(buttons==0 && blinds==0)
        {
            ac_pwm[12]=pwm[12]=EEPROM.read(12);
            analogWrite(12,ac_pwm[12]);
            ac_pwm[13]=pwm[13]=EEPROM.read(13);
            analogWrite(13,ac_pwm[13]);
        }
        if(term==0 && shutters==0)
        {
            rele[14]=EEPROM.read(14);
            my_rele(14);
            rele[16]=EEPROM.read(16);
            my_rele(16);
        }
    }
    if(blinds==1 && buttons==0)
    {
        digitalWrite(12,HIGH);
        digitalWrite(13,HIGH);
        somfy=EEPROM.read(11);
        ac_somfy=somfy;
    }
    if(shutters==1 && term==0)
    {
        rele[14]=LOW;
        my_rele(14);
        rele[16]=LOW;
        my_rele(16);
        shut=EEPROM.read(15);
        ac_shut=shut;
    }

}

void loop()
{
    if(count==1)
    {
        if(!mclient.connected())
            if(millis()%299000==0)
                my_connect();
        if(mclient.connected())
            mclient.loop();
    }
    if(count==0)
    {
        if(!mclient.connected())
            if(millis()%99000==0)
                my_connect();
        if(mclient.connected())
            mclient.loop();
    }
    if(http==1)
        my_web();
    my_button(0);
    my_button(2);
    if(sensor==0)
        if(millis()%7==0)
            my_button(17);
    if(buttons==1 && blinds==0)
    {
        my_button(12);
        my_button(13);
    }
    if(period==1)
    {
        if(millis()%(6600*(gap+1))==0)
            my_dht();
        if(millis()%(7700*(gap+1))==0)
            my_temp();
    }
    if(sensor==1)
        if(millis()%(1700*(gap+1))==0)
            my_analog();
    if(blinds==0 && buttons==0)
        if(millis()%9==0)
            my_pwm();
    if(blinds==1 && buttons==0)
        my_somfy();
    if(term==0 && shutters==1)
        my_shutters();
    if(term==1 && shutters==0)
        if(millis()%80000==0)
            my_term();
    if(millis()%90000==0)
        my_memory();
    my_rele(14);
    my_rele(16);
    my_reset();
    delay(0);
//  ESP.wdtFeed();
}

void my_ap_mode()
{
    Serial.print("Default AP Mode ");
    WiFi.mode(WIFI_AP);
    WiFi.softAPConfig(ap_ip, ap_gateway, ap_subnet);
    WiFi.softAP(ap_ssid, ap_pass);
    Serial.println("Access Point Hotspot: STARTED");
    Serial.print("  SSID: "); Serial.println(ap_ssid);
    Serial.print("  PASS: "); Serial.println(ap_pass);
    Serial.print("  AP IP: "); Serial.println(WiFi.softAPIP());
    Serial.print("  AP MAC: "); Serial.println(WiFi.softAPmacAddress());
}

void my_connect()
{

    // create AP
    if (net_ap_mode==true)
    {
        if (WiFi.getMode() != WIFI_AP)
            Serial.println("Access Point Hotspot: FAILED TO START");
    }

    if (WiFi.getMode() == WIFI_AP)
    {
        // Check Access Point Status
        int clientCount = WiFi.softAPgetStationNum();
        Serial.print("  AP Status: ACTIVE | Connected Clients: ");
        Serial.println(clientCount);
        str_ip = WiFi.softAPIP().toString();
        str_mac = WiFi.softAPmacAddress();
        Serial.printf("Set AP Network: Name: %s, SSID: %s, PASS: %s, IP: %s, MAC: %s, MQTT: %s\n",
            str_name, str_ssid, str_pass, str_ip, str_mac, mqtt_server.toString().c_str());
    }

    // connect to STA
    if (WiFi.status() != WL_CONNECTED && net_ap_mode==false)
    {
        Serial.println(F("Wifi connecting... "));
        int ssid_len=str_ssid.length()+1;
        char ssid[ssid_len];
        str_ssid.toCharArray(ssid,ssid_len);
        int pass_len=str_pass.length()+1;
        char pass[pass_len];
        str_pass.toCharArray(pass,pass_len);
        WiFi.begin(ssid, pass);
        for(int i=0;i<=9;i++)
        {
            if(WiFi.status() != WL_CONNECTED)
            {
                Serial.print("  STA Status: DISCONNECTED (Code: ");
                Serial.print(WiFi.status());
                Serial.println(" - attempting reconnection...)");
                delay(999);
            }
            if(WiFi.status() == WL_CONNECTED)
            {
                Serial.print("My IP: ");
                Serial.println(WiFi.localIP());
                str_ip = WiFi.localIP().toString();
                str_mac = WiFi.macAddress();
                break;
            }
        }
    }
    if(WiFi.status() == WL_CONNECTED || WiFi.softAPIP())
    {
        // Display the operating Wi-Fi channel
        Serial.print("  Operating Wi-Fi Channel: ");
        Serial.println(WiFi.channel());

        if(!mclient.connected())
        {
            Serial.println(F("Connecting to MQTT server..."));
            str_topic = String("/" + str_name + "/out/start");
            if(mclient.connect(str_name,str_topic,1,0,"disconnect"))
            {
                Serial.println(F("Connected to MQTT server"));
                mclient.set_callback(callback);
                mclient.subscribe("/" + str_name + "/#");
                Serial.println("Subscribe to : /" + str_name + "/#");
                str_topic = String("/" + str_name + "/out/start");
                str_payload = "start";
                my_print();
            }
            else
            {
                Serial.println(F("Could not connect to MQTT server"));
            }
        }
        if(mclient.connected())
            mclient.loop();
    }
}

void my_web()
{
    WiFiClient client = web_server.available();
    if(!client)
        return;
    Serial.println(F("new http client"));
//  while(!client.available())
//      delay(1);
    String req = client.readStringUntil('\n'); //r
//  Serial.println(req);
    client.flush();
//  Serial.println(req.length());

if(req.length()<100)
{
client.println(F("HTTP/1.1 200 OK\r\nContent-Type: text/html\r\n\r\n"));
client.println(F("<!DOCTYPE html><html><head><title>"));
client.print(str_name);
client.println(F("</title><style>body {padding: 3%}"));
client.println(F("table {border-collapse: collapse;}"));
client.println(F("td, th {text-align: right; padding: 5px;}"));
client.println(F("input[type=text] {text-align: right}</style>"));
client.println(F("<meta charset='UTF-8'>"));
client.println(F("</head><body bgcolor='#c2d4dd'><center><h2>ESP8266 MQTT client "));
client.println(F("<input type='button' onclick='reload()' value=' Reload &#x21bb; '>"));
client.println(F("</h2></center>"));
client.println(F("<script>function reload() {location.href='/';}</script>"));
client.println(F("<hr><table><tr><th>Name:</th><th>"));
client.print(str_name);
client.println(F("</th></tr><th>IP address:</th><th>"));
client.print(str_ip);
client.println(F("</th></tr><th>MAC address:</th><th>"));
client.print(str_mac);
client.println(F("</th></tr><th>MQTT Server:</th><th>"));
client.print(String(mqtt_server[0],DEC)+"."+String(mqtt_server[1],DEC)+".");
client.print(String(mqtt_server[2],DEC)+"."+String(mqtt_server[3],DEC));
client.println(F("</th></tr></table><hr>"));
client.println(F("<br>default STA mode: name=wemos, ssid=hjhome, pass=pass1234, mqtt=192.168.0.10"));
client.println(F("<br>default AP mode: name=wemos, ssid=ESP8266_hjhome, pass=pass1234, mqtt=192.168.9.10"));
client.print(F("<br>Commands: (/"));
client.print(str_name);
client.println(F("/out/start)<br>"));
client.println(F("start, disconnect"));
client.print(F("<br>Commands: (/"));
client.print(str_name);
client.println(F("/in/command)<br>"));
client.println(F("echo, test, ip, mac, srv, info, reboot, memory, millis"));
client.println(F("<br>name, ssid, pass, mqtt, default, apmode"));
client.println(F("<details><summary>Commands descriptions:</summary>"));
client.println(F("<br>current: ip,mac - IP,MAC address, mqtt - IP for MQTT server"));
client.println(F("<br>reboot - reboot esp8266, memory - free RAM, millis - milliseconds"));
client.println(F("<br>name - new name, ssid - new ssid, pass - new pass, mqtt - IP for server"));
client.println(F("<br>state - outputs rele14,16,pwm12,13 save after power off 0/1"));
client.println(F("<br>http - set web server | 0/1"));
client.println(F("<br>blinds - set out12, out13 for blinds(somfy) | 0/1"));
client.println(F("<br>buttons - set out12, out13 for buttons | 0/1"));
client.println(F("<br>shutters - set out14, out16 for shutters | 0/1"));
client.println(F("<br>revers - set reversed output for out14, out16 | 0/1"));
client.println(F("<br>term - set anti freezing function for temp4<8&#x2103 and rele14"));
client.println(F("<br>    overheating function for temp4>33&#x2103 and rele16 | 0/1"));
client.println(F("<br>count - set counter for in0, in2 | 0/1"));
client.println(F("<br>value0,1,2,3 - set unsigned long | 0 - 2147483647"));
client.println(F("<br>gap - set interval 6(2)sec-30(7)min for sensors temp,tmp,hum,(an) | 0 - 255"));
client.println(F("<br>period - set send temp5,temp4,hum4,an0 stat periodicaly | 0/1"));
client.println(F("<br>sensor - set analog input an0(A0)  | 0/1"));
client.println(F("<br>default - set default network settings | 0/1"));
client.println(F("<br>    press bt0 and bt2 for 20 sec to reset(default)"));
client.println(F("<br>    press bt0 and bt2 for 20 sec to reset(default AP mode)"));
client.println(F("<br>apmode - set default Access Point Mode 0/1"));
client.println(F("<br>_temp5 - simulate ds18b20 sensor | -100 - 100"));
client.println(F("<br>_tmp4, _hum4 - simulate dht22 sensor | -100 - 100"));
client.println(F("</details>"));
//
client.print(F("<br>topic: "));
client.print(str_topic);
client.print(F("<br>payload: "));
client.println(str_payload);
delay(0);
client.println(F("<hr><table><tr><th>Pin</th><th>I/O</th><th>Topic</th>"));
client.print(F("<th>Payload</th><th>Status</th></tr>"));
for(int i=0; i<=16; i++)
{
    if(i==0 || i==1) pin=3; if(i==2 || i==3) pin=4; if(i==4) pin=2;
    if(i==5) pin=1; if(i==12) pin=6; if(i==13) pin=7; if(i==14) pin=5;
    if(i==16) pin=0; if(i==0 || i==2)
    {
        client.print(F("<tr><th>gpio"));
        client.print(i);
        client.print(F(", D"));
        client.print(pin);
        client.print(F("</th><th>Button</th><th>/"));
        client.print(str_name);
        client.print(F("/in/bt"));
        client.print(i);
        client.print(F("</th><th>0,1,2</th><th>"));
        client.print(bt[i]);
        client.println(F("</th></tr>"));
    }
    if(i>=0 && i<=3)
    {
        client.print(F("<tr><th>gpio"));
        if(i<2)
            client.print("0");
        else
            client.print("2");
        client.print(F(", D"));
        client.print(pin);
        client.print(F("</th><th>Counter</th><th>/"));
        client.print(str_name);
        client.print(F("/in/value"));
        client.print(i);
        client.print(F("</th><th>0-(2^32-1)</th><th>"));
        client.print(value[i]);
        client.println(F("</th></tr>"));
    }
    if(i==4)
    {
        client.print(F("<tr><th>gpio"));
        client.print(i);
        client.print(F(", D"));
        client.print(pin);
        client.print(F("</th><th>DHT</th><th>/"));
        client.print(str_name);
        client.print(F("/in/temp"));
        client.print(i);
        client.print(F(",hum"));
        client.print(i);
        client.print(F("</th><th>&#x2103,&#37</th><th>"));
        client.print(tmp);
        client.print(F("&#x2103, "));
        client.print(hum);
        client.println(F("&#37</th></tr>"));
    }
    if(i==5)
    {
        client.print(F("<tr><th>gpio"));
        client.print(i);
        client.print(F(", D"));
        client.print(pin);
        client.print(F("</th><th>DS18B20</th><th>/"));
        client.print(str_name);
        client.print(F("/in/temp"));
        client.print(i);
        client.print(F("</th><th>&#x2103</th><th>"));
        client.print(temp);
        client.println(F("&#x2103</th></tr>"));
    }
    if(i==11)
    {
        client.print(F("<tr><th>gpio(12,13), D6,D7"));
        client.print(F("</th><th>Somfy</th><th>/"));
        client.print(str_name);
        client.print(F("/out/somfy"));
        client.print(i);
        client.print(F("</th><th>0-255</th><th>"));
        client.print(somfy);
        client.println(F("</th></tr>"));
    }
    if(i==12 || i==13)
    {
        client.print(F("<tr><th>gpio"));
        client.print(i);
        client.print(F(", D"));
        client.print(pin);
        client.print(F("</th><th>Pwm</th><th>/"));
        client.print(str_name);
        client.print(F("/out/pwm"));
        client.print(i);
        client.print(F("</th><th>0-255</th><th>"));
        client.print(pwm[i]);
        client.println(F("</th></tr>"));
        client.print(F("<tr><th>gpio"));
        client.print(i);
        client.print(F(", D"));
        client.print(pin);
        client.print(F("</th><th>Button</th><th>/"));
        client.print(str_name);
        client.print(F("/in/bt"));
        client.print(i);
        client.print(F("</th><th>0,1,2</th><th>"));
        client.print(bt[i]);
        client.println(F("</th></tr>"));
    }
    if(i==15)
    {
        client.print(F("<tr><th>gpio(14,16), D5,D0"));
        client.print(F("</th><th>Shutters</th><th>/"));
        client.print(str_name);
        client.print(F("/out/shut"));
        client.print(i);
        client.print(F("</th><th>0-255</th><th>"));
        client.print(shut);
        client.println(F("</th></tr>"));
    }
    if(i==14 || i==16)
    {
        client.print(F("<tr><th>gpio"));
        client.print(i);
        client.print(F(", D"));
        client.print(pin);
        client.print(F("</th><th>Rele</th><th>/"));
        client.print(str_name);
        client.print(F("/out/rele"));
        client.print(i);
        client.print(F("</th><th>0,1</th><th>"));
        client.print(rele[i]);
        client.println(F("</th></tr>"));
    }
}
delay(0);
client.print(F("<tr><th>A0</th><th>Sensor</th><th>/"));
client.print(str_name);
client.print(F("/in/an0</th><th>1-1024</th><th>"));
client.print(an);
client.print(F("<tr><th>A0</th><th>Button</th><th>/"));
client.print(str_name);
client.print(F("/in/bt17</th><th>0,1,2</th><th>"));
client.print(bt[17]);
client.println(F("</th></tr><tr><th>EEPROM</th><th>state</th><th>/"));
client.print(str_name);
client.print(F("/in/state</th><th>0,1</th><th>"));
client.print(state);
client.println(F("</th></tr><tr><th>EEPROM</th><th>period</th><th>/"));
client.print(str_name);
client.print(F("/in/period</th><th>0,1</th><th>"));
client.print(period);
client.println(F("</th></tr><tr><th>EEPROM</th><th>http</th><th>/"));
client.print(str_name);
client.print(F("/in/http</th><th>0,1</th><th>"));
client.print(http);
client.println(F("</th></tr><tr><th>EEPROM</th><th>blinds</th><th>/"));
client.print(str_name);
client.print(F("/in/blinds</th><th>0,1</th><th>"));
client.print(blinds);
client.println(F("</th></tr><tr><th>EEPROM</th><th>shutters</th><th>/"));
client.print(str_name);
client.print(F("/in/shutters</th><th>0,1</th><th>"));
client.print(shutters);
client.println(F("</th></tr><tr><th>EEPROM</th><th>revers</th><th>/"));
client.print(str_name);
client.print(F("/in/revers</th><th>0,1</th><th>"));
client.print(revers);
client.println(F("</th></tr><tr><th>EEPROM</th><th>buttons</th><th>/"));
client.print(str_name);
client.print(F("/in/buttons</th><th>0,1</th><th>"));
client.print(buttons);
client.println(F("</th></tr><tr><th>EEPROM</th><th>term</th><th>/"));
client.print(str_name);
client.print(F("/in/term</th><th>0,1</th><th>"));
client.print(term);
client.println(F("</th></tr><tr><th>EEPROM</th><th>count</th><th>/"));
client.print(str_name);
client.print(F("/in/count</th><th>0,1</th><th>"));
client.print(count);
client.println(F("</th></tr><tr><th>EEPROM</th><th>gap</th><th>/"));
client.print(str_name);
client.print(F("/in/gap</th><th>0-255</th><th>"));
client.print(gap);
client.println(F("</th></tr><tr><th>EEPROM</th><th>sensor</th><th>/"));
client.print(str_name);
client.print(F("/in/sensor</th><th>0,1</th><th>"));
client.print(sensor);
client.println(F("</th></tr><tr><th>EEPROM</th><th>default</th><th>/"));
client.print(str_name);
client.print(F("/in/default</th><th>0,1</th><th>"));
client.print(net_default);
client.println(F("</th></tr><tr><th>EEPROM</th><th>apmode</th><th>/"));
client.print(str_name);
client.print(F("/in/apmode</th><th>0,1</th><th>"));
client.print(net_ap_mode);
client.println(F("</th></tr></table><hr>"));

client.print(F("<center><a href='https://github.com/hjltu/hjmqtt'>"));
client.println(F("hjmqtt</a> 22-aug-16 -> "));
client.print(vers);
client.println(F("</center></body></html>"));
req="";
client.flush();
Serial.println(F("http end"));
}
//client.flush();
}

void my_print()
{
    if(str_topic.length()>0 && str_payload.length()>0)
        if(str_topic.length()<100 && str_payload.length()<999)
            if(mclient.connected())
                mclient.publish(str_topic,str_payload);
        Serial.print(F("topic: "));
        Serial.print(str_topic);
        Serial.print(F(" payload: "));
        Serial.println(str_payload);
        delay(0);
}

void callback(const MQTT::Publish& pub)
{
    String inc_topic = pub.topic();
    String inc_payload = pub.payload_string();
    
    if(inc_topic.indexOf("/in")>0)
    {
    Serial.print(F("incoming topic: "));
    Serial.print(inc_topic);
    Serial.print(F(" payload: "));
    Serial.println(inc_payload);

    if(inc_topic.indexOf("/echo")>0)
    {
        str_topic = String("/" + str_name + "/out/echo");
        str_payload = inc_payload;
        my_print();
    }
    if(inc_topic.indexOf("/ip")>0)
    {
        str_topic = String("/" + str_name + "/out/ip");
        str_payload = str_ip;
        //if (net_ap_mode==true)
        //    str_payload = WiFi.softAPIP().toString();
        //if (net_ap_mode==false)
        //    str_payload = WiFi.localIP().toString();
        my_print();
    }
    if(inc_topic.indexOf("/mac")>0)
    {
        str_topic = String("/" + str_name + "/out/mac");
        str_payload = str_mac;
        my_print();
    }
    if(inc_topic.indexOf("/srv")>0)
    {
        str_topic = String("/" + str_name + "/out/srv");
        str_payload = mqtt_server.toString();
            //str_payload = String(my_mqtt[0],DEC)+
            //"."+String(my_mqtt[1],DEC)+
            //"."+String(my_mqtt[2],DEC)+
            //"."+String(my_mqtt[3],DEC);
        my_print();
    }

    // TODO
    if(inc_topic.indexOf("in/info")>0)
    {
        str_topic = String("/" + str_name + "/out/info");
        str_payload = "author:hjltu@ya.ru,license:GPLv3,project:hjhome,version:"+String(vers);
        str_payload+="\nSaved: name=" + my_str_read(str_name,80) + " ssid=" + my_str_read(str_ssid,200) +
        " pass=" + my_str_read(str_pass,220) + " mqtt=" + String(EEPROM.read(50))+
        "."+String(EEPROM.read(51))+
        "."+String(EEPROM.read(52))+
        "."+String(EEPROM.read(53));
        str_payload +="\ndefault: name=wemos, ssid=hjhome, pass=pass1234, mqtt=192.168.0.10";
        str_payload += "\napmode: name=wemos, ssid="+ap_ssid+", pass="+ap_pass+", mqtt="+mqtt_ap_server.toString().c_str();
        str_payload += "\nCurrent: name="+str_name+", ssid="+str_ssid+", pass="+str_pass+", mqtt="+mqtt_server.toString().c_str();
        my_print();
    }

    if(inc_topic.indexOf("/reboot")>0)
    {
        str_topic = String("/" + str_name + "/out/reboot");
        str_payload = "manual restart";
        my_print();
        ESP.restart();
    }

    if(inc_topic.indexOf("/memory")>0)
    {
        str_topic = String("/" + str_name + "/out/memory");
        str_payload = String(ESP.getFreeHeap());
        my_print();
    }

    if(inc_topic.indexOf("/millis")>0)
    {
        str_topic = String("/" + str_name + "/out/millis");
        str_payload = String(millis());
        my_print();
    }

    if(inc_topic.indexOf("/name")>0)
    {
        if(inc_payload.length()>0 && inc_payload.length()<20)
        {
            my_str_write(inc_payload,80);
        }
        str_topic = String("/" + str_name + "/out/name");
        str_payload = my_str_read("ERR: name",80);
        my_print();
        //mclient.disconnect();
    }

    if(inc_topic.indexOf("/ssid")>0)
    {
        if(inc_payload.length()>0 && inc_payload.length()<20)
        {
            my_str_write(inc_payload,200);
        }
        str_topic = String("/" + str_name + "/out/ssid");
        str_payload = my_str_read("ERR: ssid",200);
        my_print();
    }

    if(inc_topic.indexOf("/pass")>0)
    {
        if(inc_payload.length()>0 && inc_payload.length()<20)
        {
            my_str_write(inc_payload,220);
        }
        str_topic = String("/" + str_name + "/out/pass");
        str_payload = my_str_read("ERR: pass",220);
        my_print();
    }

    if(inc_topic.indexOf("/apmode")>0)
    {
        if(inc_payload=="1" || inc_payload=="0")
        {
            net_ap_mode=inc_payload.toInt();
            ee_wr(112,net_ap_mode);
        }
        str_topic = String("/" + str_name + "/out/apmode");
        str_payload = String(net_ap_mode);
        my_print();
    }

    if(inc_topic.indexOf("/default")>0)
    {
        if(inc_payload=="1" || inc_payload=="0")
        {
            net_default=inc_payload.toInt();
            ee_wr(109,net_default);
        }
        str_topic = String("/" + str_name + "/out/default");
        str_payload = String(net_default);
        my_print();
    }

    if(inc_topic.indexOf("/mqtt")>0)
    {
        int index=0,last_i=0;
        for(int i=0; i<=inc_payload.length() && index<=3; i++)
        {
            if(inc_payload.substring(i,i+1)=="." || i==inc_payload.length())
            {
                ee_wr(50+index,inc_payload.substring(last_i,i).toInt());
                index++;
                last_i=i+1;
            }
        }
        str_topic = String("/" + str_name + "/out/mqtt");
        str_payload = String(EEPROM.read(50))+"."+String(EEPROM.read(51))+"."+String(EEPROM.read(52))+"."+String(EEPROM.read(53));
        my_print();
    }

    if(inc_topic.indexOf("/state")>0)
    {
        if(inc_payload=="1" || inc_payload=="0")
        {
            state=inc_payload.toInt();
            ee_wr(100,state);
        }
        str_topic = String("/" + str_name + "/out/state");
        str_payload = String(state);
        my_print();
    }

    if(inc_topic.indexOf("/period")>0)
    {
        if(inc_payload=="1" || inc_payload=="0")
        {
            period=inc_payload.toInt();
            ee_wr(101,period);
        }
        str_topic = String("/" + str_name + "/out/period");
        str_payload = String(period);
        my_print();
    }
    if(inc_topic.indexOf("/http")>0)
    {
        if(inc_payload=="1" || inc_payload=="0")
        {
            http=inc_payload.toInt();
            ee_wr(102,http);
        }
        str_topic = String("/" + str_name + "/out/http");
        str_payload = String(http);
        my_print();
    }
    if(inc_topic.indexOf("/blinds")>0)
    {
        if((inc_payload=="1" || inc_payload=="0"))
        {
            ee_wr(103,inc_payload.toInt());
            ee_wr(106,0); //buttons
        }
        str_topic = String("/" + str_name + "/out/blinds");
        str_payload = String(EEPROM.read(103));
        my_print();
    }
    if(inc_topic.indexOf("/shutters")>0)
    {
        if((inc_payload=="1" || inc_payload=="0"))
        {
            ee_wr(110,inc_payload.toInt());
            ee_wr(104,0); //term
        }
        str_topic = String("/" + str_name + "/out/shutters");
        str_payload = String(EEPROM.read(110));
        my_print();
    }
    if(inc_topic.indexOf("/revers")>0)
    {
        if((inc_payload=="1" || inc_payload=="0"))
        {
            ee_wr(111,inc_payload.toInt());
        }
        str_topic = String("/" + str_name + "/out/revers");
        str_payload = String(EEPROM.read(111));
        my_print();
    }
    if(inc_topic.indexOf("/buttons")>0)
    {
        if((inc_payload=="1" || inc_payload=="0"))
        {
            ee_wr(106,inc_payload.toInt());
            ee_wr(103,0); // blinds
        }
        str_topic = String("/" + str_name + "/out/buttons");
        str_payload = String(EEPROM.read(106));
        my_print();
    }
    if(inc_topic.indexOf("/term")>0)
    {
        if(inc_payload=="1" || inc_payload=="0")
        {
            ee_wr(104,inc_payload.toInt());
            ee_wr(110,0);   // shutters
        }
        str_topic = String("/" + str_name + "/out/term");
        str_payload = String(EEPROM.read(104));
        my_print();
    }
    if(inc_topic.indexOf("/count")>0)
    {
        if(inc_payload=="1" || inc_payload=="0")
        {
            count=inc_payload.toInt();
            ee_wr(105,count);
        }
        str_topic = String("/" + str_name + "/out/count");
        str_payload = String(count);
        my_print();
    }
    if(inc_topic.indexOf("/gap")>0)
    {
        if(inc_payload.toInt() >= 0 && inc_payload.toInt() <= 255)
        {
            gap=inc_payload.toInt();
            ee_wr(107,gap);
        }
        str_topic = String("/" + str_name + "/out/gap");
        str_payload = String(gap);
        my_print();
    }
    if(inc_topic.indexOf("/sensor")>0)
    {
        if((inc_payload=="1" || inc_payload=="0"))
        {
            sensor=inc_payload.toInt();
            ee_wr(108,sensor);
        }
        str_topic = String("/" + str_name + "/out/sensor");
        str_payload = String(sensor);
        my_print();
    }

    for(int i=0; i<=17; i++)
    {

        if(inc_topic.indexOf("/value" + String(i))>0 && (i>=0 && i<=3))
        {
                value[i]=inc_payload.toInt();
                int mem=60;
                if(i==1) mem=65; if(i==2) mem=70; if(i==3) mem=75;
                my_long_write(mem,value[i]);
                str_topic = String("/" + str_name + "/out/value" + i);
                str_payload = String(value[i]);
                my_print();
        }
        if(inc_topic.indexOf("/_tmp" + String(i))>0 && i==4)
        {
            if((inc_payload.toInt() > -99 && inc_payload.toInt() < 99))
                tmp=inc_payload.toInt();
            str_topic = String("/" + str_name + "/out/tmp" + i);
            str_payload = String(tmp);
            my_print();
        }
        if(inc_topic.indexOf("/_hum" + String(i))>0 && i==4)
        {
            if((inc_payload.toInt() >= 0 && inc_payload.toInt() <= 100))
                hum=inc_payload.toInt();
            str_topic = String("/" + str_name + "/out/hum" + i);
            str_payload = String(hum);
            my_print();
        }
        if(inc_topic.indexOf("/_temp" + String(i))>0 && i==5)
        {
            if((inc_payload.toInt() > -99 && inc_payload.toInt() < 99))
                temp=inc_payload.toInt();
            str_topic = String("/" + str_name + "/out/temp" + i);
            str_payload = String(temp);
            my_print();
        }
        if(inc_topic.indexOf("/test")>0)
        {
            if(i==11)
            {
                str_topic = String("/" + str_name + "/out/somfy" + i);
                str_payload = String(somfy);
                my_print();
            }
            if((i==12 || i==13))
            {
                str_payload = String(pwm[i]);
                str_topic = String("/" + str_name + "/out/pwm" + i);
                my_print();
            }
            if(i==15)
            {
                str_topic = String("/" + str_name + "/out/shut" + i);
                str_payload = String(shut);
                my_print();
            }
            if(i==14 || i==16)
            {
                str_topic = String("/" + str_name + "/out/rele" + i);
                str_payload = String(rele[i]);
                my_print();
            }
        }

        if(inc_topic.indexOf("/somfy" + String(i))>0 && i==11)
        {
            somfy=inc_payload.toInt();
            ee_wr(i,somfy);
            str_topic = String("/" + str_name + "/out/somfy" + i);
            str_payload = String(somfy);
            my_print();
        }

        if(inc_topic.indexOf("/shut" + String(i))>0 && i==15)
        {
            shut=inc_payload.toInt();
            ee_wr(i,shut);
            str_topic = String("/" + str_name + "/out/shut" + i);
            str_payload = String(shut);
            my_print();
        }

        if(inc_topic.indexOf("/pwm" + String(i))>0 && (i==12 || i==13))
        {
            pwm[i]=inc_payload.toInt();
            ee_wr(i,pwm[i]);
            str_topic = String("/" + str_name + "/out/pwm" + i);
            str_payload = String(pwm[i]);
            my_print();
        }
        if(inc_topic.indexOf("/rele" + String(i))>0 && (i==14 || i==16))
        {
            if(term==0 && shutters==0)
            {
                if(inc_payload=="0" || inc_payload=="false" || inc_payload=="OFF")
                    rele[i]=false;
                if(inc_payload=="1" || inc_payload=="true" || inc_payload=="ON")
                    rele[i]=true;
                //ee_wr(i,rele[i]);
            }
            str_topic = String("/" + str_name + "/out/rele" + i);
            str_payload = String(rele[i]);
            my_print();
        }
    }
    }
}
void my_rele(int i)
{
    if(revers == false)
        if(digitalRead(i) != rele[i])
            digitalWrite(i,rele[i]);
    if(revers == true)
        if(digitalRead(i) == rele[i])
            digitalWrite(i, !rele[i]);
    if(state == true)
        ee_wr(i,rele[i]);
}

void my_term()
// Automatic anti freeze or over heat protection depends of ds18b20
{
// heating
    int i=14, min=9, max=33, gist=2;
    if(temp<min)
        rele[i]=true;
    if(temp>min+gist)
        rele[i]=0;
    str_topic = String("/" + str_name + "/out/rele" + i);
    str_payload = String(rele[i]);
    my_print();
// cooling
    i=16;
    if(temp>max)
        rele[i]=true;
    if(temp<max-gist)
        rele[i]=false;
    str_topic = String("/" + str_name + "/out/rele" + i);
    str_payload = String(rele[i]);
    my_print();
}

void my_button(int i)
{
    if(i<17)
    {
        bool bt_stat;
        bt_stat=digitalRead(i);
        if(bt_stat==LOW && bcount[i]<15)
            bcount[i]++;
        if(bt_stat==HIGH && bcount[i]>0)
            bcount[i]--;
    }
    if(i==17)
    {
        int an_stat;
        an_stat=analogRead(A0);
        if(an_stat<10 && bcount[i]<333)
            bcount[i]++;
        if(an_stat>666 && bcount[i]>0)
            bcount[i]--;
        delayMicroseconds(300);
    }
    if(bcount[i]>10 && bt[i]==false)
    {
        bt[i]=true;
        lcount[i]=millis();
    }
    if(bcount[i]==0 && bt[i]==true)
    {
        bt[i]=false;
        rcount[i]=0;
        if(lcount[i]==0)
        {
            str_topic = String("/" + str_name + "/out/bt" + i);
            str_payload = "0";
            my_print();
        }
        if(lcount[i]>0 && lcount[i]>millis()-500)
        {
            lcount[i]=0;
            str_topic = String("/" + str_name + "/out/bt" + i);
            str_payload = "1";
            my_print();
            if(count==1 && (i==0 || i==2))
            {
                value[i]++;
                int mem=60;
                if(i==2)
                    mem=70;
                my_long_write(mem,value[i]);
                str_topic = String("/" + str_name + "/out/value" + i);
                str_payload = value[i];
                my_print();
            }
        }
    }
    if(lcount[i]>0 && lcount[i]<millis()-999)
    {
        rcount[i]=lcount[i];
        lcount[i]=0;
        str_topic = String("/" + str_name + "/out/bt" + i);
        str_payload = "2";
        my_print();
        if(count==1 && (i==0 || i==2))
        {
            value[i+1]++;
            int mem=65;
            if(i==2)
                mem=75;
            my_long_write(mem,value[i+1]);
            str_topic = String("/" + str_name + "/out/value" + (i+1));
            str_payload = value[i+1];
            my_print();
        }
    }
}

void my_temp()
// ds18b20
{
    sensors.requestTemperatures();
    temp=sensors.getTempCByIndex(0);
    if(temp<100 && temp>-100)
    {
        str_topic = String("/" + str_name + "/out/temp5");
        str_payload = String(temp);
        my_print();
    }
}

void my_dht()
{
    hum=dht.readHumidity();
    tmp=dht.readTemperature();
    if(tmp<100 && tmp>-100)
    {
        str_topic = String("/" + str_name + "/out/temp4");
        str_payload = String(tmp);
        my_print();
    }
    if(hum<100 && hum>=0)
    {
        str_topic = String("/" + str_name + "/out/hum4");
        str_payload = String(hum);
        my_print();
    }
}

void my_analog()
{
    an=0;
    for(int j=0; j<10;j++)
    {
        an+=analogRead(A0);
        delay(1);
    }
    an=an/10;
    str_topic = String("/" + str_name + "/out/an0");
    str_payload = String(an);
    my_print();
}

void my_pwm()
{
    for(int i=12; i<14; i++)    // TODO i+=2
    {
        if(ac_pwm[i] != pwm[i])
        {
            if(ac_pwm[i]>pwm[i])
                ac_pwm[i]--;
            if(ac_pwm[i]<pwm[i])
                ac_pwm[i]++;
            analogWrite(i,ac_pwm[i]);
        }
    }
}

void my_shutters()
{
    if(shcount<millis()-999)
    {
        if(ac_shut==shut && (rele[14]==1 || rele[16]==1))
        {
            rele[14]=0;
            rele[16]=0;
            shcount=millis()-500;
        }
        if(ac_shut != shut)
        {
            if(ac_shut>shut && rele[16]==1)
            {
                rele[16]=0;
                shcount=millis()-500;
                return;
            }
            if(ac_shut<shut && rele[14]==1)
            {
                rele[14]=0;
                shcount=millis()-500;
                return;
            }
            shcount=millis();
            if(ac_shut>shut && rele[16]==0)
            {
                rele[14]=1;
                ac_shut--;
            }
            if(ac_shut<shut && rele[14]==0)
            {
                rele[16]=1;
                ac_shut++;
            }
        }
    }
}

void my_somfy()
{
    if(stop_count>0)
    {
        if(stop_count<millis()-200)
        {
            digitalWrite(12,HIGH);
            digitalWrite(13,HIGH);
        }
        if(stop_count<millis()-400)
            stop_count=0;
    }
    if(stop_count==0)
    {
        if(ac_somfy==somfy && (rele[12]==1 || rele[13]==1))
            somfy_stop();

        if(ac_somfy != somfy)
        {
            if(ac_somfy>somfy && rele[12]==0)
                somfy_close();
            if(ac_somfy<somfy && rele[13]==0)
                somfy_open();
            if(scount<millis()-1000)
            {
                scount=millis();
                if(ac_somfy>somfy)
                    ac_somfy--;
                if(ac_somfy<somfy)
                    ac_somfy++;
            }
        }
    }
}

void somfy_stop()
{
    rele[12]=0;
    rele[13]=0;
    digitalWrite(12,LOW);
    digitalWrite(13,LOW);
    stop_count=millis();
}

void somfy_close()
{
    rele[12]=1;
    digitalWrite(12,LOW);
    stop_count=millis();
    scount=millis();
}

void somfy_open()
{
    rele[13]=1;
    digitalWrite(13,LOW);
    stop_count=millis();
    scount=millis();
}

void ee_wr(int addr, int val)
{
    if(EEPROM.read(addr) != val)
    {
        EEPROM.write(addr,val);
        EEPROM.commit();
    }
}

String my_str_read(String name, int shift)
{
    char ch;
    char buf[BUFSIZE];
    for(int i=0; i<=BUFSIZE; i++)
    {
        ch=EEPROM.read(i+shift);
        buf[i]=ch;
    }
    Serial.print("buf read=");
    Serial.println(String(buf));
    if(String(buf).length()>0)
        return(String(buf));
    else
        return(name);
}

// TODO test base64 comparsion
void my_str_write(String name, int shift)
{
    char buf[BUFSIZE];
    name.toCharArray(buf,BUFSIZE);
    Serial.print(F("buf write="));
    Serial.println(String(buf));
    for(int i=0; i<=BUFSIZE; i++)
    {
        if(my_base64.indexOf(String(buf[i]))>0)
        {
            Serial.println(String(buf[i]));
            EEPROM.write(i+shift,buf[i]);
        }
        else
            Serial.print(F("ERR: symbol="));
            Serial.println(String(buf[i]));
            //EEPROM.write(i+shift,buf[i]);
            return;
    }
    EEPROM.commit();
}

void my_long_write(int address, long value)
{
    //Decomposition from a long to 4 bytes by using bitshift.
    //One = Most significant -> Four = Least significant byte
    byte four = (value & 0xFF);
    byte three = ((value >> 8) & 0xFF);
    byte two = ((value >> 16) & 0xFF);
    byte one = ((value >> 24) & 0xFF);

    //Write the 4 bytes into the eeprom memory.
    EEPROM.write(address, four);
    EEPROM.write(address + 1, three);
    EEPROM.write(address + 2, two);
    EEPROM.write(address + 3, one);
    EEPROM.commit();
    delay(10);
}

long my_long_read(long address)
{
    //Read the 4 bytes from the eeprom memory.
    long four = EEPROM.read(address);
    long three = EEPROM.read(address + 1);
    long two = EEPROM.read(address + 2);
    long one = EEPROM.read(address + 3);

    //Return the recomposed long by using bitshift.
    return ((four << 0) & 0xFF) + ((three << 8) & 0xFFFF) + ((two << 16) & 0xFFFFFF) + ((one << 24) & 0xFFFFFFFF);
}

char *str_to_char(String string)
{
    int str_len=string.length()+1;
    //Serial.println(String(str_len));
    char char_array[str_len];
    string.toCharArray(char_array,str_len);
    //Serial.println(char_array);
    return(char_array);
}

void my_memory()
{
    if(ESP.getFreeHeap()<9999)
    {
        str_topic = String("/" + str_name + "/out/reset");
        str_payload = String("Error! free memory = " + ESP.getFreeHeap());
        my_print();
        ESP.reset();
    }
}

// TODO test apmode
void my_reset()
// to reset press bt0 and bt2 (D3+D4+G) for 20sec
{
    if(rcount[0]>0 && rcount[0]<millis()-19999)
        if(rcount[2]>0 && rcount[2]<millis()-19999)
            if(millis()>19999 && bt[0]==true && bt[2]==true)
            {
                rcount[0]=0;
                rcount[2]=0;
                // reset to default
                if (net_default==false)
                {
                    net_default=true;
                    ee_wr(109,net_default);
                    net_ap_mode=false;
                    ee_wr(112,net_ap_mode);
                    my_reset_print();
                }
                // reset from AP to default
                if (net_default==true && net_ap_mode==true)
                {
                    net_default=true;
                    ee_wr(109,net_default);
                    net_ap_mode=false;
                    ee_wr(112,net_ap_mode);
                    my_reset_print();
                }
                // reset from default to AP
                if (net_default==true && net_ap_mode==false)
                {
                    net_ap_mode=true;
                    ee_wr(112,net_ap_mode);
                    my_reset_print();
                }
            }
}

void my_reset_print()
{
    Serial.printf("reset: default: %d, apmode: %d\n",
    net_default, net_ap_mode);
    str_topic = String("/" + str_name + "/out/reset");
    str_payload = "Reset: default=";
    str_payload += String(net_default);
    str_payload += ", apmode=";
    my_print();
    ESP.restart();
}









