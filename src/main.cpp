#include <Arduino.h>
#include <CmdMessenger.h>
//#include <Wifi.h>
//#include <web_server.h>
//#include <Adafruit_NeoPixel.h>

#define PIN_NEOPIXEL 18
#define LED_COUNT 256
uint8_t LED_BRIGHTNESS = 5;

#define MAX_PWM 255
#define LOGIC_HIGH   1 // logic to turn lights on (dependant on relay)
enum {
    CMD_INIT_ALL,
    CMD_INIT,
    CMD_SET_LIGHT,
    CMD_ALL_ON,
    CMD_ALL_OFF,
    CMD_GET_LIGHT_DATA
};
enum {
    PIN,
    STATE
};
uint8_t pins[] = {
        2,       // Start at pin 2
        3,
        4,
        5,
        6,
        7,
        8,
        9,
        10,
        11,
        12,
        13,
        14,
        15,
        16,
        17
};

enum Lights{
    LIGHT_DEER,    // 0
    LIGHT_SNOWMAN, // 1
    LIGHT_STRING,  // 2
    LIGHT_ROLL,    // 3
    LIGHT_ANGEL1,  // 4
    LIGHT_ANGEL2,  // 5
    LIGHT_STAR1,   // 6
    LIGHT_STAR2,   // 7
    LIGHT_STAR3,   // 8
    LIGHT_STAR4,   // 9
    LIGHT_STAR5,   // 10
    LIGHT_SPARE1,  // 11
    LIGHT_SPARE2,  // 12
    LIGHT_SPARE3,  // 13
    LIGHT_SPARE4,  // 14
    LIGHT_SPARE5,  // 15
    LIGHT_LAST     // 16
};

enum Pattern {
    INIT,
    OFF,
    ON,
    TIMED_PULSE,
    TIMED_BREATHE,
    TIMED_STROBE,
};

/*
 * This struct contains all the arrays of light parameters,
 * every index pertains to the same light. Light parameters share
 * a common index
 */
typedef struct {
    bool states[LIGHT_LAST]; // state of light - on or off
    long timers[LIGHT_LAST]; // stores the value of millis() for later use
    int16_t pwm[LIGHT_LAST]; // current pwm for light in BREATHE mode
    long timeThresholds[LIGHT_LAST];
    int16_t multiplier[LIGHT_LAST];
    Pattern patterns[LIGHT_LAST];
}Light;

Light light;

CmdMessenger cmdMsg = CmdMessenger(Serial,',',';','/');

long patternChangeTimeCounter;
int16_t patternDelay;
int16_t counter=1;
bool first = true;
//WebServer server(80);
//Adafruit_NeoPixel strip(LED_COUNT, PIN_NEOPIXEL, NEO_GRBW + NEO_KHZ400);

/*
 *
 * Light state machine
 *
 */
void set_light(Lights lightPin, Pattern pattern, int16_t timeMs=0) {
    if(timeMs != 0) {
        light.timeThresholds[lightPin] = timeMs;
    }
    switch(pattern) {
        case INIT:
            for(int i=0; i<LIGHT_LAST; i++) {
            pinMode(pins[i], OUTPUT); // Set pins to output
            light.patterns[lightPin] = OFF;  // Set all lights to off
            light.timers[lightPin] = 0;
            light.states[lightPin] = !LOGIC_HIGH;
            light.pwm[lightPin] = 0;
            light.timeThresholds[lightPin] = 1000;
            light.multiplier[lightPin] = 1;
        }
            break;
        case ON:
            light.states[lightPin] = LOGIC_HIGH;
            light.timers[lightPin] = 0;
            break;
        case OFF:
            light.states[lightPin] = !LOGIC_HIGH;
            light.timers[lightPin] = 0;
            break;
        case TIMED_STROBE:
        case TIMED_PULSE:
            // Beginning of pulse and strobe - set timer and state
            if(light.timers[lightPin] == 0) {
                light.timers[lightPin] = millis();
                light.states[lightPin] = LOGIC_HIGH; // turn light on
            }
            else if(millis() - light.timers[lightPin] > light.timeThresholds[lightPin]) {
                // Pulse light only once
                if(pattern == TIMED_PULSE) {
                    light.patterns[lightPin] = OFF;
                    light.timers[lightPin] = 0;
                    light.states[lightPin] = !LOGIC_HIGH; //;
                }
                // flip flop light states for strobe
                else {
                    light.timers[lightPin] = millis();
                    light.states[lightPin] = !light.states[lightPin];
                }
            }
            break;
        case TIMED_BREATHE:
            if(light.timers[lightPin] == 0) {
                light.timers[lightPin] = millis();
                //light.states[lightPin] = 0;
                light.pwm[lightPin] = MAX_PWM;
                break;
            }
            else if(millis() - light.timers[lightPin] > light.timeThresholds[lightPin]) {
                // this if statement uses the state to handle direction of pwm ramp
                if (light.pwm[lightPin] >= MAX_PWM) {
                    light.states[lightPin] = 0;
                }
                else if (light.pwm[lightPin] <= 0) {
                    light.states[lightPin] = 1;
                }
                
                if (light.states[lightPin] == 1) {
                    light.pwm[lightPin] += 1 + light.multiplier[lightPin];
                } 
                else {
                    light.pwm[lightPin] -= 1 + light.multiplier[lightPin];
                } 
                light.timers[lightPin] = millis(); // reset the timer
            }
            break;
    }
    if(light.patterns[lightPin] == TIMED_BREATHE)
        analogWrite(pins[lightPin],light.pwm[lightPin]);
    else 
        digitalWrite(pins[lightPin], light.states[lightPin]);
}

void set_all_lights(Pattern pattern) {
    for(int i=0; i<LIGHT_LAST; i++) {
        light.patterns[i] = pattern;
    }
}

