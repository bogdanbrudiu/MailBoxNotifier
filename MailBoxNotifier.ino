#include <Base64.h>
#include <ESP8266WiFi.h>
#include <DNSServer.h>
#include <WiFiClient.h>
#include <EEPROM.h>
#include <ESP8266WebServer.h>


/////////////////////////
// Network Definitions //
/////////////////////////
const IPAddress AP_IP(192, 168, 1, 1);
const char* AP_SSID = "MAILBOX_NOTIFIER_SETUP";
boolean SETUP_MODE;
String SSID_LIST;
DNSServer DNS_SERVER;
ESP8266WebServer WEB_SERVER(80);

/////////////////////////
// Device Definitions //
/////////////////////////
String DEVICE_TITLE = "MailBox Notifier";


/////////////////////
// Pin Definitions //
/////////////////////
const int LED = BUILTIN_LED;
const int BUTTON_PIN = 0;

//////////////////////
// Button Variables //
//////////////////////
int BUTTON_STATE;
int LAST_BUTTON_STATE = LOW;
long LAST_DEBOUNCE_TIME = 0;
long DEBOUNCE_DELAY = 50;
int BUTTON_COUNTER = 0;

//////////////////////
// SETTINGS Size    //
//////////////////////
int SSID_SIZE = 32;
int PASSWORD_SIZE = 64;

int SMTP_SERVER_SIZE = 128;
int SMTP_PORT_SIZE = 5;
int SMTP_USER_SIZE = 64;
int SMTP_PASSWORD_SIZE = 64;

int FROM_SIZE = 64;
int TO_SIZE = 64;
int SUBJECT_SIZE = 128;
int BODY_SIZE = 1024;

int AP_SETTINGS_SIZE = SSID_SIZE + PASSWORD_SIZE;
int SMTP_SETTINGS_SIZE = SMTP_SERVER_SIZE + SMTP_PORT_SIZE + SMTP_USER_SIZE + SMTP_PASSWORD_SIZE;
int EMAIL_SETTINGS_SIZE = FROM_SIZE + TO_SIZE + SUBJECT_SIZE + BODY_SIZE;;
int SETTINGS_SIZE = AP_SETTINGS_SIZE + SMTP_SETTINGS_SIZE + EMAIL_SETTINGS_SIZE;

//////////////////////
// SETTINGS         //
//////////////////////
String ssid = "TP-LINK_6CF8F6";
String password = "12345678asdf";
  
String smtp_server = "mail.brudiu.ro";
String smtp_port = "25";
String smtp_user = "bogdan@brudiu.ro";
String smtp_password = "***REMOVED***";

String from = "bogdan@brudiu.ro";
String to = "bogdanbrudiu@gmail.com";
String subject = "MailBox Notifier";
String body = "New Mail";



  
void setup() {
 
  initHardware();
  // Try and restore saved settings
  if (loadSavedConfig()) {
    if (checkWiFiConnection()) {
      SETUP_MODE = false;
      startWebServer();
      // Turn the status led when the WiFi has been connected
      digitalWrite(LED, HIGH);
      return;
    }
  }
  SETUP_MODE = true;
  setupMode();
}

void loop() {



  // Handle WiFi Setup and Webserver for reset page
  if (SETUP_MODE) {
    DNS_SERVER.processNextRequest();
  }
  WEB_SERVER.handleClient();

  // Wait for button Presses
  boolean pressed = debounce();
  if (pressed == true) {
    BUTTON_COUNTER++;
    Serial.print("MailBox Door Opened ");
    Serial.print(BUTTON_COUNTER);
    Serial.println(" times");
    if(BUTTON_COUNTER > 1)
    {
      // Turn off the LED  while transmitting.
      digitalWrite(LED, LOW);
      
      triggerButtonEvent();
      // After a successful send turn the light back on 
      digitalWrite(LED, HIGH);
    }
  }
}

void initHardware()
{
  // Serial and EEPROM
  Serial.begin(115200);
  EEPROM.begin(SETTINGS_SIZE);
  delay(10);
  // LED
  pinMode(LED, OUTPUT);
  digitalWrite(LED, LOW);
  // Button
  pinMode(BUTTON_PIN, INPUT);
}

