// ============================================================
//  lcd_manager.cpp - Implementazione LCD 20x4
// ============================================================

#include "lcd_manager.h"
#include "config.h"
#include "sensors.h"
#include <Arduino.h>
#include <Wire.h>

// ============================================================
//  ISTANZA GLOBALE
// ============================================================
LCDManager lcdManager;

// ============================================================
//  ENCODER ISR (IRAM_ATTR per ESP32)
// ============================================================
static volatile uint8_t encState = 0;
static volatile int32_t encoderRawGlobal = 0;

static const int8_t ENC_TABLE[16] = {
   0, -1,  1,  0,
   1,  0,  0, -1,
  -1,  0,  0,  1,
   0,  1, -1,  0
};

void IRAM_ATTR ISR_encoder() {
  encState = ((encState << 2) |
              (digitalRead(25) << 1) |   // ENC_A
               digitalRead(26)) & 0x0F;  // ENC_B
  encoderRawGlobal += ENC_TABLE[encState];
}

void IRAM_ATTR ISR_button() {
  // Button interrupt - gestito in btnJustPressed()
}

// ============================================================
//  CONSTRUCTOR & INIT
// ============================================================
LCDManager::LCDManager()
  : uiState(STATE_DASHBOARD),
    backlightLevel(8),
    menuIndex(0),
    editIndex(0),
    editSelectingAction(false),
    editActionSave(true),
    editBatType(0),
    tankThrIndex(0),
    lastActivityMs(0),
    isDimmed(false),
    encoderRaw(0),
    encoderLast(0),
    btnState(false),
    btnPrev(false),
    btnLastMs(0)
{
  lcd = nullptr;
}

void LCDManager::init() {
  // Crea istanza LiquidCrystal
  lcd = new LiquidCrystal(LCD_RS, LCD_EN, LCD_D4, LCD_D5, LCD_D6, LCD_D7);
  
  // GPIO setup
  pinMode(LCD_BL, OUTPUT);
  pinMode(25, INPUT_PULLUP);  // ENC_A
  pinMode(26, INPUT_PULLUP);  // ENC_B
  pinMode(27, INPUT_PULLUP);  // ENC_BTN
  digitalWrite(LCD_BL, 255);
  
  // LCD init
  lcd->begin(20, 4);
  lcd->clear();
  initChars();
  
  // Encoder ISR
  attachInterrupt(digitalPinToInterrupt(25), ISR_encoder, CHANGE);
  attachInterrupt(digitalPinToInterrupt(26), ISR_encoder, CHANGE);
  
  // Welcome screen
  lcd->setCursor(0, 0); lcd->print("CAMPER MONITOR v3.0");
  lcd->setCursor(0, 1); lcd->print("LCD:        OK");
  lcd->setCursor(0, 2); lcd->print("Encoder:    OK");
  lcd->setCursor(0, 3); lcd->print("Initializing...");
  
  delay(2000);
  lcd->clear();
  
  applyBacklight();
  lastActivityMs = millis();
  
  Serial.println("[LCD] ✓ LCDManager initialized");
}

void LCDManager::initChars() {
  // Character 1: full block (█)
  byte fullBlock[8] = {
    B11111, B11111, B11111, B11111,
    B11111, B11111, B11111, B11111
  };
  lcd->createChar(1, fullBlock);
}

// ============================================================
//  ENCODER & BUTTON
// ============================================================
int8_t LCDManager::readEncoderDelta() {
  noInterrupts();
  int32_t raw = encoderRawGlobal;
  interrupts();
  
  int32_t steps = raw / 4;
  int8_t  delta = 0;
  
  if      (steps > encoderLast) delta =  1;
  else if (steps < encoderLast) delta = -1;
  
  if (delta) encoderLast = steps;
  return delta;
}

bool LCDManager::btnJustPressed() {
  bool raw = (digitalRead(27) == LOW);  // ENC_BTN = GPIO 27
  unsigned long now = millis();
  
  if (raw != btnState && (now - btnLastMs) >= BTN_DEBOUNCE_MS) {
    btnState  = raw;
    btnLastMs = now;
  }
  
  bool fired = (btnState && !btnPrev);
  btnPrev    = btnState;
  return fired;
}

void LCDManager::resetEncoderRef() {
  noInterrupts();
  encoderLast = encoderRawGlobal / 4;
  interrupts();
}

// ============================================================
//  BACKLIGHT
// ============================================================
void LCDManager::applyBacklight() {
  analogWrite(LCD_BL, (uint8_t)((backlightLevel * 255UL) / 10));
}

