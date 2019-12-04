/*
  d2 -
  d3 - Relay2 (вентилятор)
  d4 - Relay1 (свет туалет)
  d5 - DHT
  d6 - MotionSensor
  d7 - BTN (вентилятор)
  d8 - BTN (свет туалет)
  d9 -
  d10 - nrf
  d11 - nrf
  d12 -
  d13 -
  A2 - Door
*/

// Флаги конфигурации
#define MY_TRANSPORT_WAIT_READY_MS 1000 // Запуск без сети MyS
#define MY_RADIO_RF24                   // Радио
//#define MY_DEBUG                      // Дебаг
#define MY_NODE_ID 1                  // Номер ноды

#include <MySensors.h>       // Подключаем библиотеку
#include <GyverButton.h>    //библиотека кнопки
#include <GyverTimer.h>     //библиотека таймер
#include <DHT.h>            //библиотека DHT датчик

#define BTN_PIN 8 //Пин кнопки1
#define DHT_DATA_PIN 5 //Пин датчика температуры-влажности
#define RELAY1_PIN 4 //Пин Реле1
#define RELAY2_PIN 3 //Пин Реле2
#define MOTION_PIN 6 //Пин MotionSensor
#define DOOR_PIN A2 //Пин Дверь

#define SENSOR_TEMP_OFFSET 0 //смещение температуры (если нужно)

// Время сна между измерениями (в milliseconds)
// Должно быть >1000ms для DHT22 и > 2000ms для DHT11
static const uint64_t UPDATE_INTERVAL = 5000;

// Force sending an update of the temperature after n sensor reads, so a controller showing the
// timestamp of the last update doesn't show something like 3 hours in the unlikely case, that
// the value didn't change since;
// i.e. the sensor would force sending an update every UPDATE_INTERVAL*FORCE_UPDATE_N_READS [ms]
static const uint8_t FORCE_UPDATE_N_READS = 10;

#define CHILD_ID_Relay1 001
#define CHILD_ID_Relay2 002
#define CHILD_ID_HUM 003
#define CHILD_ID_TEMP 004

float lastTemp;
float lastHum;
uint8_t nNoUpdatesTemp;
uint8_t nNoUpdatesHum;
bool metric = true;
bool state1 = 0;             // Объявляем переменные
bool state2 = 0;             // статус1 и статус2
int temp = 0;               //переменная для цикла while
int stateMotion = 0;

MyMessage msg1(CHILD_ID_Relay1, V_STATUS); // Создаём контейнеры
//MyMessage msg2(1, V_STATUS);
MyMessage msgStatus(1, V_VAR1);
MyMessage msgHum(CHILD_ID_HUM, V_HUM);
MyMessage msgTemp(CHILD_ID_TEMP, V_TEMP);

DHT dht;
GButton butt1(BTN_PIN);
GButton door(DOOR_PIN);
GTimer myTimer(MS, UPDATE_INTERVAL); //создать мс. интервальный таймер для DHT
GTimer myTimer2(MS); //создать таймер мс. для MotionSensor door
GTimer myTimer3(MS);

