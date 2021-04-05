#include <Adafruit_PN532.h>
#include <EEPROM.h>
#include <LiquidCrystal.h>

//LCD initialization
LiquidCrystal lcd(8, 9, 4, 5, 6, 7);

//pins to control the electric strike and buzzer
#define CONTROL_PIN 11
#define BUZZ_PIN A3

//pn532 i2c pins declaration
#define PN532_IRQ   (2)
#define PN532_RESET (3)

#define UID_LENGTH 4 //can be 7 depending on type of ISO14443A card type


//pn532 i2c communication-type declaration
Adafruit_PN532 nfc(PN532_IRQ, PN532_RESET);

//state variable declaration
bool altState = false;

char ButtonRead(int buttonVal) {
  /*
    Returns lowercase first character of button name
    given buttonVal - usually analogRead(A0).
  */
  if (buttonVal >= 950) { // nothing pressed
    return 0;
  }
  else if (buttonVal <= 50) { // RIGHT
    return 'r';
  }
  else if (buttonVal > 50 && buttonVal <= 200) { // UP
    return 'u';
  }
  else if (buttonVal > 200 && buttonVal <= 350) { // DOWN
    return 'd';
  }
  else if (buttonVal > 350 && buttonVal <= 600) { // LEFT
    return 'l';
  }
  else if (buttonVal > 600) { // SELECT
    return 's';
  }
}

void writeUID(uint8_t uid[], int address){
  for(int i = 0; i < 4; i++){
    EEPROM.update(address, uid[i]);
    address = address + sizeof(uint8_t);
  }
}

void readUID(uint8_t arr[], int address){
  for(int i = 0; i < 4; i++){
    arr[i] = EEPROM.read(address);
    address = address + sizeof(uint8_t);
    //DEBUG
    //Serial.println(arr[i]);
    //END DEBUG
  }
}

bool checkUID(uint8_t uid[]){
  /*
   * Checks whole EEPROM for uid
   * returns 'true' if found
   * or 'false' if not
   */
  int address = 0;
  uint8_t tempArray[4];
  while (address < 1025){
    readUID(tempArray, address);
    address = address + (4 * sizeof(uint8_t));
    for (int i = 0; i < 4; i++){
      if (uid[i] != tempArray[i]){
        break;
      }
      else if (i == 4){
        return true;
      }
    }
  }
  return false;
}

void sendSignalToStrike(const int pin, const int seconds = 10){
  tone(BUZZ_PIN, 1000);
  digitalWrite(pin, HIGH);
  delay(seconds * 1000);
  digitalWrite(pin, LOW);
  noTone(BUZZ_PIN);
}

void standardState(const int controlPin = CONTROL_PIN) {
  /*
    This is the standard state of the program.
    In this state the program waits for signals from
    pn532 module and - if authentication passes successfully -
    sends control signals to specified control pin.
  */
  uint8_t success;
  uint8_t uid[] = { 0, 0, 0, 0, 0, 0, 0 };
  while (ButtonRead(analogRead(A0)) != 'l' and ButtonRead(analogRead(A0)) != 'r'){
    Serial.println("Waiting for an ISO14443A Card ...");
    // Wait for an ISO14443A type cards (Mifare, etc.).  When one is found
    // 'uid' will be populated with the UID
    success = nfc.readPassiveTargetID(PN532_MIFARE_ISO14443A, uid, UID_LENGTH);

    if (success){
      //DEBUG
      Serial.println("ID of card:");
      for (int i = 0; i < 6; i++){
        Serial.print(uid[i]);
        Serial.print(" ");
      }
      Serial.println(" ");
      //END DEBUG

      if (checkUID(uid)){
        //DEBUG
          Serial.println("Check successfull");
        //END DEBUG
        sendSignalToStrike(controlPin, 2);
      }
    }
  }
  altState = true;
}

void alternateState(short seconds = 10;){
  /*
   * This is alternate state of the program.
   * In this state program will:
   * 1. Ask for already authenticated card
   * 2. Ask for new card
   * 3. If the authentication is successfull the program
   * will add the new card to the authentication database
   * 
   * Worth to note: if the user doesn't do anything for few seconds
   * alternateState will terminate and go back to standardState.
   */
  uint8_t success;
  uint8_t success2;
  uint8_t uid[] = { 0, 0, 0, 0, 0, 0, 0 };
  uint8_t uid2[] = { 0, 0, 0, 0, 0, 0, 0 };
  int elapsedTime = 0;
  while (ButtonRead(analogRead(A0)) != 'l' and ButtonRead(analogRead(A0)) != 'r'){
    Serial.println("Waiting for working card");
    success = nfc.readPassiveTargetID(PN532_MIFARE_ISO14443A, uid, UID_LENGTH);
    if (success){
      elapsedTime = 0;
      if (!checkUID){
        Serial.println("Card not found in the database - try again");
        continue;
      }
      else{
        Serial.println("Card authenticated successfully - waiting for second card");
        success = nfc.readPassiveTargetID(PN532_MIFARE_ISO14443A, uid, UID_LENGTH);
        if (success2){
          elapsedTime = 0;
          //TODO:
          //add saving cards to memory
          //to this you will need some way of finding 'clear' addresses
          //in the EEPROM
          //also we will need some way of deleting cards from eeprom
        }
      }
    }

    delay(10);
    elapsedTime++;
    if (elapsedTime == seconds * 100){
      Serial.println("Timeout - try again");
      break;
    }
  }
}

void checkState(){
  if (altState == true){
    alternateState();
  }
  else {
    standartState();
  }
}

void setup() {
  //Serial for debugging
  Serial.begin(115200);

  //start the i2c communication
  nfc.begin();

  //BEGIN startup debug
  uint32_t versiondata = nfc.getFirmwareVersion();
  if (! versiondata) {
    Serial.print("Didn't find PN53x board");
    while (1); // halt
  }
  Serial.print("Found chip PN5"); Serial.println((versiondata >> 24) & 0xFF, HEX);
  Serial.print("Firmware ver. "); Serial.print((versiondata >> 16) & 0xFF, DEC);
  Serial.print('.'); Serial.println((versiondata >> 8) & 0xFF, DEC);
  //END startup debug

  // configure board to read RFID tags
  nfc.SAMConfig();

  //temporary led declaration
  pinMode(11, OUTPUT);

}

void loop() {
  standardState();
}