// ============================================================
//  UTILITY DRAW
// ============================================================
void LCDManager::lcdProgressBar(uint8_t row, float percent) {
  uint8_t filled = constrain((uint8_t)((percent / 100.0f) * 20), 0, 20);
  lcd->setCursor(0, row);
  for (uint8_t i = 0; i < 20; i++)
    i < filled ? lcd->write(byte(1)) : lcd->print(' ');
}

void LCDManager::lcdPrintFloat(float val, uint8_t decimals, uint8_t width) {
  char buf[12];
  dtostrf(val, width, decimals, buf);
  lcd->print(buf);
}

// ============================================================
//  SCHERMATE (PRIVATE)
// ============================================================

// ─── DASHBOARD ───────────────────────────────────────
void LCDManager::drawDashboard() {
  initChars();
  
  static bool blink = false;
  static unsigned long lastBlink = 0;
  if (millis() - lastBlink >= 500) {
    lastBlink = millis();
    blink = !blink;
  }
  
  char line[21];
  char tmp1[8], tmp2[8];
  
  // R0: V + SOC + OVR alarm
  dtostrf(sensors.voltageBus, 6, 2, tmp1);
  dtostrf(sensors.batterySOC, 4, 0, tmp2);
  snprintf(line, 21, "%sV %s%%%s", tmp1, tmp2,
    (sensors.voltageBus > config.vHighThreshold && blink) ? "  OVR" : "     ");
  lcd->setCursor(0, 0); lcd->print(line);
  
  // R1: I + P + CUR alarm
  dtostrf(sensors.currentA, 6, 1, tmp1);
  dtostrf(sensors.powerW,   5, 0, tmp2);
  snprintf(line, 21, "%sA %sW%s", tmp1, tmp2,
    (abs(sensors.currentA) > config.iHighThreshold && blink) ? "  CUR" : "     ");
  lcd->setCursor(0, 1); lcd->print(line);
  
  // R2: SOC Bar
  lcdProgressBar(2, sensors.batterySOC);
  
  // R3: stato + ETA + tag
  char timeStr[9] = {0};
  if (sensors.currentA < -0.5f) {
    float ahLeft = config.batteryCapacityAh - sensors.ahUsed;
    float hLeft  = ahLeft / abs(sensors.currentA);
    uint16_t hh  = (uint16_t)hLeft;
    uint8_t  mm  = (uint8_t)((hLeft - hh) * 60.0f);
    if (hh > 99) snprintf(timeStr, 8, ">99h   ");
    else         snprintf(timeStr, 8, "%2uh%02um", hh, mm);
  } else if (sensors.currentA > 0.5f) {
    float hFull  = sensors.ahUsed / sensors.currentA;
    uint16_t hh  = (uint16_t)hFull;
    uint8_t  mm  = (uint8_t)((hFull - hh) * 60.0f);
    if (hh > 99) snprintf(timeStr, 8, ">99h   ");
    else         snprintf(timeStr, 8, "%2uh%02um", hh, mm);
  } else {
    snprintf(timeStr, 8, " --:-- ");
  }
  
  const char* stateStr = (sensors.currentA >  0.5f) ? "Charge "
                       : (sensors.currentA < -0.5f) ? "Dischg "
                       :                              "Idle   ";
  const char* tag = (sensors.voltageBus < config.vLowThreshold && blink) ? "LOW"
                  :                                                        "   ";
  
  snprintf(line, 21, "%s %s %s", stateStr, timeStr, tag);
  lcd->setCursor(0, 3); lcd->print(line);
}

// ─── MIN/MAX LOG ─────────────────────────────────────
void LCDManager::drawMinMax() {
  lcd->clear();
  char line[21]; 
  char t1[8], t2[8];
  
  lcd->setCursor(0, 0); lcd->print("-- Min/Max Log --   ");
  
  dtostrf(config.voltageMinSeen, 6, 2, t1);
  dtostrf(config.voltageMaxSeen, 6, 2, t2);
  snprintf(line, 21, "V:%sV %sV", t1, t2);
  lcd->setCursor(0, 1); lcd->print(line);
  
  dtostrf(config.currentMinSeen, 6, 1, t1);
  dtostrf(config.currentMaxSeen, 6, 1, t2);
  snprintf(line, 21, "I:%sA %sA", t1, t2);
  lcd->setCursor(0, 2); lcd->print(line);
  
  lcd->setCursor(0, 3); lcd->print(" [click = indietro] ");
}

