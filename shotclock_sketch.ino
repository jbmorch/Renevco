/*
25/15 second shot clock
*/

// QWIIC display
#include <Wire.h>
#include <SparkFun_Alphanumeric_Display.h>

//GPIO declarations
//-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=
byte runningSwitch = 2;
byte resetButton = 3;
byte fifteenSecondSwitch = 4;

byte segmentLatch = 5;
byte segmentClock = 6;
byte segmentData = 7;
//-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=

//QWIIC declarations
HT16K33 display;

void setup()
{
  //Serial.begin(9600);
  Serial.begin(115200);
  Serial.println("Large Digit Driver");

  pinMode(segmentClock, OUTPUT);
  pinMode(segmentData, OUTPUT);
  pinMode(segmentLatch, OUTPUT);

  pinMode(runningSwitch, INPUT);
  pinMode(resetButton, INPUT);
  pinMode(fifteenSecondSwitch, INPUT);
  
  digitalWrite(segmentClock, LOW);
  digitalWrite(segmentData, LOW);
  digitalWrite(segmentLatch, LOW);

  // qwiic display LCD (this is so the operator knows what's on the clock without having to look at the big display which is facing the court and away from the operator)

  Wire.begin(); // join I2C bus
  if (display.begin() == false)
  {
    Serial.println("Device did not acknowledge! Freezing.");
    while (1);
  }
  Serial.println("Display acknowledged.");
}

// default starting count
float timeLeft = 25.0;

void loop()
{
  byte running = digitalRead(runningSwitch);
  byte reset = digitalRead(resetButton);
  byte fifteenSecond = digitalRead(fifteenSecondSwitch);
  
  float resetTime = 25.0;

  if (fifteenSecond) {
    resetTime = 15.0;
  }

  // ensures if the 25 > 15 switch is hit while the clock is over 15 seconds that we'll drop to 15 seconds
  if (timeLeft > resetTime) {
    timeLeft = resetTime;
  }
  
  if (reset) {
    timeLeft = resetTime;

    running = false;
  }

  //byte running = true;

  if (running && timeLeft > 0.0) {
    timeLeft -= 0.1;
  }

  if (timeLeft >= 10.0) {
    showNumber((float) (int) timeLeft, false);
    updateSmallLcd(timeLeft);
  } else {
    showNumber((float) (int) (timeLeft * 10), true);
    updateSmallLcd(timeLeft);
  }
 
  // NOTE: our delay is very imprecise here -- we're delaying for 100ms MINUS an estimated number of ms it takes to process the above
  //       code. This was measured through trial and error by letting the clock run over 10-20 seconds and measuring the delta with a
  //       stopwatch (ie "known good"). The difference ended up being about 2ms per loop, hence we only delay for 98ms.
  //       If we want to get fancier and/or expect to run this on other chips, then a true timer based implementation would need to be 
  //       implemented here. But... we don't need to be so fancy for now. :)
  delay(98);
  Serial.println(timeLeft); //For debugging
}

// Updates the qwiic display LCD with the time
void updateSmallLcd(float secondsLeft)
{
  // we need to convert our time to a 4-char string to pass to the display

  // for times >= 10.0, this value will be (space)-(10s digit)-(1s digit)-(period)-(0.1s digit)
  // for times < 10.0, this value be (space)-(space)-(1s digit)-(period)-(0.1s digit)
  // note we need 6 bytes because we'll need 4 displayable characters, the period, and then the null at the end of the string
  char displayString[6];

  // our first character will always be a space
  displayString[0] = ' ';
  if (secondsLeft >= 10.0) {
    displayString[1] = ((int) (secondsLeft / 10.0)) + '0';
    displayString[2] = ((int) (secondsLeft - ((int) (secondsLeft / 10.0)) * 10.0)) + '0';
    displayString[3] = '.';
    displayString[4] = ((int) ((secondsLeft - (int) secondsLeft) * 10)) + '0';
  } else {
    displayString[1] = ' ';
    displayString[2] = ((int) secondsLeft) + '0';
    displayString[3] = '.';
    displayString[4] = ((int) ((secondsLeft - (int) secondsLeft) * 10)) + '0';
  }
  displayString[5] = 0;

  display.print(displayString);
}

//Takes a number and displays 2 numbers. Displays absolute value (no negatives)
void showNumber(float value, boolean decimal)
{
  int number = abs(value); //Remove negative signs and any decimals

  //Serial.print("number: ");
  //Serial.println(number);

  for (byte x = 0 ; x < 2 ; x++)
  {
    int remainder = number % 10;

    postNumber(remainder, (decimal && (1 == 1)));

    number /= 10;
  }

  //Latch the current segment data
  digitalWrite(segmentLatch, LOW);
  digitalWrite(segmentLatch, HIGH); //Register moves storage register on the rising edge of RCK
}

//Given a number, or '-', shifts it out to the display
void postNumber(byte number, boolean decimal)
{
  //    -  A
  //   / / F/B
  //    -  G
  //   / / E/C
  //    -. D/DP

#define a  1<<0
#define b  1<<6
#define c  1<<5
#define d  1<<4
#define e  1<<3
#define f  1<<1
#define g  1<<2
#define dp 1<<7

  byte segments;

  switch (number)
  {
    case 1: segments = b | c; break;
    case 2: segments = a | b | d | e | g; break;
    case 3: segments = a | b | c | d | g; break;
    case 4: segments = f | g | b | c; break;
    case 5: segments = a | f | g | c | d; break;
    case 6: segments = a | f | g | e | c | d; break;
    case 7: segments = a | b | c; break;
    case 8: segments = a | b | c | d | e | f | g; break;
    case 9: segments = a | b | c | d | f | g; break;
    case 0: segments = a | b | c | d | e | f; break;
    case ' ': segments = 0; break;
    case 'c': segments = g | e | d; break;
    case '-': segments = g; break;
  }

  if (decimal) segments |= dp;

  //Clock these bits out to the drivers
  for (byte x = 0 ; x < 8 ; x++)
  {
    digitalWrite(segmentClock, LOW);
    digitalWrite(segmentData, segments & 1 << (7 - x));
    digitalWrite(segmentClock, HIGH); //Data transfers to the register on the rising edge of SRCK
  }
}
