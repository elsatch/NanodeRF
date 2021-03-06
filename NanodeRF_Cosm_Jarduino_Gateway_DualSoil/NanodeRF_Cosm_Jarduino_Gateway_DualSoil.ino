/*                          _                                                      _      
      ____.                .___    .__               
     |    |____ _______  __| _/_ __|__| ____   ____  
     |    \__  \\_  __ \/ __ |  |  \  |/    \ /  _ \ 
 /\__|    |/ __ \|  | \/ /_/ |  |  /  |   |  (  <_> )
 \________(____  /__|  \____ |____/|__|___|  /\____/ 
  \/           \/           \/        
 */

//--------------------------------------------------------------------------------------

// Relay's data recieved by Jarduino Client up to COSM
// Looks for 'ok' reply from http request to verify data reached COSM

// Jarduino: A Nanode RF plus a custom shield, called Jarduino because it monitors gardens. Each unit monitors 2 areas for soil moisture
// All the other parameters are common for both areas. There are 6 sensor units deployed plus a gateway at the moment. (Nov. 2012)
// Developed for Jarduino Workshop at CICUS (Sevilla University) into the Digital Orchard series by Cesar Garcia Saez (@elsatch)
// Workshop contents developed by Sara Alvarellos, Cesar Garcia and Ricardo Merino. More info at: http://www.cicuslab.com
// 
// Based on the incredible work of Trystan Lea and Glyn Hudson for OpenEnergyMonitor.org project
// Modified for COSM by Roger James
// Adapted to serve as a gateway for multiple NanodesRF/feeds for Jarduino Workshop at CICUS (Sevilla) by Cesar Garcia Saez
// Big thanks to everyone involved!!
// Licenced under GNU GPL V3
// http://openenergymonitor.org/emon/license

// EtherCard Library by Jean-Claude Wippler and Andrew Lindsay - Download at http://github.com/jcw/ethercard
// JeeLib Library by Jean-Claude Wippler - Download at: https://github.com/jcw/jeelib
//

/* Final deployment setup:
|----+----+----+-----------|
|  1 |  2 |  3 | macetas   |
|----+----+----+-----------|
| J1 | J2 | J3 | jarduinos |
|----+----+----+-----------|
|  4 |  5 |  6 | macetas   |
|  7 |  8 |  9 | macetas   |
|----+----+----+-----------|
| J4 | J5 | J6 | jarduinos |
|----+----+----+-----------|
| 10 | 11 | 11 | macetas   |
|----+----+----+-----------|

|----------+----------+----------|
| Jarduino | Maceta_A | Maceta_B |
|----------+----------+----------|
| J1       |        1 |        4 |
| J2       |        2 |        5 |
| J3       |        3 |        6 |
| J4       |        7 |       10 |
| J5       |        8 |       11 |
| J6       |        9 |       12 |
|----------+----------+----------|
*/

//--------------------------------------------------------------------------------------

#define DEBUG     //comment out to disable serial printing to increase long term stability 
#define UNO       //anti crash wachdog reset only works with Uno (optiboot) bootloader, comment out the line if using delianuova

#include <JeeLib.h>	     
#include <avr/wdt.h>

#define MYNODE 1            
#define freq RF12_433MHZ     // frequency
#define group 1            // network group 


//---------------------------------------------------
// Feed ID and APIKey DATA
//---------------------------------------------------

// Create one of this lines for each device that will be sending data through this gateway
//#define FEED1  "/v2/feeds/PutYourFeedIDHere.csv?_method=put"
//#define APIKEY1 "X-ApiKey:-------------You Api Key Here------------" 

#define FEED1  "/v2/feeds/80020.csv?_method=put"
#define APIKEY1 "X-ApiKey:5k2WzS4CV48r5YRnDJMAYIvgttySAKxIazgwMy9tUFRDST0g" 

#define FEED2  "/v2/feeds/80021.csv?_method=put"
#define APIKEY2 "X-ApiKey:j_loGZky3gfZLO7cHbSD6Mn-24eSAKxFWFNDeUF1em9qZz0g"