// ─── MENU ────────────────────────────────────────────
const char* menuLabels[] = {
  "V High Thresh",     // 0
  "V Low Thresh",      // 1
  "I High Thresh",     // 2
  "Battery Cap.(Ah)",  // 3
  "Shunt (Ohm)",       // 4
  "Tipo Batteria",     // 5
  "Luminosita",        // 6
  "Soglia Acq.Nere",   // 7
  "Soglia Acq.Grigie", // 8
  "Min/Max Log",       // 9
  "Reset Min/Max",     // 10
  "< Indietro"         // 11
};
const uint8_t MENU_COUNT       = 12;
const uint8_t MENU_BAT_IDX     = 5;
const uint8_t MENU_BL_IDX      = 6;
const uint8_t MENU_TBLK_IDX    = 7;
const uint8_t MENU_TGRY_IDX    = 8;
const uint8_t MENU_MINMAX_IDX  = 9;
const uint8_t MENU_RESET_IDX   = 10;
const uint8_t MENU_BACK_IDX    = 11;
const uint8_t MENU_EDIT_COUNT  = 5;

void LCDManager::drawMenu() {
  lcd->clear();
  int8_t start = (menuIndex / 4) * 4;
  int8_t end   = min((int)(start + 4), (int)MENU_COUNT);
  
  char batName[9];
  config.getBatName(config.batteryType, batName, 9);
  
  for (int8_t i = start, row = 0; i < end; i++, row++) {
    lcd->setCursor(0, row);
    lcd->print(i == menuIndex ? '>' : ' ');
    lcd->print(menuLabels[i]);
    lcd->setCursor(14, row);
    
    switch (i) {
      case 0: lcdPrintFloat(config.vHighThreshold,    1, 5); break;
      case 1: lcdPrintFloat(config.vLowThreshold,     1, 5); break;
      case 2: lcdPrintFloat(config.iHighThreshold,    0, 5); break;
      case 3: lcdPrintFloat(config.batteryCapacityAh, 0, 5); break;
      case 4: lcdPrintFloat(config.shuntOhm,          5, 7); break;
      case 5: lcd->print(batName); break;
      case 6: {
        lcd->setCursor(10, row);
        for (uint8_t b = 0; b < 10; b++)
          b < backlightLevel ? lcd->write(byte(1)) : lcd->print('-');
        break;
      }
      case 7: {
        char tbuf[6];
        snprintf(tbuf, 6, "%4u", config.tankBlackThreshold);
        lcd->print(tbuf);
        break;
      }
      case 8: {
        char tbuf[6];
        snprintf(tbuf, 6, "%4u", config.tankGrayThreshold);
        lcd->print(tbuf);
        break;
      }
      default: break;
    }
  }
}

// ─── EDIT PARAM ──────────────────────────────────────
void LCDManager::drawEditParam() {
  lcd->clear();
  lcd->setCursor(0, 0); lcd->print("Modifica:");
  lcd->setCursor(0, 1); lcd->print(menuLabels[editIndex]);
  lcd->setCursor(0, 2);
  lcd->print(editSelectingAction ? " " : "*");
  
  switch (editIndex) {
    case 0: lcdPrintFloat(config.vHighThreshold,    2, 7); break;
    case 1: lcdPrintFloat(config.vLowThreshold,     2, 7); break;
    case 2: lcdPrintFloat(config.iHighThreshold,    1, 7); break;
    case 3: lcdPrintFloat(config.batteryCapacityAh, 0, 7); break;
    case 4: lcdPrintFloat(config.shuntOhm,          6, 7); break;
  }
  
  lcd->setCursor(0, 3);
  if (editSelectingAction)
    lcd->print(editActionSave ? ">[Salva] [Annulla]" : " [Salva]>[Annulla]");
  else
    lcd->print(" [Salva] [Annulla]");
}

// ─── EDIT BATTERY TYPE ───────────────────────────────
void LCDManager::drawEditBatType() {
  lcd->clear();
  lcd->setCursor(0, 0); lcd->print("Tipo Batteria:");
  
  char name[9];
  uint8_t prev = (editBatType + 5 - 1) % 5;  // BAT_TYPE_COUNT = 5
  uint8_t next = (editBatType + 1) % 5;
  
  config.getBatName(prev,        name, 9);
  lcd->setCursor(0, 1); lcd->print("  "); lcd->print(name);
  
  config.getBatName(editBatType, name, 9);
  lcd->setCursor(0, 2); lcd->print("> "); lcd->print(name);
  
  config.getBatName(next,        name, 9);
  lcd->setCursor(0, 3); lcd->print("  "); lcd->print(name);
}

