#include <EEPROM.h>
#include <RgbLed.h>
#include <Bounce.h>

/***************************
 * tswitcher
 * work in progress guitar pedalboard programmable switching system.
 * 2012-09-05
 * tonymuka@gmail.com
 ***************************/

elapsedMillis sinceInit;

#define NUM_BUTTONS 2
#define NUM_RELAYS 2
#define NUM_RGB_LEDS 1

#define DEBOUNCE_MS 5

//define the single coil latching relay pins in a multi-dimensional array. remember this is zero indexed.
int relayPins[NUM_RELAYS][2] = {
  {  
    9,10       }
  ,
  {  
    9,10       }
};
int buttonPins[NUM_BUTTONS] = { 
  7,8 };

int rgbLEDpins[NUM_RGB_LEDS][3] = {
  { 
    26,25,24   }
};

#define DURATION_BLINK 2000
#define DURATION_DELAY 2000

// Create objects of the appropriate types for the two connected LEDs
// Last arg=true indicates that all of these pins are PWM enabled, omit for no PWM
// If you don't specify this and then setPwm, the library sets the LED to WHITE

int relayStates[NUM_RELAYS] = {
  0}; //inits the state array to all zeros

//int thisPin = 0; //init this we'll use in loops below

// Instantiate a Bounce object with a 5 millisecond debounce time
// couldn't figure out how to make this code dynamic for NUM_BUTTONS
Bounce bouncer[NUM_BUTTONS] = {
  Bounce( buttonPins[0],DEBOUNCE_MS ),  
  Bounce( buttonPins[1],DEBOUNCE_MS )
  };

RgbLedCommonAnode rgb_led[NUM_RGB_LEDS] = {
  RgbLedCommonAnode( rgbLEDpins[0][0], rgbLEDpins[0][1], rgbLEDpins[0][2], true )
  };

  void setup()   {
    Serial.begin(38400);

    rgb_led[0].setColor(0, 15, 5); //so we can tell init is happening

    //init button input pins with pullup
    for (int thisPin = 0; thisPin < NUM_BUTTONS; thisPin++)  {
      pinMode(buttonPins[thisPin], INPUT_PULLUP);
    }

    //init relay pins as outputs
    for (int thisPin = 0; thisPin < NUM_RELAYS; thisPin++)  {
      pinMode(relayPins[thisPin][0], OUTPUT);      
      pinMode(relayPins[thisPin][1], OUTPUT);      
    }

    //led_rgb1.cycleFromTo(Color::WHITE, Color::WHITE); //show off colors on init

    setAllRelays(relayStates); //initialize relay positions. consider skipping this to save state on power failures
  }

void loop()                     
{

  for (int thisPin = 0; thisPin < NUM_BUTTONS; thisPin++)  {
    //Serial.print("checking pin #");
    //Serial.println(thisPin,DEC);
    // Update the debouncer
    if(bouncer[thisPin].update()){
      if(bouncer[thisPin].read() == LOW){        // Get the update value
        Serial.print("Button #");
        Serial.print(thisPin,DEC);
        Serial.println(" pressed");
        setRelay(thisPin,flipBit(relayStates[thisPin]));
        debugStatus();
      }
      if(bouncer[thisPin].fallingEdge()){ 
        Serial.println("falling edge");
      }
      if(bouncer[thisPin].risingEdge()){ 
        Serial.println("rising edge"); 
        //Serial.println(bouncer[thisPin].duration());
      }

      // check for button chording (two buttons at the same time)
      // we could use that to change modes, etc.

      // check for button long press (>3s?) to save preset in preset mode
      // maybe in stomp mode a long press excludes a button from preset mode presets

      // perhaps make subsequent presses of a button in preset mode set tap tempo
      // verify that tempo is within a valid useful range, say 40-200bpm (presto!).

    }
  }

  // change this to slow down the looping.
  delay(100);

}

int flipBit(int abit){
  //Serial.println("flippin bit!");
  return (abit+1)%2; 
}

int setRelay(int relayNum, int state){
  Serial.print(sinceInit);
  Serial.print(". setRelay #");
  Serial.print(relayNum,DEC);
  Serial.print(" to ");
  Serial.println(state);

  if(relayNum > NUM_RELAYS){
    Serial.println("ERROR: requested relayNum does not exist!!!");
    return 0; 
  }

  if(state){
    digitalWrite(relayPins[relayNum][0],HIGH);  
    digitalWrite(relayPins[relayNum][1],LOW);
    rgb_led[relayNum].setColor(Color::CYAN);
  } 
  else {
    digitalWrite(relayPins[relayNum][0],LOW);  
    digitalWrite(relayPins[relayNum][1],HIGH);
    rgb_led[relayNum].setColor(Color::YELLOW);
  }
  delay(1);
  relayStates[relayNum] = state; //update the state array

  //done toggling, set both pins idle
  digitalWrite(relayPins[relayNum][0],LOW);  
  digitalWrite(relayPins[relayNum][1],LOW);
  return 1;
}


//takes a preset number and a number 0-255 representing the 8bit byte of relay settings to store.
void storePreset(int preset_number, int relayStates){
  Serial.print("savePreset #");   
  Serial.print(preset_number);
  Serial.print("\t");
  Serial.print(relayStates, BIN);
  Serial.println();

  EEPROM.write(preset_number, relayStates);
}
/*
//recall a preset number containing a number 0-255 representing the 8bit byte of relay settings to store.
 void recallPreset(int preset_number){
 
 // read a byte from the current address of the EEPROM
 int relayStatesByte = EEPROM.read(preset_number);
 if(!relayStatesByte){ 
 relayStatesByte = 255;  //default to all on with 255, or all off with 0
 }
 Serial.print("loadPreset #");   
 Serial.print(preset_number);
 Serial.print("\t");
 Serial.print(relayStatesByte, BIN);
 Serial.println();
 
 //convert relayStates from byte to array
 for (int thisPin = 0; thisPin < NUM_RELAYS; thisPin++)  {
 relayStates[thisPin] = DEC relayStatesByte[thisPin];
 }
 return relayStates;
 }
 */
void activatePreset(int preset_number){
  //setAllRelays(recallPreset(preset_number));
  Serial.print("activating Preset #");
  Serial.print(preset_number);
  Serial.println(); 
}

void printAllPresets(){
  for (int thisPin = 0; thisPin < NUM_BUTTONS; thisPin++)  {
    Serial.print("Preset #"); 
    Serial.print(thisPin);
    Serial.print(" - ");
    //Serial.print(recallPreset(thisPin), BIN);
    Serial.println();
  }
}

//set state of all relays based on passed byte of data
void setAllRelays(int *relayStates){
  //foreach bit, set corresponding relay position
  for (int thisPin = 0; thisPin < NUM_RELAYS; thisPin++)  {
    setRelay(thisPin,relayStates[thisPin]);
  }
}

//set state of all LEDs based on passed byte of data
void setAllLEDs(int relayStates){

}

void debugStatus(){
  for(int thisPin = NUM_RELAYS-1; thisPin >= 0; thisPin--) {
    Serial.print(relayStates[thisPin],DEC); 
  }
  Serial.println();
}

//send a few pulses to the tap tempo jack at the specified bpm
void setTapTempo(int bpm){
  //do we need a relay for this, or just a digital pin with the pulldown resistor?
}

