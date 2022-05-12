#include <USI_TWI_Master.h>
#include <TinyWireM.h>

#include <avr/power.h>
#include <avr/sleep.h>

uint8_t dispbuf[8];

void initDisp(int brightness) {
  TinyWireM.begin();

  delay(100);
  
  TinyWireM.beginTransmission(0x70);
  TinyWireM.send(0x21); // Oscillator On
  TinyWireM.endTransmission();

  delay(100);
  
  TinyWireM.beginTransmission(0x70);
  TinyWireM.send(0x81); // Blink Off
  TinyWireM.endTransmission();
  
  TinyWireM.beginTransmission(0x70);
  TinyWireM.write(0xE0 | brightness);
  TinyWireM.endTransmission();
}

void sleepDisp() {
  TinyWireM.beginTransmission(0x70);
  TinyWireM.write(0x20);
  TinyWireM.endTransmission();
}

void writeDisp() {
  TinyWireM.beginTransmission(0x70);
  TinyWireM.write((uint8_t)0x00);
  for (uint8_t i = 0; i < 8; ++i) {
    TinyWireM.write(dispbuf[i]);
    TinyWireM.write(0x00);
  }
  TinyWireM.endTransmission();
}

void setPixel(uint8_t x, uint8_t y, bool on) {
  if (x & 0xF8 | y & 0xF8) {
    return;
  }
  
  uint8_t col = (y + 7) % 8;
  uint8_t row = 7 - x;
  
  if (on) {
    dispbuf[row] |= 1 << col;
  } else {
    dispbuf[row] &= ~(1 << col);
  }
}

void clearDisp() {
  for (uint8_t y = 0; y < 8; ++y) {
    dispbuf[y] = 0;
  }
}

void setBitmap(uint8_t bmp[8]) {
  for (uint8_t y = 0; y < 8; ++y) {
    for (uint8_t x = 0; x < 8; ++x) {
      setPixel(x, y, bmp[y] & (0b10000000 >> x));
    }
  }
}

bool prevButtonState = HIGH;

bool buttonPressed() {
  bool buttonState = digitalRead(4);
  if (buttonState != prevButtonState) {
    prevButtonState = buttonState;
    return buttonState;
  }
  return false;
}

void awaitButton() {
  sleepDisp();
  GIMSK = _BV(PCIE);
  power_all_disable();
  set_sleep_mode(SLEEP_MODE_PWR_DOWN);
  sleep_enable();
  sei();
  sleep_mode();
  // Sleep
  GIMSK = 0;
  power_timer0_enable();
  power_usi_enable();
  TinyWireM.begin();
  initDisp(0);
}

void setup() {
  pinMode(1, OUTPUT); // LED
  digitalWrite(1, HIGH);
  pinMode(4, INPUT); // BUTTON

  power_timer1_disable(); // Disable unused peripherals
  power_adc_disable();

  PCMSK |= _BV(PCINT4); // Setup interrupt wake pin
  
  initDisp(0);
}

uint8_t gameMap[] = {0, 0, 0, 0, 0, 0, 0, 0b01111110};
uint8_t newPiecePos = 0;
int8_t newPieceLength = 6;
uint8_t newPieceHeight = 6;

uint8_t leftBound = 1;
uint8_t rightBound = 6;

bool newButtonPress = false;

void runGame() {
  // Reset game map
  for (uint8_t y = 0; y < 7; ++y) {
    gameMap[y] = 0;
  }
  newPiecePos = 0;
  newPieceLength = 6;
  newPieceHeight = 6;
  leftBound = 1;
  rightBound = 6;
  
  gameMap[7] = 0b01111110;

  // Display a "first frame" to let the user prepare
  setBitmap(gameMap);
  delay(200);
  
  while (true) {

    if (newButtonPress) {
      // Clear flag
      newButtonPress = false;

      // Calculate new bounds
      leftBound = max(newPiecePos-newPieceLength+1, leftBound);
      rightBound = min(newPiecePos, rightBound);
      
      // Bake the new piece into the map
      gameMap[newPieceHeight] = 0;
      for (uint8_t x = leftBound; x <= rightBound; ++x) {
        gameMap[newPieceHeight] |= (128 >> x);
      }
      newPieceLength = rightBound - leftBound + 1;
      
      
      // Make a new piece
      newPiecePos = 0;
      if (newPieceHeight > 0) {
        newPieceHeight--;
      } else {
        // Shuffle the rest of the map down
        for (int8_t i = 7; i >= 0; --i) {
          gameMap[i+1] = gameMap[i];
        }
        // Clear the top of the map
        gameMap[0] = 0;
      }
    } else {
      // Update the position of the current new piece
      newPiecePos++;
    }

    // Draw frame
    clearDisp();
    // Draw current game map
    setBitmap(gameMap);
    // Draw new piece
    for (int x = newPiecePos; x > newPiecePos - newPieceLength; --x) {
      if (x >= 0 && x < 8) {
        setPixel(x, newPieceHeight, 1);
      }
    }
    // Draw display
    writeDisp();

    // Test for game end condition
    if ((newPiecePos - newPieceLength) > 6 || newPieceLength <= 0) {
      clearDisp();
      writeDisp();
      return;
    }
    
    for (uint8_t i = 0; i < 10; ++i) {
      if (buttonPressed()) {
        newButtonPress = true;
      }
      delay(20);
    }
  }
}

void loop() {
  runGame();
  awaitButton();
}

ISR(PCINT0_vect) {}
