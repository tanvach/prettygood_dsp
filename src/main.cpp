/* Prettygood3D Over Ear (OE-1) DSP board
 * Copyright (c) 2021, Pithawat Vachiramon, prettygood3d@gmail.com
 *
 * Development of this code was funded by Prettygood3D.com. Please support
 * Prettygood3D's efforts to develop open source software by purchasing the
 * DSP board and other Prettygood3D products.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice, development funding notice, and this permission
 * notice shall be included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

#define VERSION "1.0.1"

#include <Arduino.h>
#include <Adafruit_ZeroI2S.h>
#include <RTCZero.h>
#include <Adafruit_TinyUSB.h>
#include "control_sgtl5000.h"
#include "config.h"

// Pins
#define LED_PIN 12
#define USER_BUTTON_PIN 13
#define POWER_PULSE_PIN 21
#define I2S_RX_PIN 17
#define I2S_TX_PIN PIN_I2S_SD

// Audio
#define SAMPLERATE_HZ 44100
#define BITWIDTH I2S_16_BIT
AudioControlSGTL5000 audioShield;
Adafruit_ZeroI2S i2s = Adafruit_ZeroI2S(PIN_I2S_FS, PIN_I2S_SCK, I2S_RX_PIN, I2S_TX_PIN);

// Stand by and alarms
RTCZero rtc;

// USB WebUSB object
Adafruit_USBD_WebUSB usb_web;
WEBUSB_URL_DEF(landingPage, 1 /*https*/, "tanvach.github.io/prettygood_dsp/index.html");

// Shared variables
volatile boolean active = true;
volatile boolean user_button_pushed = false;
volatile boolean power_pulse = false;

void SetupI2S() {
  
  if (!i2s.begin(BITWIDTH, SAMPLERATE_HZ)) {
    while (1);            
  }

  // // Change I2S clocks to output reasonable frequency for SGTL5000's PLL
  // // For SAMD21, Adafruit_ZeroI2S divides DFLL48M clock equally for MCLK and BCLK
  // // Need to change so that MCLK = 8Mhz
  // GCLK->GENDIV.reg = GCLK_GENDIV_DIV(6) |             // Divide by 4
  //                    GCLK_GENDIV_ID(I2S_GCLK_ID_0);   // Divide the clock for I2S
  // while (GCLK->STATUS.bit.SYNCBUSY);

  // Change I2S clocks to output reasonable frequency for SGTL5000's PLL
  // Override and use OSC8M Oscillator because it's lower power and less noisy
  while (GCLK->STATUS.bit.SYNCBUSY);
  GCLK->GENDIV.bit.ID = I2S_CLOCK_GENERATOR;
  GCLK->GENDIV.bit.DIV = 1;
  while (GCLK->STATUS.bit.SYNCBUSY);
  GCLK->GENCTRL.bit.ID = I2S_CLOCK_GENERATOR;
  GCLK->GENCTRL.bit.SRC = GCLK_GENCTRL_SRC_OSC8M_Val;
  GCLK->GENCTRL.bit.IDC = 1;
  GCLK->GENCTRL.bit.GENEN = 1;
  while (GCLK->STATUS.bit.SYNCBUSY);

  // Enable MCLK on Pin PA09
  I2S->CLKCTRL[0].bit.MCKEN = 1;
  PORT->Group[PORTA].PINCFG[9].bit.PMUXEN = 1;
  PORT->Group[PORTA].PMUX[9 >> 1].reg |= PORT_PMUX_PMUXO_G; 

  // Disable BCLK and FS since we're using SGTL5000 as master
  // TODO: Change this so receiving I2S works
  PORT->Group[PORTA].PINCFG[10].bit.PMUXEN = 0;
  PORT->Group[PORTA].PINCFG[11].bit.PMUXEN = 0;

  // Configure the regulator and clocks to run in standby mode
  SYSCTRL->VREG.bit.RUNSTDBY = 1;
  SYSCTRL->DFLLCTRL.bit.RUNSTDBY = 1;
  SYSCTRL->OSC8M.bit.RUNSTDBY = 1;
  GCLK->GENCTRL.bit.RUNSTDBY = 1;

  // Enable I2S to receive mode
  i2s.enableRx();
}

