char Version[] = "23072026";
// Modell Flug Stoppuhr = Abwärtszähler von voreingestellten Minuten mit Überzeitanzeige und Piepser
//  Kein Tastendruck und Zähler zählt nicht + 10 sec. ==> sleep
//
// 23.7.2026: Interrupt für Aufwachen auf LEVEL geändert, Stromverbrauch 16uA inkl. TP4056 ?
// 7.7.2026:  Umbau zu Display DOGM081 in SPI-mode
// 6.7.2026:  BATTLOW impl.
// 5.7.2026:  Erstellt von Gemini
//                    |------- Arduino pin numbers--------|                       
//                                  +----\/----+                                                    
//                             VDD 1|o         |20 GND      
//     PIN_START_STOP - 0 - PA4 -  2|          |19 - PA3 - 16 - DISPLAY_SCK_PIN                                 
//          PIN_RESET - 1 - PA5 -  3|          |18 - PA2 - 15 -                       
//                    - 2 - PA6 -  4|          |17 - PA1 - 14 - DISPLAY_MOSI_PIN (SI/D7)                 
//                    - 3 - PA7 -  5|          |16 - PA0 - 17 - UPDI                          
//                 NC - 4 - PB5 -  6|          |15 - PC3 - 13 - NC                         
//         PIN_BUZZER - 5 - PB4 -  7|          |14 - PC2 - 12 - DISPLAY_RESET_PIN 
//    DISPLAY_VCC_PIN - 6 - PB3 -  8|          |13 - PC1 - 11 - DISPLAY_CS_PIN (CSB)              
//                    - 7 - PB2 -  9|          |12 - PC0 - 10 - DISPLAY_ DC_PIN (RS) 
//                    - 8 - PB1 - 10|          |11 - PB0  - 9 -  
//                                  +----------+ 
// Software: Arduino IDE 1.8.19 with SpenceKonde/megaTinyCore 2.6.10 installed https://github.com/SpenceKonde/megaTinyCore    
//
// Arduino IDE settings: Chip: "ATtiny3216", Clock: "1 MHz internal", Programmer: "SerialUPDI", other default
//
// Useful links:  
// ATtiny3216 datasheet:     https://ww1.microchip.com/downloads/en/DeviceDoc/ATtiny3216-17-DataSheet-DS40002205A.pdf
//                           https://onlinedocs.microchip.com/oxy/GUID-A2109DC3-B5FF-4E1B-BDB5-622C21D35F43-en-US-5/GUID-0BF2B683-3906-4D3E-BA97-2634FA1779B7.html
// DS3231M datasheet:        https://www.analog.com/media/en/technical-documentation/data-sheets/ds3231m.pdf
// Display EA DOGM081 docs:  https://www.lcd-module.de/lcd-tft-beispiel-code-programmierung/application-note/arduino.html
//                           https://www.lcd-module.de/eng/pdf/doma/dog-me.pdf
// megaTinyCore Power Save           https://github.com/SpenceKonde/megaTinyCore/blob/master/megaavr/extras/PowerSave.md
// megaTinyCore Pin Interrupts       https://github.com/SpenceKonde/megaTinyCore/blob/master/megaavr/extras/Ref_PinInterrupts.md  
// ATtiny UPDI Serial Programmer     https://github.com/SpenceKonde/AVR-Guidance/blob/master/UPDI/jtag2updi.md
// megaTinyCore PWM and Timers       https://github.com/SpenceKonde/megaTinyCore/blob/master/megaavr/extras/Ref_Timers.md
// original: elektormagazine low power timer by anto
//
#include <megaTinyCore.h>       // Core support for ATtiny series SpenceKonde/megaTinyCore 2.6.10 https://github.com/SpenceKonde/megaTinyCore
#include <Arduino.h>
#include <avr/sleep.h>          // AVR Sleep mode definitions
#include <dogm_7036.h>          // Display library and docs EA DOGM081 https://www.lcd-module.com/lcd-tft-code-example-programming/application-note/arduino.html
#include <OneButton.h>
#include <SPI.h>
#include <EEPROM.h>

