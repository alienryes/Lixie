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
   - TIME_COLOUR_RGB
   - TIME_OFFSET
   - OWM_API_KEY
   - OWM_CITY_ID
   - TEMPERATURE
   - OWM_UNITS

   SIX_DIGIT is a true/false for 6 or 4 digit clocks
   TIME_COLOUR_RGB is an 8-bit RGB value to color the displays
   TIME_OFFSET is your local UTC offset for time zones

   OWM_API_KEY can be generated by signing up for an
   API key with OWM at:
    https://openweathermap.org/appid

   OWM_CITY_ID can be found by searching for your
   local weather on openweathermap.org, and looking
   at the URL:

     Los Angeles:
     https://openweathermap.org/city/5368361
                This is your City ID ^

   TEMPERATURE is the type of weather reading you'd like
   to show. Options are:
     "temp",
     "pressure",
     "humidity",
     "temp_min",
     "temp_max"

   OWM_UNITS is how you'd like the information shown.
   Options are:
    "imperial",
    "metric",
    "kelvin"

   Another feature of this code is pulling your
   current weather state (cloudy, sunny, thunder,
   etc.) and mapping it to the color your your Lixies!

    Very Pale Blue     = Thunderstorm
    Pale Blue          = Drizzle
    Cyan               = Rain
    White              = Snow
    Grey               = Atmospheric (fog, pollution)
    Yellow             = Clear
    Pale Yellow        = Clouds / Calm
    Red                = Extreme (tornado, lightning)
   -------------------------------------------------
*/

#include "Lixie.h" // Include Lixie Library
#define DATA_PIN   5
#define NUM_LIXIES 6
#define INTERVAL_API 600000
Lixie lix(DATA_PIN, NUM_LIXIES);

#include <TimeLib.h>                                      // Time Library
#include <ESP8266WiFi.h>                                  // ESP8266 WIFI Lib
#include <WiFiUdp.h>                                      // UDP Library
#include <ESP8266WiFiMulti.h>                             // WifiMulti Lib for connection handling
#include <ESP8266HTTPClient.h>                            // HTTPClient for web requests
#include <ArduinoJson.h>                                  // JSON Parser
ESP8266WiFiMulti WiFiMulti;

//---------------------------------------
const char* WIFI_SSID = "BTWholeHome-WJQ";                //  your network SSID (name)
const char* WIFI_PASS = "cqHeC6WCJxFJ";                   //  your network password

const bool HOUR_12 = false;                               // 12/24-hour format
const bool SIX_DIGIT = true;                              // True if 6-digit clock with seconds
int red = random(50, 255);                                // Random red value between 50 and 255
int green = random(50, 255);                              // Random green value between 50 and 255
int blue = random(50, 255);                               // Random blue value between 50 and 255
byte TIME_COLOUR_RGB[3] = {red, green, blue};             // Set colour as a base to start with
const int TIME_OFFSET = +1;                               // British Summer Time
const int NIGHT_ON = 23;                                  // Night mode start hour
const int NIGHT_OFF = 7;                                  // Night mode end hour
char sunrise[16];
char sunset[16];

String OWM_API_KEY = "4b200143e34e1bd084c42bab4010ff0c";  // Open Weather Map API Key
String OWM_CITY_ID = "2653974";                           // Open Weather Map CityID
String TEMPERATURE   = "temp";                            // Temperature
String HUMIDITY = "humidity";                             // Humidity
String SUNRISE = "sunrise";                               // Sunrise time
String SUNSET = "sunset";                                 // Sunset time
String OWM_UNITS   = "metric";                            // can be "imperial", "metric", or "kelvin"

byte state_colors[9][3] = {
  {127, 127, 255}, // 0 Thunderstorm Very Pale Blue
  {64, 64, 255},   // 1 Drizzle Pale Blue
  {0, 127, 255},   // 2 Rain Cyan
  {255, 255, 255}, // 3 Snow White
  {127, 127, 127}, // 4 Atmospheric Grey
  {255, 255, 0},   // 5 Clear Yellow
  {255, 255, 64},  // 6 Clouds Pale Yellow
  {255, 0, 0},     // 7 Extreme Red
  {255, 255, 64}   // 8 Calm Pale Yellow
};
//---------------------------------------