/* To be filled with real data
 #define FEED3  "/v2/feeds/80020.csv?_method=put"
 #define APIKEY3 "X-ApiKey:5k2WzS4CV48r5YRnDJMAYIvgttySAKxIazgwMy9tUFRDST0g" 
 
 #define FEED4  "/v2/feeds/80021.csv?_method=put"
 #define APIKEY4 "X-ApiKey:j_loGZky3gfZLO7cHbSD6Mn-24eSAKxFWFNDeUF1em9qZz0g" 
 
 #define FEED5  "/v2/feeds/80020.csv?_method=put"
 #define APIKEY5 "X-ApiKey:5k2WzS4CV48r5YRnDJMAYIvgttySAKxIazgwMy9tUFRDST0g" 
 
 #define FEED6  "/v2/feeds/80021.csv?_method=put"
 #define APIKEY6 "X-ApiKey:j_loGZky3gfZLO7cHbSD6Mn-24eSAKxFWFNDeUF1em9qZz0g" 
 */

//---------------------------------------------------
// Data structures for transfering data between units
//---------------------------------------------------

typedef struct 
{ 
  int jarduino;   
  int maceta_A;
  int soilMoisture_A; 
  int maceta_B;
  int soilMoisture_B;
  float temperature;
  float humidity; 
  int sunlight;
} 
PayloadJrdn;

PayloadJrdn jrdnData;

//---------------------------------------------------

//char * feed;
//char * apikey;

//---------------------------------------------------------------------
// The PacketBuffer class is used to generate the json string that is send via ethernet - JeeLabs
//---------------------------------------------------------------------
class PacketBuffer : 
public Print {
public:
  PacketBuffer () : 
  fill (0) {
  }
  const char* buffer() { 
    return buf; 
  }
  byte length() { 
    return fill; 
  }
  void reset()
  { 
    memset(buf,NULL,sizeof(buf));
    fill = 0; 
  }
  virtual size_t write (uint8_t ch)
  { 
    if (fill < sizeof buf) buf[fill++] = ch; 
  }
  byte fill;
  char buf[150];
private:
};
PacketBuffer str;

//--------------------------------------------------------------------------
// Ethernet
//--------------------------------------------------------------------------
#include <EtherCard.h>		//https://github.com/jcw/ethercard 

// ethernet interface mac address, must be unique on the LAN
static byte mymac[] = { 
  0x42,0x31,0x42,0x21,0x30,0x31 };

//IP address of remote sever, only needed when posting to a server that has not got a dns domain name (staticIP e.g local server) 
byte Ethernet::buffer[700];
static uint32_t timer;

//Domain name of remote webserver - leave blank if posting to IP address 
char website[] PROGMEM = "api.cosm.com";

const int redLED = 6;                     // NanodeRF RED indicator LED
//const int redLED = 17;  		  // Open Kontrol Gateway LED indicator

const int greenLED = 5;                   // NanodeRF GREEN indicator LED

int ethernet_error = 0;                   // Etherent (controller/DHCP) error flag
int rf_error = 0;                         // RF error flag - high when no data received 
int ethernet_requests = 0;                // count ethernet requests without reply                 

int dhcp_status = 0;
int dns_status = 0;
int data_ready=0;                         // Used to signal that Jarduino data is ready to be sent
unsigned long last_rf;                    // Used to check for regular emontx data - otherwise error

char line_buf[50];                        // Used to store line of http reply header