#define PIN_START_STOP    PIN_PA4
#define PIN_RESET         PIN_PA5
#define PIN_BUZZER        PIN_PB4
#define DISPLAY_VCC_PIN   PIN_PB3                           // Control pin to cut power to display (for power saving)
#define DISPLAY_RESET_PIN PIN_PC2
#define ADC_PIN           PIN_PA2
#define DISPLAY_CS_PIN    PIN_PC1                           // Chip Select for SPI Display
#define DISPLAY_SCK_PIN   PIN_PA3                         // SPI Clock pin

#define unusedPinNum 8                                    // Count of unused pins to set as INPUT_PULLUP for power saving
uint8_t unusedPin[10]  = {2,3,4,7,8,9,13,15}; //2, 3, 4, 7, 8, 9, 13, 15};   // List of unused pins

//const int ADC_THRESHOLD = 634;        // Berechneter ADC-Wert für 3,1V Batteriespannung
//const int ADC_THRESHOLD = 818; // 4V
const int ADC_THRESHOLD = 1000;
const uint8_t I2C_ADDR = 0x3C;
byte arrow_down[] = {0x04, 0x04, 0x04, 0x04, 0x15, 0x0E, 0x04, 0x00}; //pattern for own defined character

// Instanzen der Bibliotheken
dogm_7036 DOG;                                      // Display instance   
OneButton btnStartStop;
OneButton btnReset;

// Zustandsautomat für die Stoppuhr
enum State { IDLE, RUNNING, OVERTIME, PAUSED, SETTING, BATTLOW };
State currentState = IDLE;

// Zeitvariablen (in Sekunden)
int32_t initialMinutes = 0; // Voreinstellung: 0 Minuten
int32_t currentSeconds = initialMinutes * 60;
uint32_t lastTickTime = 0;
char bpuffer[10];

// Hilfsvariablen für Nicht-blockierenden Buzzer
uint32_t buzzerEndTime = 0;
bool buzzerActive = false;

// Prototypen
void updateDisplay();
void handleBuzzer(int32_t remainingSec);
void checkBuzzerDuration();
void goSleep();
void Spannungmessen();

// Pin als Eingang definieren
bool emptybatt = false;                 // Statusvariable für niedrigen Batteriestand

uint16_t waketimer = 10;
uint32_t lastTime = 0;

// --- Button-Events ---

// --- Interrupt Service Routine for PORTA (Buttons) ---
ISR(PORTA_PORT_vect){
  
  uint8_t flags = PORTA.INTFLAGS;                            // Read which pins triggered the interrupt
  PORTA.INTFLAGS = flags;                                    // Clear the flags by writing a 1 to them (not a 0);

  if (flags & 0x010){                                         // Check PA4 interrupt fired (start/stopp-button)                                      
      currentState = IDLE;                                     // 1 2 4 8 16 32 64 128
  }                                                       // 1 2 4 8 10 20 40 80 

  if (flags & 0x20){                                         // Check PA5 interrupt fired (reset/set-button)
      currentState = IDLE;                                      
  }  
}

void onStartStopClick() {
  switch (currentState) {
    case BATTLOW:
      updateDisplay();
      delay(2000);
      goSleep();
      break;  
    case SETTING:
      initialMinutes++;
      if (initialMinutes > 99) initialMinutes = 1; // Begrenzung auf 99 Min
      updateDisplay();
      break;
    case IDLE:
      handleBuzzer(9999);
    case PAUSED:
      if (currentSeconds >= 0) {
        currentState = RUNNING;
      } else {
        currentState = OVERTIME;
      }
      lastTickTime = millis();
      updateDisplay();
      break;
    case RUNNING:
    case OVERTIME:
      currentState = PAUSED;
      updateDisplay();
      break;
  }
waketimer=10;
}

void onStartStopLongPress() {
  if (currentState == SETTING) {
    initialMinutes--;
    if (initialMinutes < 1) initialMinutes = 99; // Umlaufschutz
    updateDisplay();
  }
waketimer=10;
}

void onResetClick() {
  switch (currentState) {
    case SETTING:
      // Speichern und Verlassen des Einstellmodus
      currentSeconds = initialMinutes * 60;
      currentState = IDLE;
      updateDisplay();
      break;
    case PAUSED:
    case IDLE:
    case OVERTIME:
      // Zurücksetzen auf Anfangswert
      currentSeconds = initialMinutes * 60;
      currentState = IDLE;
      updateDisplay();
      break;
    default:
      break;
  }
waketimer=10;
}

