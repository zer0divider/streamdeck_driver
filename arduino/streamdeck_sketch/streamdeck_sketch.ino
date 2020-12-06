#define MAGIC_WORDS       "ccstreamdeck" // first words that are send so the pc knows that this is the stream deck

#define BAUD_RATE         9600  // baud rate for serial communication
#define NUM_BUTTONS       15     // number of physical buttons connected 
#define GREEN_LED_PIN     12    // IO pin of green led
#define RED_LED_PIN       13    // IO pin of red led

#define BUTTON_TRIGGER_COOLDOWN 100 // (ms) how long until trigger event can be sent again
#define CONNECTION_LOST_TIMEOUT 500 // (ms) how long until connection is lost if no

#define PULL_UP_RESISTOR  // if PULL_UP_RESISTOR is defined, buttons connect input pins to GROUND when pressed, the internal pull up resistors of the arduino are used
                          // if PULL_UP_RESISTOR is NOT defined, buttons connect input pins to VDD when pressed, external pull down resistors have to be connected to the arduino

// assign gpio pin to each button
int BUTTON_PINS[NUM_BUTTONS] = {
  2, // button 1
  3, // button 2
  4, // button 3
  5, // button 4
  6, // button 5
  7, // button 6
  8, // button 7
  9, // button 8
  10,// button 9
  11,// button 10
  14,// button 11
  15,// button 12
  16,// button 13
  17,// button 14
  18 // button 15
};

// array keeping track of button states (1: pressed, 0: released)
int BUTTON_STATES[NUM_BUTTONS];

// keep track of last button trigger
unsigned long LAST_BUTTON_TRIGGER_TIME[NUM_BUTTONS];

// buffer for reading incoming data
#define READ_BUFFER_SIZE  256
char READ_BUFFER[READ_BUFFER_SIZE];

byte BUTTON_ACTIVE = 0; // key sequence is currently being processed (send by host)
unsigned long LAST_ACTIVE_RECEIVED = 0;

int CONNECTED = 0;
void setup() {
  // set button pins as input
  for(int i = 0; i < NUM_BUTTONS; i++){
    BUTTON_STATES[i] = 0;
    LAST_BUTTON_TRIGGER_TIME[i] = 0;
    #ifdef PULL_UP_RESISTOR
      pinMode(BUTTON_PINS[i], INPUT_PULLUP);
    #else
      pinMode(BUTTON_PINS[i], INPUT);
    #endif
  }

  // set led pins as output
  pinMode(RED_LED_PIN, OUTPUT);
  pinMode(GREEN_LED_PIN, OUTPUT);
  digitalWrite(RED_LED_PIN, LOW);
  digitalWrite(GREEN_LED_PIN, LOW);

  // initialize serial communication and check magic word sent back
  Serial.begin(BAUD_RATE);
  Serial.write(MAGIC_WORDS);
  Serial.flush();
  int magic_words_len = strlen(MAGIC_WORDS);
  int read_bytes = 0;
  while(read_bytes < magic_words_len){
    read_bytes += Serial.readBytes(READ_BUFFER+read_bytes, READ_BUFFER_SIZE-read_bytes);
  }
  READ_BUFFER[magic_words_len] = '\0';
  if(!strcmp(READ_BUFFER, MAGIC_WORDS)){// magic word sent back is correct -> connect
    digitalWrite(RED_LED_PIN, HIGH);
    CONNECTED = 1;
    byte zer0 = 0;
    Serial.write(&zer0, 1);
    LAST_ACTIVE_RECEIVED = millis();
  }
  
}

void check_time_overflow(int i)
{
  if(millis() < LAST_BUTTON_TRIGGER_TIME[i]) // millis flowed over
    LAST_BUTTON_TRIGGER_TIME[i] = 0;
}

void loop() {
  if(CONNECTED){
    // check buttons pressed
    for(byte i = 0; i < NUM_BUTTONS; i++){
      int btn_state_before = BUTTON_STATES[i];
      BUTTON_STATES[i] = digitalRead(BUTTON_PINS[i]);
      #ifdef PULL_UP_RESISTOR
        BUTTON_STATES[i] = 1-BUTTON_STATES[i];
      #endif
      check_time_overflow(i);
      if(btn_state_before == 0 && BUTTON_STATES[i] == 1 && (millis()-LAST_BUTTON_TRIGGER_TIME[i]) > BUTTON_TRIGGER_COOLDOWN){
        byte btn_id = i+1;
        Serial.write(&btn_id, 1);
        LAST_BUTTON_TRIGGER_TIME[i] = millis();
      }
    }

    // check data received by host (button active or not)
    byte b;
    if(Serial.readBytes(&b, 1) == 1){
      BUTTON_ACTIVE = (b != 0);
      digitalWrite(GREEN_LED_PIN, BUTTON_ACTIVE);
      LAST_ACTIVE_RECEIVED = millis();
    }
    if(millis() - LAST_ACTIVE_RECEIVED >= CONNECTION_LOST_TIMEOUT){
      CONNECTED = false;
      digitalWrite(GREEN_LED_PIN, LOW);
    }
    delay(1); 
  }
  else{
    // send signal - not connected
    digitalWrite(RED_LED_PIN, HIGH);
    delay(500);
    digitalWrite(RED_LED_PIN, LOW);
    delay(500);
  }
}
