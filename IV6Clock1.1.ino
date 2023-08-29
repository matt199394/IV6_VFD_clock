// IV6 VFD Real time digital clock by M.Angelini
// 
/* In time display mode:
 * - Push SW1 to set time then SW2 and SW3 to increase or decase the hours, mins and secs.
 * - Push SW2 to toggle alllarm ON and OFF, if alarma ON pushing SW1 set the alarms hours mins and secs.
 *  Code to drive the VFD tube is taken from https://www.valvewizard.co.uk/iv18clock.html and sightly  modified to operate with two ULN2803 instesad of ILQ615.
 *  The structure of the program is a state machine (see swich-case in loop())
 */





#include <Wire.h>
#include "RTClib.h"

RTC_DS3231 rtc;
DateTime t;

// Definitions 

#define BUTTON_PIN_1  6      // pushbutton pin (this is also analog pin 0, but INTERNAL_PULLUP only works when you refer to this pin as '14')
#define BUTTON_PIN_2  8 
#define BUTTON_PIN_3  7
#define BUZZER  9 

#define SER_DATA_PIN  5     // serial data pin for VFD-grids shift register
#define SER_LAT_PIN  3      // latch pin for VFD-grids shift register
#define SER_CLK_PIN  2      // clock pin for VFD-grids shift register
#define DATE_DELAY 3000     // Delay before date is displayed (loop exectutions)
//#define SERIAL            // uncomment for debug

                            

word loopCounter; //A counter to set the display update rate
int stato = 0;
int alarmh, alarmm, alarms;
bool FIRST = true;
bool ALARM = false;
int ch, cm, cs;

/******************************************************************************* 
* Digits are lit by shiting out one byte where each bit corresponds to a grid. 
* 1 = on; 0 = off;
* msb = leftmost digit grid;
* lsb = rightmost digit grid.
*******************************************************************************/

/******************************************************************************* 
* Segments are lit by shiting out one byte where each bit corresponds to a segment:  G-B-F-A-E-C-D-dp 
* --A--                                                                              1 0 1 1 1 0 1 0
* F   B
* --G--
* E   C
* --D----DP (decimal point)
* 
* 1 = on; 0 = off;
*******************************************************************************/
 byte sevenSeg[20] = {
  B01111110, //'0'
  B01000100, //'1'
  B11011010, //'2'
  B11010110, //'3'
  B11100100, //'4'
  B10110110, //'5'
  B10111110, //'6'
  B01010100, //'7'
  B11111110, //'8'
  B11110110, //'9'
  B11110000, //degrees symbol
  B00111010, //'C'
  B00111000, //'t'
  B00101000, //'i'
  B01111100, //'m'
  B10111010, //'e'
  B11111100, //'a' 
  B00101010, //'l'
  B10001000, //'r'
  B10111000, //'f'
 };

/******************************************************************************* 
Funtion prototypes
*******************************************************************************/
void updateVFD(int pos, byte num, boolean decPoint);

void clearVFD(void);
void displayTime();
void displayDate();
void curretnTime();




void setup(){
  #ifdef SERIAL
  Serial.begin(9600);   // Setup Arduino serial connection
  #endif 
 
  pinMode(SER_DATA_PIN,OUTPUT);
  pinMode(SER_CLK_PIN,OUTPUT);
  pinMode(SER_LAT_PIN,OUTPUT);
  pinMode(BUTTON_PIN_1, INPUT_PULLUP); //Button Input pin with internal pull-up
  pinMode(BUTTON_PIN_2, INPUT_PULLUP);
  pinMode(BUTTON_PIN_3, INPUT_PULLUP);
  pinMode(BUZZER, OUTPUT);
  
  if (!rtc.begin()) {
    #ifdef SERIAL
    Serial.println("Controlla le connessioni");
    #endif
    while(true);
  }

  //uncomment to set RTC time
   /*
    if (rtc.lostPower()) {
    #ifdef SERIAL
    Serial.println("Imposto data/ora");
    #endif
    rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));    
    // rtc.adjust(DateTime(2014, 1, 21, 3, 0, 0));
   }
  */
 
}

void loop(){

      switch (stato) {
    case 0:
      currentTime();
    break; 
    case 1:
      setHour();
    break; 
    case 2:
      setMinute();
    break;
    case 3:
      setSecond();
    break;
     case 20:
      setAlarm();
      break;
    case 21:
      setAHour();
      break;
    case 22:
      setAMinute();
      break;
    case 23:
      setASecond();
      break;
    case 30:
      ringing();
      break;      
     }
  
}   //End of main program loop


