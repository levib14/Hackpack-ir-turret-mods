//////////////////////////////////////////////////
         //  CRUNCHLABS IR TURRET - ENHANCED  //
         //     WITH COOL FEATURES & MODES    //
//////////////////////////////////////////////////
#pragma region LICENSE
/*
  ************************************************************************************
  * MIT License
  *
  * Copyright (c) 2025 Crunchlabs LLC (IRTurret Control Code)
  * Copyright (c) 2020-2022 Armin Joachimsmeyer (IRremote Library)
  * Enhanced Features & Modes (2025)

  * Permission is hereby granted, free of charge, to any person obtaining a copy
  * of this software and associated documentation files (the "Software"), to deal
  * in the Software without restriction, including without limitation the rights
  * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
  * copies of the Software, and to permit persons to whom the Software is furnished
  * to do so, subject to the following conditions: 
  *
  * The above copyright notice and this permission notice shall be included in all
  * copies or substantial portions of the Software.
  *
  * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED,
  * INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A
  * PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT
  * HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF
  * CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE
  * OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
  *
  ************************************************************************************
*/
#pragma endregion LICENSE

//////////////////////////////////////////////////
                //  LIBRARIES  //
//////////////////////////////////////////////////
#pragma region LIBRARIES

#include <Arduino.h>
#include <Servo.h>
#include <IRremote.hpp>

#pragma endregion LIBRARIES

//////////////////////////////////////////////////
               //  IR CODES  //
//////////////////////////////////////////////////
#pragma region IR CODES

#define left 0x65
#define right 0x62
#define up 0x60
#define down 0x61
#define ok 0x68
#define cmd1 0x4
#define cmd2 0x5
#define cmd3 0x6
#define cmd4 0x8
#define cmd5 0x9
#define cmd6 0xA
#define cmd7 0xC
#define cmd8 0xD
#define cmd9 0xE
#define cmd0 0x11
#define star 0x23
#define hashtag 0x13
#define volplus 0x7
#define volminus 0xB
#define chplus 0x12
#define chminus 0x10

#define DECODE_NEC

#pragma endregion IR CODES

//////////////////////////////////////////////////
          //  PINS AND PARAMETERS  //
//////////////////////////////////////////////////
#pragma region PINS AND PARAMS

// ===== PASSCODE SYSTEM =====
#define PASSCODE_LENGTH 5
#define CORRECT_PASSCODE "62213"
char passcode[PASSCODE_LENGTH + 1] = "";
bool passcodeEntered = false;
int failCount = 0;
unsigned long lockoutUntil = 0;

// ===== SERVO CONFIGURATION =====
Servo yawServo;
Servo pitchServo;
Servo rollServo;

int yawServoVal = 90;
int pitchServoVal = 100;
int rollServoVal = 90;

int lastYawServoVal = 90;
int lastPitchServoVal = 90;
int lastRollServoVal = 90;

int pitchMoveSpeed = 8;
int yawMoveSpeed = 90;
int yawStopSpeed = 90;
int rollMoveSpeed = 90;
int rollStopSpeed = 90;

int yawPrecision = 50;
int rollPrecision = 158;

int pitchMax = 150;
int pitchMin = 33;

// ===== MODE SYSTEM =====
enum TurretMode {
  MODE_NORMAL = 0,
  MODE_RAPID_FIRE = 1,
  MODE_DANCE = 2,
  MODE_EVASION = 3,
  MODE_SNIPER = 4,
  MODE_TURRET_BURST = 5
};

TurretMode currentMode = MODE_NORMAL;
bool modeActive = false;

// ===== FEATURE FLAGS =====
unsigned long lastOkPress = 0;
int okPressCount = 0;
unsigned long lastActivity = 0;
bool attractModeActive = false;

// ===== KONAMI CODE =====
const int KONAMI_LEN = 10;
int konamiCode[KONAMI_LEN] = {up, up, down, down, left, right, left, right, cmd2, cmd1};
int konamiIndex = 0;
bool konamiUnlocked = false;

// Function prototypes
void shakeHeadYes(int moves = 3);
void shakeHeadNo(int moves = 3);
void activateRapidFire();
void activateDanceMode();
void activateEvasionMode();
void activateSniperMode();
void activateTurretBurst();
void rapidFireRounds(int rounds);

#pragma endregion PINS AND PARAMS

