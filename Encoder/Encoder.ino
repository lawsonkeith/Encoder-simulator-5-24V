/* 
 Encoder sim
 -----------
 
 K Lawson 2014
 
 Use a pot to control the cycle speed of an encoder simulator.  An A and B channel drive in either direction at speeds from 1-10K Hz.
 A bi-colour LED indiates speed and when we reach 1k and 10kHz.
 
 When in the mid-position the unit is 'off'; this is taken from a dedband.
 
 The encoder SM has 4 states and outputs at 4x lower f than it's called.
 The analog sampling and LED control are done using ms timing.
 
 1-1KHz output frequency
 
 Green LED    0-10Hz
 Red LED      10-.1KHz
 Bi           .1K - 1K
 
 
 Notes on Mini
 -------------
 
 The mini is not pin-pin compatible with the minipro programming header and an adapter
 header needs to e made to program the unit.  This is done using a 5 w 2.54 header strip
 remove a min and bend the last one then either solder or friction fit to program it.
 
          PGMR
 | dtr rx tx vcc cts gnd |
 -------------------------
       |  |  |     __|
       |  |  |    /
 -------------------------
 | x  tx rx vcc gnd  x    |
 
 reset btn just before pgm.  Use 2.54 header to make interface and balance
 in holes.
 
 */

// Pin 13 has an LED connected on most Arduino boards.
// give it a name:
int led = 13;

// time delay constants (us).  First 14 are for deadband.  This is us delay times for a 5 cycle state machine.
// f = 250'000/t  Hz
// so 250'000 = 1Hz   and 250 = 1KHz
//
// note check the clock frequency is right; some chinese 3.3 models are clocked at 16Mhz; if so the set the model to the 
// 5V arduino 16Mhz model in the IDE.  Otherwise the timer functions are all out.
//
unsigned long scale[256] = {  
//DEADBAND  
1000,1000,1000,1000,1000,1000,1000,1000,1000,1000,1000,1000,1000,1000,       
//.25 Hz (Responder testing - 100Hz
2500000, 2500000, 2500000, 2500000, 2500000, 2500000, 2500000, 900000, 600000, 250000,225225,204918,187969,173611,161290,150602,141242,
132978,125628,119047,113122,107758,102880,98425,94339,90579,87108,83892,80906,78125,75528,73099,68681,66666,64766,62972,61274,
59665,58139,56689,55309,53995,52742,51546,50403,49309,48262,47258,46296,45372,44483,43630,42808,42016,41254,40518,39808,39123,38461,
37821,37202,36603,36023,35460,34916,34387,33875,33377,32894,32425,31969,31525,31094,30674,30266,29481,29103,28735,28376,28026,
27685,27352,27027,26709,26399,26096,25799,25510,25000,22321,20161,18382,16891,15625,14534,13586,12755,12019,11363,10775,10245,
9765,9328,8928,8561,8223,7911,7621,7352,7102,6868,6648,6443,6250,6067,5896,5733,5580,5434,5296,5165,4921,4807,4699,4595,4496,
4401,4310,4222,4139,4058,3980,3906,3834,3765,3698,3633,3571,3511,3453,3396,3342,3289,3238,3188,3140,3094,3048,3004,2962,2880,2840,
2802,2765,2729,2693,2659,2626,2593,2561,2530,
//100-1kHz
2500,2500,2500,2500,2500,2500,2500,2500,2500,2500,2500,2500,1923,1562,1315,1136,1000,892,806,735,675,625,581,543,510,480,454,431,
409,390,373,357,342,328,304,284,274,265,257,
//1k-10kHz
250,250,250,250,250,250,250,250,250,250,192,156,131,113,100,89,80,73,67,62,58,54,51,48,45,43,40,39,37,35,34,32,31,30,29,28,27,26,
24,24,24,24
};



// the setup routine runs once when you press reset:
void setup() {                
  // initialize the digital pin as an output.
  pinMode(led, OUTPUT);  

  //A-B drive
  pinMode(8,OUTPUT);
  pinMode(9,OUTPUT);

  //BI LED drive
  pinMode(10,OUTPUT);
  pinMode(11,OUTPUT);

  //AI
  // AI 0 = pot

  //debug
  Serial.begin(9600);  
}//END Setup





// the loop routine runs over and over again forever:
void loop() 
{
  static unsigned long msDelay, usDelay, TrigTime, SampleTime, FlashTime, uTime;
  static byte Pos,Speed,SeqEnd,Power, On;
  static int x;

  uTime = micros();

  
 
  // @@@@@@@@@@ 100Hz loop @@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@
  if(uTime > SampleTime)
  {
    SampleTime += 10000;
    DecodePot(&msDelay, &usDelay, &Pos, &Speed, &Power, &On);  

    //Drive LEDs at rate and colour depending on freq; uses timer to flash led
    //1-100        G
    //100-1000     R
    //1000=10000   Mix
    //
    DriveLED(Speed,Power,On); 
  }//End sample loop

  // @@@@@@@@@@ 1-40KHz FAST Driver loop ( variable freq ) @@@@@@@@@@@@@
  //
  if(uTime >= TrigTime)
  {
    TrigTime += usDelay;

    //drive IO; 5 cycle state machine Tell it dir and on/off status
    SimulateEncoder(Pos,On);


  }//DELAY

  // Catch any rollover strangeness
  if(uTime < 100)
  {
      SampleTime  = 0;
      TrigTime = 0;
  }
  
}//END Loop