//////////////////////
// Button Functions //
//////////////////////
void triggerButtonEvent()
{
  // Define the WiFi Client
  WiFiClient client;
  // Set the http Port
  const int httpPort = 80;

  // Make sure we can connect
   Serial.println("Connecting to "+smtp_server+" port "+smtp_port);
  if (!client.connect(smtp_server.c_str(), atoi( smtp_port.c_str() ))) {
    Serial.println("Can't connect!");
    return;
  }

  if(!Receive(client)) {Serial.println("before ehlo");return;}

  Serial.println("Sending hello");
  IPAddress ip = WiFi.localIP();
  String ipStr = String(ip[0]) + '.' + String(ip[1]) + '.' + String(ip[2]) + '.' + String(ip[3]);
  client.println("EHLO "+ipStr);
  if(!Receive(client)) {Serial.println("ehlo");return;}

  Serial.println(F("Sending auth login"));
  client.println("auth login");
  if(!Receive(client)) {Serial.println("auth");return;}

  Serial.println("Sending User");
  char* base64user;
  base64user=(char*) malloc(Base64encode_len((int)smtp_user.length())+1);
  const size_t smtp_user_size = (smtp_user.length() + 1);
  char *smtp_user_chars = new char[smtp_user_size];
  strcpy(smtp_user_chars, smtp_user.c_str());
  Base64encode(base64user, smtp_user_chars,smtp_user.length());
  Serial.println(base64user);
  client.println(base64user);
  //free (base64user);
  
  if(!Receive(client)) {Serial.println("user");return;}

  Serial.println("Sending Password");
  char* base64pass;
  base64pass=(char*) malloc(Base64encode_len((int)smtp_password.length())+1);
  const size_t smtp_password_size = (smtp_password.length() + 1);
  char *smtp_password_chars = new char[smtp_password_size];
  strcpy(smtp_password_chars, smtp_password.c_str());
  Base64encode(base64pass, smtp_password_chars,smtp_password.length());
  Serial.println(base64pass);
  client.println(base64pass);
  //free(base64pass);
  
  if(!Receive(client)) {Serial.println("pass");return;}

// change to your email address (sender)
  Serial.println(F("Sending From"));
  client.println("MAIL From: "+from);
  if(!Receive(client)) {Serial.println("from");return;}

// change to recipient address
  Serial.println("Sending To");
  client.println("RCPT To: "+to);
  if(!Receive(client)) {Serial.println("tomail");return;}

  Serial.println("Sending DATA");
  client.println("DATA");
  if(!Receive(client)) {Serial.println("data");return;}

  Serial.println("Sending email");

// change to recipient address
  client.println("To: "+to);

// change to your address
  client.println("From: "+from);

  client.println("Subject: "+subject+"\r\n");

  client.println(body);

  client.println(".");

  if(!Receive(client)) return;

  Serial.println("Sending QUIT");
  client.println("QUIT");
  
  if(!Receive(client)) return;

  client.stop();

  Serial.println("disconnected");


  
}

byte Receive(WiFiClient client)
{
  byte respCode;
  byte thisByte;
  int loopCount = 0;

  while(!client.available()) {
    delay(1);
    loopCount++;

    // if nothing received for 10 seconds, timeout
    if(loopCount > 10000) {
      client.stop();
      Serial.println("10 sec Timeout");
      return 0;
    }
  }

  respCode = client.peek();

  while(client.available())
  {  
    thisByte = client.read();    
    Serial.write(thisByte);
  }

  if(respCode >= '4')
  {
    efail(client);
    return 0;  
  }

  return 1;
}

void efail(WiFiClient client)
{
 byte thisByte = 0;

 client.write("QUIT\r\n");

 while(!client.available()) delay(1);

 while(client.available())
 {  
   thisByte = client.read();    
   Serial.print(thisByte);
 }

 client.stop();
 
 Serial.println("disconnected");
 
}