void onResetLongPress() {
  if (currentState == IDLE || currentState == PAUSED) {
    currentState = SETTING;
    updateDisplay();
  }
  waketimer=10;
}

////////////////////////////////// START SETUP ////////////////////////////////////////////////////////////////////
void setup() {
  // Buzzer-Pin initialisieren
  pinMode(PIN_BUZZER, OUTPUT);
  digitalWrite(PIN_BUZZER, LOW);

  //analogReference(INTERNAL2V5);
  //pinMode(ADC_PIN, INPUT);

// --- Configure Sleep Mode ---
  set_sleep_mode(SLEEP_MODE_PWR_DOWN);                   // Deepest sleep mode
  sleep_enable();
// --- Configure Unused Pins (Prevents floating pins consuming power) ---

  for (uint8_t i = 0; i < unusedPinNum; i++){ 
    pinMode(unusedPin[i], INPUT_PULLUP);   
  }
/*
  pinMode(2, INPUT_PULLUP);
  pinMode(3, INPUT_PULLUP);
  pinMode(4, INPUT_PULLUP);
  pinMode(7, INPUT_PULLUP);
  pinMode(8, INPUT_PULLUP);
  pinMode(9, INPUT_PULLUP);
  pinMode(13, INPUT_PULLUP);
*/
pinMode(PIN_START_STOP, INPUT_PULLUP);
pinMode(PIN_RESET, INPUT_PULLUP);

  EEPROM.get(0x05, initialMinutes);
  currentSeconds = initialMinutes * 60;
  // Display initialisieren
  pinMode(DISPLAY_VCC_PIN, OUTPUT);
  digitalWrite(DISPLAY_VCC_PIN, HIGH);
  delay(100);
  DOG.initialize(11, 0, 0, 10, 12, 0, DOGM081);          // HW Config for Display (CS, 0, 0, DC, RESET, 0, DOGM081)
  DOG.contrast(15); 
  DOG.displ_onoff(true);                                 // Turn Display on
  DOG.cursor_onoff(false);                               // Turn cursor blinking off
  DOG.define_char(0, arrow_down); //define own char on memory adress 0

  // Tasten über OneButton konfigurieren (Aktiv LOW, Pull-Up ein)
  btnStartStop.setup(PIN_START_STOP, INPUT_PULLUP, true);
  btnReset.setup(PIN_RESET, INPUT_PULLUP, true);

  // Event-Handler zuweisen
  btnStartStop.attachClick(onStartStopClick);
  btnStartStop.attachLongPressStart(onStartStopLongPress);
  btnReset.attachClick(onResetClick);
  btnReset.attachLongPressStart(onResetLongPress);
  Spannungmessen(); delay(200); Spannungmessen();
  updateDisplay();
} ////////////////////////////////// ENDE SETUP ////////////////////////////////////////////////////////////////////

void loop() {
  // OneButton Zustände dauerhaft prüfen
  btnStartStop.tick();
  btnReset.tick();

  // Nicht-blockierende Steuerung für den Piepser
  checkBuzzerDuration();

  // Zeit-Logik im Sekundentakt
  if (currentState == RUNNING || currentState == OVERTIME) {
    if (millis() - lastTickTime >= 1000) {
      lastTickTime += 1000;

      if (currentState == RUNNING) {
        currentSeconds--;
        handleBuzzer(currentSeconds); // Töne ausgeben

        if (currentSeconds <= 0) {
          currentState = OVERTIME;
        }
      } else if (currentState == OVERTIME) {
        currentSeconds--; // Zählt im negativen Bereich weiter (-1, -2...)
      }
      updateDisplay();
    }
  waketimer=10;  
  }
if (millis() - lastTime >= 1000) {
  lastTime += 1000;
  waketimer-=1;  
  }
  if(!waketimer) goSleep();
}

// --- Hilfsfunktionen ---

