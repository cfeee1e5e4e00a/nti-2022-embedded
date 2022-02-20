// #include <Arduino.h>
// #include "HardwareSerial.h"

// Aa1111111111

#include <ArduinoJson.h>
#include <cstdint>
#include "art.h"
#include "pins.h"
// #include <TroykaDHT.h>
#include <DHT.h>

#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <HX711.h>
#include <Adafruit_PN532.h>

#include <TroykaCurrent.h>
#include <Servo.h>



#define SCREEN_WIDTH 128 // OLED display width, in pixels
#define SCREEN_HEIGHT 64 // OLED display height, in pixels

#define SCREEN_ADDRESS 0x3d ///< See datasheet for Address; 0x3D for 128x64, 0x3C for 128x32

// #define DEBUG Serial3

// minimum interval between packets
#define MIN_INTERVAL 333
// it will retry every ACK_WAIT +- MIN_INTERVAL
#define ACK_WAIT 2000


#define VARS \
    X(int, pot) \
    X(bool, motion) \
    X(float, temperature) \
    X(float, humidity) \
    X(int, lighting) \
    X(float, weight) \
    X(uint64_t, rfid) \
    X(float, heater_power) \
    X(bool, heater_enabled) \
    X(bool, cooler_enabled) \
    X(int, target_light) \
    X(float, cool_temp) \
    X(float, heat_temp) \
    X(bool, door_opened) \
    X(int, window) \
    X(bool, alarm) \
    X(bool, alarm_enabled)


#define X(type, name) type name;
typedef struct{
    VARS
} vars_t;
#undef X


typedef struct{
    uint32_t packet_id;
    bool was_ack;
}var_metadata_t;

#define X(type, name) var_metadata_t name;
struct{
    VARS
}stateMetadata;
#undef X



vars_t old_state, state;

bool wereChanges(){
    #define X(type, name) if(state.name != old_state.name)return true;
    VARS
    #undef X
    return false;
}

StaticJsonDocument<1024> recvDoc;
StaticJsonDocument<1024> doc;


void sendJson(JsonDocument &doc){
    Serial.write(0xff);
    Serial.println(measureJson(doc));
    serializeJson(doc, Serial);
}

void sender(bool sendAll = false){
    static uint32_t prevTime;
    static uint32_t currentPacket = 0;

    uint32_t time = millis();

    bool needSend = wereChanges();

    #define X(type, name) needSend |= !stateMetadata.name.was_ack && (time - stateMetadata.name.packet_id > ACK_WAIT);
    VARS
    #undef X

    needSend &= (time - prevTime > MIN_INTERVAL);

    needSend |= sendAll;

    if(!needSend)return;

    prevTime = time;

    doc.clear();

    #define X(type, name) if(sendAll || state.name != old_state.name || (!stateMetadata.name.was_ack && (time - stateMetadata.name.packet_id > ACK_WAIT))){ \
        old_state.name = state.name; \
        stateMetadata.name.packet_id = time; \
        stateMetadata.name.was_ack = false; \
        doc[#name] = state.name; \
    }
    VARS
    #undef X

    doc["__ack__"] = time;

    
    sendJson(doc);
}



void handleInput();






void serialEvent() {
    // DEBUG.println("EVENT");
    while(Serial.read() != 0xff){
        if(!Serial.available())return;
    }
    

    int sz = Serial.parseInt();
    auto err = deserializeJson(recvDoc, Serial);
    if(err) {
        // DEBUG.print("Json parsing error: ");
        // DEBUG.println(err.f_str());
    } else {
        // DEBUG.println("Json parsed successfully");
    }


    if(recvDoc.containsKey("__ack__")){
        uint32_t packet_id = doc["__ack__"];
        // DEBUG.print("Ack = ");
        // DEBUG.println(packet_id);
        #define X(type, name) if(stateMetadata.name.packet_id == packet_id)stateMetadata.name.was_ack = true; 
        VARS
        #undef X
    }

    if(recvDoc.containsKey("__inack__")){
        doc.clear();
        doc["__inack__"] = recvDoc["__inack__"].as<int>();
        sendJson(doc);
    }

    // here goes input handling
    if(recvDoc.containsKey("__all__")){
        // DEBUG.println("Sending all");
        sender(true);
    }
    // if(recvDoc.containsKey("led")) state.led = recvDoc["led"].as<bool>();
    handleInput();
    
} 

