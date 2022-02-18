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


#define SCREEN_WIDTH 128 // OLED display width, in pixels
#define SCREEN_HEIGHT 64 // OLED display height, in pixels

#define SCREEN_ADDRESS 0x3d ///< See datasheet for Address; 0x3D for 128x64, 0x3C for 128x32

// #define DEBUG Serial3

// minimum interval between packets
#define MIN_INTERVAL 333
// it will retry every ACK_WAIT +- MIN_INTERVAL
#define ACK_WAIT 1000


#define VARS \
    X(int, pot) \
    X(bool, motion) \
    X(float, temperature) \
    X(float, humidity) \
    X(int, lighting)

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

StaticJsonDocument<100> recvDoc;
StaticJsonDocument<100> doc;


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
    
} 

DHT dht(DHT_PIN, DHT11);
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire);

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

    display.begin(SSD1306_SWITCHCAPVCC, 0x3c);
    display.clearDisplay();
    display.display();


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
    display.print("Temp: ");
    display.print(state.temperature);
    display.println(" c");
    display.display();
    // TODO: target temp

}