void updateVFD(int pos, byte num, boolean decPoint){ //This shifts 16 bits out LSB first on the rising edge of the clock, clock idles low. Leftmost byte is position 7-0, rightmost byte is 7-seg digit
      
     if(pos >= 0 && pos < 6){               //Only accept valid positons on the display
      digitalWrite(SER_CLK_PIN, LOW);
      digitalWrite(SER_DATA_PIN, LOW);
      digitalWrite(SER_LAT_PIN, LOW);
      num = num + decPoint;                // sets the LSB to turn on the decimal point
      word wordOut = (1 << (pos+8)) + num; // concatenate bytes into a 16-bit word to shift out
      boolean pinState;

        for (byte i=0; i<16; i++){        // once for each bit of data
          digitalWrite(SER_CLK_PIN, LOW);
          if (wordOut & (1<<i)){          // mask the data to see if the bit we want is 1 or 0
            pinState = 0;                 // set to 1 if true
          }
          else{
            pinState = 1; 
          }
                 
          digitalWrite(SER_DATA_PIN, pinState); //Send bit to register
          digitalWrite(SER_CLK_PIN, HIGH);      //register shifts bits on rising edge of clock
          digitalWrite(SER_DATA_PIN, LOW);      //reset the data pin to prevent bleed through
        }
      
        digitalWrite(SER_CLK_PIN, LOW);
        digitalWrite(SER_LAT_PIN, HIGH); //Latch the word to light the VFD
        delay(1); //This delay slows down the multiplexing to get get the brightest display (but too long causes flickering)
      
      clearVFD();
    }
} 

void clearVFD(void){
    digitalWrite(SER_DATA_PIN, HIGH);          //clear data and latch pins
    digitalWrite(SER_LAT_PIN, LOW);
        for (byte i=0; i<16; i++){            //once for each bit of data
            digitalWrite(SER_CLK_PIN, LOW);
            digitalWrite(SER_CLK_PIN, HIGH);  //register shifts bits on rising edge of clock
            }
    digitalWrite(SER_CLK_PIN, LOW);
    digitalWrite(SER_LAT_PIN, HIGH);          //Latch the word to update the VFD
}



void displayTime(){
	
	  t = rtc.now();   // Get time from the DS3231

    ch = t.hour();
    cm = t.minute();
    cs = t.second();
    
	  
    #ifdef SERIAL
    Serial.print(ch); Serial.print(":"); Serial.println(cm);
    #endif

    
    
    byte tensHour = ch / 10; //Extract the individual digits
    byte unitsHour = ch % 10;
    byte tensMin = cm / 10;
    byte unitsMin = cm % 10;
    byte tensSec = cs / 10;
    byte unitsSec = cs % 10;
	
	
    updateVFD(5, sevenSeg[tensHour], false);  
    updateVFD(4, sevenSeg[unitsHour], true);
    //updateVFD(5, 128, false);                //dash
    updateVFD(3, sevenSeg[tensMin], false);  
    updateVFD(2, sevenSeg[unitsMin], true);
    //updateVFD(2, 128, false);                //dash
    updateVFD(1, sevenSeg[tensSec], false);  
    if (ALARM){      
      updateVFD(0, sevenSeg[unitsSec], true); 
    } else {
      updateVFD(0, sevenSeg[unitsSec], false);
    }
}

void displayDate(){
  
	  t = rtc.now();
	
    byte tensDay = t.day() / 10; //Extract the individual digits
    byte unitsDay = t.day() % 10;
    byte tensMon = t.month() / 10;
    byte unitsMon = t.month() % 10;
    byte thousandsYear = t.year() / 1000;
    byte hundredsYear = (t.year() /100) % 10;
    byte tensYear = (t.year() / 10) % 10;
    byte unitsYear = t.year() % 10;

    #ifdef SERIAL
	  Serial.print(t.year(), DEC);
    Serial.print('/');
    Serial.print(t.month(),DEC);
    Serial.print('/');
    Serial.println(t.day(),DEC);
    #endif
	
	

    for (int i = 0; i<7; i++){                       //scroll the display across the VFD
      for(word j = 0; j<25; j++){                      //j count determines the speed of scrolling
        updateVFD(i-3, sevenSeg[tensDay], false); 
        updateVFD(i-4, sevenSeg[unitsDay], false);   //with decimal point 
        updateVFD(i-5, sevenSeg[tensMon], false);  
        updateVFD(i-6, sevenSeg[unitsMon], false);    //with decimal point
        //updateVFD(i-7, sevenSeg[thousandsYear], false); 
        //updateVFD(i-8, sevenSeg[hundredsYear], false); 
        updateVFD(i-7, sevenSeg[tensYear], false);  
        updateVFD(i-8, sevenSeg[unitsYear], false);
      }
    }

for (int i = 0; i<4; i++){
    for(word j = 0; j<100; j++){   //Hold the display for a short time
        updateVFD(5, sevenSeg[tensDay], false); 
        updateVFD(4, sevenSeg[unitsDay], false);   //with decimal point 
        updateVFD(3, sevenSeg[tensMon], false); 
        updateVFD(2, sevenSeg[unitsMon], false);    //with decimal point
        //updateVFD(3, sevenSeg[thousandsYear], false); 
        //updateVFD(2, sevenSeg[hundredsYear], false); 
        updateVFD(1, sevenSeg[tensYear], false);  
        updateVFD(0, sevenSeg[unitsYear], false);
    }

    
        clearVFD();
        delay(200);
    

}
}