//**********************************************************************************************************************
// SETUP
//**********************************************************************************************************************
void setup () {

  //Nanode RF LED indictor  setup - green flashing means good - red on for a long time means bad! 
  //High means off since NanodeRF tri-state buffer inverts signal 
  pinMode(redLED, OUTPUT); 
  digitalWrite(redLED,LOW);            
  pinMode(greenLED, OUTPUT); 
  digitalWrite(greenLED,LOW);       
  delay(100); 
  digitalWrite(redLED,HIGH);                          // turn off redLED

  Serial.begin(9600);
  Serial.println("\n[webClient]");

  //if (ether.begin(sizeof Ethernet::buffer, mymac, 10) == 0) {	//for use with Open Kontrol Gateway 
  if (ether.begin(sizeof Ethernet::buffer, mymac) == 0) {	//for use with NanodeRF
    Serial.println( "Failed to access Ethernet controller");
    ethernet_error = 1;  
  }

  dhcp_status = 0;
  dns_status = 0;
  ethernet_requests = 0;
  ethernet_error=0;
  rf_error=0;

  //For use with the modified JeeLib library to enable setting RFM12B SPI CS pin in the sketch.
  //rf12_set_cs(9);  //Open Kontrol Gateway	
  //rf12_set_cs(10); //emonTx, emonGLCD, NanodeRF, JeeNode

  rf12_initialize(MYNODE, freq, group);
  Serial.println("Radio inicializada");
  Serial.print("Nodo ");
  Serial.print(MYNODE);
  Serial.print(", Frecuencia ");
  Serial.print(freq);
  Serial.print(" , Grupo ");
  Serial.println(group);

  last_rf = millis()-40000;                                       // setting lastRF back 40s is useful as it forces the ethernet code to run straight away

  digitalWrite(greenLED,HIGH);                                    // Green LED off - indicate that setup has finished 

#ifdef UNO
  wdt_enable(WDTO_8S); 
#endif;

}

//**********************************************************************************************************************
// LOOP
//**********************************************************************************************************************
void loop () {

#ifdef UNO
  wdt_reset();
#endif

  dhcp_dns();   // handle dhcp and dns setup - see dhcp_dns tab

  // Display error states on status LED
  if (ethernet_error==1 || rf_error==1 || ethernet_requests > 0) digitalWrite(redLED,LOW);
  else digitalWrite(redLED,HIGH);

  //-----------------------------------------------------------------------------------------------------------------
  // 1) On RF recieve
  //-----------------------------------------------------------------------------------------------------------------
  if (rf12_recvDone()){      
    if (rf12_crc == 0 && (rf12_hdr & RF12_HDR_CTL) == 0)
    {

      int node_id = (rf12_hdr & 0x1F);
      Serial.print("node_id ");
      Serial.println(node_id);

      jrdnData = *(PayloadJrdn*) rf12_data;                          // get first garden sensor statio data
      Serial.println();                                              // print Jarduino Node 1 data to serial          
      Serial.print("Jarduino Number");
      Serial.println(jrdnData.jarduino);
      Serial.print("Monitors macetas numbered: ");
      Serial.print(jrdnData.maceta_A); 
      Serial.print(" and "); 
      Serial.print(jrdnData.maceta_B); 
      Serial.println();                                              // print Jarduino Node 1 data to serial          

      last_rf = millis();                                            // reset lastRF timer

      delay(50);                                                     // make sure serial printing finished
      // Aqui ponia feed_id, pero deberia poner datastream                    
      // CSV data datastrem_id,value\r\n one feed per line

      str.reset();                                                   // Reset csv string      
      str.println("rf_fail,0");      // RF recieved so no failure
      str.print("jarduino,");
      str.println(jrdnData.jarduino);
      str.print("maceta_A,");          
      str.println(jrdnData.maceta_A);
      str.print("soilMoisture_A,");       
      str.println(jrdnData.soilMoisture_A);
      str.print("maceta_B,");          
      str.println(jrdnData.maceta_B);
      str.print("soilMoisture_B,");       
      str.println(jrdnData.soilMoisture_B);
      str.print("temperature,");        
      str.println(jrdnData.temperature);
      str.print("humidity,");           
      str.println(jrdnData.humidity);
      str.print("sunlight,");           
      str.println(jrdnData.sunlight);
      data_ready = 1;                                                // data is ready
      rf_error = 0;
    }

  }


  //-----------------------------------------------------------------------------------------------------------------
  // 2) If no data is recieved from rf12 module the server is updated every 30s with RFfail = 1 indicator for debugging
  //-----------------------------------------------------------------------------------------------------------------
  if ((millis()-last_rf)>10000)
  {
    last_rf = millis();                                                 // reset lastRF timer
    str.reset();                                                        // reset csv string
    str.println("rf_fail,1");                                           // No RF received in 30 seconds so send failure 
    data_ready = 1;                                                     // Ok, data is ready
    rf_error=1;
  }


  //-----------------------------------------------------------------------------------------------------------------
  // 3) Send data via ethernet
  //-----------------------------------------------------------------------------------------------------------------
  ether.packetLoop(ether.packetReceive());

  if (data_ready) {

    Serial.println("CSV data"); 
    Serial.println(str.buf); // print to serial csv string
    ethernet_requests ++;
    //This super unelegant solution was put in place to prevent SRAM exhaustion by bringing the strings from Flash memory. 
    if (jrdnData.jarduino==1){                                             // Asocio el feed y la key segun el num jarduino
      ether.httpPost(PSTR(FEED1), website, PSTR(APIKEY1), str.buf, my_callback);
    } 
    else if (jrdnData.jarduino==2){                                             
      ether.httpPost(PSTR(FEED2), website, PSTR(APIKEY2), str.buf, my_callback);
    } /*else if (jrdnData.jarduino==3){                                             
     ether.httpPost(PSTR(FEED3), website, PSTR(APIKEY3), str.buf, my_callback);
     } else if (jrdnData.jarduino==4){                                             
     ether.httpPost(PSTR(FEED4), website, PSTR(APIKEY4), str.buf, my_callback);
     } else if (jrdnData.jarduino==5){                                             
     ether.httpPost(PSTR(FEED5), website, PSTR(APIKEY5), str.buf, my_callback);
     } else if (jrdnData.jarduino==6){                                             
     ether.httpPost(PSTR(FEED6), website, PSTR(APIKEY6), str.buf, my_callback);
     } */
    else {
      str.reset();
      Serial.println("Wow, unregistered node");
    }

    data_ready =0;

    /* Example of posting to COSM
     ethernet_requests ++;
     //ether.httpPost(feed, website, apikey, str.buf, my_callback);
     Serial.println(feed);
     Serial.println(apikey);
     ether.httpPost(PSTR(FEED1), website, PSTR(APIKEY1), str.buf, my_callback);
     data_ready =0; */
  }

  if (ethernet_requests > 10) delay(10000); // Reset the nanode if more than 10 request attempts have been tried without a reply

}
//**********************************************************************************************************************