void SetupSGTL5000() {

  audioShield.muteHeadphone();
  audioShield.inputSelect(AUDIO_INPUT_LINEIN);
  audioShield.enhanceBassDisable();
  audioShield.autoVolumeDisable();

  // Psycho-acoutic bass boost
  if (config_doc["enhance_bass"]) {
    float lr_vol = config_doc["enhance_bass_lr_vol"];
    float bass_vol = config_doc["enhance_bass_bass_vol"];
    uint8_t high_pass = config_doc["enhance_bass_high_pass"];
    uint8_t cutoff = config_doc["enhance_bass_cutoff"];
    audioShield.enhanceBassEnable();
    audioShield.enhanceBass(lr_vol, bass_vol, high_pass, cutoff);
  }

    // Auto volume
  if (config_doc["auto_volume"]) {
    uint8_t maxGain = config_doc["auto_volume_max_gain"];
    uint8_t lbiResponse = config_doc["auto_volume_lbi_response"];
    uint8_t hardLimit = config_doc["auto_volume_hard_limit"];
    float threshold = config_doc["auto_volume_threshold"];
    float attack = config_doc["auto_volume_attack"];
    float decay = config_doc["auto_volume_decay"];
    audioShield.autoVolumeEnable();
    audioShield.autoVolumeControl(maxGain, lbiResponse, hardLimit, threshold, attack, decay);
  }

  // Set up EQ filters
  uint8_t filter_type = config_doc["filter_type"];
  uint8_t filter_count = config_doc["filter_count"];
  audioShield.eqSelect(filter_type);
  audioShield.eqFilterCount(filter_count);

  // Parametric EQ
  if (filter_type == 1) {

    int updateFilter[5];    

    for (uint8_t i = 0; i < filter_count; i++) {
      // Usage: calcBiquad(filtertype, fC, dB_Gain, Q, quantization_unit, fS, *coef)
      // quantization_unit is fixed at 524288 for SGTL5000
      float filter_fc = config_doc["filter_fc"][i];
      float filter_db = config_doc["filter_db"][i];
      float filter_q = config_doc["filter_q"][i];
      // Serial.print("Biquad filter ");
      // Serial.print(i);
      // Serial.print(" with fc:");
      // Serial.print(filter_fc);
      // Serial.print(" gain:");
      // Serial.print(filter_db);
      // Serial.print(" q:");
      // Serial.print(filter_q);
      // Serial.println();

      calcBiquad(
        FILTER_PARAEQ, 
        filter_fc, 
        filter_db, 
        filter_q, 
        524288, 
        SAMPLERATE_HZ, 
        updateFilter
      );
      audioShield.eqFilter(i, updateFilter);
    }
  }

  // High pass filter adds noise for some reason. We don't need it.
  audioShield.adcHighPassFilterDisable();

  // Enable output
  float volume = config_doc["volume"];
  audioShield.volume(volume);
  audioShield.unmuteHeadphone();
}


// WebUSB
void SendConfigJSON() {
  serializeJsonPretty(config_doc, usb_web);
  usb_web.println("\n");
  usb_web.println("<<EOF>>");
  delay(100);
}

// Call backs
void user_button_isr() {
  user_button_pushed = true;
}

void line_state_callback(bool connected)
{
  digitalWrite(LED_PIN, connected);
  // Send config JSON to browser
  if ( connected ) SendConfigJSON();
}

void alarm_callback() {
  power_pulse = true;
}

// Power pulse regular alarm
void SetAlarmThenSleep() {
  bool keep_usb_battery_on = config_doc["keep_usb_battery_on"];
  int total_sec = config_doc["keep_usb_battery_on_pulse_period_sec"];
  if (keep_usb_battery_on & (total_sec > 0) & (total_sec < 3600)) {
    uint8_t seconds = total_sec % 60;
    uint8_t minutes = total_sec / 60;
    rtc.setTime(0, 0, 0);
    rtc.setDate(1, 1, 21);
    rtc.setAlarmTime(0, minutes, seconds);
    rtc.enableAlarm(rtc.MATCH_HHMMSS);
    rtc.attachInterrupt(alarm_callback);
  }
  rtc.standbyMode();
}