//////////////////////////////////////////////////
                //  S E T U P  //
//////////////////////////////////////////////////
#pragma region SETUP

void setup() {
    Serial.begin(9600);

    yawServo.attach(10);
    pitchServo.attach(11);
    rollServo.attach(12);

    Serial.println(F("START " __FILE__ " from " __DATE__ "\r\nUsing library version " VERSION_IRREMOTE));
    Serial.println(F("\n========================================"));
    Serial.println(F("CRUNCHLABS IR TURRET - ENHANCED EDITION"));
    Serial.println(F("========================================"));
    Serial.println(F("Available Modes (after unlock):"));
    Serial.println(F("  [MODE_NORMAL] Standard operation"));
    Serial.println(F("  [MODE_RAPID_FIRE] Double-tap OK button"));
    Serial.println(F("  [MODE_DANCE] Press # button"));
    Serial.println(F("  [MODE_EVASION] Press * button"));
    Serial.println(F("  [MODE_SNIPER] Press 7 then OK"));
    Serial.println(F("  [MODE_TURRET_BURST] Press 9 then OK"));
    Serial.println(F("  [VOL+] Fire 2 rounds"));
    Serial.println(F("  [VOL-] Fire 3 rounds"));
    Serial.println(F("  [CH+] Fire 4 rounds"));
    Serial.println(F("  [CH-] Fire 5 rounds"));
    Serial.println(F("  [KONAMI CODE] Up,Up,Down,Down,Left,Right,Left,Right,2,1"));
    Serial.println(F("========================================\n"));

    IrReceiver.begin(9, ENABLE_LED_FEEDBACK);

    Serial.print(F("Ready to receive IR signals of protocols: "));
    printActiveIRProtocols(&Serial);
    Serial.println(F("at pin 9"));

    homeServos();
    lastActivity = millis();
}

#pragma endregion SETUP

//////////////////////////////////////////////////
                //  L O O P  //
//////////////////////////////////////////////////
#pragma region LOOP

void loop() {
    // Check passcode lockout
    if (millis() < lockoutUntil) {
        Serial.println(F("SYSTEM LOCKED - WAIT FOR TIMEOUT"));
        delay(1000);
        return;
    }

    // Handle IR receiver
    if (IrReceiver.decode()) {
        int command = IrReceiver.decodedIRData.command;
        IrReceiver.resume();
        handleCommand(command);
        lastActivity = millis();
    }

    // Demo/Attract mode if idle for 45 seconds while locked
    if (! passcodeEntered && (millis() - lastActivity > 45000)) {
        if (!attractModeActive) {
            attractModeActive = true;
            activateAttractMode();
            attractModeActive = false;
            lastActivity = millis();
        }
    }

    delay(5);
}

#pragma endregion LOOP

//////////////////////////////////////////////////
               // FUNCTIONS  //
//////////////////////////////////////////////////
#pragma region FUNCTIONS

// ===== PASSCODE SYSTEM =====

void checkPasscode() {
    if (strcmp(passcode, CORRECT_PASSCODE) == 0) {
        Serial.println(F("✓ CORRECT PASSCODE - TURRET UNLOCKED!"));
        passcodeEntered = true;
        failCount = 0;
        shakeHeadYes();
    } else {
        passcodeEntered = false;
        failCount++;
        Serial.print(F("✗ INCORRECT PASSCODE - Attempts remaining: "));
        Serial.println(3 - failCount);
        shakeHeadNo();
        
        if (failCount >= 3) {
            lockoutUntil = millis() + 30000;  // 30 second lockout
            Serial.println(F("!!! TOO MANY FAILED ATTEMPTS - SYSTEM LOCKED FOR 30 SECONDS !!!"));
            for (int i = 0; i < 5; i++) {
                shakeHeadNo(1);
            }
        }
    }
    passcode[0] = '\0';
}

void addPasscodeDigit(char digit) {
    if (!passcodeEntered && digit >= '0' && digit <= '9' && strlen(passcode) < PASSCODE_LENGTH) {
        strncat(passcode, &digit, 1);
        Serial.print(F("Passcode: "));
        Serial.println(passcode);
    }
}

// ===== COMMAND HANDLING =====

