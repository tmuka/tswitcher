/* Pushbuttons control RGB LED Color, Teensyduino Tutorial #3
 http://www.pjrc.com/teensy/tutorial3.html
 
 This example code is in the public domain.
 */
#include <RgbLed.h>
#include <Bounce.h>
#define BUTTON1 7
#define BUTTON2 8

#define relayOnPin1  9
#define relayOffPin1  10

#define LED1_PIN_RED   26
#define LED1_PIN_GREEN 25
#define LED1_PIN_BLUE  24

#define DURATION_BLINK 2000
#define DURATION_DELAY 2000

// Create objects of the appropriate types for the two connected LEDs
// Last arg=true indicates that all of these pins are PWM enabled, omit for no PWM
// If you don't specify this and then setPwm, the library sets the LED to WHITE
RgbLedCommonAnode led_rgb1(LED1_PIN_RED, LED1_PIN_GREEN, LED1_PIN_BLUE, true);




// Instantiate a Bounce object with a 5 millisecond debounce time
Bounce bouncer1 = Bounce( BUTTON1,5 ); 
Bounce bouncer2 = Bounce( BUTTON2,5 ); 

int mode = 0;
int button1 = 0;
int button2 = 0;

void setup()   {
  Serial.begin(38400);
  //pinMode(7, INPUT);
  pinMode(BUTTON1,INPUT_PULLUP);
  pinMode(BUTTON2, INPUT_PULLUP);
  pinMode(relayOnPin1, OUTPUT);
  pinMode(relayOffPin1, OUTPUT);
  //led_rgb1.cycleFromTo(Color::WHITE, Color::WHITE); //show off colors on init
  led_rgb1.setColor(10, 0, 10);
  setRelay(button1); //initialize relay positions. consider skipping this to save state on power failures
}

void loop()                     
{

  // Update the debouncer
  if(bouncer1.update()){
    // Get the update value
    if(bouncer1.read() == LOW){
      // use mode zero when the first button is pressed
      mode = 0;
      button1 = flipBit(button1);
      Serial.println("mode 0");
      setRelay(button1);
    }
  }

  if(bouncer2.risingEdge()){
    // Get the update value
    //if(bouncer2.read() == LOW){
      // use mode zero when the first button is pressed
      mode = 1;
      button1 = flipBit(button1);
      Serial.println("mode 1");
      setRelay(button1);
    //}
  }

  // remain at this color, but not for very long
  //delay(1);

}

int flipBit(int abit){
  Serial.println("flippin bit!");
  return (abit+1)%2; 
}

int setRelay(int state){
  Serial.println("toggle relay!");
  if(state){
    digitalWrite(relayOnPin1,HIGH);  
    digitalWrite(relayOffPin1,LOW);
    led_rgb1.setColor(Color::CYAN);
  } 
  else {
    digitalWrite(relayOnPin1,LOW);  
    digitalWrite(relayOffPin1,HIGH);
    led_rgb1.setColor(Color::YELLOW);
  }
  delay(2);

  //done toggling, set both pins idle
  digitalWrite(relayOnPin1,LOW);  
  digitalWrite(relayOffPin1,LOW);
}



