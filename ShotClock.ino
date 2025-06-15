// Shot clock controller
// 2025 JB Morch
// with contributions from Som Naderi
// 
// Designed for implementation using an Arduino ESP32 nano microcontroller

// To-do list:
// # refactor for readibility and comments, including better serial messages for debugging
// # Implement wireless (designate one unit as controller = wifi AP mode, and others as remotes = wifi station mode; same code for both)
// # stretch goal: integrate into Nevco scoreboard system so only one controller is needed
// # [DONE] add beep/buzz when timer expires
// # [DONE] change 'delay(98)' in the main loop to use a millis() counter instead
// # [DONE] implement time adjustment using the rotary encoder
// # [DONE] Add 2 small 7-segment displays that face the scorekeeper and show the same timer as the large digits
// # [DONE] decide on '#define' OR 'const int' for pin assignments, and make them all the same

// Include the library that handles addressable LED strips
#include <FastLED.h>

// The shot clock uses a string of addressable LEDs to form large letters in a 7-segment
// Note that pin numbering in the IDE compiler should be set to 'By GPIO number (legacy)'

// Rotary encoder hints taken from https://lastminuteengineers.com/rotary-encoder-arduino-tutorial/

// 7-segment display mutiplexed through a 4017 decade counter taken from YouTube channel 'Tulsa OpenSource Hardware'
// and GitHub repository by gbhug5a. https://github.com/gbhug5a/7-Segment-Displays-Multiplex-by-Segment


// Digital IOs
#define runningSwitch D2              // On-off rocker switch
#define resetButton D3                // reset timer to 25 or 15
#define fifteenSecondSwitch D5        // Toggle between 25 or 15 second mode
#define CLK D6                        // rotary encoder output A
#define DT D7                         // rotary encoder output B
#define DATA_PIN D8                   // the arduino pin which is connected to the DataIn of the LED strip
#define CLOCK4017pin D9               // Small LEDs multiplexed through 74HC4017 clock
#define CC0pin D10                    // Small LEDs multiplexed through 74HC4017 enable low digit
#define CC1pin D11                    // Small LEDs multiplexed through 74HC4017 enable high digit
#define buzzer D12                     // Out of time buzzer

// Big Digits (LED strip) definitions
#define NUM_LEDS_PER_DIGIT 29         // number of LEDs used for each large digit
#define NUM_LEDS 58                   // total number of LEDs in the string

// ================================================
// Small digits (7-segment displays) definitions
const byte SEGMENTS  = 8;                  // Number of segments. 8 if using DP (decimal point)
const byte DISPLAYS  = 2;                  // Number of displays used - two here
const byte Refresh   = 3;                  // Number of millis changes between segments
// Arrays allow using any number of DISPLAYS - add others as needed
byte  CCpin[] = {CC0pin,CC1pin};           // The digit's CC driver pin number
byte  DIGIT[DISPLAYS];                     // And its displayed character
// Use these defs for common cathode displays - LOW to sink current
const byte CCON    = LOW;
const byte CCOFF   = HIGH;
// The bit value of each segment
const byte segA  = bit(0);
const byte segB  = bit(1);
const byte segC  = bit(2);
const byte segD  = bit(3);
const byte segE  = bit(4);
const byte segF  = bit(5);
const byte segG  = bit(6);
const byte segH  = bit(7);               //if the decimal point if used
// Segment patterns of the characters "0" thru "9", plus a "10" character
const byte char0 = segA + segB + segC + segD + segE + segF;
const byte char1 = segB + segC;
const byte char2 = segA + segB + segD + segE + segG;
const byte char3 = segA + segB + segC + segD + segG;
const byte char4 = segB + segC + segF + segG;
const byte char5 = segA + segC + segD + segF + segG;
const byte char6 = segA + segC + segD + segE + segF + segG;
const byte char7 = segA + segB + segC;
const byte char8 = segA + segB + segC + segD + segE + segF + segG;
const byte char9 = segA + segB + segC + segD + segF + segG;
const byte char10  = segA + segD + segG;     //used to display "100"
// Array links a value to its character
byte charArray[] = {char0, char1, char2, char3, char4, char5,
                    char6, char7, char8, char9, char10};
unsigned long PREVmillis;
unsigned long CURmillis;
byte milliCount = 0;                       //Number of millis changes so far
byte SEGCOUNT;                             //Segment counter - zero to six (or seven)
byte CURSEG;                               //Current segment bit position
byte i_mux;
int TEMP = LOW;
// ================================================

