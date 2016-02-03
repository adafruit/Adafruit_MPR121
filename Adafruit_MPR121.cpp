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
* This method is used to set the global debounce timeout for all channels based on time.
* Note: this is not the chip-internal debounce! You might want to use setInternalDebounce()!
* Debounce is used to avoid double processing of a single touch. 
*/
void Adafruit_MPR121::setDebounce(uint16_t debounce) {
  _debounce = debounce;
}

/*
* This method is used to set a channel to a specific type;
* used as touch sensor or LED controller.
* Returns true on success
*/
boolean Adafruit_MPR121::setChannelType(uint8_t channelID, uint8_t type) {
  //first 4 (0-3) channels can not be set to LED mode
  if(type != MPR121_SENSOR && channelID <= 3){
    return false;
  }
  
  if(type >= 3){
    //unknown type
    return false;
  }
  _channels[channelID]._type = type;
  
  
  
  //set GPIO enabled (remember: sensor register have higher priority!)
  writeRegister(MPR121_GPIOEN, 0xFF);
  writeRegister(MPR121_GPIODIR, 0xFF);      // 0x76 is GPIO Dir
  
  //apply config for the channel
  Channel channel = _channels[channelID];
    
  //get current status of GPIO enabled register
  uint8_t gpioEnabled = readRegister8(MPR121_GPIOEN);
    
  if(channel._type == MPR121_SENSOR){
      
    //disable GPIO
    gpioEnabled ^= (-0 ^ gpioEnabled) & (1 << (channelID-4));
    writeRegister(MPR121_GPIOEN,gpioEnabled);
  } else {      
    //enable GPIO
    gpioEnabled ^= (-1 ^ gpioEnabled) & (1 << (channelID-4));
    writeRegister(MPR121_GPIOEN,gpioEnabled);
      
    //set direction
    uint8_t direction = readRegister8(MPR121_GPIODIR);
    if(channel._type == MPR121_GPIO_OUT){
      direction ^= (-1 ^ direction) & (1 << (channelID-4));
    }else if(channel._type == MPR121_GPIO_IN){
      direction ^= (-0 ^ direction) & (1 << (channelID-4));
    }
    writeRegister(MPR121_GPIODIR,direction);
  }
  
  //enable / disable electrodes in ECR register @ 0x5E
  int maxElectrodeID = 0;
  for (uint8_t i=0; i<12; i++) {
    if (_channels[i]._type == MPR121_SENSOR){
      maxElectrodeID = i;
    }else if( (i-1) > maxElectrodeID){
      //ok, something is wrong -> all electrodes must be at the lower channels
      //mixing is not possible! Seet datasheet for ECR register description
      //lower GPIO pins will not work!
    }
  }
  
  //see datasheet: enables electrodes 0 - channelID for touch
  writeRegister(MPR121_ECR,maxElectrodeID);
  
  writeRegister(MPR121_GPIOCLR, 0xFF);    // GPIO Data Clear
}

/*
* This method can be used to enable internal pullup resistor on a specific channel.
* The channel must be set as GPIO_IN.
* returns true on success, false otherwise
*/
boolean Adafruit_MPR121::enablePullUp(uint8_t channelID){
  if(_channels[channelID]._type != MPR121_GPIO_IN){
    return false;
  }
  //get control register
  uint8_t ctrl0 = readRegister8(MPR121_GPIOCTL0);
  uint8_t ctrl1 = readRegister8(MPR121_GPIOCTL1);
  
  //pull enabled: ctrl0 = 1; ctrl1 = 1
  ctrl0 ^= (-1 ^ ctrl0) & (1 << (channelID-4));
  ctrl1 ^= (-1 ^ ctrl1) & (1 << (channelID-4));
  
  //write back to chip
  writeRegister(MPR121_GPIOCTL0, ctrl0);
  writeRegister(MPR121_GPIOCTL1, ctrl1);
  
  //TODO clear all?
  //writeRegister(MPR121_GPIOCLR, 0xFF);
  
  return true;
}

