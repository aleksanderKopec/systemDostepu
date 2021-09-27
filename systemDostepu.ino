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
//This const has to be the BEGINNING (for example 0) of last admin card address
//not the end of last admin card address.
#define ADMIN_ADDRESS 0

//adress to store access_code
#define CODE_ADDRESS 1020

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

bool ifChangeState(){
    if (ButtonRead(analogRead(A0)) == 'r'){
        state++;
        if (state == 5) state = 0;
        return true;
    }

    else if (ButtonRead(analogRead(A0)) == 'l'){
        state--;
        if (state == -1) state = 4;
        return true;
    }
    return false;
}

void standardState(){
    lcd.clear();
    startListeningToNFC();      // start listening
    
    while (true) {

        Serial.println("Standard State");


        lcd.print("Zbliz karte");    

        if (ifChangeState()){
            return;
        }

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

        if (ifChangeState()){
            return;
        }

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
                            //delay(50);

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

                            if (ifChangeState()){
                                return;
                            }
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

        if (ifChangeState()){
            return;
        }

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

                            if (ifChangeState()){
                                return;
                            }

                            lcd.clear();

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

void powiekszLiczbe(uint8_t arr[], short ktora){
    arr[ktora]++;
    if (arr[ktora] == 10){
        arr[ktora] = 0;
    }
}

void pomniejszLiczbe(uint8_t arr[], short ktora){
    arr[ktora]--;
    if(arr[ktora] == 255){
        arr[ktora] = 9;
    }
}

void printCodeAndOptions(uint8_t code[], char message[]){
    lcd.print(message);
    lcd.setCursor(0, 1);

    lcd.print(code[0]);
    lcd.print(' ');

    lcd.print(code[1]);
    lcd.print(' ');

    lcd.print(code[2]);
    lcd.print(' ');

    lcd.print(code[3]);
    lcd.print("  ");

    lcd.print("OK");
    lcd.print(' ');
    lcd.print("END");
}

bool codeInput(uint8_t code[], char message[]){
    lcd.clear();
    short curr_selection = 0;
    printCodeAndOptions(code, message);
    while (true){

        if (curr_selection == 4){
            lcd.setCursor(9, 1);
        }
        else if (curr_selection == 5){
            lcd.setCursor(12, 1);
        }
        else{
            lcd.setCursor(curr_selection * 2, 1);
        }
        
        lcd.cursor();

        if (ButtonRead(analogRead(A0)) == 'u'){
            powiekszLiczbe(code, curr_selection);
            lcd.clear();
            printCodeAndOptions(code, message);
            delay(300);
        }

        if (ButtonRead(analogRead(A0)) == 'd'){
            pomniejszLiczbe(code, curr_selection);
            lcd.clear();
            printCodeAndOptions(code, message);
            delay(300);
        }

        if (ButtonRead(analogRead(A0)) == 'l'){
            curr_selection--;
            if (curr_selection == -1){
                curr_selection = 5;
            }
            delay(600);
        }

        if (ButtonRead(analogRead(A0)) == 'r'){
            curr_selection++;
            if (curr_selection == 6){
                curr_selection = 0;
            }
            delay(600);
        }

        if (ButtonRead(analogRead(A0)) == 's'){
            if (curr_selection == 4){
                return true;
            }
            else if (curr_selection == 5){
                return false;
            }
        }



    }
}

bool checkIfArraysEqual(uint8_t arr1[], uint8_t arr2[], const short length){
    for (int i = 0; i < length; i++){
        if (arr1[i] != arr2[i]){
            return false;
        }
    }
    return true;
}

void codeState(){
    lcd.clear();
    uint8_t obecny_kod[] = { 0, 0, 0, 0 };
    uint8_t kod_dostepu[] = { 0, 0, 0, 0};

    readUID(kod_dostepu, CODE_ADDRESS, 4);
    while(true){


        Serial.println("Code state");

        char message[] = "PODAJ KOD";
        bool result = codeInput(obecny_kod, message);
        lcd.noCursor();
        lcd.clear();
        if (!result){
            lcd.print("Przerwano wpi-");
            lcd.setCursor(0, 1);
            lcd.print("sywanie kodu");
            state = 4;
            delay(1000);
            lcd.clear();
            return;
        }
        
        
        lcd.clear();

        if (!checkIfArraysEqual(obecny_kod, kod_dostepu, 4)){
            lcd.print("Niepoprawny kod");
            delay(1000);
            lcd.clear();
            lcd.print("Sprobuj jeszcze");
            lcd.setCursor(0, 1);
            lcd.print("raz.");
            state = 3;
            delay(1000);
            lcd.clear();
            return;
        }
        else{
            lcd.print("Poprawny kod");
            lcd.setCursor(0, 1);
            lcd.print("Drzwi otwarte");
            state = 4;
            sendSignalToStrike(CONTROL_PIN);
            lcd.clear();
        }

        
        return;

    }
}

void changeCodeState(){
    lcd.clear();
    while (true) {

        Serial.println("Deleting State");

        lcd.print("Zbliz karte");
        lcd.setCursor(0, 1);
        lcd.print("administratora");

        if (ifChangeState()){
            return;
        }

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

                        uint8_t nowy_kod[] = { 0, 0, 0, 0 };
                        char message[] = "PODAJ NOWY KOD:";
                        bool result = codeInput(nowy_kod, message);

                        lcd.noCursor();
                        lcd.clear();
                        if (!result){
                            lcd.print("Przerwano wpi-");
                            lcd.setCursor(0, 1);
                            lcd.print("sywanie kodu");
                            delay(1000);
                            lcd.clear();
                            state = 0;
                            startListeningToNFC();
                            return;
                        }


                        lcd.clear();
                        lcd.print("Podano nowy kod");
                        lcd.setCursor(0, 1);
                        lcd.print("Zapisywanie...");
                        writeUID(nowy_kod, CODE_ADDRESS, 4);
                        delay(1000);

                        lcd.clear();
                        lcd.print("Zapis wykonany");
                        lcd.setCursor(0, 1);
                        lcd.print("pomyslnie");
                        delay(1000);

                        lcd.clear();
                        state = 0;
                        startListeningToNFC();
                        return;
                        
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

    startListeningToNFC();

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

        case 3:
            delay(400);
            lcd.clear();
            lcd.print("Wpisywanie kodu");
            delay(1000);
            lcd.clear();
            codeState();

        case 4:
            delay(400);
            lcd.clear();
            lcd.print("Zmiana kodu");
            delay(1000);
            lcd.clear();
            changeCodeState();   
    }
    //END CONTROL BLOCK
}