// Debounce Button Presses
boolean debounce() {
  boolean retVal = false;
  int reading = digitalRead(BUTTON_PIN);
  if (reading != LAST_BUTTON_STATE) {
    LAST_DEBOUNCE_TIME = millis();
  }
  if ((millis() - LAST_DEBOUNCE_TIME) > DEBOUNCE_DELAY) {
    if (reading != BUTTON_STATE) {
      BUTTON_STATE = reading;
      if (BUTTON_STATE == HIGH) {
        retVal = true;
      }
    }
  }
  LAST_BUTTON_STATE = reading;
  return retVal;
}

/////////////////////////////
// AP Setup Mode Functions //
/////////////////////////////

// Load Saved Configuration from EEPROM
boolean loadSavedConfig() {
  Serial.println("Reading Saved Config....");
  String loaded_ssid = "";
  String loaded_password = "";
    
  String loaded_smtp_server = "";
  String loaded_smtp_port = "";
  String loaded_smtp_user = "";
  String loaded_smtp_password = "";
  
  String loaded_from = "";
  String loaded_to = "";
  String loaded_subject = "";
  String loaded_body = "";

 
  if (EEPROM.read(0) != 0) {

    //AP Settings
    for (int i = 0; i < SSID_SIZE; ++i) {
      //loaded_ssid += char(EEPROM.read(i));
    }
    if(loaded_ssid!=""){
      ssid=String(loaded_ssid);
      Serial.print("Loaded SSID: ");
    }else{
      Serial.print("Using default SSID: ");
    }
    Serial.println(ssid);
    
    for (int i = 0; i < PASSWORD_SIZE; ++i) {
      //loaded_password += char(EEPROM.read(SSID_SIZE + i));
    }
    if(loaded_password!=""){
      password=String(loaded_password);
      Serial.print("Loaded Password: ");
    }else{
      Serial.print("Using default Password: ");
    }
    Serial.println(password);
    
    //SMTP Settings
    for (int i = 0; i < SMTP_SERVER_SIZE; ++i) {
      loaded_smtp_server += char(EEPROM.read(AP_SETTINGS_SIZE + i));
    }
    if(loaded_smtp_server!=""){
      smtp_server=String(loaded_smtp_server);    
      Serial.print("Loaded SMTP Server: ");
    }else{
      Serial.print("Using default SMTP Server: ");
    }
    Serial.println(smtp_server);
          
    for (int i = 0; i < SMTP_PORT_SIZE; ++i) {
      loaded_smtp_port += char(EEPROM.read(AP_SETTINGS_SIZE + SMTP_SERVER_SIZE + i));
    }
    if(loaded_smtp_port!=""){
      smtp_port=String(loaded_smtp_port);    
      Serial.print("Loaded SMTP Port: ");
    }else{
      Serial.print("Using default SMTP Port: ");
    }
    Serial.println(smtp_port);
          
    for (int i = 0; i < SMTP_USER_SIZE; ++i) {
      loaded_smtp_user += char(EEPROM.read(AP_SETTINGS_SIZE + SMTP_SERVER_SIZE + SMTP_PORT_SIZE + i));
    }
    if(loaded_smtp_user!=""){
      smtp_user=String(loaded_smtp_user);    
      Serial.print("Loaded SMTP User: ");
    }else{
      Serial.print("Using default SMTP User: ");
    }
    Serial.println(smtp_user);
    
    for (int i = 0; i < SMTP_PASSWORD_SIZE; ++i) {
      loaded_smtp_password += char(EEPROM.read(AP_SETTINGS_SIZE + SMTP_SERVER_SIZE + SMTP_PORT_SIZE + SMTP_USER_SIZE + i));
    }
    if(loaded_smtp_password!=""){
      smtp_password=String(loaded_smtp_password);    
      Serial.print("Loaded SMTP Password: ");
    }else{
      Serial.print("Using default SMTP Password: ");
    }
    Serial.println(smtp_password);
    


    //Email Settings
    for (int i = 0; i < FROM_SIZE; ++i) {
      loaded_from += char(EEPROM.read(AP_SETTINGS_SIZE + SMTP_SETTINGS_SIZE + i));
    }
    if(loaded_from!=""){
      from=String(loaded_from);    
      Serial.print("From: ");
    }else{
      Serial.print("Using default From: ");
    }
    Serial.println(from);
    
    for (int i = 0; i < TO_SIZE; ++i) {
      loaded_to += char(EEPROM.read(AP_SETTINGS_SIZE + SMTP_SETTINGS_SIZE + FROM_SIZE + i));
    }
    if(loaded_to!=""){
      to=String(loaded_to);    
      Serial.print("To: ");
    }else{
      Serial.print("Using default To: ");
    }
    Serial.println(to);
    
    for (int i = 0; i < SUBJECT_SIZE; ++i) {
      loaded_subject += char(EEPROM.read(AP_SETTINGS_SIZE + SMTP_SETTINGS_SIZE + FROM_SIZE + TO_SIZE + i));
    }
    if(loaded_subject!=""){
      subject=String(loaded_subject);    
      Serial.print("Subject: ");
    }else{
      Serial.print("Using default Subject: ");
    }
    Serial.println(subject);
    
    for (int i = 0; i < BODY_SIZE; ++i) {
      loaded_body += char(EEPROM.read(AP_SETTINGS_SIZE + SMTP_SETTINGS_SIZE + FROM_SIZE + TO_SIZE + SUBJECT_SIZE + i));
    }
    if(loaded_body!=""){
      body=String(loaded_body);    
      Serial.print("Body: ");
    }else{
      Serial.print("Using default Body: ");
    }
    Serial.println(body);
    
    WiFi.begin(ssid.c_str(), password.c_str());
    return true;
  }
  else {
    Serial.println("Saved Configuration not found.");
    return false;
  }
}

