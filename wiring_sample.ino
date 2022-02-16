// #include <Arduino.h>
// #include "HardwareSerial.h"

#include <ArduinoJson.h>
#include <cstdint>
#include "art.h"


#define DEBUG Serial3

// minimum interval between packets
#define MIN_INTERVAL 333
// it will retry every ACK_WAIT +- MIN_INTERVAL
#define ACK_WAIT 1000


#define VARS \
    X(int, pot) \
    X(bool, led)

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
    Serial1.write(0xff);
    Serial1.println(measureJson(doc));
    serializeJson(doc, Serial1);
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





void setup() {   
    Serial1.begin(115200);
    DEBUG.begin(115200);

	pinMode(PC13, OUTPUT);
    pinMode(PA1, INPUT);
    
    DEBUG.println(art);
}




void serialEvent1() {
    DEBUG.println("EVENT");
    while(Serial1.read() != 0xff){
        if(!Serial1.available())return;
    }
    

    int sz = Serial1.parseInt();
    auto err = deserializeJson(recvDoc, Serial1);
    if(err) {
        DEBUG.print("Json parsing error: ");
        DEBUG.println(err.f_str());
    } else {
        DEBUG.println("Json parsed successfully");
    }


    if(recvDoc.containsKey("__ack__")){
        uint32_t packet_id = doc["__ack__"];
        DEBUG.print("Ack = ");
        DEBUG.println(packet_id);
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
    if(recvDoc.containsKey("led")) state.led = recvDoc["led"].as<bool>();
    if(recvDoc.containsKey("__all__")){
        DEBUG.println("Sending all");
        sender(true);
    }
    
} 



void loop() {
    sender();
    int pot = analogRead(PA1);
    if(abs(pot - state.pot) > 3)state.pot = pot;

    digitalWrite(PC13, !state.led);

}
