#include <DS1307.h>
#include <DHT.h>
#include <DHT_U.h>
#include <Wire.h>
#include "SPIFFS.h"
#include <WiFi.h>
#include <WiFiClient.h>
#include <WiFiAP.h>

#define AP_SSID "birb"
#define AP_PASSWORD "phuid5160"

#define WEBSERVER_BUTTON_PIN 34
#define SENSOR_PIN GPIO_NUM_33

#define DHTPIN 5     // Digital pin connected to the DHT sensor
// Feather HUZZAH ESP8266 note: use pins 3, 4, 5, 12, 13 or 14 --
// Pin 15 can work but DHT must be disconnected during program upload.

// Uncomment whatever type you're using!
#define DHTTYPE DHT11   // DHT 11
//#define DHTTYPE DHT22   // DHT 22  (AM2302), AM2321
//#define DHTTYPE DHT21   // DHT 21 (AM2301)

#define RTC_SDA 21
#define RTC_SCL 22

RTC_DATA_ATTR unsigned int bootCount = 1;

/*
  Method to print the reason by which ESP32
  has been awaken from sleep
*/
void print_wakeup_reason(esp_sleep_wakeup_cause_t wakeup_reason) {
  switch (wakeup_reason)
  {
    case ESP_SLEEP_WAKEUP_EXT0 : Serial.println("Wakeup caused by external signal using RTC_IO"); break;
    case ESP_SLEEP_WAKEUP_EXT1 : Serial.println("Wakeup caused by external signal using RTC_CNTL"); break;
    case ESP_SLEEP_WAKEUP_TIMER : Serial.println("Wakeup caused by timer"); break;
    case ESP_SLEEP_WAKEUP_TOUCHPAD : Serial.println("Wakeup caused by touchpad"); break;
    case ESP_SLEEP_WAKEUP_ULP : Serial.println("Wakeup caused by ULP program"); break;
    default : Serial.printf("Wakeup was not caused by deep sleep: %d\n", wakeup_reason); break;
  }
}

WiFiServer server(80);