// Boolean function to check for a WiFi Connection
boolean checkWiFiConnection() {
  int count = 0;
  Serial.print("Waiting to connect to the specified WiFi network");
  while ( count < 30 ) {
    if (WiFi.status() == WL_CONNECTED) {
      Serial.println();
      Serial.println("Connected!");
      return (true);
    }
    delay(500);
    Serial.print(".");
    count++;
  }
  Serial.println("Timed out.");
  return false;
}

// Start the web server and build out pages
void startWebServer() {
  if (SETUP_MODE) {
    Serial.print("Starting Web Server at IP address: ");
    Serial.println(WiFi.softAPIP());
    // Settings Page
    WEB_SERVER.on("/settings", []() {
      String s = "<h2>Wi-Fi Settings</h2>";
      s += "<form method=\"get\" action=\"setap\">";
      s += "<label for=\"ssid\">SSID: </label><select id=\"ssid\" name=\"ssid\">";
      s += SSID_LIST;
      s += "</select><br><br>";
      s += "<label for=\"pass\">Password: </label><input id=\"pass\" name=\"pass\" length=" + String(PASSWORD_SIZE) + " type=\"password\"><br><br>";
      
      s += "<label for=\"smtp_server\">SMTP Server: </label><input id=\"smtp_server\" name=\"smtp_server\" length=" + String(SMTP_SERVER_SIZE) + " value=\"" + smtp_server + "\" type=\"text\"><br><br>";
      s += "<label for=\"smtp_port\">SMTP Port: </label><input id=\"smtp_port\" name=\"smtp_port\" length=" + String(SMTP_PORT_SIZE) + " value=\"" + smtp_port + "\" type=\"text\"><br><br>";
      s += "<label for=\"smtp_user\">User: </label><input id=\"smtp_user\" name=\"smtp_user\" length=" + String(SMTP_USER_SIZE) + " value=\"" + smtp_user + "\" type=\"text\"><br><br>";
      s += "<label for=\"smtp_password\">Pass: </label><input id=\"smtp_password\" name=\"smtp_password\" length=" + String(SMTP_PASSWORD_SIZE) + " value=\"" + smtp_password + "\" type=\"password\"><br><br>";
     
      s += "<label for=\"from\">From: </label><input id=\"from\" name=\"from\" length=" + String(FROM_SIZE) + " value=\"" + from + "\" type=\"text\"><br><br>";
      s += "<label for=\"to\">To: </label><input id=\"to\" name=\"to\" length=" + String(TO_SIZE) + " value=\"" + to + "\" type=\"text\"><br><br>";
      s += "<label for=\"subject\">Subject: </label><input id=\"subject\" name=\"subject\" length=" + String(SUBJECT_SIZE) + " value=\"" + subject + "\" type=\"text\"><br><br>";
      s += "<label for=\"body\">Body: </label><textarea id=\"body\" name=\"body\">" + body + "</textarea><br><br>";
      
      s += "<input type=\"submit\">";
      s +="</form>";
      Serial.println(s);
      WEB_SERVER.send(200, "text/html", makePage("Wi-Fi Settings", s));
    });
    // setap Form Post
    WEB_SERVER.on("/setap", []() {
      for (int i = 0; i < SETTINGS_SIZE; ++i) {
        EEPROM.write(i, 0);
      }
      //AP Settings
      String ssid = urlDecode(WEB_SERVER.arg("ssid"));
      Serial.print("SSID: ");
      Serial.println(ssid);
      String pass = urlDecode(WEB_SERVER.arg("pass"));
      Serial.print("Password: ");
      Serial.println(pass);

      //SMTP Settings
      String smtp_server = urlDecode(WEB_SERVER.arg("smtp_server"));
      Serial.print("SMTP Server: ");
      Serial.println(smtp_server);
      String smtp_port = urlDecode(WEB_SERVER.arg("smtp_port"));
      Serial.print("SMTP Port: ");
      Serial.println(smtp_port);
      String smtp_user = urlDecode(WEB_SERVER.arg("smtp_user"));
      Serial.print("Username: ");
      Serial.println(smtp_user);
      String smtp_password = urlDecode(WEB_SERVER.arg("smtp_password"));
      Serial.print("Password: ");
      Serial.println(smtp_password);

      //Email Settings
      String from = urlDecode(WEB_SERVER.arg("from"));
      Serial.print("From: ");
      Serial.println(from);
      String to = urlDecode(WEB_SERVER.arg("to"));
      Serial.print("To: ");
      Serial.println(to);
      String subject = urlDecode(WEB_SERVER.arg("subject"));
      Serial.print("Subject: ");
      Serial.println(subject);
      String body = urlDecode(WEB_SERVER.arg("body"));
      Serial.print("Body: ");
      Serial.println(body);
      
      Serial.println("Writing SSID to EEPROM...");
      for (int i = 0; i < ssid.length(); ++i) {
        EEPROM.write(i, ssid[i]);
      }
      Serial.println("Writing Password to EEPROM...");
      for (int i = 0; i < pass.length(); ++i) {
        EEPROM.write(SSID_SIZE + i, pass[i]);
      }

      Serial.println("Writing SMTP Server to EEPROM...");
      for (int i = 0; i < smtp_server.length(); ++i) {
        EEPROM.write(AP_SETTINGS_SIZE + i, smtp_server[i]);
      }
      Serial.println("Writing SMTP Port to EEPROM...");
      for (int i = 0; i < smtp_port.length(); ++i) {
        EEPROM.write(AP_SETTINGS_SIZE + SMTP_SERVER_SIZE + i, smtp_port[i]);
      }
      Serial.println("Writing Username to EEPROM...");
      for (int i = 0; i < smtp_user.length(); ++i) {
        EEPROM.write(AP_SETTINGS_SIZE + SMTP_SERVER_SIZE + SMTP_PORT_SIZE + i, smtp_user[i]);
      }
      Serial.println("Writing Password to EEPROM...");
      for (int i = 0; i < smtp_password.length(); ++i) {
        EEPROM.write(AP_SETTINGS_SIZE + SMTP_SERVER_SIZE + SMTP_PORT_SIZE + SMTP_USER_SIZE + i, smtp_password[i]);
      }
     
      Serial.println("Writing From to EEPROM...");
      for (int i = 0; i < from.length(); ++i) {
        EEPROM.write(AP_SETTINGS_SIZE + SMTP_SETTINGS_SIZE + i, from[i]);
      }
      Serial.println("Writing To to EEPROM...");
      for (int i = 0; i < to.length(); ++i) {
        EEPROM.write(AP_SETTINGS_SIZE + SMTP_SETTINGS_SIZE + FROM_SIZE + i, to[i]);
      }
      Serial.println("Writing Subject to EEPROM...");
      for (int i = 0; i < subject.length(); ++i) {
        EEPROM.write(AP_SETTINGS_SIZE + SMTP_SETTINGS_SIZE + FROM_SIZE + TO_SIZE + i, subject[i]);
      }
      Serial.println("Writing Body to EEPROM...");
      for (int i = 0; i < body.length(); ++i) {
        EEPROM.write(AP_SETTINGS_SIZE + SMTP_SETTINGS_SIZE + FROM_SIZE + TO_SIZE + SUBJECT_SIZE + i, body[i]);
      }
      
      EEPROM.commit();
      Serial.println("Write EEPROM done!");
      String s = "<h1>WiFi Setup complete.</h1><p>The button will be connected automatically to \"";
      s += ssid;
      s += "\" after the restart.";
      Serial.println(s);
      WEB_SERVER.send(200, "text/html", makePage("Wi-Fi Settings", s));
      ESP.restart();
    });
    // Show the configuration page if no path is specified
    WEB_SERVER.onNotFound([]() {
      String s = "<h1>WiFi Configuration Mode</h1><p><a href=\"/settings\">Wi-Fi Settings</a></p>";
      Serial.println(s);
      WEB_SERVER.send(200, "text/html", makePage("Access Point mode", s));
    });
  }
  else {
    Serial.print("Starting Web Server at ");
    Serial.println(WiFi.localIP());
    WEB_SERVER.on("/", []() {
      IPAddress ip = WiFi.localIP();
      String ipStr = String(ip[0]) + '.' + String(ip[1]) + '.' + String(ip[2]) + '.' + String(ip[3]);
      String s = "<h1>IoT Button Status</h1>";
      s += "<h3>Network Details</h3>";
      s += "<p>Connected to: " + String(WiFi.SSID()) + "</p>";
      s += "<p>IP Address: " + ipStr + "</p>";
      
      s += "<h3>SMTP Details</h3>";
      s += "<p>SMTP Server: " + String(smtp_server) + "</p>";
      s += "<p>SMTP Port: " + String(smtp_port) + "</p>";
      s += "<p>Username: " + String(smtp_user) + "</p>";
      s += "<p>Password: " + String(smtp_password) + "</p>";
      
      s += "<h3>Email Details</h3>";
      s += "<p>From: " + String(from) + "</p>";
      s += "<p>To: " + String(to) + "</p>";
      s += "<p>Subject: " + String(subject) + "</p>";
      s += "<p>Body: " + String(body) + "</p>";
      
      s += "<h3>Options</h3>";
      s += "<p><a href=\"/reset\">Clear Saved Settings</a></p>";
      Serial.println(s);
      WEB_SERVER.send(200, "text/html", makePage("Station mode", s));
    });
    WEB_SERVER.on("/reset", []() {
      for (int i = 0; i < SETTINGS_SIZE; ++i) {
        EEPROM.write(i, 0);
      }
      EEPROM.commit();
      String s = "<h1>Wi-Fi settings was reset.</h1><p>Please reset device.</p>";
      Serial.println(s);
      WEB_SERVER.send(200, "text/html", makePage("Reset Wi-Fi Settings", s));
    });
  }
  WEB_SERVER.begin();
}

