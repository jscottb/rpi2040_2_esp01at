/*
   Raspberry Pi Pico ESP8266-01 demo

   Scott Beasley - 2021

   The code does:
      - Init's UART1 for propper comm's
      - Connects to Wifi using the given SSID and PASSWORD
      - Get's the IP adress assinged to the ESP from the router
      - Sends a "Dweet" with the IP given via dweet.io (to be used later to find the Pi)
      - Starts up the local web responder
        - Serves up a landing page
        - Responds to url routes: 
          - /ledon
          - /ledoff
          - /flash

    Will work with either the MBED OS RPI2040 or RPI 2040 for Ardunio 
*/

#include <Arduino.h>

// Comment out to turn off the serial debug info
#define DEBUG false

// Uncomment if using the MBED OS RP2040 SDK
//#define MBED_OS

#define esp8266 Serial2 // UART1 is on Serial2 for both SDK's
#define CommSpeed 115200
#define LONGWAIT 12000
#define SHORTWAIT 2000

// SET with your Wifi connection info.
const String SSID = "**********", PASSWD = "***********";

// CHANGE THIS to your DWEET "thing" name!!!
const String DWEET_THINGY = "MyShinyThingInTheGreyCloud-1453-E-Zc";

String sendData (String command, int timeout);
void initWifiModule (String ssid, String passwd);
char * getstrfld (char *strbuf, int fldno, int ofset, char *sep, char *retstr);
String send_html_response (void);

// Globals
#ifdef MBED_OS
// Used for the MBED OS RPI Pico SDK
UART Serial2 (4, 5, 0, 0);
#endif

String ip = "";
// If there is an error detected during a sendData call
// was_error will be set to true
boolean was_error = false;
// Flag to tell if the LED should be blinked
boolean flash = false;
int hit_cnt = 0, state = 0;

void setup ( )
{
  if (DEBUG) Serial.begin (CommSpeed);

#ifndef MBED_OS
  // Used for the Non-MBED OS SDK
  esp8266.setTX (4);
  esp8266.setRX (5);
#endif
  esp8266.begin (CommSpeed);

  initWifiModule (SSID, PASSWD);
  pinMode (LED_BUILTIN, OUTPUT);
}

void loop ( )
{
  char line_buff[81], method[9], url[65], con_id[4];
  static long run_time = 0;


  if (flash && (millis ( ) - run_time > 1000)) {
    run_time = millis ( );
    digitalWrite (LED_BUILTIN, state);
    state = !state;
  }

  while (esp8266.available ( )) {
    // Read request data sent from the ESP module. The request is \n terminated.
    String req_data = esp8266.readStringUntil ('\n');

    // "+IPD" precedes the connection/request info we need.
    if (req_data.indexOf ("+IPD") != -1) {
      String response = "";

      // Get the connection and request info sent by the web browser.
      // The con_id field will be used later for sending data back to the ESP.
      getstrfld ((char *)req_data.c_str ( ), 1, 0, (char *)",", con_id);
      // HTP request method, GET, POST, DELETE, etc... 
      getstrfld ((char *)req_data.c_str ( ), 1, 0, (char *)": ", method);
      getstrfld ((char *)req_data.c_str ( ), 2, 0, (char *)": ", url);

      String str_url = String (url);
      if (DEBUG)
        Serial.println ("<" + String (con_id) + "><" + String (method) + "><" + String (url) + ">");

      // Look for URIs we want to handle
      if (str_url.indexOf ("/favicon.ico") != -1) { // Shut the browser up by sending back a null icon.
        response = "HTTP/1.1 200 OK\r\nContent-Type: image/x-icon\r\nContent-Length: 0\r\nConnection: close\r\n\r\n";
      } else if (str_url.indexOf ("/ledon") != -1) {
        // LED on
        digitalWrite (LED_BUILTIN, HIGH);
        flash = false;        
        response = send_html_response ( );
      } else if (str_url.indexOf ("/ledoff") != -1) {
        // LED off
        digitalWrite (LED_BUILTIN, LOW);
        flash = false;
        response = send_html_response ( );
      } else if (str_url.indexOf ("/flash") != -1) {
        // LED off
        digitalWrite (LED_BUILTIN, LOW);
        flash = !flash;        
        response = send_html_response ( );
      } else { // Send back server web page here.
        hit_cnt++;
        response = send_html_response ( );
      }

      // Send the response data back to the caller
      String response_send = "AT+CIPSEND=" + String (con_id) + "," + response.length ( ) + "\r\n";

      sendData (response_send, SHORTWAIT);
      sendData (response, SHORTWAIT);

      String closeCommand = "AT+CIPCLOSE=" + String (con_id) + "\r\n";
      sendData (closeCommand, SHORTWAIT);
    }
  }
}

