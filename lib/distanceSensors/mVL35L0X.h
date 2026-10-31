#pragma once

#include <Wire.h>
#include <VL53L0X.h>

class mVL35L0X
{
private:
    uint8_t xshut_pin, addr_sensor;
    VL53L0X *sensor;
    int distance;
public:
    mVL35L0X(uint8_t xshut_pin, uint8_t addr_sensor);
    ~mVL35L0X();

    void init();
    void update();
    int getData();

};
