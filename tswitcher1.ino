#include <EEPROM.h>
#include <RgbLed.h>
#include <Bounce.h>

/***************************
 * tswitcher
 * work in progress guitar pedalboard programmable switching system.
 * 2012-09-05
 * tonymuka@gmail.com
 * 
 * COMMENTS: this is a first attempt, i'll refine it as I learn. i imagine i should use button interrupts rather than polling.
 ***************************/

elapsedMillis sinceInit;

#define NUM_BUTTONS 3
#define NUM_RELAYS 3
#define NUM_LEDS 3
#define NUM_RGB_LEDS 1

#define DEBOUNCE_MS 5

//define the single coil latching relay pins in a multi-dimensional array. remember this is zero indexed.
int relayPins[NUM_RELAYS][2] = {
    {  9,10 },
    { 11,12 },
    { 13,14 }
};

#define TAP_TEMPO_RELAY_NUM NULL

int buttonPins[NUM_BUTTONS] = { 6,7,8 };
int ledPins[NUM_LEDS] = { 38,39,40 };
int rgbLEDpins[NUM_RGB_LEDS][3] = {
    { 26,25,24 }
};

#define DURATION_BLINK 2000
#define DURATION_DELAY 2000

// Create objects of the appropriate types for the two connected LEDs
// Last arg=true indicates that all of these pins are PWM enabled, omit for no PWM
// If you don't specify this and then setPwm, the library sets the LED to WHITE

int relayStates[NUM_RELAYS] = {0}; //inits the state array to all zeros
int excludeButtonFromPresets[NUM_BUTTONS] = {0}; //inits the state array to all zeros

//int thisPin = 0; //init this we'll use in loops below

// Instantiate a Bounce object with a 5 millisecond debounce time
// couldn't figure out how to make this code dynamic for NUM_BUTTONS
Bounce bouncer[NUM_BUTTONS] = {
    Bounce( buttonPins[0],DEBOUNCE_MS ),
    Bounce( buttonPins[1],DEBOUNCE_MS ),
    Bounce( buttonPins[2],DEBOUNCE_MS )
};

int buttonLastRise[NUM_BUTTONS] = {0}; //inits the state array to all zeros
int buttonLastFall[NUM_BUTTONS] = {0}; //inits the state array to all zeros
int lastLongPressTime[NUM_BUTTONS] = {0}; //save timestamp of long press so we don't do funky stuff

RgbLedCommonAnode rgb_led[NUM_RGB_LEDS] = {
    RgbLedCommonAnode( rgbLEDpins[0][0], rgbLEDpins[0][1], rgbLEDpins[0][2], true )
};

int globalPresetMode = 1; //switch between independent and preset global settings
int lastButtonPressed = 0; //switch between independent and preset global settings


void setup()   {
    Serial.begin(38400);

    for (int thisPin = 0; thisPin < NUM_RGB_LEDS; thisPin++)  {
        rgb_led[thisPin].setColor(0, 15, 5); //so we can tell init is happening
    }

    //init button input pins with pullup
    for (int thisPin = 0; thisPin < NUM_BUTTONS; thisPin++)  {
        pinMode(buttonPins[thisPin], INPUT_PULLUP);
    }

    //init relay pins as outputs
    for (int thisPin = 0; thisPin < NUM_RELAYS; thisPin++)  {
        pinMode(relayPins[thisPin][0], OUTPUT);
        pinMode(relayPins[thisPin][1], OUTPUT);
    }
    //init led pins as outputs
    for (int thisPin = 0; thisPin < NUM_LEDS; thisPin++)  {
        pinMode(ledPins[thisPin], OUTPUT);
        digitalWrite(ledPins[thisPin],LOW);
    }

    //led_rgb1.cycleFromTo(Color::WHITE, Color::WHITE); //show off colors on init

    //setAllRelays(relayStates); //initialize relay positions. Consider skipping this to save state on power failures

}