TwoWire lcdWire;
DHT dht(DHT_PIN, DHT11);
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &lcdWire);
HX711 loadcell;

// we do not actually have reset
Adafruit_PN532 nfc(RFID_IRQ, PC7);
const int DELAY_BETWEEN_CARDS = 500;
long timeLastCardRead = 0;
boolean readerDisabled = false;
int irqCurr;
int irqPrev;
void startListeningToNFC() {
  irqPrev = irqCurr = HIGH;
  nfc.startPassiveTargetIDDetection(PN532_MIFARE_ISO14443A);
}

ACS712 sensorCurrent(CURRENT_PIN);

Servo door;
Servo window;

void setup() {   
    Serial.begin(115200);
    // User code here

    pinMode(PA0, INPUT);
    pinMode(MOVEMENT_PIN, INPUT);
    dht.begin();
    pinMode(LIGHT_PIN, INPUT);

    Wire.setSDA(PB9);
    Wire.setSCL(PB8);
    Wire.begin();

    lcdWire.setSDA(RFID_DATA);
    lcdWire.setSCL(RFID_CLK);
    lcdWire.begin();

    display.begin(SSD1306_SWITCHCAPVCC, 0x3c);
    display.clearDisplay();
    display.display();

    loadcell.begin(SCALE_DATA, SCALE_CLOCK);
    loadcell.get_units(10);
    loadcell.set_scale(10);
    loadcell.tare();

    nfc.SAMConfig();
    startListeningToNFC();

    pinMode(HEATER_PIN, OUTPUT);

    //defaults
    state.target_light = 500;
    state.cool_temp = 100;
    state.heat_temp = 1;
    pinMode(COOLER_PIN, OUTPUT);

    pinMode(DISP_BTN_PIN, INPUT);
    pinMode(FACE_BTN_PIN, INPUT);

    door.attach(DOOR_PIN);
    window.attach(WINDOW_PIN);

    state.alarm_enabled = true;
    state.alarm = false;

}

String dsp_msg = "";
unsigned long set_msg_time = 0;

void writeServo(Servo &servo, int value){
    if(value != servo.read()){
        servo.write(value);
    }
}