// Send commands and data to the ESP.
String sendData (String command, int timeout)
{
  String response = "", line = "";
  was_error = false;

  esp8266.print (command);
  long int mills = millis ( );
  while ((mills + (long int)timeout) > millis ( )) {
    if (esp8266.available ( )) {
      line = esp8266.readStringUntil ('\n');
      line.trim ( ); // Remove the \r from the line

      // Exit loop if an end status is found.
      if (line == "OK" || line == "SEND OK" || line == "CLOSED" ||
          line == "CLOSE" || line == "ERROR") {
        if (DEBUG) {
          Serial.println ("DEBUG end status found >> " + line);
        }
        break;
      }

      response += line + "\n";
    }
  }

  // Let the caller know if an error occured.
  if (line == "ERROR") {
    was_error = true;
  }

  if (DEBUG) {
    Serial.println ("DEBUG >> " + response);
    Serial.println ("DEBUG >> send done");
  }

  return response;
}

void initWifiModule (String ssid, String passwd)
{
  // Reset the module.
  sendData ("AT+RST\r\n", LONGWAIT);

  // Connect to the WiFi router.
  sendData ("AT+CWJAP=\"" + ssid + "\",\"" + passwd + "\"\r\n", LONGWAIT);
  delay (SHORTWAIT);

  // Set the module to "STATION" mode.
  sendData ("AT+CWMODE=1\r\n", SHORTWAIT);
  delay (SHORTWAIT);

  // Get the IP and "Dweet" it out.
  ip = getIP ( );
  webget ("dweet.io", "/dweet/for/" + DWEET_THINGY + "?ip=" + ip, "80");

  // Set the server up for multi connections and start it up.
  sendData ("AT+CIPMUX=1\r\n", SHORTWAIT);
  delay (SHORTWAIT);
  sendData ("AT+CIPSERVER=1,80\r\n", SHORTWAIT);
}

String getIP (void)
{
  String ip_addy = "";

  // Request the station settings
  String response = sendData ("AT+CIPSTA?\r\n", SHORTWAIT);

  // Dig the ip out of the returned data.
  if (response.indexOf (":ip:") == -1) {
    return String("");
  }

  int start = response.lastIndexOf (":ip:\"") + 5;
  int end = response.indexOf ("\"", start);
  ip_addy = response.substring (start, end);

  if (DEBUG) Serial.println ("<" + ip_addy + ">");
  return ip_addy;
}

// Send a "GET" request to a webserver.
String webget (String server, String url, String port)
{
  String response = "";

  // Build the GET request and send it.
  url = "GET " + url + " HTTP/1.1\r\nHost: " + server + "\r\nConnection: close\r\n\r\n";

  // Build the ESP AT send command data
  response = sendData ("AT+CIPSTART=\"TCP\",\"" + server + "\"," + port + String("\r\n"), LONGWAIT);
  if (DEBUG) Serial.println ("<" + response + ">");

  // Send the request data
  response = sendData("AT+CIPSEND=" + String (url.length ( )) + String("\r\n"), LONGWAIT);
  if (DEBUG) Serial.println ("<" + response + ">");

  // Get the server responce from the ESP
  response = sendData(url, LONGWAIT);
  if (DEBUG) Serial.println ("<" + response + ">");

  // Wrap-up the request
  response = sendData("AT+CIPCLOSE\r\n", LONGWAIT);
  if (DEBUG) Serial.println ("<" + response + ">");

  return response;
}

String send_html_response (void)
{
  // You can add your own html here that has forms, images with ajax (jquery, etc.) type embeded code.
  String response = "HTTP/1.1 200 OK\r\nContent-Type: text/html\r\nConnection: close\r\n\r\n"
                    "<html><h1>Hello from the Worlds worst web page!</h1><i>Hello comrade Strudel, we meet again...</i>"
                    "<br><br><a href=\"/ledon\">Led On</a> - <a href=\"/ledoff\">Led Off</a> - <a href=\"/flash\">Flash " +
                    ((flash) ? String ("Off") : String ("On")) + "</a><br><br>Hit count: <b>" +
                    String (hit_cnt) + "</b></html>";

  return (response);
}

// My old stand-by to break delimited strings up.
char * getstrfld (char *strbuf, int fldno, int ofset, char *sep, char *retstr)
{
  char *offset, *strptr;
  int curfld;

  offset = strptr = (char *)NULL;
  curfld = 0;

  strbuf += ofset;

  while (*strbuf) {
    strptr = !offset ? strbuf : offset;
    offset = strpbrk ((!offset ? strbuf : offset), sep);

    if (offset) {
      offset++;
    } else if (curfld != fldno) {
      *retstr = 0;
      break;
    }

    if (curfld == fldno) {
      strncpy (retstr, strptr,
               (int)(!offset ? strlen (strptr) + 1 :
               (int)(offset - strptr)));
      if (offset)
        retstr[offset - strptr - 1] = 0;

      break;
    }

    curfld++;
  }

  return retstr;
}