void updateDisplay() {
  DOG.clear_display();
  DOG.position(1,1);
  switch (currentState) {
    case IDLE:     break;
    case RUNNING:  DOG.string("   "); break;
    case OVERTIME: DOG.string(" - "); break;
    case PAUSED:   if (currentSeconds < 0 && currentState != SETTING) DOG.string(" - "); else DOG.string("end"); break;  
    case SETTING:  DOG.string("set"); break;
    case BATTLOW:  DOG.string("low"); break;
  }

  // Zeitausgabe (Format: MM:SS oder -MM:SS)
  DOG.position(4,1);
  int32_t absSeconds = abs(currentSeconds);
  int32_t displayMin = (currentState == SETTING) ? initialMinutes : (absSeconds / 60);
  int32_t displaySec = (currentState == SETTING) ? 0 : (absSeconds % 60);

  sprintf(bpuffer,"%02d:%02d", (int)displayMin, (int)displaySec);
  DOG.string(bpuffer);
}

void startBeep(uint32_t durationMs,unsigned int freq) {
  tone(PIN_BUZZER,freq);
  buzzerEndTime = millis() + durationMs;
  buzzerActive = true;
}

void checkBuzzerDuration() {
  if (buzzerActive && millis() >= buzzerEndTime) {
    //digitalWrite(PIN_BUZZER, LOW);
    noTone(PIN_BUZZER);
    buzzerActive = false;
  }
}

void handleBuzzer(int32_t remainingSec) {
  if (remainingSec == 30 || remainingSec == 20 || remainingSec == 10) {
    startBeep(250,1500); // Normaler Piepston bei 30, 20, 10 Sek.
  } 
  else if (remainingSec >= 1 && remainingSec <= 9) {
    startBeep(100,2000); // Kurze Piepstöne von 9 bis 1 Sek.
  } 
  else if (remainingSec == 0 || remainingSec == 9999) {
    startBeep(1500,900); // Langer Piepston bei Null
  }
  else if (!(remainingSec % 60)) {
    startBeep(500,1000); 
  }
}


// --- Battery Check (using internal VCC reading) ---
void Spannungmessen() {
  int16_t voltageReading = readSupplyVoltage();      // readSupplyVoltage() is usually part of a specific AVR library extension (3.3V = 3300mV)

  if (voltageReading < 3100) {                        // Threshold < 3.1V
    emptybatt = true; currentState=BATTLOW;
  } else { 
    emptybatt = false; currentState=IDLE;
  }
}

void goSleep(){
  DOG.clear_display();                          
  delay(500);
  DOG.displ_onoff(false);                         // Turn display on
  SPI.end();
  digitalWrite(DISPLAY_CS_PIN, LOW);             // Set communication pins LOW *
  digitalWrite(DISPLAY_RESET_PIN, LOW);
  digitalWrite(DISPLAY_SCK_PIN, LOW); 

  digitalWrite(PIN_BUZZER, LOW);
  digitalWrite(DISPLAY_VCC_PIN, LOW);            // Cut power to display module
  
  EEPROM.put(0x05,initialMinutes);               // Aktuell konfigurierte Startzeit retten
  PORTA.PIN4CTRL |= PORT_ISC_LEVEL_gc;         // Enable Asynchronous Interrupts for Wake-up
  PORTA.PIN5CTRL |= PORT_ISC_LEVEL_gc;         // PA4 (Start/Stopp-button) and PA5 (Re/Set-Btn) -> Trigger on Falling Edge

  sleep_cpu();                                   // Enter sleep (CPU halts here)

  // ======= WAKE UP POINT =======
  
  PORTA.PIN4CTRL &= ~PORT_ISC_gm;                // Disable Interrupts immediately after wake up
  PORTA.PIN5CTRL &= ~PORT_ISC_gm;

  digitalWrite(DISPLAY_VCC_PIN, HIGH);           // Restore display power
  delay(100);
  
  DOG.initialize(11, 0, 0, 10, 12, 0, DOGM081);  // Re-init display and SPI
  DOG.contrast(15); 
  DOG.displ_onoff(true);                         // Turn display on
  DOG.cursor_onoff(false);
  waketimer = 10;                   // Reset idle timer

  Spannungmessen();                         // Battery check
  updateDisplay();
}