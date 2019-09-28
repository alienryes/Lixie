/* -------------------------------------------------
   NTP Clock
   using ESP8266 and Lixie Displays!

   by Connor Nishijima - 12/28/2016
   -------------------------------------------------

   To use your Lixie Displays / ESP8266 as an NTP
   clock, you'll need a few things:

   - WIFI_SSID
   - WIFI_PASSWORD
   - SIX_DIGIT
   - TIME_COLOR_RGB
   - TIME_OFFSET

  SIX_DIGIT is a true/false for 6 or 4 digit clocks
  TIME_COLOR_RGB is an 8-bit RGB value to color the displays
  TIME_OFFSET is your local UTC offset for time zones

   -------------------------------------------------
*/

#include "Lixie.h" // Include Lixie Library
#define DATA_PIN   5
#define NUM_LIXIES 4
Lixie lix(DATA_PIN, NUM_LIXIES);

#include <TimeLib.h>
#include <SPI.h>
#include <WiFi101.h>         
#include <WiFiUdp.h>

//---------------------------------------
const char* WIFI_SSID = "BTWholeHome-WJQ";  //  your network SSID (name)
const char* WIFI_PASS = "cqHeC6WCJxFJ";  //  your network password

const bool SIX_DIGIT = false; // True if 6-digit clock with seconds
const byte TIME_COLOR_RGB[3] = {0,255,255}; // CYAN
const int offset = +1;     // US Mountain Time
//---------------------------------------

// NTP Servers:
static const char ntpServerName[] = "us.pool.ntp.org";
//static const char ntpServerName[] = "time.nist.gov";
//static const char ntpServerName[] = "time-a.timefreq.bldrdoc.gov";
//static const char ntpServerName[] = "time-b.timefreq.bldrdoc.gov";
//static const char ntpServerName[] = "time-c.timefreq.bldrdoc.gov";
IPAddress ntpServerIP(129, 6, 15, 28);  // time.nist.gov NTP server

WiFiClient wifi;
int status = WL_IDLE_STATUS;          
WiFiUDP Udp;
unsigned int localPort = 8888;  // local port to listen for UDP packets

time_t getNtpTime();
void digitalClockDisplay();
void sendNTPpacket(IPAddress &address);

void setup()
{
  lix.begin(); // Initialize LEDs
  Serial.begin(9600);
   // This sets all lights to yellow while we're connecting to WIFI
  while ((status != WL_CONNECTED)) {
    lix.color(255, 255, 0);
    lix.write(8888);
    Serial.print("Attempting to connect to SSID: ");
    Serial.println(WIFI_SSID);
    status = WiFi.begin(WIFI_SSID, WIFI_PASS);
    delay(100);
  }

  // Green on connection success
  lix.color(0, 255, 0);
  lix.write(9999);
  delay(500);

  // Reset colors to default
  lix.color(255, 255, 255);
  lix.clear();

  Serial.print("IP number assigned by DHCP is ");
  Serial.println(WiFi.localIP());
  Serial.println("Starting UDP");
  Udp.begin(localPort);
  Serial.print("Local port: ");
  //Serial.println(Udp.localPort());
  Serial.println("waiting for sync");
  setSyncProvider(getNtpTime);
  setSyncInterval(300);
}

time_t prevDisplay = 0; // when the digital clock was displayed

void loop()
{
  if (timeStatus() != timeNotSet) {
    if (now() != prevDisplay) { //update the display only if time has changed
      prevDisplay = now();
      digitalClockDisplay();
    }
  }
}

void digitalClockDisplay()
{
  // digital clock display of the time
  String time_now;
  String h;
  String m;
  String s;

  if(hour() < 10){
    h = "0"+String(hour());
  }
  else{
    h = String(hour());
  }
  if(minute() < 10){
    m = "0"+String(minute());
  }
  else{
    m = String(minute());
  }
  if(second() < 10){
    s = "0"+String(second());
  }
  else{
    s = String(second());
  }

  time_now += h;
  time_now += ":";
  time_now += m;
  if(SIX_DIGIT == true){
    time_now += ":";
    time_now += s;
  }

  char buf[10];
  time_now.toCharArray(buf,10);

  lix.color(TIME_COLOR_RGB[0],TIME_COLOR_RGB[1],TIME_COLOR_RGB[2]);
  lix.write(buf);
  Serial.println(time_now);
}

/*-------- NTP code ----------*/

const int NTP_PACKET_SIZE = 48; // NTP time is in the first 48 bytes of message
byte packetBuffer[NTP_PACKET_SIZE]; //buffer to hold incoming & outgoing packets

time_t getNtpTime()
{
  sendNTPpacket(ntpServerIP);
  delay(1000);
  if ( Udp.parsePacket() ) {
    Serial.println("packet received");
    Udp.read(packetBuffer, NTP_PACKET_SIZE);  // read packet into the buffer
    unsigned long secsSince1900;
    // Convert four bytes starting at location 40 to a long integer
    secsSince1900 =  (unsigned long)packetBuffer[40] << 24;
    secsSince1900 |= (unsigned long)packetBuffer[41] << 16;
    secsSince1900 |= (unsigned long)packetBuffer[42] << 8;
    secsSince1900 |= (unsigned long)packetBuffer[43];
    int time_offset = offset / 3600;
    return secsSince1900 - 2208988800UL + time_offset * SECS_PER_HOUR;
  }
  Serial.println("No NTP Response :-(");
  // return 0 if unable to get the time
  return 0;
}

// send an NTP request to the time server at the given address
void sendNTPpacket(IPAddress &address)
{
  // set all bytes in the buffer to 0
  memset(packetBuffer, 0, NTP_PACKET_SIZE);
  // Initialize values needed to form NTP request
  // (see URL above for details on the packets)
  packetBuffer[0] = 0b11100011;   // LI, Version, Mode
  packetBuffer[1] = 0;     // Stratum, or type of clock
  packetBuffer[2] = 6;     // Polling Interval
  packetBuffer[3] = 0xEC;  // Peer Clock Precision
  // 8 bytes of zero for Root Delay & Root Dispersion
  packetBuffer[12] = 49;
  packetBuffer[13] = 0x4E;
  packetBuffer[14] = 49;
  packetBuffer[15] = 52;
  // all NTP fields have been given values, now
  // you can send a packet requesting a timestamp:
  Udp.beginPacket(address, 123); //NTP requests are to port 123
  Udp.write(packetBuffer, NTP_PACKET_SIZE);
  Udp.endPacket();
}
