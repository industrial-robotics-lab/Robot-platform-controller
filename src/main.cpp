#include <Wire.h>
#include <iarduino_I2C_Motor.h>
#include "mVL35L0X.h"

// ===== Конфигурация платформы =====
const float WHEEL_RADIUS_MM = 33.0f;     // мм
const float TRACK_WIDTH_M   = 0.178f;    // м (для расчёта скоростей мотора)
const uint8_t ADDR_LEFT     = 0x09;      // левый мотор
const uint8_t ADDR_RIGHT    = 0x10;      // правый мотор
const unsigned long TICK_MS = 1000 / 30; // 30 Гц одометрия

// Максимальная частота применения команд управления к моторам:
const unsigned long CTRL_MS = 100;       // 100 ms = 10 Гц

// Пины для управления XSHUT каждого датчика расстояния
#define XSHUT_PIN_1 2
#define XSHUT_PIN_2 3

// Новые I2C адреса для датчиков расстояния
#define ADDR_SENSOR_1 0x30
#define ADDR_SENSOR_2 0x31

mVL35L0X* sensor1 = new mVL35L0X(XSHUT_PIN_1, ADDR_SENSOR_1);
mVL35L0X* sensor2 = new mVL35L0X(XSHUT_PIN_2, ADDR_SENSOR_2);

// ===== Объекты =====
iarduino_I2C_Motor motL(ADDR_LEFT);
iarduino_I2C_Motor motR(ADDR_RIGHT);

// ===== Состояние =====
volatile float v_cmd = 0.0f; // м/с (команда от ПК)
volatile float w_cmd = 0.0f; // рад/с

float sumAbsL_prev = 0.0f, sumAbsR_prev = 0.0f;
unsigned long lastTick = 0;

// таймер и флаг для применения команд к моторам
unsigned long lastCtrl = 0;
volatile bool need_apply_cmd = false;

// ===== CRC8 (polynomial 0x07, init 0x00) =====
uint8_t crc8_update(uint8_t crc, uint8_t data){
  crc ^= data;
  for (uint8_t i=0;i<8;i++){
    crc = (crc & 0x80) ? (uint8_t)((crc<<1) ^ 0x07) : (uint8_t)(crc<<1);
  }
  return crc;
}

// ===== Сервис =====
void initMotor(iarduino_I2C_Motor& m, const uint8_t addr){
  m.begin(&Wire);
  m.setVoltage(12);
  m.setNominalRPM(333);
  m.setFreqPWM(1000);

  // Направление вращения
  if (addr == ADDR_RIGHT) m.setInvGear(true, false);
  else                    m.setInvGear(false, true);

  m.setReducer(48);
  m.radius = WHEEL_RADIUS_MM; // мм
}

void setWheelSpeed(iarduino_I2C_Motor& m, float v_mps){
  m.setDirection(v_mps >= 0.0f);
  m.setSpeed(fabs(v_mps), MOT_M_S);
}

void applyVW(float v, float w){
  float vr = v + (w * TRACK_WIDTH_M * 0.5f);
  float vl = v - (w * TRACK_WIDTH_M * 0.5f);
  setWheelSpeed(motR, vr);
  setWheelSpeed(motL, vl);
}

void resetOdom(){
  // Сбрасываем счётчики пути моторов
  motL.delSum();
  motR.delSum();
  sumAbsL_prev = motL.getSum(MOT_MET);
  sumAbsR_prev = motR.getSum(MOT_MET);
}

// ===== Отправка пакета =====
void send_frame(uint8_t id, const uint8_t* payload, uint8_t len){
  Serial.write(0xAA);
  Serial.write(0x55);
  Serial.write(id);
  uint8_t crc = 0x00;
  crc = crc8_update(crc, id);
  for (uint8_t i=0;i<len;i++){
    Serial.write(payload[i]);
    crc = crc8_update(crc, payload[i]);
  }
  Serial.write(crc);
}

// Отправляем dL, dR и millis() (uint32_t) за текущий тик.
void send_odom(float dL, float dR){
  // Пакет: float dL, float dR, uint32_t millis
  struct __attribute__((packed)) {
    float dL;
    float dR;
  } pkt;

  pkt.dL   = dL;
  pkt.dR   = dR;

  send_frame(0x10, (uint8_t*)&pkt, sizeof(pkt)); // 12 байт payload
}