/*
* This method can be used to enable internal pulldown resistor on a specific channel.
* The channel must be set as GPIO_IN.
* returns true on success, false otherwise
*/
boolean Adafruit_MPR121::enablePullDown(uint8_t channelID){
  if(_channels[channelID]._type != MPR121_GPIO_IN){
    return false;
  }
  //get control register
  uint8_t ctrl0 = readRegister8(MPR121_GPIOCTL0);
  uint8_t ctrl1 = readRegister8(MPR121_GPIOCTL1);
  
  //pull enabled: ctrl0 = 1; ctrl1 = 0
  ctrl0 ^= (-1 ^ ctrl0) & (1 << (channelID-4));
  ctrl1 &= ~(1 << (channelID-4));
  
  //write back to chip
  writeRegister(MPR121_GPIOCTL0, ctrl0);
  writeRegister(MPR121_GPIOCTL1, ctrl1);
  
  //TODO clear all?
  //writeRegister(MPR121_GPIOCLR, 0xFF);
  
  return true;  
} 

/*
* returns the GPIO status of the specific channel;
* channel 0 -  3 always return false -> sensor only
* channel 4 - 11:
    MPR121_SENSOR   -> return false
*   MPR121_GPIO_IN  -> read from chip and returns status
*   MPR121_GPIO_OUT -> get status from channel if LED if enabled
*/
boolean Adafruit_MPR121::getGPIOStatus(uint8_t channelID){
    if(channelID <= 3 || _channels[channelID]._type == MPR121_SENSOR){
      return false;
    }else if(_channels[channelID]._type == MPR121_GPIO_OUT){
      return _channels[channelID]._gpioHigh;
    }else{
      // MPR121_GPIO_IN -> read current status from chip
      uint8_t status = readRegister8(MPR121_GPIODATA);
      _channels[channelID]._gpioHigh = (status >> (channelID-4)) & 1;
      return _channels[channelID]._gpioHigh;
    }
}

/*
* This function is used to enabled/disable (set high/low) the GPIO port on a specific channel.
* If the channel is configured as MPR121_SENSOR this method is ignored.
*/
void Adafruit_MPR121::setGPIOEnabled(uint8_t channelID, boolean enable){
  if(_channels[channelID]._type == MPR121_SENSOR){
    return;
  }
  
  uint8_t status = readRegister8(MPR121_GPIOSET);
  if(enable){   
    status ^= (-1 ^ status) & (1 << (channelID-4));
    writeRegister(MPR121_GPIOSET,status);  
  }else{
    status ^= (-1 ^ status) & (1 << (channelID-4)); //set n.th bit to 0
    writeRegister(MPR121_GPIOCLR,status);
  }
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

void Adafruit_MPR121::useIRQ(boolean value){
  _useIRQ = value;
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
  //if channel is used as MPR121_GPIO_OUT it is never touched
  if(_channels[channel]._type == MPR121_GPIO_OUT) {
    //Serial.println("MPR121: channel-type=GPIO_OUT -> touched false");
    return false;
  }
  
  //if IRQ is used -> check irq status first
  if(_useIRQ && !_interrupted){
    //Serial.println("MPR121: irq=true , !interrupted");
    return false;
  }
  
  // check debounce
  if((_channels[channel]._lastTouch + _debounce) > millis()) {
    return false;
  }
  
  // check touched
  uint16_t touched = this->touched();
  for (uint8_t i=0; i<12; i++){
    if (touched & _BV(channel)){
      //reset debounce
      _channels[channel]._lastTouch = millis();
      
      //set touch state
      _channels[channel]._touched = true;
      
      //reset interrupt
      _interrupted = false;
    }else{
      //not touched//set touch state
      _channels[channel]._touched = false;
    }
  }
  return _channels[channel]._touched;
}

uint16_t  Adafruit_MPR121::touched(void) {
  uint16_t result = 0;
  //if interrupt is used then return IRQ status
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