//------------------Функция Температура влажность----------------------------------------
void dhtSensor() {

  // Force reading sensor, so it works also after sleep()
  dht.readSensor(true);
  // Get temperature from DHT library
  float temperature = dht.getTemperature();
  if (isnan(temperature)) {
    Serial.println("Failed reading temperature from DHT!");
  } else if (temperature != lastTemp || nNoUpdatesTemp == FORCE_UPDATE_N_READS) {
    // Only send temperature if it changed since the last measurement or if we didn't send an update for n times
    lastTemp = temperature;
    // apply the offset before converting to something different than Celsius degrees
    temperature += SENSOR_TEMP_OFFSET;
    if (!metric) {
      temperature = dht.toFahrenheit(temperature);
    }
    // Reset no updates counter
    nNoUpdatesTemp = 0;
    send(msgTemp.set(temperature, 1));
#ifdef MY_DEBUG
    Serial.print("T: ");
    Serial.println(temperature);
#endif
  } else {
    // Increase no update counter if the temperature stayed the same
    nNoUpdatesTemp++;
  }
  // Get humidity from DHT library
  float humidity = dht.getHumidity();
  if (isnan(humidity)) {
    Serial.println("Failed reading humidity from DHT");
  } else if (humidity != lastHum || nNoUpdatesHum == FORCE_UPDATE_N_READS) {
    // Only send humidity if it changed since the last measurement or if we didn't send an update for n times
    lastHum = humidity;
    // Reset no updates counter
    nNoUpdatesHum = 0;
    send(msgHum.set(humidity, 1));
#ifdef MY_DEBUG
    Serial.print("H: ");
    Serial.println(humidity);
#endif
  } else {
    // Increase no update counter if the humidity stayed the same
    nNoUpdatesHum++;
  }
}
// -----------------------------------------------------------------------------------------
void presentation()
{
  sendSketchInfo("Relay+Button+TemperatureAndHumidity", "1.0");
  present(CHILD_ID_Relay1, S_BINARY, "Relay1");
  present(CHILD_ID_Relay2, S_BINARY, "Relay2");
  present(CHILD_ID_HUM, S_HUM);
  present(CHILD_ID_TEMP, S_TEMP);
  metric = getControllerConfig().isMetric;
}
void setup()
{
  butt1.setDebounce(50);        // настройка антидребезга (по умолчанию 80 мс)
  door.setDebounce(50);
  //  pinMode(DOOR_PIN, INPUT_PULLUP); //Пин кнопка двери
  pinMode(MOTION_PIN, INPUT);  //Пин вход MotionSensor
  pinMode(RELAY1_PIN, OUTPUT);       // Пин выход (Реле1)
  pinMode(RELAY2_PIN, OUTPUT);       // Пин выход (Реле2)
  dht.setup(DHT_DATA_PIN); // set data pin of DHT sensor
  if (UPDATE_INTERVAL <= dht.getMinimumSamplingPeriod()) {
    Serial.println("Warning: UPDATE_INTERVAL is smaller than supported by the sensor!");
  }
  // Sleep for the time of the minimum sampling period to give the sensor time to power up
  // (otherwise, timeout errors might occure for the first reading)
  //  sleep(dht.getMinimumSamplingPeriod());
}

void loop() {

  butt1.tick();
  door.tick();
  //-------------запуск функции датчика темп-влажности в ванной------
  if (myTimer.isReady()) {
    dhtSensor();
  }
  //-------------обработка кнопки включения света в туалете----------------------
  if (butt1.isSingle()) {        // Если нажата кнопка1
    state1 = !(state1);           // Инвертируем переменную
    send(msg1.set(state1));       // Отправляем состояние реле1 на контроллер
  }
  //------------------Дабл клик------------------------------------------------------

  if (butt1.isDouble()) {
    Serial.println("Double click");
  }
  //------------------Обработка датчика двери--------------------------------
  if (door.isRelease()) {                             //Если дверь открыта и свет не горит, включаем
    if (state1 == 0) state1 = (!state1);
  }
    
  //-------------------Если дверь закрыта и свет горит, через 1с запускаем датчик движения
//-------------------в течении 3с проверяем есть ли кто внутри, если нет выключаем свет
  if (door.isPress() && (state1 == 1)) {              
    temp = 0;                                         
    myTimer2.setTimeout(1000);                        //включаем режим таймера таймаут и устанавливаем через сколько запускать датчик движения
  }
  if (myTimer2.isReady()) {                           //Запускаем таймер и через 1с включаем режим и время второго таймера
    stateMotion = 0;
    myTimer3.setTimeout(3000);                        
    while (temp == 0) {                               //Записывем в переменную данные от датчика движения
      stateMotion += digitalRead(MOTION_PIN);
      Serial.println(stateMotion);
      if (myTimer3.isReady()) {                      //Запускаем второй таймер
        temp = 1;
        if (stateMotion == 0) state1 = (!state1);    //через 3с проверяем переменную с данными от датчика если 0 то выключаем свет
      }
    }
  }
  //----------------------END---------------------------------------
  digitalWrite(RELAY1_PIN, state1);      // Записываем значение переменных
  //  digitalWrite(RELAY1_PIN, state2);      // Записываем значение переменных
}
//---------------------------Функция обработка вход. сообщений--------------------------------------------------
void receive(const MyMessage &message) {
  // Если пришло сообщение для Relay1 в ответ на запрос от GW, отправляем сообщение с текущем статусом реле1
  if ((message.type ==  V_VAR1) && (message.sensor == CHILD_ID_Relay1)) send(msg1.set(state1));
  // Если пришло сообщение для сенсора 1, записываем его в переменную state1
  if ((message.type ==  V_STATUS) && (message.sensor == CHILD_ID_Relay1)) state1 = message.getBool();
}