void handleCommand(int command) {
    // Debounce repeated commands during passcode entry
    if ((IrReceiver.decodedIRData.flags & IRDATA_FLAGS_IS_REPEAT) && !passcodeEntered) {
        return;
    }

    // KONAMI CODE CHECK (always monitor)
    if (command == konamiCode[konamiIndex]) {
        konamiIndex++;
        if (konamiIndex == KONAMI_LEN) {
            Serial.println(F("╔═══════════════════════════════════╗"));
            Serial.println(F("║      KONAMI CODE ACTIVATED!         ║"));
            Serial.println(F("║    ULTIMATE TURRET MODE UNLOCKED   ║"));
            Serial.println(F("╚═══════════════════════════════════╝"));
            konamiUnlocked = true;
            activateDanceMode();
            activateTurretBurst();
            konamiIndex = 0;
        }
    } else {
        konamiIndex = 0;
    }

    // MOVEMENT COMMANDS (require unlock)
    switch (command) {
        case up:
            if (passcodeEntered) {
                upMove(1);
            }
            break;

        case down:
            if (passcodeEntered) {
                downMove(1);
            }
            break;

        case left:
            if (passcodeEntered) {
                leftMove(1);
            }
            break;

        case right: 
            if (passcodeEntered) {
                rightMove(1);
            }
            break;

        case ok:
            if (passcodeEntered) {
                // Rapid Fire Mode: Double-tap detection
                unsigned long now = millis();
                if (now - lastOkPress < 500) {
                    okPressCount++;
                    if (okPressCount == 2) {
                        activateRapidFire();
                        okPressCount = 0;
                    }
                } else {
                    okPressCount = 1;
                    if (okPressCount == 1) {
                        fire();
                        Serial.println(F("FIRE"));
                    }
                }
                lastOkPress = now;
            }
            break;

        case star:
            if (passcodeEntered) {
                // Evasion Mode
                Serial.println(F("EVASION MODE ACTIVATED"));
                activateEvasionMode();
            } else {
                Serial.println(F("LOCKING TURRET"));
                passcodeEntered = false;
            }
            break;

        case hashtag:
            if (passcodeEntered) {
                // Dance Mode
                Serial.println(F("DANCE MODE ACTIVATED"));
                activateDanceMode();
            }
            break;

        // VOLUME/CHANNEL RAPID FIRE CONTROLS
        case volplus:
            if (passcodeEntered) {
                rapidFireRounds(2);
            }
            break;

        case volminus:
            if (passcodeEntered) {
                rapidFireRounds(3);
            }
            break;

        case chplus:
            if (passcodeEntered) {
                rapidFireRounds(5);
            }
            break;

        case chminus:
            if (passcodeEntered) {
                rapidFireRounds(4);
            }
            break;

        // PASSCODE INPUT
        case cmd1:
            if (!passcodeEntered) addPasscodeDigit('1');
            else if (passcodeEntered && cmd7 != -1) activateSniperMode();
            break;

        case cmd2:
            if (!passcodeEntered) addPasscodeDigit('2');
            break;

        case cmd3:
            if (!passcodeEntered) addPasscodeDigit('3');
            break;

        case cmd4:
            if (!passcodeEntered) addPasscodeDigit('4');
            break;

        case cmd5:
            if (!passcodeEntered) addPasscodeDigit('5');
            break;

        case cmd6:
            if (!passcodeEntered) addPasscodeDigit('6');
            break;

        case cmd7:
            if (!passcodeEntered) addPasscodeDigit('7');
            break;

        case cmd8:
            if (!passcodeEntered) addPasscodeDigit('8');
            break;

        case cmd9:
            if (!passcodeEntered) addPasscodeDigit('9');
            else if (passcodeEntered) activateTurretBurst();
            break;

        case cmd0:
            if (!passcodeEntered) addPasscodeDigit('0');
            break;

        default:
            Serial.println(F("Command Read Failed or Unknown, Try Again"));
            break;
    }

    // Check if passcode is complete
    if (strlen(passcode) == PASSCODE_LENGTH) {
        checkPasscode();
    }
}

// ===== MOVEMENT FUNCTIONS =====

void leftMove(int moves) {
    for (int i = 0; i < moves; i++) {
        yawServo.write(yawStopSpeed + yawMoveSpeed);
        delay(yawPrecision);
        yawServo.write(yawStopSpeed);
        delay(5);
        Serial.println(F("LEFT"));
    }
}

