/*                          _                                                      _      
      ____.                .___    .__               
     |    |____ _______  __| _/_ __|__| ____   ____  
     |    \__  \\_  __ \/ __ |  |  \  |/    \ /  _ \ 
 /\__|    |/ __ \|  | \/ /_/ |  |  /  |   |  (  <_> )
 \________(____  /__|  \____ |____/|__|___|  /\____/ 
  \/           \/           \/        
 */

//---------------------------------------------------------------------------
// Jarduino: A Nanode RF plus a custom shield, called Jarduino because it monitors gardens. Each unit monitors 2 areas for soil moisture
// All the other parameters are common for both areas. There are 6 sensor units deployed plus a gateway at the moment. (Nov. 2012)
// Developed for Jarduino Workshop at CICUS (Sevilla University) into the Digital Orchard series by Cesar Garcia Saez (@elsatch)
// Workshop contents developed by Sara Alvarellos, Cesar Garcia and Ricardo Merino. More info at: www.cicuslab.com
// 
// Jarduino Client expanded to include:
// -DHT22 Sensor
// -2xsoilMoisture
// -Sunlight measured via LDR
// Upload to Cosm is handled by the Nanode Gateway code with hardcoded feed/apikey for each node
//
// Based on RF12B test_tx code by Ian Chilton <ian@chilton.me.uk> 
// Original URL: https://github.com/ichilton/nanode-code/blob/master/test_rfm12b/test_tx/test_tx.pde
//
// JeeLib Library by Jean-Claude Wippler - Download at: https://github.com/jcw/jeelib
//
// Big thanks to everyone involved!!
// Licenced under GNU GPL V3
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


 
// RF12b Requirements
#include <JeeLib.h>

#define freq RF12_433MHZ     // frequency
#define group 1            // network group 

// Use the watchdog to wake the processor from sleep:
ISR(WDT_vect) { 
  Sleepy::watchdogEvent(); 
}

// Send a single unsigned long
typedef struct {
  int jarduino;   
  int maceta_A;
  int soilMoisture_A; 
  int maceta_B;
  int soilMoisture_B;
  float temperature;
  float humidity; 
  int sunlight;
} PayloadJrdn;

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
//Pin Numbers Jardinera 1
int moisture_input_A=1; //A1
int divider_top_A=A2; 
int divider_bottom_A=A3;

//Pin Numbers Jardinera2
int moisture_input_B=4; //A4
int divider_top_B=A5;
int divider_bottom_B=3; // Don't have analogs enough, and as I'm using only to send current, digital is ok

//Aux Maceta A
int moisture_result_A=0; // Resultado de la lectura del A1
int moisture_percentage_A=0; // Humedad en porcentaje respecto a los maximos del sensor
// End of Soil Moisture Sensor 
// TODO: Review why printing right behind pinMode(divider_bottom, LOW) prints crap on serial port

//Aux Maceta B
int moisture_result_B=0; // Resultado de la lectura del A4
int moisture_percentage_B=0; // Humedad en porcentaje respecto a los maximos del sensor

// LDR Code
int ldrPin = A0;
// End of LDR Code

//Dummy values
int jrdn;  //Jarduino ID
int mct_A; //Maceta A connected to Jarduino ID=jrdn
int soil_A;
int mct_B; //Maceta B connected to Jarduino ID=jrdn
int soil_B;
float temp;
float hum; 
int sun;
int node_id;

void setup()
{
  // Serial output at 9600 baud:
  Serial.begin(9600);

  // LED on Pin Digital 5:
  pinMode(5, OUTPUT);

  // DHT Initialization
  Serial.println("DHT22 sending!");
  dht.begin();
  
  // Soil pins
  pinMode(divider_top_A,OUTPUT);
  pinMode(divider_bottom_A,OUTPUT);
  
  // Soil 2 pins
  pinMode(divider_top_B, OUTPUT);
  pinMode(divider_bottom_B, OUTPUT);
  
  //Select here which garden are we monitoring (1-12)
  jrdn=1;
  mct_A=1;
  mct_B=4;
  //  temp = 37.2;
  //  hum = 20; 
  //  soil = 40; 
  //  sun = 50;
  node_id=jrdn+10;

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
  digitalWrite(5, HIGH);

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
  digitalWrite(5, LOW);

  Serial.println("Woke up...");
  Serial.flush();
  delay(5);

  // Jardinera_ID
  jrdnData.maceta_A=mct_A;
  jrdnData.maceta_B=mct_B;

  
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
  
  // Soil moisture 1 measurement code
  moisture_result_A=ReadSoilMoistureA();
  moisture_percentage_A=map(moisture_result_A,1024,0,0,100);
  jrdnData.soilMoisture_A=moisture_percentage_A;
  // End of Soil moisture 1 measurement code 
  
  // Soil moisture 2 measurement code
  moisture_result_B=ReadSoilMoistureB();
  moisture_percentage_B=map(moisture_result_B,1024,0,0,100);
  jrdnData.soilMoisture_B=moisture_percentage_B;
  // End of Soil moisture 2 measurement code 


  //LDR Code
  sun=analogRead(ldrPin);
  jrdnData.sunlight=sun;
  //End of LDR Code
  
  // Wait until we can send:
  while(!rf12_canSend())
    rf12_recvDone();

  // Send:
  rf12_sendStart(1, &jrdnData, sizeof jrdnData);
  rf12_sendWait(2);

  Serial.println("Sent payload");
}

int ReadSoilMoistureA(){
  int reading_A;
  // drive a current through the divider in one direction
  digitalWrite(divider_top_A,HIGH);
  digitalWrite(divider_bottom_A,LOW);

  // wait a moment for capacitance effects to settle
  delay(1000);

  // take a reading
  reading_A=analogRead(moisture_input_A);
  // reverse the current
  digitalWrite(divider_top_A,LOW);
  digitalWrite(divider_bottom_B,HIGH);

  // give as much time in 'revers'e as in 'forward'
  delay(1000);

  // stop the current
  digitalWrite(divider_bottom_A,LOW);
  return reading_A;
}

int ReadSoilMoistureB(){
  int reading_B;
  // drive a current through the divider in one direction
  digitalWrite(divider_top_B,HIGH);
  digitalWrite(divider_bottom_B,LOW);

  // wait a moment for capacitance effects to settle
  delay(1000);

  // take a reading
  reading_B=analogRead(moisture_input_B);
  // reverse the current
  digitalWrite(divider_top_B,LOW);
  digitalWrite(divider_bottom_B,HIGH);

  // give as much time in 'revers'e as in 'forward'
  delay(1000);

  // stop the current
  digitalWrite(divider_bottom_B,LOW);
  return reading_B;
}