// NTP Servers:
static const char ntpServerName[] = "uk.pool.ntp.org";

WiFiUDP Udp;
unsigned int localPort = 8888;  // local port to listen for UDP packets

time_t getNtpTime();
void digitalClockDisplay();
void sendNTPpacket(IPAddress &address);
//lix.max_power(5,400);

void setup()
{
  lix.begin(); // Initialize LEDs
  Serial.begin(115200);
  WiFiMulti.addAP(WIFI_SSID, WIFI_PASS); // Your WIFI credentials

  // This sets all lights to yellow while we're connecting to WIFI
  while ((WiFiMulti.run() != WL_CONNECTED)) {
    lix.color(255, 255, 0);
    lix.write(8888);
    delay(100);
  }

  // Green on connection success
  lix.color(0, 255, 0);
  lix.write(9999);
  delay(500);

  // Reset colors to default
  lix.color(255, 255, 255);
  lix.clear();

  Serial.print("IP assigned by DHCP is ");
  Serial.println(WiFi.localIP());
  Serial.println("Starting UDP");
  Udp.begin(localPort);
  Serial.print("Local port: ");
  Serial.println(Udp.localPort());
  Serial.println("Waiting for sync");
  setSyncProvider(getNtpTime);
  setSyncInterval(3600);
}

time_t prevDisplay = 0; // when the digital clock was displayed

void loop()
{
  if (timeStatus() != timeNotSet) {
    if (hour() >= NIGHT_ON || hour() <= NIGHT_OFF ) {
      nightmode();
    }
    //update the display only if time has changed
    else if (now() != prevDisplay) { 
      prevDisplay = now();
      digitalClockDisplay();
    }
  }
}

void digitalClockDisplay()
{
  // Using 1000,000 as our base number creates zero-padded times.
  uint32_t sum = 1000000;
  if (second() < 51 ) {
    // Put the hour on two digits,
    sum += hour() * 10000;
    // The minute on two more,
    sum += minute() * 100;
    // and the seconds on two more.
    sum += second();
    // Take out the seconds if we just have a 4-digit clock
    if (SIX_DIGIT == false) {
      sum /= 100;
    }
  }
  // Put sunrise time on 4 digits
  else if ( second() >= 51 && second() <= 53 ) {
    unsigned int srout = atoi(sunrise);
    sum += srout;
  }
  // Put sunset time on 4 digits
  else if ( second() >= 54 && second() <= 56 ) {
    unsigned int ssout = atoi(sunset);
    sum += ssout;
  }
  else if ( second() >= 57 && second() <= 59 ) {
    // Put the day on 2 digits
    sum += day() * 10000;
    // Put the month on 2 digits
    sum += month() * 100;
    // Put the year on 2 digits
    sum += ((year() / 10) % 10) * 10;
    sum += year() % 10;
  }
  // On the hour set random colour between 50 and 255 for RGB
  if ( minute() == 0 && second() == 0 ) {
    red = random (50, 255);
    green = random (50, 255);
    blue = random (50, 255);
  }
  TIME_COLOUR_RGB [0] = red;
  TIME_COLOUR_RGB [1] = green;
  TIME_COLOUR_RGB [2] = blue;
  lix.color(TIME_COLOUR_RGB[0], TIME_COLOUR_RGB[1], TIME_COLOUR_RGB[2]);
  if ( second() < 30 || second() > 35 ) {
    lix.write(sum);
  }
  else if (second() >= 30 && second() <= 35) {
    // Go to weather function
    checkOWM();
  }
}

void nightmode()
// Put nixies to sleep during night mode
{
  lix.color(0, 0, 0);
  lix.clear();
}

/*-------- NTP code ----------*/

const int NTP_PACKET_SIZE = 48; // NTP time is in the first 48 bytes of message
byte packetBuffer[NTP_PACKET_SIZE]; //buffer to hold incoming & outgoing packets