void rightMove(int moves) {
    for (int i = 0; i < moves; i++) {
        yawServo.write(yawStopSpeed - yawMoveSpeed);
        delay(yawPrecision);
        yawServo.write(yawStopSpeed);
        delay(5);
        Serial.println(F("RIGHT"));
    }
}

void upMove(int moves) {
    for (int i = 0; i < moves; i++) {
        if ((pitchServoVal + pitchMoveSpeed) < pitchMax) {
            pitchServoVal = pitchServoVal + pitchMoveSpeed;
            pitchServo.write(pitchServoVal);
            delay(50);
            Serial.println(F("UP"));
        }
    }
}

void downMove(int moves) {
    for (int i = 0; i < moves; i++) {
        if ((pitchServoVal - pitchMoveSpeed) > pitchMin) {
            pitchServoVal = pitchServoVal - pitchMoveSpeed;
            pitchServo.write(pitchServoVal);
            delay(50);
            Serial.println(F("DOWN"));
        }
    }
}

void fire() {
    rollServo.write(rollStopSpeed + rollMoveSpeed);
    delay(rollPrecision);
    rollServo.write(rollStopSpeed);
    delay(5);
    Serial.println(F("FIRING"));
}

void fireAll() {
    rollServo.write(rollStopSpeed + rollMoveSpeed);
    delay(rollPrecision * 6);
    rollServo.write(rollStopSpeed);
    delay(5);
    Serial.println(F("FIRING ALL"));
}

void homeServos() {
    yawServo.write(yawStopSpeed);
    delay(20);
    rollServo.write(rollStopSpeed);
    delay(100);
    pitchServo.write(100);
    delay(100);
    pitchServoVal = 100;
    Serial.println(F("HOMING"));
}

// ===== HEAD SHAKE ANIMATIONS =====

void shakeHeadYes(int moves = 3) {
    Serial.println(F("YES"));

    if ((pitchMax - pitchServoVal) < 15) {
        pitchServoVal = pitchServoVal - 15;
    } else if ((pitchServoVal - pitchMin) < 15) {
        pitchServoVal = pitchServoVal + 15;
    }
    pitchServo.write(pitchServoVal);

    int startAngle = pitchServoVal;
    int nodAngle = startAngle + 15;

    for (int i = 0; i < moves; i++) {
        for (int angle = startAngle; angle <= nodAngle; angle++) {
            pitchServo.write(angle);
            delay(7);
        }
        delay(50);
        for (int angle = nodAngle; angle >= startAngle; angle--) {
            pitchServo.write(angle);
            delay(7);
        }
        delay(50);
    }
}

void shakeHeadNo(int moves = 3) {
    Serial.println(F("NO"));

    for (int i = 0; i < moves; i++) {
        yawServo.write(140);
        delay(190);
        yawServo.write(yawStopSpeed);
        delay(50);
        yawServo.write(40);
        delay(190);
        yawServo.write(yawStopSpeed);
        delay(50);
    }
}

// ===== RAPID FIRE FUNCTION =====

void rapidFireRounds(int rounds) {
    Serial.print(F("\n>>> RAPID FIRE: "));
    Serial.print(rounds);
    Serial.println(F(" ROUNDS <<<"));
    
    for (int i = 0; i < rounds; i++) {
        fire();
        Serial.print(F("  Round "));
        Serial.print(i + 1);
        Serial.print(F(" of "));
        Serial.println(rounds);
        delay(120);
    }
    
    Serial.println(F(">>> RAPID FIRE COMPLETE <<<\n"));
}

// ===== COOL MODES =====

void activateRapidFire() {
    Serial.println(F("\n╔═══════════════════════════════════╗"));
    Serial.println(F("║    RAPID FIRE MODE ACTIVATED!      ║"));
    Serial.println(F("║  Firing all 6 darts in sequence!  ║"));
    Serial.println(F("╚═══════════════════════════════════╝\n"));
    
    currentMode = MODE_RAPID_FIRE;
    
    for (int i = 0; i < 6; i++) {
        fire();
        delay(120);
    }
    
    Serial.println(F("Rapid Fire sequence complete!"));
    currentMode = MODE_NORMAL;
}