void loop() {
    sender();
    // User code here

    state.pot = digitalRead(PA0);

    // motion
    state.motion = digitalRead(MOVEMENT_PIN);

    // dht
    state.humidity = dht.readHumidity();
    state.temperature = dht.readTemperature();

    // light
    state.lighting = 1024 - analogRead(LIGHT_PIN);


    // display
    display.clearDisplay();

    display.setTextSize(1);      // Normal 1:1 pixel scale
    display.setTextColor(SSD1306_WHITE); // Draw white text
    display.setCursor(0, 0);     // Start at top-left corner
    display.cp437(true);
    if(millis() - set_msg_time > 1000){
        if(digitalRead(DISP_BTN_PIN)){
            display.print("Temp: ");
            display.print(state.temperature);
            display.println(" c");
            display.println(String("Hum: ") + state.humidity);
            display.println(String("Light: ") + state.lighting);
            display.println(String("Cooler:") + (state.cooler_enabled ? "on" : "off"));
            display.println(String("Heater:") + (state.heater_enabled ? "on" : "off"));
            display.println(String("Power ") + state.heater_power + "Wt*h");
            if(state.alarm_enabled){
                if(state.alarm){
                    display.println("!ALARM!");
                }else {
                    display.println("Alarm enabled");
                }
            }else{
                display.println("Alarm disabled");
            }
        }else{
            display.println(String("Target light: ") + state.target_light);
            display.println(String("Cool temp: ") + state.cool_temp);
            display.println(String("Heat temp: ") + state.heat_temp);
        }
    }else {
        display.println(dsp_msg);
    }
    
    
    display.display();
    // TODO: target temp
    

    // scale
    // TODO: to this every second
    if (loadcell.wait_ready_timeout(1)) {
        state.weight = abs(loadcell.get_units(4));
    }

    // RFID
    if (readerDisabled) {
        if (millis() - timeLastCardRead > DELAY_BETWEEN_CARDS) {
            readerDisabled = false;
            startListeningToNFC();
            
        }
    } else {
        irqCurr = digitalRead(RFID_IRQ);
        if (irqCurr == LOW && irqPrev == HIGH) {
            uint8_t success;
            uint8_t uidLength;          
            uint64_t uid = 0;  
            success = nfc.readPassiveTargetID(PN532_MIFARE_ISO14443A, (uint8_t*)&uid, &uidLength, 1000);
            if(success){
                state.rfid = uid;
                timeLastCardRead = millis();
            }
            readerDisabled = true;
        }else{
            state.rfid = 0;
        }
    
        irqPrev = irqCurr;
    }

    // cooler, heater and current
    float heater_watts;
    state.heater_enabled = state.temperature < state.heat_temp;
    state.cooler_enabled = state.temperature > state.cool_temp;
    digitalWrite(HEATER_PIN, state.heater_enabled);
    analogWrite(COOLER_PIN, state.cooler_enabled ? 0 : 255);
    if(state.heater_enabled){
        heater_watts = sensorCurrent.readCurrentDC()/*amps*/ * 12/*volts*/;
    }else {
        heater_watts = 0;
    }
    static unsigned long last_measurement = 0;
    if(last_measurement == 0){
        state.heater_power = 0;
        last_measurement = millis();
    }else {
        unsigned long current_measurement = millis();
        float delta = current_measurement - last_measurement;
        last_measurement = current_measurement;
        state.heater_power += (delta/1000.0/60.0/60.0)/*hours*/ * heater_watts;
    }



    // TODO: ENABLE THIS
    // // door
    writeServo(door, state.door_opened ? 120 : 170);
    // // door.write(state.target_light);
    // // door.write(state.target_light);



    // window
    const int win_min = 10;
    const int win_wax = 50;
    static int win_cur = win_min;
    if(abs(state.lighting - state.target_light) > 100){
        if(state.lighting > state.target_light){
            win_cur = min(win_cur + 1, win_wax);
        }else{
            win_cur = max(win_cur - 1, win_min);
        }
    }
    writeServo(window, win_cur);
    state.window = win_cur;

    // door button
    if(!digitalRead(FACE_BTN_PIN)){
        // delay(10000);
        if(state.door_opened){
            state.door_opened = false;
            // TODO: remove this
            delay(100);
        }
        else{
            state.alarm_enabled = true;
        } 
    }

    // alarm
    if(state.door_opened){
        state.alarm = false;
        state.alarm_enabled = false;
    }
    if(state.alarm_enabled && state.motion){
        state.alarm = true;
    }

    
}

void handleInput(){
    if(recvDoc.containsKey("target_light")){
        state.target_light = recvDoc["target_light"];
    }
    if(recvDoc.containsKey("cool_temp")){
        state.cool_temp = recvDoc["cool_temp"];
    }
    if(recvDoc.containsKey("heat_temp")){
        state.heat_temp = recvDoc["heat_temp"];
    }
    if(recvDoc.containsKey("door_opened")){
        state.door_opened = recvDoc["door_opened"];
    }
    if(recvDoc.containsKey("message")){
        dsp_msg = recvDoc["message"].as<String>();
        set_msg_time = millis();
    }
}