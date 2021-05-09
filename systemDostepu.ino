#include <Adafruit_PN532.h>
#include <EEPROM.h>
#include <LiquidCrystal.h>

//pins to control the electric strike and buzzer
#define CONTROL_PIN 11
#define BUZZ_PIN A3

//pn532 i2c pins declaration
#define PN532_IRQ   (2)
#define PN532_RESET (3)

uint8_t uidLength = 4; //length of readed uid - for my cards should be 4, can be 7 for other cards

//pn532 i2c communication-type declaration
Adafruit_PN532 nfc(PN532_IRQ, PN532_RESET);

//
LiquidCrystal lcd(8, 9, 4, 5, 6, 7);

//declaration of last admin card Address
//those addresses cant be deleted/rewrited and have additional permissions
//such as adding new cards to memory.
//Default is 0.
//This const has to be the BEGINNING (for example 0) of last admin card address
//not the end of last admin card address.
#define ADMIN_ADDRESS 0

//State variable declaration. The program will start in this state. Default 0.
//Change to 1 for adding state or 2 for deleting state.
short state = 0;

//Variables needed to succesfully detect cards
int irqCurr;
int irqPrev;

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

int seekNextEmptyAddress(){
    int address = ADMIN_ADDRESS + 4; //starting address is the next one after admin addresses
    int i = 0;
    while(i < 1024){
        if(i % 4 == 0 && EEPROM.read(i) == 0){
            return i;
        }
        i = i + 4;
    }
    return -1;
}

int writeUID(uint8_t uid[], int address, short uidLength){
    /*
        Writes UID of uidLength to specified address.
        Returns next address if successfull.
    */
    if (address == -1){
        address = seekNextEmptyAddress();
    }

    for(int i = 0; i < uidLength; i++){
        EEPROM.update(address, uid[i]);
        address++;

        //DEBUG
        //Serial.println(address)
        //END DEBUG
    }

    //DEBUG
    Serial.print("\nWriten uid to memory on address: ");
    Serial.print(address - 4);
    Serial.print("\nUID value: ");
    for(int i = 0; i < 4; i++){
        Serial.print(uid[i]);
        Serial.print(" ");
    }
    Serial.print("\n");
    //END DEBUG

    return address;
}

int readUID(uint8_t arr[], int address, short uidLength){
    /*
        Reads uid of uidLength from specified address.
        Returns next address if successfull

        Returns -1 if specified address is invalid
    */

    if (address % uidLength != 0){              // Check for invalid address
        return -1;
    }

    for(int i = 0; i < uidLength; i++){
        arr[i] = EEPROM.read(address);
        address++;

        //DEBUG
        //Serial.println(arr[i]);
        //END DEBUG

    }
    return address;
}

int checkUID(uint8_t uid[], short uidLength){
    /*
        Checks whole EEPROM for uid
        returns address if found
        or -1 if not found.
        
        This is (kind of) ok performance-wise because EEPROM is only 1024 long
        and reading from EEPROM is extremely fast.
    */

    int address = 0;                                // Variable to store current address 
    uint8_t tempArray[] = {0, 0, 0, 0, 0, 0, 0};    // array to temporarily store read uid to before checking

    while (address < 1024){                                 // loop through whole EEPROM 

        address = readUID(tempArray, address, uidLength);   // read uid from address

        for (int i = 0; i < uidLength; i++){                // loop to check values in both arrays

            if (uid[i] != tempArray[i]){                    // if any value in array is different check next address
                break;
            }

            else if (i == uidLength - 1){                   // if the whole array has been checked and all values match
                return address - uidLength;                 // we found the uid in EEPROM so return its address.
            }                                               // Note it has to be '- uidLength' because readUID returns
        }                                                   // next address.
        
    }
    return -1;                                              // if not found return -1
}

void deleteUID(uint8_t uid[], const short adminAddress, short uidLength){
    /*
        Finds the uid in EEPROM and deletes it.

        Is setup to not delete the adminAddress variable, look at the definition
        of ADMIN_ADDRESS const in the beggining of file for more insight.

        The function also won't do anything if uid was not found in EEPROM
    */

    int address = checkUID(uid, uidLength);        //find the address of uid

    if (address > adminAddress){                    //if the uid is not AdminID        
        uint8_t arr[] = {0, 0, 0, 0};               
        writeUID(arr, address, uidLength);          //rewrite it with zeroes

        //DEBUG
        Serial.print("\nDeleted ID from address: ");
        Serial.print(address);
        Serial.print("\n");
        //END DEBUG
    }

    //DEBUG
    else if (address >= 0 && address < adminAddress){
        Serial.println("Can't delete an admin card");
    }
    else{
        Serial.println("Can't find this uid in memory");
    }
    //END DEBUG

}

