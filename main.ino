// Adafruit GFX Library - Version: Latest
#include <Adafruit_GFX.h>
#include <Adafruit_SPITFT.h>
#include <Adafruit_SPITFT_Macros.h>
#include <gfxfont.h>


// Adafruit SSD1306 - Version: 2.1.0
#include <Adafruit_SSD1306.h>
#include <splash.h>


//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
#pragma region DISPLAY
// DISPLAY 
#define SCREEN_WIDTH 128 // OLED display width, in pixels
#define SCREEN_HEIGHT 64 // OLED display height, in pixels

#define OLED_RESET     4 // Reset pin # (or -1 if sharing Arduino reset pin)
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);
#pragma endregion

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
#pragma region TUNER variables
// TUNER 
//clipping indicator variables
boolean clipping = 0;

//data storage variables
byte newData = 0;
byte prevData = 0;
unsigned int time = 0;//keeps time and sends vales to store in timer[] occasionally
int timer[10];//storage for timing of events
int slope[10];//storage for slope of events
unsigned int totalTimer;//used to calculate period
unsigned int period;//storage for period of wave
byte index = 0;//current storage index
float frequency;//storage for frequency calculations
int maxSlope = 0;//used to calculate max slope as trigger point
int newSlope;//storage for incoming slope data

//variables for decided whether you have a match
byte noMatch = 0;//counts how many non-matches you've received to reset variables if it's been too long
byte slopeTol = 3;//slope tolerance- adjust this if you need
int timerTol = 10;//timer tolerance- adjust this if you need

//variables for amp detection
unsigned int ampTimer = 0;
byte maxAmp = 0;
byte checkMaxAmp;
byte ampThreshold = 30;//raise if you have a very noisy signal

#pragma endregion


//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// FUNCTIONS
void setup()
{
  //Initialize display by providing the display type and its I2C address.
  display.begin(SSD1306_SWITCHCAPVCC, 0x3C);
  //Set the text size and color.
  display.setTextSize(1);
  display.setTextColor(WHITE);

  /* cli();//disable interrupts
  //set up continuous sampling of analog pin 0 at 38.5kHz
  //clear ADCSRA and ADCSRB registers
  ADCSRA = 0;
  //ADCSRB = 0;
  
  ADMUX |= (1 << REFS0); //set reference voltage
  ADMUX |= (1 << ADLAR); //left align the ADC value- so we can read highest 8 bits from ADCH register only
  
  ADCSRA |= (1 << ADPS2) | (1 << ADPS0); //set ADC clock with 32 prescaler- 16mHz/32=500kHz
  ADCSRA |= (1 << ADATE); //enabble auto trigger
  ADCSRA |= (1 << ADIE); //enable interrupts when measurement complete
  ADCSRA |= (1 << ADEN); //enable ADC
  ADCSRA |= (1 << ADSC); //start ADC measurements
  
  sei();//enable interrupts */


}








void loop()
{
  byte poti_threshold = (analogRead(A1)/4);
  byte poti_ratio = (analogRead(A2)/4);
  float audio_in = analogRead(A0)*(5.0 / 1023.0);

  byte reg = ADCSRA;
  
  //Clear previous image.
  display.clearDisplay();
  display.setCursor(0, 10);
  display.print(reg);

  checkClipping();
  if (checkMaxAmp>ampThreshold){
    frequency = 38462/float(period);//calculate frequency timer rate/period
  
    //print results
    display.print(frequency);
  }



  if (poti_threshold > 0){
    drawCompSettings(poti_threshold,poti_ratio);
  }

  display.display();
  delay(100);
}


void drawCompSettings (byte thresh, float ratio) {
  // Variable declaration
  #define potiMax 255
  byte threshX;
  byte threshY;
  byte ratioX;
  byte ratioY;

  //calculate threshold
  byte thresh_percent = ((long)thresh*100)/potiMax;
  threshX = SCREEN_HEIGHT + ((thresh_percent * SCREEN_HEIGHT)/100);
  threshY = SCREEN_HEIGHT - ((thresh_percent * SCREEN_HEIGHT)/100);
  
  // draw Threshhold
  display.setCursor(SCREEN_WIDTH/2, 10);
  display.print(thresh);
  display.fillTriangle(SCREEN_HEIGHT, SCREEN_HEIGHT, threshX, threshY, threshX, SCREEN_HEIGHT, WHITE);
  display.fillRect(threshX, threshY, (128-threshX), (SCREEN_HEIGHT-threshY), WHITE);

  //calculate ratio
  ratio = (((float)ratio*19)/potiMax)+1; // Die 20 ist das Maximum aus dem Compressor Script // 19+1 damit der Minimal-Wert 1 ist
  display.setCursor(SCREEN_WIDTH/2, 20);
  display.print(ratio);
  display.fillTriangle(threshX+1 , threshY-1, SCREEN_WIDTH, (threshY-((float)threshY/ratio)), SCREEN_WIDTH, threshY-1, WHITE);
}







ISR(ADC_vect) {//when new ADC value ready
  
  PORTB &= B11101111;//set pin 12 low
  prevData = newData;//store previous value
  newData = ADCH;//get value from A0
  if (prevData < 127 && newData >=127){//if increasing and crossing midpoint
    newSlope = newData - prevData;//calculate slope
    if (abs(newSlope-maxSlope)<slopeTol){//if slopes are ==
      //record new data and reset time
      slope[index] = newSlope;
      timer[index] = time;
      time = 0;
      if (index == 0){//new max slope just reset
        PORTB |= B00010000;//set pin 12 high
        noMatch = 0;
        index++;//increment index
      }
      else if (abs(timer[0]-timer[index])<timerTol && abs(slope[0]-newSlope)<slopeTol){//if timer duration and slopes match
        //sum timer values
        totalTimer = 0;
        for (byte i=0;i<index;i++){
          totalTimer+=timer[i];
        }
        period = totalTimer;//set period
        //reset new zero index values to compare with
        timer[0] = timer[index];
        slope[0] = slope[index];
        index = 1;//set index to 1
        PORTB |= B00010000;//set pin 12 high
        noMatch = 0;
      }
      else{//crossing midpoint but not match
        index++;//increment index
        if (index > 9){
          reset();
        }
      }
    }
    else if (newSlope>maxSlope){//if new slope is much larger than max slope
      maxSlope = newSlope;
      time = 0;//reset clock
      noMatch = 0;
      index = 0;//reset index
    }
    else{//slope not steep enough
      noMatch++;//increment no match counter
      if (noMatch>9){
        reset();
      }
    }
  }
    
  if (newData == 0 || newData == 1023){//if clipping
    clipping = 1;//currently clipping
    Serial.println("clipping");
  }
  
  time++;//increment timer at rate of 38.5kHz
  
  ampTimer++;//increment amplitude timer
  if (abs(127-ADCH)>maxAmp){
    maxAmp = abs(127-ADCH);
  }
  if (ampTimer==1000){
    ampTimer = 0;
    checkMaxAmp = maxAmp;
    maxAmp = 0;
  }
  
}

void reset(){//clean out some variables
  index = 0;//reset index
  noMatch = 0;//reset match couner
  maxSlope = 0;//reset slope
}


void checkClipping(){//manage clipping indication
  if (clipping){//if currently clipping
    clipping = 0;
  }
}