// Array defining the sequence of LEDs that should be lit for each digit
//      /-A-\
//      F   B
//      \-G-/
//      E   C
//      \-D-/ Z
// There are 4 pixels per segment, with an unused pixel between E and F.
// The constant corresponds to the segments as follows:
// 0b000ggggbbbbaaaaffff0eeeeddddcccc

// Four pixels per segment with an unused pixel between d and e segments
const uint32_t digits[10] = {
  //000GGGGBBBBAAAAFFFFEEEE0DDDDCCCC
  0b00000001111111101111111111111111, // 0
  0b00000001111000000000000000001111, // 1
  0b00011111111111100000111111110000, // 2
  0b00011111111111100000000011111111, // 3
  0b00011111111000011110000000001111, // 4
  0b00011110000111111110000011111111, // 5
  0b00011110000111111110111111111111, // 6
  0b00000001111111100000000000001111, // 7
  0b00011111111111101111111111111111, // 8
  0b00011111111111111110000000001111, // 9
};

// The array of RGB values (or maybe HSV values) assigned to each LED in the strip
CRGB leds[NUM_LEDS];

// Rotary encoder setup
volatile int counter = 0;
volatile int currentStateCLK;
volatile int lastStateCLK;
String currentDir ="";    // CW or CCW, used for debugging
String lastDir ="";

// Debouncing parameters
const unsigned long DEBOUNCE_DELAY = 3;  // milliseconds
unsigned long lastDebounceTime = 0;
int lastrunning = HIGH;
int running = HIGH;      // Current, debounced switch state
bool stateChanged = false;   // Flag to track state changes

// Time variables
unsigned long mSec;
unsigned long lastmSec = 0;
float timeLeft = 25.0;
int tens;                   // The tens digit of timeLeft
int ones;                   // The ones digit of timeLeft
float resetTime = 25.0;
int buzzerReady = 1;

// LED variables
int timeHue = 0; // 0 = Red
int timeSat = 0; // 0 = White
//bool dotOn = FALSE; // is the decimal point LED on?

// FUNCTIONS
// Set the values in the LED strip corresponding to a particular display/value
void setDigit(int display, int val, CHSV colour) {
  for(int i=0;i<NUM_LEDS_PER_DIGIT; i++){
    colour.v = bitRead(digits[val], i) * 255;
    leds[display*NUM_LEDS_PER_DIGIT + i] = colour;
  }
}

// Interrupt service routine for the rotary encoder
void updateEncoder(){

  // Debounce
  if ((millis() - lastDebounceTime) < DEBOUNCE_DELAY) {
    return;
  }

	// Read the current state of CLK
	currentStateCLK = digitalRead(CLK);
  lastDir = currentDir;

	// If last and current state of CLK are different, then pulse occurred
	// React to only 1 state change to avoid double count
	if (currentStateCLK != lastStateCLK  && currentStateCLK == 1){

		// If the DT state is different than the CLK state then
		// the encoder is rotating CCW so decrement
		if (digitalRead(DT) != currentStateCLK) {
			counter --;
			currentDir ="CCW";
		} else {
			// Encoder is rotating CW so increment
			counter ++;
			currentDir ="CW";
		}
	}

	// Remember last CLK state
	lastStateCLK = currentStateCLK;

  // Reset debouce timer
  lastDebounceTime = millis();
}


