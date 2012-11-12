/***************************************************************************
 * Script to test wireless communication with the RFM12B tranceiver module
 * with an Arduino or Nanode board.
 *
 * Transmitter - Sends an incrementing number and flashes the LED every second.
 * Puts the ATMega and RFM12B to sleep between sends in case it's running on
 * battery.
 *
 * Ian Chilton <ian@chilton.me.uk>
 * December 2011
 *
 * Requires Arduino version 0022. v1.0 was just released a few days ago so
 * i'll need to update this to work with 1.0.
 *
 * Requires the Ports and RF12 libraries from Jeelabs in your libraries directory:
 *
 * http://jeelabs.org/pub/snapshots/Ports.zip
 * http://jeelabs.org/pub/snapshots/RF12.zip
 *
 * Information on the RF12 library - http://jeelabs.net/projects/11/wiki/RF12
 *
 * Original URL: https://github.com/ichilton/nanode-code/blob/master/test_rfm12b/test_tx/test_tx.pde
 *
 * Jarduino Client expanded to include:
 * -DHT22 Sensor
 * -soilMoisture
 * -sunlight measured via an LDR
 * -servo control capabilities
 * Upload to Cosm is handled by the Nanode Gateway code with hardcoded feed/apikey for each node
 ***********************************************************************************************/


// RF12b Requirements
#include <JeeLib.h>

#define freq RF12_433MHZ     // frequency
#define group 1            // network group 

// Use the watchdog to wake the processor from sleep:
ISR(WDT_vect) { 
  Sleepy::watchdogEvent(); 
}

// Send a single unsigned long
typedef struct 
{ 
  int jardinera; 
  float temperature;
  float humidity; 
  int soilMoisture; 
  int sunlight;
  int angle;
} 
PayloadJrdn;

PayloadJrdn jrdnData;

// DHT22 Requirements
// DHT Circuit
// Connect pin 1 (on the left) of the sensor to +5V
// Connect pin 2 of the sensor to whatever your DHTPIN is
// Connect pin 4 (on the right) of the sensor to GROUND
// Connect a 10K resistor from pin 2 (data) to pin 1 (power) of the sensor

#include "DHT.h"             // DHT library
#define DHTPIN 7             // what pin we're connected to
#define DHTTYPE DHT22        // DHT 22  Model (AM2302)
DHT dht(DHTPIN, DHTTYPE);

// Soil Moisture Sensor - DIY version with 2 large nails
// Two analog pins (A2, A3) are used as digitals to create AC switching the current direction every two seconds
// A1 is used to measure soil resistance to current. The more humid the less resistance
// We tried with Digital 3 & 4, but Ethernet controller doesn't initialize properly if selected, so we swtiched to analog pins
//Pin Numbers
int moisture_input=1; 
int divider_top=A3; 
int divider_bottom=A2;

//Aux
int moisture_result=0; // Resultado de la lectura del A0
int moisture_percentage=0; // Humedad en porcentaje respecto a los maximos del sensor

//Dummy values
int jardi;
float temp;
float hum; 
int soil; 
int sun;
int ang;
int node_id;

void setup()
{
  // Serial output at 9600 baud:
  Serial.begin(9600);

  // LED on Pin Digital 6:
  pinMode(5, OUTPUT);

  // DHT Initialization
  Serial.println("DHT22 sending!");
  dht.begin();
  
  // Soil pins
  pinMode(divider_top,OUTPUT);
  pinMode(divider_bottom,OUTPUT);
  
  //Select here which garden are we monitoring (1-8)
  jardi=2;
  
//  temp = 37.2;
//  hum = 20; 
//  soil = 40; 
  sun = 50;
  ang = 0;
  node_id=jardi+10;

  // Initialize RFM12B as an 868Mhz module and Node 11 + Group 1:
  // Node number = Jardinera number + 10; 1 is reserved for the NanodeRF Gateway  

  rf12_initialize(node_id, freq, group);
  Serial.println("Radio inicializada");
  Serial.print("Nodo ");
  Serial.print(node_id);
  Serial.print(", Frecuencia ");
  Serial.print(freq);
  Serial.print(" , Grupo ");
  Serial.println(group);
}


void loop()
{
  // LED OFF:
  digitalWrite(5, LOW);

  Serial.println("Going to sleep...");

  // Need to flush the serial before we put the ATMega to sleep, otherwise it
  // will get shutdown before it's finished sending:
  Serial.flush();
  delay(5);

  // Power down radio:
  rf12_sleep(RF12_SLEEP);

  // Sleep for 5s:
  Sleepy::loseSomeTime(5000);

  // Power back up radio:
  rf12_sleep(RF12_WAKEUP);

  // LED ON:
  digitalWrite(5, HIGH);

  Serial.println("Woke up...");
  Serial.flush();
  delay(5);

  // Jardinera_ID
  jrdnData.jardinera=jardi;
  
  //DHT22 Code
  float temp = dht.readTemperature();
  float hum = dht.readHumidity();     
  
  // check if returns are valid, if they are NaN (not a number) then something went wrong!
  if (isnan(temp) || isnan(hum)) {             
    Serial.println("Failed to read from DHT");
  } 
  else {
    jrdnData.temperature=temp; //Get temp from DHT22
    jrdnData.humidity=hum;
    Serial.println("Read from DHT");    
  }
  // End of DHT22 Code
  
  // Soil moisture measurement code
 
  moisture_result=ReadSoilMoisture();
  moisture_percentage=map(moisture_result,1024,0,0,100);
  
  jrdnData.soilMoisture=moisture_percentage;
  // End of Soil moisture measurement code 
  
  jrdnData.sunlight=sun;
  jrdnData.angle=ang;

  // Wait until we can send:
  while(!rf12_canSend())
    rf12_recvDone();

  // Send:
  rf12_sendStart(1, &jrdnData, sizeof jrdnData);
  rf12_sendWait(2);

  Serial.println("Sent payload");
}

int ReadSoilMoisture(){
  int reading;
  // drive a current through the divider in one direction
  digitalWrite(divider_top,HIGH);
  digitalWrite(divider_bottom,LOW);

  // wait a moment for capacitance effects to settle
  delay(1000);

  // take a reading
  reading=analogRead(moisture_input);
  // reverse the current
  digitalWrite(divider_top,LOW);
  digitalWrite(divider_bottom,HIGH);

  // give as much time in 'revers'e as in 'forward'
  delay(1000);

  // stop the current
  digitalWrite(divider_bottom,LOW);
  return reading;
}