void loop()
{

    for (int thisPin = 0; thisPin < NUM_BUTTONS; thisPin++)  {
        //Serial.print(" checking pin #");
        //Serial.println(thisPin,DEC);
        // Update the debouncer
        if(bouncer[thisPin].update()){
            if(bouncer[thisPin].read() == LOW){        // Get the update value
                Serial.print(sinceInit);
                Serial.print(" Button #");
                Serial.print(thisPin,DEC);
                Serial.println(" pressed");
            }

            if(bouncer[thisPin].fallingEdge()){
                Serial.print(sinceInit);
                Serial.print(" falling edge button #");
                Serial.println(thisPin);
                // check for button chording (two buttons at the same time)
                // we can use that to change modes, etc.
                //if(!checkForChording(thisPin)){
                checkForRepeatPress(thisPin); //set tap tempo on repeat presses
                //}
                buttonLastFall[thisPin] = sinceInit; //be careful to check this before setting it for tempo repeats
            }
            if(bouncer[thisPin].risingEdge()){
                Serial.print(sinceInit);
                Serial.print(" rising edge button #");
                Serial.println(thisPin);
                //buttonLastRise[thisPin] = 0;
                //Serial.println(bouncer[thisPin].duration());
                if(isOldButton(thisPin)==true){
                    if(globalPresetMode){
                        recallPreset(thisPin);
                    } else {
                        setRelay(thisPin,flipBit(relayStates[thisPin]));
                    }
                }
                buttonLastRise[thisPin] = sinceInit;
            }
            debugStatus();
        }
        // check for button long press (>3s?) to save preset in preset mode
        // maybe in stomp mode a long press excludes a button from preset mode presets
        if(detectLongPress(thisPin)){
            if(!checkForChording(thisPin)){
                if(globalPresetMode == 1){
                    storePreset(thisPin);
                } else {
                    toggleExcludedButtonFromPresets(thisPin);
                }
            }
        }
    }

    // change this to slow down the looping.
    delay(1);

}

// make subsequent presses of a button in preset mode set tap tempo
// verify that tempo is within a valid useful range, say 40-200bpm (presto!).
int checkForRepeatPress(int thisPin){
    if(thisPin == lastButtonPressed){
        int timeDelta = sinceInit - buttonLastFall[thisPin];
        if(timeDelta > 450 && timeDelta < 1500){ //60-200BPM
            //assume tapping quarter notes
            setTapTempo(timeDelta);    
        }
    }
}

// flip bit in array to exclude button from preset toggles
int toggleExcludedButtonFromPresets(int pin_number){
    excludeButtonFromPresets[pin_number] = flipBit(excludeButtonFromPresets[pin_number]);
    //todo: do something with led to indicate excluded bit
    Serial.print(sinceInit);
    Serial.print(" ExcludeFromPreset state for button #");
    Serial.print(pin_number);
    Serial.print(" is ");
    Serial.print(excludeButtonFromPresets[pin_number]);
    Serial.println();
}

