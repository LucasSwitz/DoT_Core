#include <Adafruit_NeoPixel.h>
#include "C:\Users\Lucas\Documents\GitHub\DoT_Core\src\IoTDevice.h"

Adafruit_NeoPixel strip = Adafruit_NeoPixel(30, 4, NEO_GRB + NEO_KHZ800);

IoTDevice::sub_desc subs[] = {IoTDevice::sub_desc("R",IoTDevice::sub_type::INT,0,(byte)255),
                              IoTDevice::sub_desc("G",IoTDevice::sub_type::INT,0,(byte)255),
                              IoTDevice::sub_desc("B",IoTDevice::sub_type::INT,0,(byte)255),
                              IoTDevice::sub_desc("Pattern",IoTDevice::sub_type::ENUM,0,(byte)3)};
                              
IoTDevice device("Light",5,subs,4);
boolean updated = false;

void setup() {
  #if defined (__AVR_ATtiny85__)
    if (F_CPU == 16000000) clock_prescale_set(clock_div_1);
  #endif
  
  device.onSend(&send);

  Serial.begin(9600);
  while (!Serial);
  
  strip.begin();
  colorWipe(strip.Color(0,128,0),5);
}

void loop() {

  delay(50);
  feedDevice();  
  
  switch(device.getSubscription("Pattern")->valAsInt())
  {
    case 0:
      if(updated)
      colorWipe(strip.Color(device.getSubscription("R")->valAsInt(),
                            device.getSubscription("G")->valAsInt(),
                            device.getSubscription("B")->valAsInt()),
                            50);
      break;    
    case 1:
      runFire();
      break;
    case 2:
      rainbowCycle(10);
      break;
     default: 
     ;
  }
}

void runFire()
{
  int r = 240 ;
  int g = 60;
  int b = 0;

  for(int x = 0; x < strip.numPixels(); x++)
  {
    int flicker = random(0,150);
    int r1 = r-flicker;
    int g1 = g-flicker;
    int b1 = b-flicker;
    if(g1<0) g1=0;
    if(r1<0) r1=0;
    if(b1<0) b1=0;
    strip.setPixelColor(x,r1,g1, b1);
}
strip.show();
delay(random(10,300));  
}

void rainbowCycle(uint8_t wait) {
  uint16_t i, j;

  for(j=0; j<256*5; j++) { // 5 cycles of all colors on wheel
    for(i=0; i< strip.numPixels(); i++) {
      strip.setPixelColor(i, Wheel(((i * 256 / strip.numPixels()) + j) & 255));
    }
    strip.show();
    delay(wait);
  }
}

uint32_t Wheel(byte WheelPos) {
  WheelPos = 255 - WheelPos;
  if(WheelPos < 85) {
    return strip.Color(255 - WheelPos * 3, 0, WheelPos * 3);
  }
  if(WheelPos < 170) {
    WheelPos -= 85;
    return strip.Color(0, WheelPos * 3, 255 - WheelPos * 3);
  }
  WheelPos -= 170;
  return strip.Color(WheelPos * 3, 255 - WheelPos * 3, 0);
}

void colorWipe(uint32_t c, uint8_t wait) {
  for(uint16_t i=0; i<strip.numPixels(); i++) {
    strip.setPixelColor(i, c);
    strip.show();
    delay(wait);
  }
}

void send(const unsigned char* data,int size)
{
  Serial.write(data,size);
  
}

void feedDevice()
{
  if (Serial.available()) {
    byte buf[256];
      int i = 0;
      while(Serial.available())
      {
        buf[i] = Serial.read();
        i++;
        if(buf[i-1] == (char)13)
        {
          device.feed(buf,i);
          updated = true;
          i = 0;
        }
      }
  }
  delay(250);
}