void activateDanceMode() {
    Serial.println(F("\n╔═══════════════════════════════════╗"));
    Serial.println(F("║      TURRET DANCE MODE!             ║"));
    Serial.println(F("║  Gettin' DOWN to business!         ║"));
    Serial.println(F("╚═══════════════════════════════════╝\n"));
    
    currentMode = MODE_DANCE;

    // Dance routine pattern
    for (int sequence = 0; sequence < 2; sequence++) {
        leftMove(2);
        delay(150);
        upMove(1);
        delay(150);
        rightMove(2);
        delay(150);
        downMove(1);
        delay(150);
    }

    // Spin and fire finale
    for (int i = 0; i < 3; i++) {
        leftMove(1);
        delay(100);
        fire();
        delay(150);
        rightMove(1);
        delay(100);
        fire();
        delay(150);
    }

    homeServos();
    Serial.println(F("Dance complete! Returning to home position."));
    currentMode = MODE_NORMAL;
}

void activateEvasionMode() {
    Serial.println(F("\n╔═══════════════════════════════════╗"));
    Serial.println(F("║     EVASION MODE ACTIVATED!         ║"));
    Serial.println(F("║  Dodging incoming fire for 5sec!   ║"));
    Serial.println(F("╚═══════════════════════════════════╝\n"));
    
    currentMode = MODE_EVASION;
    unsigned long startTime = millis();

    while (millis() - startTime < 5000) {
        int r = random(0, 4);
        
        switch (r) {
            case 0:
                leftMove(1);
                Serial.println(F("  ~ DODGE LEFT ~"));
                break;
            case 1:
                rightMove(1);
                Serial.println(F("  ~ DODGE RIGHT ~"));
                break;
            case 2:
                upMove(1);
                Serial.println(F("  ~ DODGE UP ~"));
                break;
            case 3:
                downMove(1);
                Serial.println(F("  ~ DODGE DOWN ~"));
                break;
        }
        delay(200);
    }

    Serial.println(F("Evasion complete!"));
    homeServos();
    currentMode = MODE_NORMAL;
}

void activateSniperMode() {
    Serial.println(F("\n╔═══════════════════════════════════╗"));
    Serial.println(F("║      SNIPER MODE ACTIVATED!        ║"));
    Serial.println(F("║  Precision targeting engaged!      ║"));
    Serial.println(F("╚═══════════════════════════════════╝\n"));
    
    currentMode = MODE_SNIPER;

    // Slow, methodical targeting
    Serial.println(F("Acquiring target..."));
    for (int i = 0; i < 3; i++) {
        delay(500);
        Serial.println(F("."));
    }

    Serial.println(F("Target locked. Preparing shot..."));
    delay(1000);

    // Precise single shot
    fire();
    Serial.println(F("SNIPER SHOT - TARGET ELIMINATED"));

    delay(1000);
    homeServos();
    currentMode = MODE_NORMAL;
}

void activateTurretBurst() {
    Serial.println(F("\n╔═══════════════════════════════════╗"));
    Serial.println(F("║    TURRET BURST MODE ACTIVATED!    ║"));
    Serial.println(F("║  Unleashing MAXIMUM FIREPOWER!     ║"));
    Serial.println(F("╚═══════════════════════════════════╝\n"));
    
    currentMode = MODE_TURRET_BURST;

    // Sweep left while firing
    for (int i = 0; i < 3; i++) {
        leftMove(1);
        fire();
        delay(100);
    }

    delay(200);

    // Sweep right while firing
    for (int i = 0; i < 6; i++) {
        rightMove(1);
        fire();
        delay(100);
    }

    delay(200);

    // Return to center and final volley
    for (int i = 0; i < 3; i++) {
        leftMove(1);
        fire();
        delay(100);
    }

    Serial.println(F("BURST COMPLETE - SYSTEMS NOMINAL"));
    homeServos();
    currentMode = MODE_TURRET_BURST;
}

void activateAttractMode() {
    Serial.println(F("\n[ATTRACT MODE] Turret is idle - demo time!"));
    
    // Attractive idle animations
    shakeHeadYes(2);
    delay(300);

    for (int i = 0; i < 2; i++) {
        leftMove(1);
        delay(150);
        rightMove(2);
        delay(150);
        leftMove(1);
        delay(150);
    }

    upMove(2);
    fire();
    downMove(2);

    delay(500);
    homeServos();
    
    Serial.println(F("[ATTRACT MODE] Demonstration complete. Waiting for input..."));
}

#pragma endregion FUNCTIONS

//////////////////////////////////////////////////
               //  END CODE  //
//////////////////////////////////////////////////