void send_dist(int d1, int d2) {
  struct __attribute__((packed)) {
    int d1;
    int d2;
  } pkt;
  
  pkt.d1 = d1;
  pkt.d2 = d2;

  send_frame(0x11, (uint8_t*)&pkt, sizeof(pkt));
}

// ===== Приём пакетов (простой парсер) =====
// Форматы ожидаем:
// [AA 55] 01 <8байт(v,w)> CRC
// [AA 55] 02 CRC
enum RxState { RX_SYNC1, RX_SYNC2, RX_ID, RX_PAYLOAD, RX_CRC };
RxState rx_state = RX_SYNC1;
uint8_t rx_id = 0;
uint8_t rx_buf[8]; // максимум 8 байт (v,w)
uint8_t rx_len = 0; // ожидаемая длина
uint8_t rx_pos = 0;
uint8_t rx_crc = 0;

void process_cmd(uint8_t id, const uint8_t* data, uint8_t len){
  if (id == 0x02 && len == 0){
    // RST: обнуляем команды и одометрию
    v_cmd = 0.0f; 
    w_cmd = 0.0f; 
    resetOdom();
    // применим остановку при следующем окне CTRL_MS
    need_apply_cmd = true;
  } else if (id == 0x01 && len == 8){
    // команда v,w
    float v, w;
    memcpy(&v, data+0, 4);
    memcpy(&w, data+4, 4);
    v_cmd = v; 
    w_cmd = w;
    // отметим, что нужно применить её (но не чаще 10 Гц)
    need_apply_cmd = true;
  }
}

void poll_serial(){
  while (Serial.available()){
    uint8_t b = (uint8_t)Serial.read();
    switch (rx_state){
      case RX_SYNC1:
        if (b == 0xAA) rx_state = RX_SYNC2;
        break;
      case RX_SYNC2:
        if (b == 0x55) rx_state = RX_ID;
        else           rx_state = RX_SYNC1;
        break;
      case RX_ID:
        rx_id = b;
        rx_crc = crc8_update(0x00, rx_id);
        // определить ожидаемую длину по ID
        if (rx_id == 0x01) {          // команда v,w
          rx_len = 8; 
          rx_pos = 0; 
          rx_state = RX_PAYLOAD;
        }
        else if (rx_id == 0x02) {     // RST без payload
          rx_len = 0; 
          rx_pos = 0; 
          rx_state = RX_CRC;
        }
        else {
          // неизвестный ID — сброс
          rx_state = RX_SYNC1;
        }
        break;
      case RX_PAYLOAD:
        rx_buf[rx_pos++] = b;
        rx_crc = crc8_update(rx_crc, b);
        if (rx_pos >= rx_len) rx_state = RX_CRC;
        break;
      case RX_CRC:
        if (b == rx_crc){
          process_cmd(rx_id, rx_buf, rx_len);
        }
        // вне зависимости от результата — начать поиск заново
        rx_state = RX_SYNC1;
        break;
    }
  }
}

void setup(){
  Serial.begin(115200);
  Wire.begin();
  initMotor(motL, ADDR_LEFT);
  initMotor(motR, ADDR_RIGHT);
  resetOdom();
  unsigned long now = millis();
  lastTick = now;
  lastCtrl = now;
  sensor1->init();
  sensor2->init();
}

void loop(){
  unsigned long now = millis();

  // 1) 30 Гц — отправка СЫРЫХ приращений пути по колёсам
  if (now - lastTick >= TICK_MS){
    sensor1->update();
    sensor2->update();
  
    lastTick += TICK_MS;

    float sumL = motL.getSum(MOT_MET);
    float sumR = motR.getSum(MOT_MET);

    float dL_abs = sumL - sumAbsL_prev;
    float dR_abs = sumR - sumAbsR_prev;

    sumAbsL_prev = sumL;
    sumAbsR_prev = sumR;

    // добавляем знак по направлению вращения
    float dL = motL.getDirection() ? (+dL_abs) : (-dL_abs);
    float dR = motR.getDirection() ? (+dR_abs) : (-dR_abs);

    // шлём dL, dR + millis()
    send_odom(dL, dR);

    send_dist(sensor1->getData(), sensor2->getData());
  }

  // 2) Применяем управление НЕ ЧАЩЕ 10 Гц
  if (need_apply_cmd && (now - lastCtrl >= CTRL_MS)){
    lastCtrl += CTRL_MS;
    applyVW(v_cmd, w_cmd);
    need_apply_cmd = false;  // ждём следующую новую команду
  }

  // 3) Принимаем команды бинарного протокола
  poll_serial();
}