void currentTime() {
  
  loopCounter++;
     displayTime();
            
     if(loopCounter == DATE_DELAY){
        displayDate();
    loopCounter = 0;
  }

   if (ALARM && (ch == alarmh) && (cm == alarmm) && (cs == alarms)) {
      stato = 30;
      FIRST = true;
    }
  
  if (digitalRead(BUTTON_PIN_1) == LOW) {
    //imposta ora
    stato = 1;
    delay(300);
    FIRST = true;
  }

  if (digitalRead(BUTTON_PIN_2) == LOW) {
    //imposta l'allarme
    stato = 20;  
    delay(300);
    FIRST = true;
   
}
}

int seth = 0;

void setHour() {
        if (FIRST) {
        t = rtc.now();
        seth = t.hour();  
        FIRST = false;
        }        

        byte tens = seth / 10; //Extract the individual digits
        byte units = seth % 10;
        
        updateVFD(5, sevenSeg[tens], false); 
        updateVFD(4, sevenSeg[units], false);   
        
        if (digitalRead(BUTTON_PIN_2)== LOW) {
        seth++;
        delay(200);
        if (seth > 23) seth = 0;
        delay(200);  
        } 

        if (digitalRead(BUTTON_PIN_3)== LOW) {
        seth--;
        delay(200);
        if (seth < 0 ) seth = 23;
        delay(200);  
        } 
        if (digitalRead(BUTTON_PIN_1)== LOW) {
        //salva ora scelta e passa ai minuti
        rtc.adjust(DateTime(t.year(), t.month(), t.day(), seth, t.minute(), t.second()));
        stato = 2;
        FIRST = true;
        delay(200);
  }   
  }


int setm = 0;

void setMinute() {
        if (FIRST) {
        t = rtc.now();
        setm = t.minute();  
        FIRST = false;
        }        

        byte tens = setm / 10; //Extract the individual digits
        byte units = setm % 10;
        
        updateVFD(3, sevenSeg[tens], false); 
        updateVFD(2, sevenSeg[units], false);   
        
        if (digitalRead(BUTTON_PIN_2)== LOW) {
        setm++;
        delay(300);
        if (setm > 59) setm = 0;
        delay(200);  
        } 

        if (digitalRead(BUTTON_PIN_3)== LOW) {
        setm--;
        delay(300);
        if (setm < 0 ) setm = 59;
        delay(200);  
        } 
        if (digitalRead(BUTTON_PIN_1)== LOW) {
        //salva ora scelta e passa ai minuti
        rtc.adjust(DateTime(t.year(), t.month(), t.day(), t.hour(), setm, t.second()));
        stato = 3;
        FIRST = true;
        delay(200);
  }   
  }

int sets = 0;