bool isUSBConnected() {
  // Detect USB connection by checking if there's a change in USB frame number
  uint16_t f = USB->DEVICE.FNUM.bit.FNUM;
  delay(3);
  return f != USB->DEVICE.FNUM.bit.FNUM;
}

// Pulse USB power by wasting current
void PowerPulse(bool state) {
  if (state) {
    pinMode(A0, OUTPUT);
    analogWrite(A0, 1023);
  } else {
    digitalWrite(A0, LOW);
    pinMode(A0, INPUT);
  }
  digitalWrite(POWER_PULSE_PIN, state);
  if (config_doc["keep_usb_battery_on_led"]) digitalWrite(LED_PIN, state);
}

// Begin usual Arduino code

void setup() {
  Serial.begin(115200);

  // delay(2000);
  LoadConfig();
  serializeJsonPretty(config_doc, Serial);
  Serial.println();
  // delay(1000);

  // Set up I2S clocks
  SetupI2S();
  delay(10);

  // Enable as Master, input clock 8Mhz
  audioShield.enable(8000000);
  // Set up SGTL5000
  SetupSGTL5000();

  // Turn LED off
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, LOW);

  // Enable USER button (active low)
  pinMode(USER_BUTTON_PIN, INPUT_PULLUP);
  attachInterrupt(USER_BUTTON_PIN, user_button_isr, CHANGE);

  // USB power pulse (v1.1 board and above only)
  pinMode(POWER_PULSE_PIN, OUTPUT);
  analogWriteResolution(10);
  
  // Enter stand by to save power if USER button is not held down while booting
  if (digitalRead(USER_BUTTON_PIN)) {
    active = false;
    rtc.begin();
    // Allow USB to make connection first
    delay(1000);
    SetAlarmThenSleep();
  } else {
    // If USER button is pressed then set up WebUSB for configuration
    active = true;
    usb_web.setLandingPage(&landingPage);
    usb_web.setLineStateCallback(line_state_callback);
    usb_web.setStringDescriptor("Prettygood DSP WebUSB");
    usb_web.begin();
    // Allow USB to make connection first
    delay(1000);
  }
}

void loop() {

  // Process user button push event
  if (user_button_pushed) {
    // Do nothing
    user_button_pushed = false;
  }

  // Process power pulse alarm event
  if (power_pulse) {
    power_pulse = false;
    // Only pulse if not real USB connection (i.e. USB powerbank)
    if (!isUSBConnected()) {
      uint16_t pulse_duration_ms = config_doc["keep_usb_battery_on_pulse_duration_msec"];
      // Max pulse = 2s
      pulse_duration_ms = min(pulse_duration_ms, 2000);
      // Pulse
      PowerPulse(HIGH);
      delay(pulse_duration_ms);
      PowerPulse(LOW);
    }
  }

  if (active) {

    // WebUSB
    if (usb_web.available()) {

      DynamicJsonDocument received_doc(JSON_DOC_SIZE);
      deserializeJson(received_doc, usb_web);

      // Check that the config is valid
      // TODO: Better check
      if ((received_doc["volume"] >= 0.0) & (received_doc["volume"] <= 1.0)) {
        config_doc = received_doc;
        SetupSGTL5000();
        SendConfigJSON();
      }

      // Reload defaults config
      if (received_doc["reload_defaults"]) {
        LoadConfig(true);
        SetupSGTL5000();
        SendConfigJSON();
      }

      // Save to flash 
      if (received_doc["save_to_flash"]) {
        SaveConfig();
        SendConfigJSON();
      }
      
    }

  } else {
    // Stand by if not booted in active mode
    SetAlarmThenSleep();
  }
}