void handle_init_all() {
    set_all_lights(INIT);
}

void handle_set_light() {
    int16_t lightFixture = cmdMsg.readBinArg<int16_t>();
    int16_t pattern = cmdMsg.readBinArg<int16_t>();
    int16_t time = cmdMsg.readBinArg<int16_t>();
    int16_t multiplier = cmdMsg.readBinArg<int16_t>();
    light.patterns[lightFixture] = (Pattern)pattern;
    light.timeThresholds[lightFixture] = time;
    light.multiplier[lightFixture] = multiplier;
    light.timers[lightFixture] = 0;
    cmdMsg.sendCmdStart(CMD_SET_LIGHT);
    cmdMsg.sendCmdBinArg<int16_t>(lightFixture);
    cmdMsg.sendCmdBinArg<int16_t>(light.patterns[lightFixture]);
    cmdMsg.sendCmdBinArg<int16_t>(light.timeThresholds[lightFixture]);
    cmdMsg.sendCmdBinArg<int16_t>(light.multiplier[lightFixture]);
    cmdMsg.sendCmdEnd();
}

void handle_all_on() {
    set_all_lights(ON);
}

void handle_all_off() {
    set_all_lights(OFF);
}

void handle_get_light_data() {
    byte *ptr = ((byte *) &light);
    cmdMsg.sendCmdStart(CMD_GET_LIGHT_DATA);
    for(int i = 0; i < sizeof(light); i++) {
        cmdMsg.sendCmdBinArg<byte>(*ptr++);
    }
    cmdMsg.sendCmdEnd();
}

void attach_callbacks() {
    cmdMsg.attach(CMD_SET_LIGHT, handle_set_light);
    cmdMsg.attach(CMD_INIT_ALL, handle_init_all);
    cmdMsg.attach(CMD_ALL_ON, handle_all_on);
    cmdMsg.attach(CMD_ALL_OFF, handle_all_off);
    cmdMsg.attach(CMD_GET_LIGHT_DATA, handle_get_light_data);
}

void set_change_time();
void setup() {
   
    Serial.begin(115200); 
    //analogWriteResolution(12);
    set_all_lights(INIT);
    
    attach_callbacks();
    //server.init(); 
    //strip.begin();
    //strip.setBrightness(LED_BRIGHTNESS);
    //strip.setPixelColor(3, strip.Color(10, 0, 0, 0));
    //strip.show();
    patternChangeTimeCounter = millis(); 
    set_change_time();
}

void set_pattern();

void serial_debug();
void loop() {
    //server.run_loop(); 
    //cycle_colors(100, 0, 0, 0); strip.clear();
    //serial_debug();
    //cmdMsg.feedinSerialData();
    if(first) {
        set_all_lights(ON);
        first = false;
    } 
    set_pattern();
    for(int i=0; i< LIGHT_LAST; i++) {
        set_light((Lights)i, light.patterns[i]);
    }
}


void set_change_time() {
    patternDelay = random(1000, 20000); // 1 - 20 second pattern display time
}

void set_pattern() {
    if(millis() - patternChangeTimeCounter > patternDelay) {
        uint8_t index = random(4);
        light.timeThresholds[index] = random(300, 700);
        light.patterns[index] = TIMED_STROBE;
        light.timers[index] = 0;
        set_change_time();
        patternChangeTimeCounter = millis();
        counter++;
    }
    if(counter % 18 == 0) {
        patternDelay = random(150000, 3000000);
        patternChangeTimeCounter = millis();
        set_all_lights(INIT);
        set_all_lights(ON);
    }
}

void serial_debug() {
    if(Serial.available()) {
        uint8_t cmd = Serial.read();
        /*
        if(cmd == 'a') {
            uint8_t pin = Serial.parseInt();
            uint8_t state = Serial.parseInt();
            digitalWrite(pin, state);
            Serial.println(cmd + pin + state);
        }
        else if(cmd == 'b') {
            uint8_t pin = Serial.parseInt();
            uint8_t pwm = Serial.parseInt();
            analogWrite(pin, pwm);
            Serial.println(cmd + pin + pwm);
        }
        */
        if(cmd == 'n') {
            set_all_lights(ON);
        }
        else if (cmd == 'z') {
            Serial.println(sizeof(light));
        }
        else if(cmd == 'f') {
            set_all_lights(OFF);
        }
        else if(cmd == 'b') {
            set_all_lights(TIMED_BREATHE);
        }
        else if(cmd == 's') {
            set_all_lights(TIMED_STROBE);
        }
        else if(cmd == 'p') {
            set_all_lights(TIMED_PULSE);
        }
        else if(cmd == 't') {
            light.timeThresholds[0] = 300;
            light.timeThresholds[1] = 400;
            light.timeThresholds[2] = 200;
            light.timeThresholds[3] = 400;
            light.timeThresholds[4] = 450;
            light.timeThresholds[5] = 84;
            light.timeThresholds[6] = 120;
            light.timeThresholds[7] = 200;
        }
        else if(cmd == 'd') {
            light.timeThresholds[0] = 2;
            light.timeThresholds[1] = 5;
            light.timeThresholds[2] = 9;
            light.timeThresholds[3] = 5;
            light.timeThresholds[4] = 2;
            light.timeThresholds[5] = 5;
            light.timeThresholds[6] = 8;
            light.timeThresholds[7] = 4;
            light.timeThresholds[8] = 9;
        }
        else if(cmd == 'z') {
            light.patterns[0] = TIMED_PULSE;
            light.patterns[1] = TIMED_STROBE;
            light.patterns[2] = ON;
        }
        else if(cmd == 'i') {
            set_all_lights(INIT);
        }
    }
}