void setup() {
  // Initialize a serial connection, used only for debugging
  // Consider removing this when deploying
  Serial.begin(115200);
  // while (!Serial); // This will hold up the whole program if USB isn't connected
  Serial.println("Begin");
  Serial.println(__FILE__ __DATE__);

  //Initialize the LED strip
  FastLED.addLeds<WS2812B, DATA_PIN, GRB>(leds, NUM_LEDS);
  //FastLED.addLeds<WS2812B, DATA_PIN>(leds, NUM_LEDS);
  
  // Initialize the On-Off switch
  // pinMode(SW_PIN, OUTPUT);
  pinMode(runningSwitch, INPUT_PULLUP);
  pinMode(resetButton, INPUT_PULLUP);
  pinMode(fifteenSecondSwitch, INPUT_PULLUP);
  //pinMode(buzzer, OUTPUT);
  //digitalWrite(buzzer,LOW);

  // Init the rotary encoder
  pinMode(CLK,INPUT_PULLUP);
  pinMode(DT,INPUT_PULLUP);
  lastStateCLK = digitalRead(CLK);
  attachInterrupt(CLK, updateEncoder, CHANGE);
  //attachInterrupt(DT, updateEncoder, CHANGE);

  // Initialize the timer

  mSec = millis();

  // ============================================
  // Initialize the multiplexed 7-segment displays
  
  for(i_mux = 0; i_mux < DISPLAYS; ++i_mux) {          //Initialize all CC pins to OUTPUT, OFF
    pinMode(CCpin[i_mux],OUTPUT);
    digitalWrite(CCpin[i_mux],CCOFF);          //Set displayed characters to "0"
    DIGIT[i_mux] = char0;
  }

     //Clock the 4017 once to establish a stable clock line

  pinMode(CLOCK4017pin, OUTPUT);           //Set up 4017 Clock driver pin - OUTPUT
  digitalWrite(CLOCK4017pin,LOW);          //clock the 4017 once
  digitalWrite(CLOCK4017pin,HIGH);         //clock occurs on rising edge

     //Clock the 4017 until the segment A output goes HIGH

  while (TEMP == LOW) {
    pinMode(CLOCK4017pin,INPUT);           //change port to INPUT (floating)
    TEMP = digitalRead(CLOCK4017pin);      //read state of Q0 (and Clock)
    digitalWrite(CLOCK4017pin,HIGH);       //must do this before changing back to output
    pinMode(CLOCK4017pin,OUTPUT);          //switching back to OUTPUT
                                           //     clocks the 4017 if Q0 was LOW
                                           //once Q0 reads HIGH, we're done - segment A ON
  }

     // Initialize so everything else also points to segment A

  SEGCOUNT   = 0;                          //Segments counter - set to segment A
  CURSEG     = 1;                          //Bit position of segment A

  PREVmillis = millis();

     // Below for demo purposes - normally to display a VALUE 0-9, it would be
     //     DIGIT[i] = charArray[VALUE]

  DIGIT[0]    = char2;                     //Current segment pattern of Digit0
  DIGIT[1]    = char4;                     //Current segment pattern of Digit1
  // ============================================


}

