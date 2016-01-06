/*************************************************** 
  This is a library for the MPR121 I2C 12-chan Capacitive Sensor

  Designed specifically to work with the MPR121 sensor from Adafruit
  ----> https://www.adafruit.com/products/1982

  These sensors use I2C to communicate, 2+ pins are required to  
  interface
  Adafruit invests time and resources providing this open source code, 
  please support Adafruit and open-source hardware by purchasing 
  products from Adafruit!

  Written by Limor Fried/Ladyada for Adafruit Industries.  
  BSD license, all text above must be included in any redistribution
 ****************************************************/

#include "Adafruit_MPR121.h"

Adafruit_MPR121::Adafruit_MPR121() {
}

boolean Adafruit_MPR121::begin(uint8_t i2caddr) {
  Wire.begin();
    
  _i2caddr = i2caddr;

  return initMPR121();
}

boolean Adafruit_MPR121::begin(uint8_t i2caddr, uint8_t sdaPin, uint8_t sclPin) {
  Wire.begin(sdaPin, sclPin);
    
  _i2caddr = i2caddr;

  return initMPR121();
}

boolean Adafruit_MPR121::initMPR121(){
  //set default; automatically enabled when irq pin is set
  _useIRQ = false;
  
  // soft reset
  writeRegister(MPR121_SOFTRESET, 0x63);
  delay(1);
  for (uint8_t i=0; i<0x7F; i++) {
  //  Serial.print("$"); Serial.print(i, HEX); 
  //  Serial.print(": 0x"); Serial.println(readRegister8(i));
  }
  

  writeRegister(MPR121_ECR, 0x0);

  uint8_t c = readRegister8(MPR121_CONFIG2);
  
  if (c != 0x24) return false;


  setThresholds(12, 6);
  writeRegister(MPR121_MHDR, 0x01);
  writeRegister(MPR121_NHDR, 0x01);
  writeRegister(MPR121_NCLR, 0x0E);
  writeRegister(MPR121_FDLR, 0x00);

  writeRegister(MPR121_MHDF, 0x01);
  writeRegister(MPR121_NHDF, 0x05);
  writeRegister(MPR121_NCLF, 0x01);
  writeRegister(MPR121_FDLF, 0x00);

  writeRegister(MPR121_NHDT, 0x00);
  writeRegister(MPR121_NCLT, 0x00);
  writeRegister(MPR121_FDLT, 0x00);

  writeRegister(MPR121_DEBOUNCE, 0);
  writeRegister(MPR121_CONFIG1, 0x10); // default, 16uA charge current
  writeRegister(MPR121_CONFIG2, 0x20); // 0.5uS encoding, 1ms period

//  writeRegister(MPR121_AUTOCONFIG0, 0x8F);

//  writeRegister(MPR121_UPLIMIT, 150);
//  writeRegister(MPR121_TARGETLIMIT, 100); // should be ~400 (100 shifted)
//  writeRegister(MPR121_LOWLIMIT, 50);
  // enable all electrodes
  writeRegister(MPR121_ECR, 0x8F);  // start with first 5 bits of baseline tracking
  return true;
}

/*
* This method is used to set the global debounce timeout for all channels.
* Debounce is used to avoid double processing of a single touch.
*
* Setting the global debounce will ovveride debounce timeout set for a specific channel. 
*/
void Adafruit_MPR121::setDebounce(uint16_t debounce) {
  for (int i = 0; i < 11; i++) {
    setDebounce(debounce, i);
  }
}

/*
* This method is used to set the debounce timeout for a specific channel.
* Debounce is used to avoid double processing of a single touch.
*/
void Adafruit_MPR121::setDebounce(uint16_t debounce, uint8_t channel) {
  _channels[channel]._debounce = debounce;
}