time_t getNtpTime()
{
  IPAddress ntpServerIP; // NTP server's ip address

  while (Udp.parsePacket() > 0) ; // discard any previously received packets
  Serial.println("Transmit NTP Request");
  // get a random server from the pool
  WiFi.hostByName(ntpServerName, ntpServerIP);
  Serial.print(ntpServerName);
  Serial.print(": ");
  Serial.println(ntpServerIP);
  sendNTPpacket(ntpServerIP);
  uint32_t beginWait = millis();
  while (millis() - beginWait < 1500) {
    int size = Udp.parsePacket();
    if (size >= NTP_PACKET_SIZE) {
      Serial.println("Received NTP Response");
      Udp.read(packetBuffer, NTP_PACKET_SIZE);  // read packet into the buffer
      unsigned long secsSince1900;
      // convert four bytes starting at location 40 to a long integer
      secsSince1900 =  (unsigned long)packetBuffer[40] << 24;
      secsSince1900 |= (unsigned long)packetBuffer[41] << 16;
      secsSince1900 |= (unsigned long)packetBuffer[42] << 8;
      secsSince1900 |= (unsigned long)packetBuffer[43];
      return secsSince1900 - 2208988800UL + TIME_OFFSET * SECS_PER_HOUR;
    }
  }
  Serial.println("No NTP Response :-(");
  return 0; // return 0 if unable to get the time
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

// Weather code

void checkOWM() {
  // Set up JSON Parser
  StaticJsonDocument<1200> owm_data;
  // Wait for WiFi connection
  if ((WiFiMulti.run() == WL_CONNECTED)) {
    HTTPClient http;
    http.begin("http://api.openweathermap.org/data/2.5/weather?id=" + OWM_CITY_ID + "&appid=" + OWM_API_KEY + "&units=" + OWM_UNITS);
    int httpCode = http.GET();
    if (httpCode > 0) {
      if (httpCode == HTTP_CODE_OK) {
        String payload = http.getString();
        // Parse JSON
        DeserializationError error = deserializeJson(owm_data, payload);
        if (error) {
          Serial.print(F("deserializeJson() failed with code "));
          Serial.println(error.c_str());
          return;
        }
        // Get temp, humidity, weather state, sunrise and sunset from parsed JSON
        int tempfield = owm_data["main"][TEMPERATURE];
        int humfield = owm_data["main"][HUMIDITY];
        unsigned long sunriseul = owm_data["sys"][SUNRISE];
        unsigned long sunsetul = owm_data["sys"][SUNSET];
        int code = owm_data["weather"][0]["id"];
        int weather_state = codeToState(code);
        // Convert sunrise and sunset times from epoch time to 4 hours and minutes        
        sprintf(sunrise, "%02d%02d", hour(sunriseul), minute(sunriseul));
        sprintf(sunset, "%02d%02d", hour(sunsetul), minute(sunsetul));
        // Set Lixie colour based on weather code
        lix.color(
          state_colors[weather_state][0],
          state_colors[weather_state][1],
          state_colors[weather_state][2]
        );
        // Using 1000,000 as our base number creates zero-padded temperature
        uint32_t temppad = 1000000;
        // Write temperature
        temppad += tempfield;
        lix.write(temppad);
        delay(2997);
        // Using 1000,000 as our base number creates zero-padded humidity
        uint32_t humpad = 1000000;
        // Write humidity
        humpad += humfield;
        lix.write(humpad);
        delay(2997);
      }
    }
    http.end();
  }
}



byte codeToState(uint16_t code) {
  byte state = 0;
  if (code >= 200 && code < 300) {
    state = 0;
  }
  else if (code >= 300 && code < 400) {
    state = 1;
  }
  else if (code >= 500 && code < 600) {
    state = 2;
  }
  else if (code >= 600 && code < 700) {
    state = 3;
  }
  else if (code >= 700 && code < 800) {
    state = 4;
  }
  else if (code == 800) {
    state = 5;
  }
  else if (code > 800 && code < 900) {
    state = 6;
  }
  else if (code >= 900 && code < 907) {
    state = 7;
  }
  else if (code >= 907 && code < 956) {
    state = 8;
  }
  else if (code >= 956) {
    state = 7;
  }
  return state;
}

byte format_hour(byte hr) {
  if (hr > 12) {
    hr += 12;
  }
  if (hr == 0) {
    hr = 12;
  }
  return hr;
}