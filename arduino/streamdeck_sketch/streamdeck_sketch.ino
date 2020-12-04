#define MAGIC_WORDS       "ccstreamdeck" // first words that are send so the pc knows that this is the stream deck

#define BAUD_RATE         9600  // baud rate for serial communication
#define NUM_BUTTONS       2     // number of physical buttons connected 
#define BUTTON_START_PIN  2     // first pin of first button, second button needs to be connected to BUTTON_START_PIN+1, etc.
#define GREEN_LED_PIN     18    // IO pin of green led
#define RED_LED_PIN       13    // IO pin of red led

#define BUTTON_TRIGGER_COOLDOWN 100 // (ms) how long until trigger event can be sent again

// array keeping track of button states (1: pressed, 0: released)
int BUTTON_STATES[NUM_BUTTONS];

// keep track of last button trigger
unsigned long LAST_BUTTON_TRIGGER_TIME = 0;

// buffer for reading incoming data
#define READ_BUFFER_SIZE  256
char READ_BUFFER[READ_BUFFER_SIZE];

int CONNECTED = 0;
void setup() {
  // set button pins as input
  for(int i = 0; i < NUM_BUTTONS; i++){
    BUTTON_STATES[i] = 0;
    pinMode(BUTTON_START_PIN+i, INPUT);
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
  }
  
}

void loop() {
  if(CONNECTED){
    for(byte i = 0; i < NUM_BUTTONS; i++){
      int btn_state_before = BUTTON_STATES[i];
      BUTTON_STATES[i] = digitalRead(BUTTON_START_PIN+i);
      if(btn_state_before == 0 && BUTTON_STATES[i] == 1 && (millis()-LAST_BUTTON_TRIGGER_TIME) > BUTTON_TRIGGER_COOLDOWN){
        Serial.write(&i, 1);
        LAST_BUTTON_TRIGGER_TIME = millis();
      }
    }
    delay(1);
  }
  else{
    digitalWrite(RED_LED_PIN, HIGH);
    delay(500);
    digitalWrite(RED_LED_PIN, LOW);
    delay(500);
  }
}