// ─── EDIT BACKLIGHT ──────────────────────────────────
void LCDManager::drawEditBacklight() {
  lcd->clear();
  lcd->setCursor(0, 0); lcd->print("Luminosita:");
  lcd->setCursor(0, 1);
  for (uint8_t b = 0; b < 10; b++)
    b < backlightLevel ? lcd->write(byte(1)) : lcd->print('-');
  char buf[8];
  snprintf(buf, 8, "  %2u/10", backlightLevel);
  lcd->print(buf);
  
  lcd->setCursor(0, 2); lcd->print("< gira encoder    >");
  lcd->setCursor(0, 3); lcd->print("  [click = salva] ");
}

// ─── EDIT TANK THRESHOLD ─────────────────────────────
void LCDManager::drawEditTankThreshold() {
  lcd->clear();
  char buf[21];
  
  if (tankThrIndex == 0) {
    // Acque nere
    lcd->setCursor(0, 0); lcd->print("Soglia Acq.Nere:");
    snprintf(buf, 21, "%cSoglia: %4u",
             editSelectingAction ? ' ' : '*', config.tankBlackThreshold);
    lcd->setCursor(0, 1); lcd->print(buf);
    snprintf(buf, 21, " A0 live:  %4u", sensors.tankADC[0]);
    lcd->setCursor(0, 2); lcd->print(buf);
    lcd->setCursor(0, 3); lcd->print("  [click = salva] ");
  } else {
    // Acque grigie
    snprintf(buf, 21, "%cSoglia grigie:%4u",
             editSelectingAction ? ' ' : '*', config.tankGrayThreshold);
    lcd->setCursor(0, 0); lcd->print(buf);
    snprintf(buf, 21, " Mar(1/3) A1:%4u", sensors.tankADC[1]);
    lcd->setCursor(0, 1); lcd->print(buf);
    snprintf(buf, 21, " Blu(2/3) A2:%4u", sensors.tankADC[2]);
    lcd->setCursor(0, 2); lcd->print(buf);
    snprintf(buf, 21, " Ner(pien)A3:%4u", sensors.tankADC[3]);
    lcd->setCursor(0, 3); lcd->print(buf);
  }
}

// ─── CONFIRM RESET ───────────────────────────────────
void LCDManager::drawConfirmReset() {
  lcd->clear();
  lcd->setCursor(0, 1); lcd->print("  Reset Min/Max?  ");
  lcd->setCursor(0, 3); lcd->print("  Click per OK    ");
}

// ============================================================
//  UPDATE - DISPATCHER LCD
// ============================================================
void LCDManager::update() {
  // Auto-dim check
  if (!isDimmed && (millis() - lastActivityMs) >= AUTODIM_MS) {
    isDimmed = true;
    analogWrite(LCD_BL, (uint8_t)((AUTODIM_LEVEL * 255UL) / 10));
  }
  
  // Render appropriata schermata
  switch (uiState) {
    case STATE_DASHBOARD:      drawDashboard();          break;
    case STATE_MINMAX:         drawMinMax();             break;
    case STATE_MENU:           drawMenu();               break;
    case STATE_EDIT_PARAM:     drawEditParam();          break;
    case STATE_EDIT_BATTYPE:   drawEditBatType();        break;
    case STATE_EDIT_BACKLIGHT: drawEditBacklight();      break;
    case STATE_EDIT_TANK_THR:  drawEditTankThreshold();  break;
    case STATE_CONFIRM_RESET:  drawConfirmReset();       break;
  }
}