// read the freq pot.  0-3V equates to 0-1023
// The delays vary a lot so we output ms or us.
// Pos tells the drive module to go fwd / rev
// pow describes the output frequncy and is used to
// change the LED colour.
//
void DecodePot(unsigned long *msDelay, unsigned long *usDelay, byte *Pos, byte *Speed, byte *Power, byte *On)
{
  unsigned long val;
  int index;
  static int LastVal;


  // 0-1023;
  index = analogRead(0);  //0-1023 

  // time saver; only do this stuff if there's a change
  /*if(index == LastVal)
   return;
   else
   LastVal = index;*/

  index -= 512; //-512  <>   511

  //go fwd / back
  if(index<0)
    *Pos = 0;
  else
    *Pos = 1;


  //get the scaled time delay from the scale array/
  //previously derived from excel
  // / by 2 as thats the res of the lookup table
  index = (abs(index) - 1) >> 1; //0-255
  val = scale[index] ; //originlly did 10K; now just go to 1KHz
//Serial.print(val);
//Serial.print(",");
//Serial.println(index);

  *Speed = index; //for LED drive delay 255 = fast 0 = slow

  //on off indicator
  if(*Speed < 15)
    *On = 0;
  else
    *On = 1;

  // use the fast or slow timer since us has a 65k max time delay
  // after that val use ms timer
  *usDelay = val; 

  // set power of indicator to change LED colour
  //     
  // 0   200  2000   300000us
  //   2     1      0 
  *Power=0; // 0-100 hz
  if(val <= 250 ) //  1k-10k
    *Power = 2;
  else if(val <= 2500) // 100-1k
    *Power=1;

}//End DecodePot





// Drive the LED this is done using a bi-colour LED. 2 DOs decide
// the colour red or green and a third colour is used also
// The log power is used to determine the LED flash colour and
// when the colour chnges the flash rate is divided down so it's 
// visible.
//
// Colour   Drive               Hz          Pow
// ---------------------------------------------
// GREEN    Gn                  0-100        0
// YELLOW   Red + Gn alternate  100-1000     1
// RED      Red                 1000-10000   2
//
// This  routine is called at a non constant frequency therefore a timer
// is used to flash the LED.
//
void  DriveLED(byte Speed, byte Pow, byte On)
{
  static char toggle,subtoggle;
  static int divisor;
  char trigger=0,A,B;
  static unsigned long TimeToggle;

  // Speed = 0-255
  // 0=1000ms 255=5ms
  if(millis() > TimeToggle)
  {
    //set next time to toggle leds (ms)
    TimeToggle = 260 - Speed;
    TimeToggle += millis();

    //toggle bit
    toggle++;
    toggle &= 1; 

    if(toggle && On)
    {
      //toggle LED
      if(Pow == 0) //Gn
      {
        digitalWrite(10,HIGH);
        digitalWrite(11,LOW);
      }
      else if (Pow == 2) //Yl
      {
        if(subtoggle==0)
        {
          digitalWrite(10,HIGH);
          digitalWrite(11,LOW); 
        }
        else
        {
          digitalWrite(10,LOW);
          digitalWrite(11,HIGH);
        }
        //toggle bit
        subtoggle++;
        subtoggle &= 1; 

      }
      else //Rd
      {
        digitalWrite(10,LOW);
        digitalWrite(11,HIGH); 
      }
    }
    else
    {
      digitalWrite(10,LOW);
      digitalWrite(11,LOW);
    }//END toggle
  }//END if time

}//END DriveLEDs










// Simulate an encoder using 2 DOs.  The sim goes fwd or backwards
// and raises  flag when the cycle is complete.
// the simulation has 5 states.
//
// 4 cycles:
// A--0 0 1 1
// B--0 1 1 0
void SimulateEncoder(byte Pos, byte On)
{
  static char State=0;
  char A,B;

  // Direction change using IO mapping
  // make encoder go fwd / backwards
  if(Pos)
  {
    A=8;
    B=9;
  }
  else
  {
    A=9;
    B=8;
  }

  //manage states
  if(On)
  {
    State++;
  }
  else
  {
    digitalWrite(A, LOW);
    digitalWrite(B, LOW); 
    digitalWrite(led, LOW);
    State = 0; 
  }

  //Encoder sim  
  switch(State)
  {
  case 0: 
    digitalWrite(A, LOW);//Serial.println("1"); 
    //digitalWrite(B, LOW); //S L OOOOO W
    //digitalWrite(led, LOW);
    break;
  case 1: 
    //digitalWrite(A, LOW);//Serial.println("2"); 
    digitalWrite(B, HIGH);
    digitalWrite(led, HIGH);  // onboard LED
    break;
  case 2: 
    digitalWrite(A, HIGH);//Serial.println("3"); 
    //digitalWrite(B, HIGH);
    //digitalWrite(led, HIGH);
    break;
  case 3: 
    //digitalWrite(A, HIGH);//Serial.println("4"); 
    digitalWrite(B, LOW);
    digitalWrite(led, LOW);  // onboard LED
    State = -1;
    break;
  default:
    State = 0;
  }//END case

}//END SimulateEncoder