/*
* This method is used to set a channel to a specific type;
* used as touch sensor or LED controller.
* Returns true on success
*/
boolean Adafruit_MPR121::setChannelType(uint8_t channel, uint8_t type) {
  //first 4 (0-3) channels can not be set to LED mode
  if(type == MPR121_LED && channel <= 3){
    return false;
  }
  _channels[channel]._type = type;
  
  //set GPIO enabled (remember: sensor register have higher priority!)
  writeRegister(MPR121_GPIOEN, 0xFF);
  writeRegister(MPR121_GPIODIR, 0xFF);      // 0x76 is GPIO Dir
  
  //apply config for each channel
  for (uint8_t i = 0; i < 11; i++) {
    Channel channel = _channels[i];
    if(channel._type == MPR121_SENSOR){
      //disable LED
      //set as sensor
    } else {
      //enable LED
      //disable sensor
    
    }
  }
  writeRegister(MPR121_GPIOCLR, 0xFF);    // GPIO Data Clear
}


void Adafruit_MPR121::setThreshholds(uint8_t touch, uint8_t release) {
  setThresholds(touch, release);
}

void Adafruit_MPR121::setThresholds(uint8_t touch, uint8_t release) {
  for (uint8_t i=0; i<12; i++) {
    writeRegister(MPR121_TOUCHTH_0 + 2*i, touch);
    writeRegister(MPR121_RELEASETH_0 + 2*i, release);
  }
}

void Adafruit_MPR121::useIRQ(){
  _useIRQ = true;
}

void Adafruit_MPR121::fireIRQ(){
  _interrupted = true;
}

uint16_t  Adafruit_MPR121::filteredData(uint8_t t) {
  if (t > 12) return 0;
  return readRegister16(MPR121_FILTDATA_0L + t*2);
}

uint16_t  Adafruit_MPR121::baselineData(uint8_t t) {
  if (t > 12) return 0;
  uint16_t bl = readRegister8(MPR121_BASELINE_0 + t);
  return (bl << 2);
}

/*
* Checks the touch status for a single channel including debounce
*/
boolean Adafruit_MPR121::touched(uint8_t channel){
  //if channel is used as LED it is never touched
  if(_channels[channel]._type == MPR121_LED) {
    return false;
  }
  
  // check debounce
  if((_channels[channel]._lastTouch + _channels[channel]._debounce) < millis()) {
    return false;
  }
  
  // check touched
  if(touched() & _BV(channel)){
    //reset debounce
    _channels[channel]._lastTouch = millis();
    return true;
  }
  return false;
}

uint16_t  Adafruit_MPR121::touched(void) {
  uint16_t result = 0;
  //if interrupt / IRQ is used then return IRQ status
  if(_useIRQ){
    result = _interrupted ? 1 : 0;
  }else{ 
    //else read status from MPR121 registers
    result = readRegister16(MPR121_TOUCHSTATUS_L) & 0x0FFF;
  }
  
  //reset IRQ status for next loop
  _interrupted = false;
  return result;
}

/*********************************************************************/


uint8_t Adafruit_MPR121::readRegister8(uint8_t reg) {
    Wire.beginTransmission(_i2caddr);
    Wire.write(reg);
    Wire.endTransmission(false);
    while (Wire.requestFrom(_i2caddr, 1) != 1);
    return ( Wire.read());
}

uint16_t Adafruit_MPR121::readRegister16(uint8_t reg) {
    Wire.beginTransmission(_i2caddr);
    Wire.write(reg);
    Wire.endTransmission(false);
    while (Wire.requestFrom(_i2caddr, 2) != 2);
    uint16_t v = Wire.read();
    v |=  ((uint16_t) Wire.read()) << 8;
    return v;
}

/**************************************************************************/
/*!
    @brief  Writes 8-bits to the specified destination register
*/
/**************************************************************************/
void Adafruit_MPR121::writeRegister(uint8_t reg, uint8_t value) {
    Wire.beginTransmission(_i2caddr);
    Wire.write((uint8_t)reg);
    Wire.write((uint8_t)(value));
    Wire.endTransmission();
}
