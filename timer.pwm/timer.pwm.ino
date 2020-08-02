#include <TM1637Display.h>
#include <ClickEncoder.h>
#include <TimerOne.h>

#define numberOfcentiSeconds( _time_ ) ( _time_ / 10 )                // amount of centiseconds
#define numberOfSeconds( _time_ ) (( _time_ / 1000 ) % 60 )           // amount of seconds
#define numberOfMinutes( _time_ ) ((( _time_ / 1000 ) / 60 ) % 60 )   // amount of minutes 
#define numberOfHours( _time_ ) (( _time_ / 1000 ) / 3600 )           // amount of hours

const int enc_pin_A = 5;                                              // rotary encoder first data pin A at D5 pin
const int enc_pin_B = 4;                                              // rotary encoder second data pin B at D4 pin, if the encoder moved in the wrong direction, swap A and B
const int enc_pin_SW = 7;                                             // rotary encoder switch button at D7 pin
const int pwm_pin = 6;                                                // PWM output to gear motor at D6 pin      

bool pwmset = false;                                                  // no pwm menu at start
bool colon = true;                                                    // timer colon active at start
bool done = true;                                                     
int PWM, lastPWM, dutyPWM, timerHours, timerMinutes, timerSeconds;
int16_t value, lastValue;
unsigned long colon_ms, timeLimit, timeRemaining;

TM1637Display display( 2, 3 );                                        // TM1637 CLK connected to D2 and DIO to D3

ClickEncoder *encoder;



void timerIsr() {                                                     // encoder interupt service routine
  
  encoder->service();  
  
}



void setup(){
  
  Serial.begin( 9600 );                                               // serial for debug

  display.setBrightness( 0x02 );                                      // low brightness, to mimimize LM1117-5V current and overheating, maximum is 7
 
  encoder = new ClickEncoder( enc_pin_A, enc_pin_B, enc_pin_SW, 4 );  // rotary encoder init, last value is encoder steps per notch
  
  Timer1.initialize( 1000 );                                          // set the timer period in us, 1ms
  Timer1.attachInterrupt( timerIsr );                                 // attach the service routine
  
  pinMode( pwm_pin, OUTPUT );                                         
  lastValue = 0;
  PWM = 50;                                                           // 50% duty cycle as default
  colon_ms = millis();
  timerHours = 0;
  timerMinutes = 0;
  timerSeconds = 0;
  
}



void menuTimer() {
  
   unsigned long runTime;
   timeLimit = 0;
  
   while  ( done && !pwmset )  {
      value += encoder -> getValue();
            if ( value > lastValue ) {
              timerMinutes += 30;                                     // one rotary step is 30 minutes
              if (timerMinutes >= 99) {
                timerHours = 99;                                      // max 99 hours
                timerMinutes = 0;
              }
              if (timerMinutes >= 60) {
                timerHours++;
                timerMinutes = 0;
              }
              calculateHours( true );     
            } 
            else if ( value < lastValue ) {
              if (timerMinutes == 0 && timerHours > 0) {
                timerHours--;
                timerMinutes = 30;
              }
              else if ( timerMinutes == 30) timerMinutes = 0;
              calculateHours( true );
            }
            if ( value != lastValue ) {
              lastValue = value;
              Serial.print( "Encoder value: " );
              Serial.println( value );
            }
                                                                      // display time to countdown, leading zeros active if no hours, colon active
    display.showNumberDecEx( timeToInteger(), 0x80 >> true , timerHours == 0 );
    
    buttonCheck();                                                    // check if rotary encoder button pressed
    
  }
   
  runTime = millis();                                                 // 1000 ms = 1s, so 1 minute is 60000 ms, and 1 hour is 3600000 ms
  timeLimit = timerHours * 3600000 + timerMinutes * 60000 + runTime;  // add the runtime until timer starts to timeLimit, limit is compared with mcu millis in main loop
   
}



void menuPWM()  {
  
  value += encoder -> getValue();
          if ( value > lastValue ) {
            if ( dutyPWM >= 100 )
              dutyPWM = 100;                                          // max duty 100%
            else
              dutyPWM += 5;                                           // one rotary step is 5%
            } 
          else if ( value < lastValue && dutyPWM > 0 )
            dutyPWM -= 5;
            
    PWM = dutyPWM;
    if ( lastPWM != PWM ) {
      Serial.print( "PWM value: " );
      Serial.println( PWM );
      analogWrite( pwm_pin, map( PWM, 0, 100, 0, 255 ));              // remap 0-100% duty range to 0-255
      lastPWM = PWM;
    }  
  
  if ( value != lastValue ) lastValue = value;
  
  display.showNumberDecEx( dutyPWM, 0x80 >> false , false );          // show pwm duty, no colon, no leading zeros
  
  buttonCheck();                                                      // check rotary encoder button
  timeCheck();                                                        // check timer if finished
  
}