// ============================================================
//  HANDLER ENCODER/BUTTON (da integrare in main loop)
// ============================================================
void LCDManager::handleEncoderInput() {
  int8_t delta = readEncoderDelta();
  
  if (delta == 0 && !btnJustPressed()) return;
  
  if (delta != 0) lastActivityMs = millis();
  if (btnJustPressed()) lastActivityMs = millis();
  
  // Restore brightness on activity
  if (isDimmed) {
    isDimmed = false;
    applyBacklight();
  }
  
  // ─── ENCODER HANDLING ──────────────────────────────
  if (delta != 0) {
    switch (uiState) {
      case STATE_MENU:
        menuIndex = (int8_t)((menuIndex + delta + MENU_COUNT) % MENU_COUNT);
        break;
      
      case STATE_EDIT_PARAM:
        if (!editSelectingAction) {
          switch (editIndex) {
            case 0: config.vHighThreshold    = constrain(config.vHighThreshold    + delta * 0.1f,    10.0f,   20.0f); break;
            case 1: config.vLowThreshold     = constrain(config.vLowThreshold     + delta * 0.1f,     5.0f,   15.0f); break;
            case 2: config.iHighThreshold    = constrain(config.iHighThreshold    + delta * 1.0f,     0.0f,  500.0f); break;
            case 3: config.batteryCapacityAh = constrain(config.batteryCapacityAh + delta * 1.0f,     1.0f, 1000.0f); break;
            case 4: config.shuntOhm          = constrain(config.shuntOhm          + delta * 0.00005f, 0.0001f, 0.01f); break;
          }
        } else {
          editActionSave = !editActionSave;
        }
        break;
      
      case STATE_EDIT_BATTYPE:
        editBatType = (uint8_t)((editBatType + delta + 5) % 5);
        break;
      
      case STATE_EDIT_BACKLIGHT:
        backlightLevel = (uint8_t)constrain((int)backlightLevel + delta, 0, 10);
        applyBacklight();
        break;
      
      case STATE_EDIT_TANK_THR:
        if (tankThrIndex == 0)
          config.tankBlackThreshold = (uint16_t)constrain((int)config.tankBlackThreshold + delta, 0, 1023);
        else
          config.tankGrayThreshold  = (uint16_t)constrain((int)config.tankGrayThreshold  + delta, 0, 1023);
        break;
      
      default: break;
    }
  }
  
  // ─── BUTTON HANDLING ───────────────────────────────
  if (btnJustPressed()) {
    switch (uiState) {
      case STATE_DASHBOARD:
        menuIndex = 0;
        resetEncoderRef();
        uiState = STATE_MENU;
        lcd->clear();
        break;
      
      case STATE_MINMAX:
        uiState = STATE_MENU;
        lcd->clear();
        break;
      
      case STATE_MENU:
        if (menuIndex < MENU_EDIT_COUNT) {
          editIndex = menuIndex;
          editSelectingAction = false;
          editActionSave = true;
          resetEncoderRef();
          uiState = STATE_EDIT_PARAM;
          lcd->clear();
        } else if (menuIndex == MENU_BAT_IDX) {
          editBatType = config.batteryType;
          resetEncoderRef();
          uiState = STATE_EDIT_BATTYPE;
          lcd->clear();
        } else if (menuIndex == MENU_BL_IDX) {
          resetEncoderRef();
          uiState = STATE_EDIT_BACKLIGHT;
          lcd->clear();
        } else if (menuIndex == MENU_TBLK_IDX) {
          tankThrIndex = 0;
          resetEncoderRef();
          uiState = STATE_EDIT_TANK_THR;
          lcd->clear();
        } else if (menuIndex == MENU_TGRY_IDX) {
          tankThrIndex = 1;
          resetEncoderRef();
          uiState = STATE_EDIT_TANK_THR;
          lcd->clear();
        } else if (menuIndex == MENU_MINMAX_IDX) {
          uiState = STATE_MINMAX;
          lcd->clear();
        } else if (menuIndex == MENU_RESET_IDX) {
          uiState = STATE_CONFIRM_RESET;
          lcd->clear();
        } else {
          uiState = STATE_DASHBOARD;
          lcd->clear();
        }
        break;
      
      case STATE_EDIT_PARAM:
        if (!editSelectingAction) {
          editSelectingAction = true;
          editActionSave = true;
        } else {
          if (editActionSave) {
            if (editIndex == 4) {
              if (config.shuntOhm >= 0.0001f && config.shuntOhm <= 0.01f) {
                // applyShunt() nel main
              } else {
                config.shuntOhm = 0.00025f;
              }
            }
            config.save();
          } else {
            config.load();
            applyBacklight();
          }
          editSelectingAction = false;
          editActionSave = true;
          resetEncoderRef();
          uiState = STATE_MENU;
          lcd->clear();
        }
        break;
      
      case STATE_EDIT_BATTYPE:
        config.batteryType = editBatType;
        config.save();
        resetEncoderRef();
        uiState = STATE_MENU;
        lcd->clear();
        break;
      
      case STATE_EDIT_BACKLIGHT:
        config.save();
        resetEncoderRef();
        uiState = STATE_MENU;
        lcd->clear();
        break;
      
      case STATE_EDIT_TANK_THR:
        config.save();
        resetEncoderRef();
        uiState = STATE_MENU;
        lcd->clear();
        break;
      
      case STATE_CONFIRM_RESET:
        config.voltageMinSeen = 100.0f;
        config.voltageMaxSeen = 0.0f;
        config.currentMinSeen = 1000.0f;
        config.currentMaxSeen = 0.0f;
        uiState = STATE_MENU;
        lcd->clear();
        break;
      
      default: break;
    }
  }
}