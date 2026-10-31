#include "mVL35L0X.h"

mVL35L0X::mVL35L0X(uint8_t xshut_pin, uint8_t addr_sensor) : xshut_pin(xshut_pin), addr_sensor(addr_sensor) {
    distance = 8000;
    sensor = new VL53L0X;
}

mVL35L0X::~mVL35L0X() {
    delete sensor;
}

void mVL35L0X::init() {
  
  pinMode(xshut_pin, OUTPUT);
  
  digitalWrite(xshut_pin, LOW);
  delay(10);

  digitalWrite(xshut_pin, HIGH);
  delay(50);
  sensor->init(true);
  sensor->setAddress(addr_sensor);
  sensor->startContinuous();

}

void mVL35L0X::update() {
    distance = sensor->readRangeContinuousMillimeters();
}

int mVL35L0X::getData() {
    return distance;
}