void setSecond() {
        if (FIRST) {
        t = rtc.now();
        sets = t.second();  
        FIRST = false;
        }        

        byte tens = sets / 10; //Extract the individual digits
        byte units = sets % 10;
        
        updateVFD(1, sevenSeg[tens], false); 
        updateVFD(0, sevenSeg[units], false);   
        
        if (digitalRead(BUTTON_PIN_2)== LOW) {
        sets++;
        delay(300);
        if (sets > 59) sets = 0;
        delay(200);  
        } 

        if (digitalRead(BUTTON_PIN_3)== LOW) {
        sets--;
        delay(300);
        if (seth < 0 ) sets = 59;
        delay(200);  
        } 
        if (digitalRead(BUTTON_PIN_1)== LOW) {
        //salva ora scelta e passa ai minuti
        rtc.adjust(DateTime(t.year(), t.month(), t.day(), t.hour(), t.minute(), sets));
        stato = 0;
        FIRST = true;
        delay(200);
  }   
  }

  void setAlarm(){
  if (FIRST) {
        for(int i=0; i<200; i++){
        updateVFD(5, sevenSeg[16], false); 
        updateVFD(4, sevenSeg[17], false);   
        updateVFD(3, sevenSeg[16], false); 
        updateVFD(2, sevenSeg[18], false); 
        updateVFD(1, sevenSeg[14], false); 
        }
          
    FIRST = false;
  }        
       

  if (digitalRead(BUTTON_PIN_2) == LOW) {
    ALARM = !ALARM;    
    delay(200);  
  }
  if (digitalRead(BUTTON_PIN_1) == LOW) {
    if (!ALARM) stato = 0;
    else stato = 21;
    delay(200);
    FIRST = true;
  }

  if (ALARM){
        updateVFD(5, sevenSeg[0], false); 
        updateVFD(4, sevenSeg[14], false);       
  }
  else {
        updateVFD(5, sevenSeg[0], false); 
        updateVFD(4, sevenSeg[19], false);   
        updateVFD(3, sevenSeg[19], false); 
      }
}

void setAHour() {
  if (FIRST) {
    t = rtc.now();
    alarmh = t.hour();      
    FIRST = false;
  }
        byte tens = alarmh / 10; //Extract the individual digits
        byte units = alarmh % 10;
        
        updateVFD(5, sevenSeg[tens], false); 
        updateVFD(4, sevenSeg[units], false);   
        
        if (digitalRead(BUTTON_PIN_2) == LOW) {
        alarmh++;
        delay(200);
        if (alarmh > 23) alarmh = 0;
        delay(200);  
        } 

        if (digitalRead(BUTTON_PIN_3) == LOW) {
        alarmh--;
        delay(200);
        if (alarmh < 0 ) alarmh = 23;
        delay(200);  
        }

        if (digitalRead(BUTTON_PIN_1) == LOW) {
        //salva ora scelta e passa ai minuti
        stato = 22;
        FIRST = true;
        delay(200);
        }   
}

void setAMinute() {
  if (FIRST) {
    t = rtc.now();
    alarmm = t.minute();    
    FIRST = false;
  }

        byte tens = alarmm / 10; //Extract the individual digits
        byte units = alarmm % 10;
        
        updateVFD(3, sevenSeg[tens], false); 
        updateVFD(2, sevenSeg[units], false);   
        
        if (digitalRead(BUTTON_PIN_2) == LOW) {
        alarmm++;
        delay(200);
        if (alarmm > 59) alarmm = 0;
        delay(200);  
        } 

        if (digitalRead(BUTTON_PIN_3) == LOW) {
        alarmm--;
        delay(200);
        if (alarmm < 0 ) alarmm = 59;
        delay(200);  
        }

        if (digitalRead(BUTTON_PIN_1) == LOW) {
        //salva ora scelta e passa ai minuti
        stato = 23;
        FIRST = true;
        delay(200);
        } 
}

void setASecond() {
  if (FIRST) {
     
    t = rtc.now();
    alarms = t.second();      
    FIRST = false;
  }

          
        byte tens = alarms / 10; //Extract the individual digits
        byte units = alarms % 10;
        
        updateVFD(1, sevenSeg[tens], false); 
        updateVFD(0, sevenSeg[units], false);   
        
        if (digitalRead(BUTTON_PIN_2) == LOW) {
        alarms++;
        delay(200);
        if (alarms > 59) alarms = 0;
        delay(200);  
        } 

        if (digitalRead(BUTTON_PIN_3) == LOW) {
        alarms--;
        delay(200);
        if (alarms < 0 ) alarms = 59;
        delay(200);  
        }

        if (digitalRead(BUTTON_PIN_1) == LOW) {
        //salva ora scelta e passa ai minuti
        stato = 0;
        FIRST = true;
        delay(200);
        }

}

int ringing(){
  if (FIRST) {
    FIRST = false;
  }
  tone(BUZZER, 440, 100);
  updateVFD(5, sevenSeg[17], false); 
  updateVFD(4, sevenSeg[16], false);   
  updateVFD(3, sevenSeg[12], false); 
  updateVFD(2, sevenSeg[15], false); 
  
  if ((digitalRead(BUTTON_PIN_1) == LOW) 
  || (digitalRead(BUTTON_PIN_2) == LOW)
  || (digitalRead(BUTTON_PIN_3)) == LOW) {
    FIRST = true;
    stato = 0;
    delay(300);  
  }
}