//-----------------------------------------------------------------------------------
// Ethernet callback
// recieve reply and decode
//-----------------------------------------------------------------------------------
static void my_callback (byte status, word off, word len) {

  Serial.println("Server Reply");
  get_header_line(1, off);
  Serial.println(line_buf);
  Serial.println(strlen(line_buf));

  if (0 == strncmp(line_buf, "HTTP/1.1 200 OK", 15)) {

    get_header_line(2,off);      // Get the date and time from the header
    Serial.print("ok recv from server | ");    // Print out the date and time
    Serial.println(line_buf);    // Print out the date and time
    /*
    // Decode date time string to get integers for hour, min, sec, day
     // We just search for the characters and hope they are in the right place
     char val[1];
     val[0] = line_buf[23]; 
     val[1] = line_buf[24];
     int hour = atoi(val);
     val[0] = line_buf[26]; 
     val[1] = line_buf[27];
     int minute = atoi(val);
     val[0] = line_buf[29]; 
     val[1] = line_buf[30];
     int second = atoi(val);
     val[0] = line_buf[11]; 
     val[1] = line_buf[12];
     int day = atoi(val);
     
     // Don't send all zeros, happens when server failes to returns reponce to avoide GLCD getting mistakenly set to midnight
     if (hour>0 || minute>0 || second>0) 
     {  
     delay(100);
     
     char data[] = {
     't',hour,minute,second                        };
     int i = 0; 
     while (!rf12_canSend() && i<10) {
     rf12_recvDone(); 
     i++;
     }
     rf12_sendStart(0, data, sizeof data);
     rf12_sendWait(0);
     
     Serial.println("time sent to emonGLCD"); 
     }
     */


    ethernet_requests = 0; 
    ethernet_error = 0;
  }
}