void setup() {
  Serial.begin(115200);
  delay(1000); //Take some time to open up the Serial Monitor

  pinMode(WEBSERVER_BUTTON_PIN, INPUT_PULLUP);

  if (!digitalRead(WEBSERVER_BUTTON_PIN)) {
    Serial.println("starting web server");

    // You can remove the password parameter if you want the AP to be open.
    WiFi.softAP(AP_SSID, AP_PASSWORD);
    server.begin();

    if (!SPIFFS.begin(true)) {
      Serial.println("An Error has occurred while mounting SPIFFS");
      return;
    }

    File namefile = SPIFFS.open("/name.txt");
    if (!namefile) {
      Serial.println("Failed to open file for reading");
      return;
    }

    String birbname = "";
    while (namefile.available()) {
      birbname += char(namefile.read());
    }
    namefile.close();


    while (!digitalRead(WEBSERVER_BUTTON_PIN)) {
      WiFiClient client = server.available();   // listen for incoming clients

      if (client) {                             // if you get a client,
        Serial.println("New Client.");           // print a message out the serial port
        String currentLine = "";                // make a String to hold incoming data from the client
        while (client.connected() && !digitalRead(WEBSERVER_BUTTON_PIN)) {            // loop while the client's connected
          if (client.available()) {             // if there's bytes to read from the client,
            char c = client.read();             // read a byte, then
            Serial.write(c);                    // print it out the serial monitor
            if (c == '\n') {                    // if the byte is a newline character

              // if the current line is blank, you got two newline characters in a row.
              // that's the end of the client HTTP request, so send a response:
              if (currentLine.length() == 0) {

                // HTTP headers always start with a response code (e.g. HTTP/1.1 200 OK)
                // and a content-type so the client knows what's coming, then a blank line:
                client.println("HTTP/1.1 200 OK");
                client.println("Content-Type: text/html; charset=UTF-8");
                client.println();

                client.println("<a href='/download' download>   <button style='width: 100vw; height:50vh; font-size: 6rem; background-color: lime;'>DOWNLOAD</button> </a>");
                client.print("<form action='/nameupload' style='font-size:4rem;'>   <label for='name'>RENAME</label>   <input value='");
                client.print(birbname);
                client.println("' name='name' type='text' style='font-size:4rem;'>   <input type='submit' style='font-size:4rem;'> </form>");
                client.println("<button onclick='delbtn()'   style='width: 100vw; height:10vh; font-size: 3vh; background-color: lightcoral;'>DELETE</button>");
                client.println("<script>   function delbtn() { if (confirm('Are you sure you want to delete database?')) { var xmlHttp = new XMLHttpRequest(); xmlHttp.open('GET', '/delete', false); xmlHttp.send(null); return xmlHttp.responseText; } } </script>");

                // The HTTP response ends with another blank line:
                client.println();
                // break out of the while loop:
                break;
              } else {    // if you got a newline, then clear currentLine:
                currentLine = "";
              }
            } else if (c != '\r') {  // if you got anything else but a carriage return character,
              currentLine += c;      // add it to the end of the currentLine
            }
            // Check to see if the client request was "GET /H" or "GET /L":
            if (currentLine.endsWith("GET /download")) {
              // HTTP headers always start with a response code (e.g. HTTP/1.1 200 OK)
              // and a content-type so the client knows what's coming, then a blank line:
              client.println("HTTP/1.1 200 OK");
              client.print("Content-Disposition : attachment;filename=");
              client.print(birbname);
              client.println(".csv");
              client.println();

              if (!SPIFFS.begin(true)) {
                Serial.println("An Error has occurred while mounting SPIFFS");
                return;
              }

              File file = SPIFFS.open("/test_example.txt");
              if (!file) {
                client.print("Failed to open file for reading");
                return;
              }

              String data;
              while (file.available()) {
                char c = file.read();
                if (c == '\n') {
                  client.println(data);
                  data = "";
                }
                else if (c != '\r')
                  data += c;
              }
              client.println(data);
              file.close();

              // The HTTP response ends with another blank line:
              client.println();
              // break out of the while loop:
              break;
            }
            else if (currentLine.endsWith("GET /delete")) {
              Serial.println("deleting database");
              if (!SPIFFS.begin(true)) {
                Serial.println("An Error has occurred while mounting SPIFFS");
                return;
              }
              File wfile = SPIFFS.open("/test_example.txt", "w");
              wfile.print("");
              client.println();
              break;
            }
            else if (currentLine.endsWith("GET /nameupload?name=")) {
              String newname = "";
              while (client.available()) {
                char c = client.read();
                if (c == '\n' || c == '\r') break;
                else newname += c;
              }
              newname = newname.substring(0, newname.indexOf(" HTTP"));
              if (newname.length() > 0)
              {
                if (!SPIFFS.begin(true)) {
                  Serial.println("An Error has occurred while mounting SPIFFS");
                  return;
                }

                File wnamefile = SPIFFS.open("/name.txt", "w");
                if (!wnamefile) {
                  client.println("Failed to open file for writing");
                  return;
                }
                if (wnamefile.print(newname) < newname.length()) {
                  client.println("file write failed");
                }
                wnamefile.close();
                
                birbname = newname;
                Serial.print("changing birbname");
                client.print("new name:");
                client.println(birbname);
              }
              else {
                client.println("name not changed");
              }
              client.println();
              break;
            }
          }
        }
        // close the connection:
        client.stop();
        Serial.println("Client Disconnected.");
      }
    }
    server.stop();

  }

  esp_sleep_wakeup_cause_t wakeup_reason;

  wakeup_reason = esp_sleep_get_wakeup_cause();

  if (!SPIFFS.begin(true)) {
    Serial.println("An Error has occurred while mounting SPIFFS");
    return;
  }

  if (wakeup_reason == ESP_SLEEP_WAKEUP_EXT0) {
    Serial.println("writing to file");

    DS1307 clock;//define a object of DS1307 class

    clock.begin(RTC_SDA, RTC_SCL);

    /*clock.fillByYMD(2022, 10, 8); //Jan 19,2013
      clock.fillByHMS(20, 9, 00); //15:28 30"
      clock.fillDayOfWeek(SAT);//Saturday
      clock.setTime();//write time to the RTC chip*/

    clock.getTime();

    File wfile = SPIFFS.open("/test_example.txt", "a");

    if (wfile.print(bootCount) <= 0) {
      Serial.println("File write failed");
    }

    DHT dht(DHTPIN, DHTTYPE);

    dht.begin();

    // Reading temperature or humidity takes about 250 milliseconds!
    // Sensor readings may also be up to 2 seconds 'old' (its a very slow sensor)
    float h = dht.readHumidity();
    // Read temperature as Celsius (the default)
    float t = dht.readTemperature();

    // Check if any reads failed and exit early (to try again).
    if (isnan(h) || isnan(t)) {
      Serial.println(F("Failed to read from DHT sensor!"));
    }
    wfile.print(",");

    wfile.print(int(h));
    wfile.print(",");
    wfile.print(int(t));
    wfile.print(",");

    wfile.print(clock.year + 2000, DEC);
    wfile.print(",");
    wfile.print(clock.month, DEC);
    wfile.print(",");
    wfile.print(clock.dayOfMonth, DEC);
    wfile.print(",");

    wfile.print(clock.hour, DEC);
    wfile.print(",");
    wfile.print(clock.minute, DEC);
    wfile.print(",");
    wfile.print(clock.second, DEC);
    wfile.println(",");

    wfile.close();
    ++bootCount;
  }

  //Increment boot number and print it every reboot
  Serial.println("Boot number: " + String(bootCount));

  //Print the wakeup reason for ESP32
  print_wakeup_reason(wakeup_reason);

  /*
    First we configure the wake up source
    We set our ESP32 to wake up for an external trigger.
    There are two types for ESP32, ext0 and ext1 .
    ext0 uses RTC_IO to wakeup thus requires RTC peripherals
    to be on while ext1 uses RTC Controller so doesnt need
    peripherals to be powered on.
    Note that using internal pullups/pulldowns also requires
    RTC peripherals to be turned on.
  */
  esp_sleep_enable_ext0_wakeup(SENSOR_PIN, 0); //1 = High, 0 = Low

  //If you were to use ext1, you would use it like
  //esp_sleep_enable_ext1_wakeup(BUTTON_PIN_BITMASK,ESP_EXT1_WAKEUP_ANY_HIGH);

  //Go to sleep now
  Serial.println("Going to sleep now");
  delay(1000);
  esp_deep_sleep_start();
  Serial.println("This will never be printed");
}

void loop() {
  //This is not going to be called
}