// Build the SSID list and setup a software access point for setup mode
void setupMode() {
  WiFi.mode(WIFI_STA);
  WiFi.disconnect();
  delay(100);
  int n = WiFi.scanNetworks();
  delay(100);
  Serial.println("");
  for (int i = 0; i < n; ++i) {
    SSID_LIST += "<option value=\"";
    SSID_LIST += WiFi.SSID(i);
    SSID_LIST += "\">";
    SSID_LIST += WiFi.SSID(i);
    SSID_LIST += "</option>";
  }
  delay(100);
  WiFi.mode(WIFI_AP);
  WiFi.softAPConfig(AP_IP, AP_IP, IPAddress(255, 255, 255, 0));
  WiFi.softAP(AP_SSID);
  DNS_SERVER.start(53, "*", AP_IP);
  startWebServer();
  Serial.print("Starting Access Point at \"");
  Serial.print(AP_SSID);
  Serial.println("\"");
}

String makePage(String title, String contents) {
  String s = "<!DOCTYPE html><html><head>";
  s += "<meta name=\"viewport\" content=\"width=device-width,user-scalable=0\">";
  s += "<style>";
  // Simple Reset CSS
  s += "*,*:before,*:after{-webkit-box-sizing:border-box;-moz-box-sizing:border-box;box-sizing:border-box}html{font-size:100%;-ms-text-size-adjust:100%;-webkit-text-size-adjust:100%}html,button,input,select,textarea{font-family:sans-serif}article,aside,details,figcaption,figure,footer,header,hgroup,main,nav,section,summary{display:block}body,form,fieldset,legend,input,select,textarea,button{margin:0}audio,canvas,progress,video{display:inline-block;vertical-align:baseline}audio:not([controls]){display:none;height:0}[hidden],template{display:none}img{border:0}svg:not(:root){overflow:hidden}body{font-family:sans-serif;font-size:16px;font-size:1rem;line-height:22px;line-height:1.375rem;color:#585858;font-weight:400;background:#fff}p{margin:0 0 1em 0}a{color:#cd5c5c;background:transparent;text-decoration:underline}a:active,a:hover{outline:0;text-decoration:none}strong{font-weight:700}em{font-style:italic}";
  // Basic CSS Styles
  s += "body{font-family:sans-serif;font-size:16px;font-size:1rem;line-height:22px;line-height:1.375rem;color:#585858;font-weight:400;background:#fff}p{margin:0 0 1em 0}a{color:#cd5c5c;background:transparent;text-decoration:underline}a:active,a:hover{outline:0;text-decoration:none}strong{font-weight:700}em{font-style:italic}h1{font-size:32px;font-size:2rem;line-height:38px;line-height:2.375rem;margin-top:0.7em;margin-bottom:0.5em;color:#343434;font-weight:400}fieldset,legend{border:0;margin:0;padding:0}legend{font-size:18px;font-size:1.125rem;line-height:24px;line-height:1.5rem;font-weight:700}label,button,input,optgroup,select,textarea{color:inherit;font:inherit;margin:0}input{line-height:normal}.input{width:100%}input[type='text'],input[type='email'],input[type='tel'],input[type='date']{height:36px;padding:0 0.4em}input[type='checkbox'],input[type='radio']{box-sizing:border-box;padding:0}";
  // Custom CSS
  s += "header{width:100%;background-color: #2c3e50;top: 0;min-height:60px;margin-bottom:21px;font-size:15px;color: #fff}.content-body{padding:0 1em 0 1em}header p{font-size: 1.25rem;float: left;position: relative;z-index: 1000;line-height: normal; margin: 15px 0 0 10px}";
  s += "</style>";
  s += "<title>";
  s += title;
  s += "</title></head><body>";
  s += "<header><p>" + DEVICE_TITLE + "</p></header>";
  s += "<div class=\"content-body\">";
  s += contents;
  s += "</div>";
  s += "</body></html>";
  Serial.println(s);
  return s;
}

String urlDecode(String input) {
  String s = input;
  s.replace("%20", " ");
  s.replace("+", " ");
  s.replace("%21", "!");
  s.replace("%22", "\"");
  s.replace("%23", "#");
  s.replace("%24", "$");
  s.replace("%25", "%");
  s.replace("%26", "&");
  s.replace("%27", "\'");
  s.replace("%28", "(");
  s.replace("%29", ")");
  s.replace("%30", "*");
  s.replace("%31", "+");
  s.replace("%2C", ",");
  s.replace("%2E", ".");
  s.replace("%2F", "/");
  s.replace("%2C", ",");
  s.replace("%3A", ":");
  s.replace("%3A", ";");
  s.replace("%3C", "<");
  s.replace("%3D", "=");
  s.replace("%3E", ">");
  s.replace("%3F", "?");
  s.replace("%40", "@");
  s.replace("%5B", "[");
  s.replace("%5C", "\\");
  s.replace("%5D", "]");
  s.replace("%5E", "^");
  s.replace("%5F", "-");
  s.replace("%60", "`");
  return s;
}


