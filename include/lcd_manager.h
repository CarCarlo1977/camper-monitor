// ============================================================
//  lcd_manager.h - Gestione LCD 20x4 HD44780
//  CamperMonitor v3.0 - Tutte le schermate
// ============================================================

#ifndef LCD_MANAGER_H
#define LCD_MANAGER_H

#include <stdint.h>
#include <LiquidCrystal.h>

// ============================================================
//  PINOUT ESP32 LCD 20x4
// ============================================================
#define LCD_RS      19
#define LCD_EN      18
#define LCD_D4       5
#define LCD_D5      17
#define LCD_D6      16
#define LCD_D7       4
#define LCD_BL      12   // PWM backlight

// ============================================================
//  COSTANTI TIMER
// ============================================================
#define LCD_MS              500UL    // aggiorna LCD ogni 500ms
#define SAVE_MS          300000UL    // salva EEPROM ogni 5 minuti
#define BTN_DEBOUNCE_MS     50UL
#define AUTODIM_MS       10000UL     // auto-dim dopo 10 secondi
#define AUTODIM_LEVEL        1       // livello min brightness

// ============================================================
//  ENUM STATI UI
// ============================================================
enum UIState : uint8_t {
  STATE_DASHBOARD,
  STATE_MINMAX,
  STATE_MENU,
  STATE_EDIT_PARAM,
  STATE_EDIT_BATTYPE,
  STATE_EDIT_BACKLIGHT,
  STATE_EDIT_TANK_THR,
  STATE_CONFIRM_RESET
};

// ============================================================
//  CLASSE LCD MANAGER
// ============================================================
class LCDManager {
public:
  LCDManager();
  
  // Inizializzazione
  void init();
  void initChars();
  
  // Aggiornamento UI
  void update();
  
  // Encoder/Button
  int8_t readEncoderDelta();
  bool btnJustPressed();
  
  // Pubbliche per accesso esterno
  UIState uiState;
  uint8_t backlightLevel;      // 0-10
  int8_t  menuIndex;           // 0-11
  uint8_t editIndex;           // per edit param
  bool    editSelectingAction;
  bool    editActionSave;
  uint8_t editBatType;
  uint8_t tankThrIndex;        // 0=nere, 1=grigie
  void handleEncoderInput();
  // Activity tracking  
  unsigned long lastActivityMs;
  bool isDimmed;
  
private:
  // Encoder
  volatile int32_t encoderRaw;
  int32_t encoderLast;
  
  // Button
  bool btnState;
  bool btnPrev;
  unsigned long btnLastMs;
  
  // Display
  LiquidCrystal* lcd;
  
  // Disegno schermate
  void drawDashboard();
  void drawMinMax();
  void drawMenu();
  void drawEditParam();
  void drawEditBatType();
  void drawEditBacklight();
  void drawEditTankThreshold();
  void drawConfirmReset();
  
  // Utility
  void lcdProgressBar(uint8_t row, float percent);
  void lcdPrintFloat(float val, uint8_t decimals, uint8_t width);
  void resetEncoderRef();
  void applyBacklight();
  
};

extern LCDManager lcdManager;

#endif // LCD_MANAGER_H