void loop() {

  // Handle the real-time clock
  CURmillis = millis();
  if (CURmillis != PREVmillis) {
    milliCount++;
    PREVmillis = CURmillis;
  }
  
  //  Read the 3 switches
  //int reading = digitalRead(runningSwitch); // on-off switch
  int running = digitalRead(runningSwitch); // on-off switch
  int resetClock = digitalRead(resetButton); // reset button
  int fifteenSecond = digitalRead(fifteenSecondSwitch); // twenty-five & fifteen second clock toggle

  /*
  // =============================================
	// Read and process the rotary encoder
	currentStateCLK = digitalRead(CLK);
  lastDir = currentDir;

	// If last and current state of CLK are different, then pulse occurred
	// React to only 1 state change to avoid double count
	if (currentStateCLK != lastStateCLK && currentStateCLK == HIGH) {

		// If the DT state is different than the CLK state then
		// the encoder is rotating CCW so decrement
		if (digitalRead(DT) == HIGH) {
			counter --;
			currentDir = "CCW";
		} else {
			// Encoder is rotating CW so increment
			counter ++;
			currentDir = "CW";
		}
    // Save the last clock state
    Serial.print("Encoder CLK last state: ");
    Serial.print(lastStateCLK);
    Serial.print(" current state: ");
    Serial.println(currentStateCLK);
    lastStateCLK = currentStateCLK;
  }
  // End of rotary encoder processing  
  // =============================================
  */

  // If you have borked your switch wiring, invert the read value
  if (running == HIGH) {
    running = LOW;
  } else {
    running = HIGH;
  }

  // If the switch changed, due to noise or pressing
  //if (reading != lastrunning) {
  //  // Reset the debouncing timer
  //  lastDebounceTime = millis();
  // }

  // Check if enough time has passed since the last state change
  //if ((millis() - lastDebounceTime) > DEBOUNCE_DELAY) {
  //  // If the reading has been stable longer than the debounce delay
  //  if (reading != running) {
  //    running = reading;
  //    stateChanged = true;
  //  }
  //}

  resetTime = 25.0;
  if (fifteenSecond) {
    resetTime = 15.0;
  }
  
  // if the 25-15 switch is hit while the clock is over 15 seconds that we'll drop to 15 seconds
  if (timeLeft > resetTime) {
    timeLeft = resetTime;
  }
  
  if (resetClock == LOW) {
    timeLeft = resetTime;
    buzzerReady = 1;
    Serial.print("Time left: ");
    Serial.println(timeLeft);
  }

  // Handle the time adjustment due to countdown
  if (running && (timeLeft > 0.0)) {
    if (CURmillis - lastmSec > 99) {
      timeLeft -= 0.1;
      Serial.print("Time left: ");
      Serial.println(timeLeft);
      lastmSec = CURmillis;
    } 
  } 
  
  // Handle the time adjustment due to rotary switch
  if (!running) {
    if (currentDir == "CCW") {
      Serial.print("Decrement timeLeft from ");
      Serial.print(timeLeft);
      if (timeLeft > 5.99) {
        timeLeft -= 1.0;
      } else if (timeLeft > 4.99) {
        timeLeft = 4.9;
      } else {
        timeLeft -= 0.1;
      } 
      if (timeLeft < 0.0) {
        timeLeft = 0.0;
      }
      Serial.print(" to ");
      Serial.println(timeLeft);
    }
    if (currentDir == "CW") {
      Serial.print("Increment timeLeft from ");
      Serial.print(timeLeft);
      if ((timeLeft < 5.0) && (timeLeft > 4.90)) {
        timeLeft = 5.0;
      } else if (timeLeft > 4.99) {
        timeLeft += 1.0;
      } else {
        timeLeft += 0.1;
      }
      if (timeLeft > 98.9) {
        timeLeft = 99.0;
      }
      Serial.print(" to ");
      Serial.println(timeLeft);
    }
		//Serial.print("Direction: ");
		//Serial.print(currentDir);
		//Serial.print(" | Counter: ");
		//Serial.println(counter);
    currentDir = "";
  }

  if (running && (timeLeft < 0.1) && (buzzerReady > 0)) {
    //Serial.println("Analog write 200 to buzzer");
    //analogWrite(buzzer,200);
    //Serial.println("Delay");
    //delay(1500);
    //Serial.println("Analog write 0 to buzzer");
    //analogWrite(buzzer,0);
    //Serial.println("Done with buzzer code");
    //digitalWrite(buzzer,HIGH);
    //delay(500);
    //digitalWrite(buzzer,LOW);
    tone(buzzer, 520, 1500);
    noTone(buzzer);
    buzzerReady = 0;
  }

 // Set both the large and small digits' values
  tens = (int) timeLeft / 10;
  ones = (int) timeLeft % 10;
  if (timeLeft > 4.99) {
    // The big LED display
    if (ones > 0) {
      tens += 1;
    }
    setDigit(1,tens, CHSV(timeHue, timeSat, 255));
    setDigit(0,ones, CHSV(timeHue, timeSat, 255));
    // The small LED display
    DIGIT[0] = charArray[tens];
    if (timeLeft < 10.0) {
      DIGIT[0] = 0x0;
    }
    DIGIT[1] = charArray[ones];
    //dotOn = FALSE;
  } else {
    setDigit(1,(int) timeLeft, CHSV(timeHue, 255, 255)); 
    DIGIT[0] = charArray[(int) timeLeft] + segH;    // adding segH lights up the decimal point
    float timeShift = timeLeft * 10;
    setDigit(0,(int) timeShift % 10, CHSV(timeHue, 255, 155)); 
    DIGIT[1] = charArray[(int) timeShift % 10];
    //dotOn = TRUE;
  }

  // ============================================
  // Display the multiplexed 7-sement displays
  CURmillis = millis();
  if (CURmillis != PREVmillis) {
    milliCount++;
    PREVmillis = CURmillis;
  }
  if (milliCount == Refresh) {
    milliCount = 0;

       //This section selects the next segment

    CURSEG     = CURSEG << 1;            //shift to next bit position
    SEGCOUNT  += 1;                      //and increment the count
    if (SEGCOUNT == SEGMENTS) {          //if done with last segment, start over
      SEGCOUNT = 0;                      //re-initialize
      CURSEG   = 1;
    }

       //Turn the CC pins OFF while making changes - prevents ghosting

    for(i_mux = 0;i_mux < DISPLAYS; ++i_mux) digitalWrite(CCpin[i_mux],CCOFF);

       //Execute one clock pulse on the 4017 - shift output to next segment

    digitalWrite(CLOCK4017pin,LOW);
    digitalWrite(CLOCK4017pin,HIGH);

    // Send the digits to the LED string lights while the 7-segment display is off
    // NOTE: including the LED strip here makes the digits dim - I guess it takes too long
    FastLED.show();


       //This section turns the CC pins back ON per the patterns of the characters
       //If the CURSEG bit of the DIGIT[n] segment pattern is 1, turn on the CCpin[n]

    for(i_mux = 0; i_mux < DISPLAYS; ++i_mux) {
      if (DIGIT[i_mux] & CURSEG) digitalWrite(CCpin[i_mux], CCON);
    }
  }
  // ============================================

  // Send the digits to the LED string lights
  // NOTE: including the LED strip here causes flickering and strange delays
  //FastLED.show();

  // Save the run/stop switch reading for next comparison
  //lastrunning = reading;

}