bool checkIfAdmin(){
    //this might be useless, will leave for now
}

void startListeningToNFC() {
  // Reset our IRQ indicators
  irqPrev = irqCurr = HIGH;
  
  Serial.println("Waiting for an ISO14443A Card ...");
  nfc.startPassiveTargetIDDetection(PN532_MIFARE_ISO14443A);
}

bool checkIfCardFound(){

    irqCurr = digitalRead(PN532_IRQ);


    if (irqCurr == LOW && irqPrev == HIGH) {
        Serial.println("Got NFC IRQ");  
        return true;
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

void standardState(){
    lcd.clear();
    startListeningToNFC();      // start listening
    
    while (true) {

        Serial.println("Standard State");


        lcd.print("Zbliz karte");    


        //For controlling the flow of program, don't change if not necessary
        //BEGIN CONTROL BLOCK
        if (ButtonRead(analogRead(A0)) == 'r'){
            state = 1;
            return;
        }

        else if (ButtonRead(analogRead(A0)) == 'l'){
            state = 2;
            return;
        }
        //END CONTROL BLOCK

        //Below is the true functionality of this state

        //initialise the variables needed in this state
        uint8_t success = false;
        uint8_t uid[] = { 0, 0, 0, 0, 0, 0, 0 };  // Buffer to store the returned UID
        uint8_t uidLength;                        // Length of the UID (4 or 7 bytes depending on ISO14443A card type)

        

        if (checkIfCardFound()){                                        //if the card is found

            success = nfc.readDetectedPassiveTargetID(uid, &uidLength); //read the card

            if (success){
                
                //DEBUG
                Serial.println("Found card");
                Serial.print("  UID Value: ");
                for(short i = 0; i < uidLength; i++){
                    Serial.print(uid[i]);
                    Serial.print(" ");
                }
                Serial.println("\n");
                //END DEBUG

                if(uidLength != 4){                                     //if card is invalid do nothing
                    Serial.println("Invalid card");                     //expected length of card uid is 4
                }

                else{                                                   //if card is valid

                    if(checkUID(uid, uidLength) != -1){                       //if card is in memory
                        lcd.clear();
                        lcd.print("Drzwi otwarte!");
                        sendSignalToStrike(CONTROL_PIN, 5);             //open the strike (or light led etc.)
                        lcd.clear();
                    }

                    else{
                        Serial.println("Card is not in memory");
                        lcd.clear();
                        lcd.print("Zla karta");
                        delay(1000);
                        lcd.clear();
                    }
                    
                } 
            }

            startListeningToNFC(); //restart the listening utility

        }
        


    delay(100); //temporary delay, subject to change
    lcd.clear();
    }
}

void addingState(){

    startListeningToNFC();      // start listening
    lcd.clear();
    while (true) {

        Serial.println("Adding State");

        lcd.print("Zbliz karte");
        lcd.setCursor(0, 1);
        lcd.print("administratora");


        //For controlling the flow of program, don't change if not necessary
        //BEGIN CONTROL BLOCK
        if (ButtonRead(analogRead(A0)) == 'r'){
            state = 2;
            return;
        }

        else if (ButtonRead(analogRead(A0)) == 'l'){
            state = 0;
            return;
        }
        //END CONTROL BLOCK

        //Below is the true code of this state
        uint8_t success = false;
        uint8_t uid[] = { 0, 0, 0, 0, 0, 0, 0 };  // Buffer to store the returned UID
        uint8_t uidLength;                        // Length of the UID (4 or 7 bytes depending on ISO14443A card type)

        Serial.println("Waiting for Admin card");

        if (checkIfCardFound()){                                        //if the card is found

            success = nfc.readDetectedPassiveTargetID(uid, &uidLength); //read the card

            if (success){
                
                //DEBUG
                Serial.println("Found card");
                Serial.print("  UID Value: ");
                for(short i = 0; i < uidLength; i++){
                    Serial.print(uid[i]);
                    Serial.print(" ");
                }
                Serial.println("\n");
                //END DEBUG

                if(uidLength != 4){                                     //if card is invalid do nothing
                    Serial.println("Invalid card");                     //expected length of card uid is 4
                }

                else{                                                   //if card is valid

                    if(checkUID(uid, uidLength) <= ADMIN_ADDRESS && checkUID(uid, uidLength) != -1){      //if card is admin card

                        Serial.println("Valid Admin card found");

                        lcd.clear();
                        lcd.print("Podano karte");
                        lcd.setCursor(0, 1);
                        lcd.print("administratora");

                        delay(1000);
                        lcd.clear();

                        startListeningToNFC();

                        while(true){
                            
                            Serial.println("Waiting for another card");

                            lcd.print("Zbliz druga");
                            lcd.setCursor(0, 1);
                            lcd.print("karte");

                            if(checkIfCardFound()){

                                

                                success = nfc.readDetectedPassiveTargetID(uid, &uidLength);

                                if(success && checkUID(uid, uidLength) == -1){

                                    if(uidLength != 4){
                                        Serial.println("Invalid card");
                                        
                                        lcd.clear();
                                        lcd.print("Nieprawidlowa");
                                        lcd.setCursor(0,1);
                                        lcd.print("karta");

                                        delay(1000);
                                        lcd.clear();
                                        break;
                                    }

                                    else{
                                        Serial.println("Found second card, adding to memory");

                                        lcd.clear();
                                        lcd.print("Dodaje do");
                                        lcd.setCursor(0,1);
                                        lcd.print("pamieci");

                                        writeUID(uid, -1, uidLength);
                                        delay(1000);
                                        lcd.clear();
                                        break;
                                    }

                                

                                }

                                if(success && checkUID(uid, uidLength) != -1){
                                    Serial.println("Can't add an existing card, try again");

                                    lcd.clear();
                                    lcd.print("Ta karta juz");
                                    lcd.setCursor(0,1);
                                    lcd.print("jest w pamieci");

                                    delay(1000);
                                    lcd.clear();
                                    break;
                                }
                            }

                            //For controlling the flow of program, don't change if not necessary
                            //BEGIN CONTROL BLOCK
                            if (ButtonRead(analogRead(A0)) == 'r'){
                                state = 2;
                                return;
                            }

                            else if (ButtonRead(analogRead(A0)) == 'l'){
                                state = 0;
                                return;
                            }
                            //END CONTROL BLOCK
                        }
                    }

                    else{
                        Serial.println("Invalid admin card, try again");
                        lcd.clear();
                        lcd.print("To nie karta");
                        lcd.setCursor(0,1);
                        lcd.print("administratora");
                        delay(1000);
                        lcd.clear();
                    }
                    
                } 
            }

            startListeningToNFC(); //restart the listening utility

        }
        


    delay(100); //temporary delay, subject to change
    lcd.clear();
    }
}

void deletingState(){
    lcd.clear();
    while (true) {

        Serial.println("Deleting State");

        lcd.print("Zbliz karte");
        lcd.setCursor(0, 1);
        lcd.print("administratora");


        //For controlling the flow of program, don't change if not necessary
        //BEGIN CONTROL BLOCK
        if (ButtonRead(analogRead(A0)) == 'r'){
            state = 0;
            return;
        }

        else if (ButtonRead(analogRead(A0)) == 'l'){
            state = 1;
            return;
        }
        //END CONTROL BLOCK

        uint8_t success = false;
        uint8_t uid[] = { 0, 0, 0, 0, 0, 0, 0 };  // Buffer to store the returned UID
        uint8_t uidLength;                        // Length of the UID (4 or 7 bytes depending on ISO14443A card type)

        Serial.println("Waiting for Admin card");

        if (checkIfCardFound()){                                        //if the card is found

            success = nfc.readDetectedPassiveTargetID(uid, &uidLength); //read the card

            if (success){
                
                //DEBUG
                Serial.println("Found card");
                Serial.print("  UID Value: ");
                for(short i = 0; i < uidLength; i++){
                    Serial.print(uid[i]);
                    Serial.print(" ");
                }
                Serial.println("\n");
                //END DEBUG

                if(uidLength != 4){                                     //if card is invalid do nothing
                    Serial.println("Invalid card");                     //expected length of card uid is 4
                }

                else{                                                   //if card is valid

                    if(checkUID(uid, uidLength) <= ADMIN_ADDRESS && checkUID(uid, uidLength) != -1){      //if card is admin card

                        Serial.println("Valid Admin card found");

                        lcd.clear();
                        lcd.print("Podano karte");
                        lcd.setCursor(0, 1);
                        lcd.print("administratora");

                        delay(1000);
                        lcd.clear();

                        startListeningToNFC();

                        while(true){
                            
                            Serial.println("Waiting for another card");

                            lcd.print("Zbliz druga");
                            lcd.setCursor(0, 1);
                            lcd.print("karte");

                            if(checkIfCardFound()){

                                

                                success = nfc.readDetectedPassiveTargetID(uid, &uidLength);

                                if(success && checkUID(uid, uidLength) > ADMIN_ADDRESS){

                                    if(uidLength != 4){
                                        Serial.println("Invalid card");
                                        
                                        lcd.clear();
                                        lcd.print("Nieprawidlowa");
                                        lcd.setCursor(0,1);
                                        lcd.print("karta");

                                        delay(1000);
                                        lcd.clear();
                                    }

                                    else{
                                        Serial.println("Found second card, deleting from memory");

                                        lcd.clear();
                                        lcd.print("Usuwam z ");
                                        lcd.setCursor(0,1);
                                        lcd.print("pamieci");

                                        deleteUID(uid, ADMIN_ADDRESS, uidLength);
                                        delay(1000);
                                        lcd.clear();
                                        break;

                                    }

                                }
                            }

                            //For controlling the flow of program, don't change if not necessary
                            //BEGIN CONTROL BLOCK
                            if (ButtonRead(analogRead(A0)) == 'r'){
                                state = 0;
                                return;
                            }

                            else if (ButtonRead(analogRead(A0)) == 'l'){
                                state = 1;
                                return;
                            }
                            //END CONTROL BLOCK

                        }
                    }

                    else{
                        Serial.println("Invalid admin card, try again");
                        lcd.clear();
                        lcd.print("To nie karta");
                        lcd.setCursor(0,1);
                        lcd.print("administratora");
                        delay(1000);
                        lcd.clear();
                    }
                    
                } 
            }

            startListeningToNFC(); //restart the listening utility

        }
        


    delay(100); //temporary delay, subject to change
    lcd.clear();
    }
}

void setup(){
    Serial.begin(115200);

    lcd.begin(16, 2);
    lcd.print("Starting up");
    Serial.println("Starting up");

    /*
    int address = 0;
    uint8_t arr[] = {0, 0, 0, 0};
    short length = 4;
    Serial.print("\n");
    while(address < 1024){
        address = readUID(arr, address, length);
        for(short i = 0; i < length; i++){
            Serial.print(arr[i]);
            Serial.print(" ");
        }
        Serial.print("\n");
    }

    */

    

    //Starting the i2c communication and printing some debug info
    //BEGIN i2c BLOCK
    nfc.begin();
    Serial.println("Began nfc communication, waiting for firmware version");

    uint32_t versiondata = nfc.getFirmwareVersion();
    if (! versiondata) {
        Serial.print("Didn't find PN53x board");
        while (1); // halt
    }
    Serial.print("Found chip PN5"); Serial.println((versiondata >> 24) & 0xFF, HEX);
    Serial.print("Firmware ver. "); Serial.print((versiondata >> 16) & 0xFF, DEC);
    Serial.print('.'); Serial.println((versiondata >> 8) & 0xFF, DEC);

    nfc.SAMConfig();
    //END i2c BLOCK

    //pinmode to control the electric strike (or anything really)
    pinMode(CONTROL_PIN, OUTPUT);

    //LCD initialization and starting communication

    lcd.clear();
    lcd.print("Finished!");
    delay(500);
    lcd.clear();
}

void loop(){

    //DEBUG
    Serial.print("State: ");
    Serial.print(state);
    Serial.print("\n");
    //END DEBUG


    //this code controls flow of the program, don't change if not necessary
    //BEGIN CONTROL BLOCK
    switch (state){

        case 0:
            delay(400);
            lcd.clear();
            lcd.print("Tryb otwierania");
            delay(1000);
            lcd.clear();
            standardState();
        break;

        case 1:
            delay(400);
            lcd.clear();
            lcd.print("Tryb dodawania");
            delay(1000);
            lcd.clear();
            addingState();
        break;

        case 2:
            delay(400);
            lcd.clear();
            lcd.print("Tryb usuwania");
            delay(1000);
            lcd.clear();
            deletingState();
        break;
    }
    //END CONTROL BLOCK
}