void countdown() {

  int n_centisec = numberOfcentiSeconds( timeRemaining );             // amount of centiseconds in remaining time
  int n_seconds = numberOfSeconds( timeRemaining );                   // amount of seconds in remaining time
  int n_minutes = numberOfMinutes( timeRemaining );                   // amount of minutes in remaining time 
  int n_hours = numberOfHours( timeRemaining );                       // amount of hours in remaining time
  
   if (( millis() - colon_ms ) >= 500 ) {                             // colon is blinking with about 0.5s period
        colon_ms = millis();
        colon =! colon;
        if ( colon ) {                                                // print timer countdown with about 1s period
          if ( n_hours )  {
            Serial.print( n_hours );
            Serial.print( " Hours " );
          }
          if ( n_minutes )  {
           Serial.print( n_minutes );
            Serial.print( " Minutes " );
          }  
          Serial.print( n_seconds );
          Serial.println( " Seconds" );
        }
   }
   if ( !n_hours ) {                                                  
      if ( n_minutes )  {                                             // show minutes and seconds if no hours left
        n_hours = n_minutes;
        n_minutes = n_seconds;
      }
      else  {                                                         // show seconds and centiseconds if no minutes left
        n_hours = n_seconds;
        n_minutes = n_centisec;
    }
   }
                                                                      // show time, hours in first two positions, with colon and leading zeros enabled 
   display.showNumberDecEx( n_minutes, 0x80 >> colon, true, 2, 2 );
   display.showNumberDecEx( n_hours, 0x80 >> colon, true, 2, 0 );

   buttonCheck();                                                     // check rotary encoder button
   timeCheck();                                                       // check timer if finished
   
}



int timeToInteger() {
  
  int result = 0;
  result += timerHours * 100;
  result += timerMinutes;
  return result;
  
}



void calculateHours( bool asce ) {
  
  if ( timerSeconds == -1 && !asce ) {
    if ( timerMinutes > 0 || timerHours > 0 ) {
      timerMinutes--;
      timerSeconds = 59; 
    }   
  }
  
  if ( timerMinutes == 60 && asce ) {
    timerHours++;
    timerMinutes = 0;  
  } 
  else if ( timerMinutes == -1 && !asce && timerHours > 0 ) {
    timerHours--;
    timerMinutes = 59;
  }
}



void buttonCheck() {
  
 ClickEncoder::Button b = encoder -> getButton();
   if ( b != ClickEncoder::Open ) {
      Serial.print( "Button: " );
      
      #define VERBOSECASE( label ) case label: Serial.println( #label ); break;
      
      switch ( b ) {
         VERBOSECASE( ClickEncoder::Pressed );
         VERBOSECASE( ClickEncoder::Released )
         VERBOSECASE( ClickEncoder::DoubleClicked )
         
       case ClickEncoder::Clicked:
         Serial.println( "ClickEncoder::Clicked" );
         if ( !isTimerFinished() )  {                                 // can't set pwm duty or start countdown if timer not set (00:00)
            if ( !pwmset )  {                                         // set pwm duty to gear motor
                analogWrite( pwm_pin, map( PWM, 0, 100, 0, 255 ));
                dutyPWM = PWM;
                pwmset = true;
                Serial.println( "PWM set" );
            }
            else {                                                    // start or go back to countdown if pwm set 
              done = false;
              pwmset = false;
              Serial.println( "Countdown" );
            } 
         }
       break;
                
       case ClickEncoder::Held:                                       // timer reset if rotary encoder button held for about 2s
         Serial.println( "ClickEncoder::Held" );
         if ( !done ) timerFinished();
       break;
      } 
   }
}



void timeCheck() {
  
  timeRemaining = timeLimit - millis();                               // calculate time remaining
    
  if ( timeRemaining < 100 ) timerFinished();                         // timer reset if coundown finished

}



bool isTimerFinished() {
  
  return timerHours == 0 && timerMinutes == 0 && timerSeconds == 0;  
  
}



void timerFinished()  {
  
  timerHours = 0;                                                     // timer reset, disable gear motor
  timerMinutes = 0;
  timerSeconds = 0; 
  value = encoder -> getValue();
  lastValue = value;                                                  // set last encoder value
  pwmset = false;
  done = true;
  analogWrite( pwm_pin, 0 );
  Serial.println( "Timer finished" );
  
}



void loop() {
  
  if ( !pwmset ) {
    if (done) menuTimer();
    else countdown();
  }
  else
    menuPWM();

}