int checkForChording(int pin_number){
    for (int thisPin = 0; thisPin < NUM_BUTTONS; thisPin++)  {
        // if(thisPin != pin_number && buttonLastFall[pin_number] >= buttonLastRise[thisPin] ){
        if(bouncer[thisPin].read() == LOW && thisPin != pin_number){
            //thisPin is still held down. chording exists
            Serial.print(sinceInit);
            Serial.print(" Button Chording True, pin #");
            Serial.print(thisPin);
            Serial.println(" is still down.");
            //do chording action for the two pin combo
            //maybe switch modes, etc.
            if((pin_number == 0 || pin_number == 1) && (thisPin == 0 || thisPin ==1)){
                toggleGlobalPresetMode();
            }
            return true;
        }
    }
    return false; //use negative for false since button zero is not false.
    }

    // toggle between global preset mode and independent switches mode
    void toggleGlobalPresetMode(){
        Serial.print(sinceInit);
        Serial.print(" Switching from GlobalPresetMode ");
        Serial.print(globalPresetMode);
        globalPresetMode = flipBit(globalPresetMode); //change from preset to standalone
        Serial.print(" to ");
        Serial.println(globalPresetMode);
    }

    // return false if button has been down over 3 seconds. helps prevent default actions after long press actions
    int isOldButton(int pin_number){
        if((sinceInit - lastLongPressTime[pin_number])>3000){ //button has been down over 3 seconds
            return true;
        } else {
            return false;
        }
    }

    int detectLongPress(int pin_number){
        if(buttonLastFall[pin_number] > buttonLastRise[pin_number]){ //button is still down
            if((sinceInit - buttonLastFall[pin_number])>3000){ //button has been down over 3 seconds
                //do long press action for this pin
                Serial.print(sinceInit);
                Serial.print(" Long Press Pin #");
                Serial.print(pin_number);
                Serial.println(" Detected.");
                lastLongPressTime[pin_number] = sinceInit; //reset counter to zero so we dont have multiple saves
                buttonLastFall[pin_number] = sinceInit; //reset counter to zero so we dont have multiple saves
                buttonLastRise[pin_number] = sinceInit+15; //reset counter to zero so we dont have multiple saves. add 5 to be sure rise is after fall
                return true;
            }
        }
        return false;
    }

    int flipBit(int abit){
        //Serial.println("flippin bit!");
        return !abit;
    }

    int setRelay(int relayNum, int state){
        Serial.print(sinceInit);
        Serial.print(" setRelay #");
        Serial.print(relayNum,DEC);
        Serial.print(" to ");
        Serial.print(state);

        if(relayNum > NUM_RELAYS){
            Serial.print("\tERROR: requested relayNum #");
            Serial.print(relayNum);
            Serial.println(" does not exist!!!");
            return 0;
        }
        Serial.println("");
        if(state){
            //engage relay loop
            digitalWrite(relayPins[relayNum][0],HIGH);
            digitalWrite(relayPins[relayNum][1],LOW);
            rgb_led[relayNum].setColor(Color::CYAN);
            digitalWrite(ledPins[relayNum],LOW);
        }
        else {
            //bypass relay loop
            digitalWrite(relayPins[relayNum][0],LOW);
            digitalWrite(relayPins[relayNum][1],HIGH);
            rgb_led[relayNum].setColor(Color::YELLOW);
            digitalWrite(ledPins[relayNum],HIGH);
        }

        delay(1);
        //done toggling, set both pins idle
        digitalWrite(relayPins[relayNum][0],LOW);
        digitalWrite(relayPins[relayNum][1],LOW);

        relayStates[relayNum] = state; //update the state array
        return 1;
    }


    //takes a preset number and a number 0-255 representing the 8bit byte of relay settings to store.
    void storePreset(int preset_number){
        Serial.print(sinceInit);
        Serial.print(" storePreset #");
        Serial.print(preset_number);
        Serial.print("\t");
        debugStatus();
        Serial.println();


        //todo: figure out converting relayStates array to a byte for storage
        //EEPROM.write(preset_number, relayStates);

        //todo: flash led or something for feedback that setting is saved
    }

    //recall a preset number containing a number 0-255 representing the 8bit byte of relay settings to store.
    void recallPreset(int preset_number){
        /* todo: write recallPreset

        // read a byte from the current address of the EEPROM
        int relayStatesByte = EEPROM.read(preset_number);
        if(!relayStatesByte){ 
        relayStatesByte = 255;  //default to all on with 255, or all off with 0
        }
         */
        Serial.print(sinceInit);
        Serial.print(" loadPreset #");   
        Serial.print(preset_number);
        Serial.print("\t");
        //Serial.print(relayStatesByte, BIN);
        Serial.println();
        /*
        //convert relayStates from byte to array
        for (int thisPin = 0; thisPin < NUM_RELAYS; thisPin++)  {
        relayStates[thisPin] = DEC relayStatesByte[thisPin];
        }
        return relayStates;
         */
    }

    void activatePreset(int preset_number){
        //setAllRelays(recallPreset(preset_number));
        Serial.print(sinceInit);
        Serial.print(" Activating Preset #");
        Serial.print(preset_number);
        Serial.println(); 
    }

    void printAllPresets(){
        for (int thisPin = 0; thisPin < NUM_BUTTONS; thisPin++)  {
            Serial.print(sinceInit);
            Serial.print(" Preset #"); 
            Serial.print(thisPin);
            Serial.print(" - ");
            //Serial.print(recallPreset(thisPin), BIN);
            //Serial.println();
        }
    }

    //set state of all relays based on passed byte of data
    void setAllRelays(int *relayStates){
        //foreach bit, set corresponding relay position
        for (int thisPin = 0; thisPin < NUM_RELAYS; thisPin++)  {
            if(! excludeButtonFromPresets[thisPin]){
                setRelay(thisPin,relayStates[thisPin]);
            } else {
                Serial.print(" ExcludingFromPreset assignment #");
                Serial.print(thisPin);
                Serial.println();
            }
        }
    }

    //set state of all LEDs based on passed byte of data
    void setAllLEDs(int relayStates){

    }

    void debugStatus(){
        Serial.print(sinceInit);
        Serial.print("\tRELAY STATES: ");
        for(int thisPin = NUM_RELAYS-1; thisPin >= 0; thisPin--) {
            Serial.print(relayStates[thisPin],DEC);
        }
        Serial.println("");
    }

    //send a few pulses to the tap tempo jack at the specified delay in ms
    //Normally Open (TC Electronic Nova Delay and Nova Modulator, Line 6 pedals [after modification], Eventide Factor Series, Tap-a-Cusack, Strymon Timeline, Modified DD-6, Empress Phaser, Diamond Tremelo and Memory Lane Jr., Strymon Brigadier and El Capistain [after modification])
    //Normally Closed (Boss DD-5, DD-7, DD-20, PH-3, RE-20, SL-20, RC Loopstation Series)
    //Latching (Empress Tremolo, Empress Superdelay and Vintage Modified Superdelay)
    void setTapTempo(int msDelay){
        //send three grounded pulses to the tap relayNum. assuming normally open pedal tap operation.
        Serial.print("\tSetting Tap Tempo Delay (ms) to ");
        Serial.println(msDelay);
        for(int i=0;i < 3;i++){
            setRelay(TAP_TEMPO_RELAY_NUM,1);
            delay(1);
            setRelay(TAP_TEMPO_RELAY_NUM,0);
            if(i < 2){
                delay(msDelay-1); //no need to delay after the last tap
            }
        }

    }

