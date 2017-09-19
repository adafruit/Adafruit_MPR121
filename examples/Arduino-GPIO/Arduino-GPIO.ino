/*
 * This example shows how to use MPR121 library on Arduino with hardware I2C support
 * using GPIO functions and touch buttons with debounce.
 * 
 * Hardware:
 *    Use an Arduino (Nano) with I2C support and a MPR121 breakout board.
 *    If your Arduino doesn´t support hardware I2C you can use the software I2C version for ESP8266 in the other example!
 *    Connect Arduino to 5V VCC and MPR121 to 3.3V VCC output of the Arduino and GND (or use a voltage regulator such as LM117 or LF33CV).
 *    Connect SDA/SCAL lines from MPR121 board to pins 18/19 on Arduino (see note below!)
 *    
 *    If your breakout board already has pullup-resistors for SDA/SCL lines no extra resistors are needed
 *    otherwise you may need to hook SDA/SCL lines with 4.7k resistors to ground for a stable communication.
 *    To show the touch control feature a LED will be used. Hook up the LED and resistors on pins as defined below.
 *    
 *    - Connect a touchpad on MPR121 pin 0 (channel 0); normal cable will do
 *    - Connect MPR121 pin 8 (channel 8) with resistor to VCC
 *    - Connect a LED with resistor on MPR121 pin 11 (Channel 11) and to ground
 *    
 * Note:
 *    If using an 5V compliant Arduino note that there are two different logic level in your circuit now:
 *    5V for the Arduino and 3.3V for the MPR121.
 *    This means for the SDA/SCL lines you would need a bidirectional logic-level-shifter in between the Arduino and the MPR121.
 *    I would recommend to use a LLS in between for stable communication on to prevent your MPR121 from burning.
 *    Please check your data-sheet carfully which voltage is accepted for I2C communication on MPR and Arduino!
 *    Same might be for the IRQ pin!
 *    (For me it works without a LLV on an Arduino Nano but no guarantee!)
 */

#include <Adafruit_MPR121.h>
#include <Wire.h>

//define pins used
#define IRQ_PIN 3

//touch button on MPR121 channel 0
#define PIN_MPR_SENSOR 0

//GPIO in on channel 10
#define PIN_MPR_GPIO_IN 10

//GPIO out on MPR121 Channel 11
#define PIN_MPR_GPIO_OUT 11

static boolean debug = true;

Adafruit_MPR121 cap = Adafruit_MPR121();


void setup() {
  //start-Serial for output
  Serial.begin(9600);
  Serial.println(F("Starting MPR121-Arduino example...."));

  //show us which SDA/SCL pins are used
  if(debug){
    Serial.print("Using I2C Pins: SDA=");
    Serial.print(SDA);
    Serial.print(", SCL=");
    Serial.println(SCL);
  }
  
  //init MPR121 touch sensor using default SDA/SCL pins for arduino
  if (!cap.begin(0x5A)) {
    Serial.println(F("MPR121 touch sensor not found!"));
    while (true) {
      delay(100);
      Serial.print(".");
    }
  }
  Serial.println("MPR121 found");
  
  //set button debounce
  cap.setDebounce(100);

  //configure MPR121 channels
  cap.setChannelType(PIN_MPR_GPIO_OUT,MPR121_GPIO_OUT); //channel 11 is GPIO out (LED)
  cap.setChannelType(PIN_MPR_GPIO_IN,MPR121_GPIO_IN); //channel 10 is GPIO in
  
  Serial.println(F("MPR121 touch sensor initialized..."));
 

  Serial.println(F("Init complete..."));
  Serial.println(F("------------------------------"));
}


void loop() {
   
  //check if touch button was pressed
  boolean isButtonPressed = cap.touched(PIN_MPR_SENSOR);

  if(isButtonPressed & debug){
    Serial.print("Buttons: ");
    for (uint8_t channel = 0; channel <= 11; channel++) {
      //only print if the channel is of type SENSOR
      if(cap.getChannelType(channel) != MPR121_SENSOR){
        continue;
      }
      Serial.print(channel);
      Serial.print("=");
      if(cap.touched(channel)){
        Serial.print("Y");
      }else {
        Serial.print("N");
      }
      Serial.print(" | ");
    }
    Serial.println("");
  }

  //if button is pressed -> light up LED through MPR121
  cap.setGPIOEnabled(PIN_MPR_GPIO_OUT,isButtonPressed);

  //check if GPIO is enabled
  boolean gpioStatus = cap.getGPIOStatus(PIN_MPR_GPIO_IN);
  
  //if false -> error (must always be high as connected to VCC!)
  if(!gpioStatus){
    Serial.println("ERROR GPIO IS NOT HIGH! cables?");
    delay(100);
  }

}


