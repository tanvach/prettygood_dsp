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

#include <Arduino.h>
#include "control_sgtl5000.h"
#include <Adafruit_ZeroI2S.h>
#include <RTCZero.h>

// Pins
#define LED_PIN 12
#define I2S_RX_PIN 17
#define I2S_TX_PIN PIN_I2S_SD

#define SAMPLERATE_HZ 44100
#define BITWIDTH I2S_16_BIT

AudioControlSGTL5000 audioShield;

Adafruit_ZeroI2S i2s = Adafruit_ZeroI2S(PIN_I2S_FS, PIN_I2S_SCK, I2S_RX_PIN, I2S_TX_PIN);

RTCZero rtc;

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

  // Configure the regulator to run in normal mode when in standby mode
  // Otherwise it defaults to low power mode and can only supply 50 uA
  SYSCTRL->VREG.bit.RUNSTDBY = 1;

  // Run 8MHz oscillator in deep sleep
  SYSCTRL->OSC8M.bit.RUNSTDBY = 1;

  // Run generic clock in deep sleep
  GCLK->GENCTRL.bit.RUNSTDBY = 1;

  // Enable I2S to receive mode
  i2s.enableRx();
}

void SetupSGTL5000() {

  // Enable as Master, input clock 8Mhz
  audioShield.enable(8000000);
  audioShield.muteHeadphone();
  audioShield.inputSelect(AUDIO_INPUT_LINEIN);

  // Usage: audioShield.enhanceBass((float)lr_level,(float)bass_level,(uint8_t)hpf_bypass,(uint8_t)cutoff);
  // Please see http://www.pjrc.com/teensy/SGTL5000.pdf page 50 for valid values for BYPASS_HPF and CUTOFF
  // 1: Pass through most of LR channel
  // 140: somewhat aggressive bass boost. Usable range ~ 100 - 160. Lower is more bass
  // 0: enable high pass filter
  // 4: Cutoff freq ~175Hz
  // Disabled since it reduces sound staging
  // audioShield.enhanceBass((float)0.7,(float)0.3,(uint8_t)0,(uint8_t)4);
  // audioShield.enhanceBassEnable();

  // Parametric EQ based on AutoEQ output
  audioShield.eqSelect(1);
  audioShield.eqFilterCount(7);

  int updateFilter[5];
  
  // Usage: calcBiquad(filtertype, fC, dB_Gain, Q, quantization_unit, fS, *coef)
  // quantization_unit is fixed at 524288 for SGTL5000
  calcBiquad(FILTER_PARAEQ, 59, 6.6, 0.17, 524288, SAMPLERATE_HZ, updateFilter);
  audioShield.eqFilter(0,updateFilter);
  calcBiquad(FILTER_PARAEQ, 243, -6.4, 1.71, 524288, SAMPLERATE_HZ, updateFilter);
  audioShield.eqFilter(1,updateFilter);
  calcBiquad(FILTER_PARAEQ, 811, 2.2, 1.90, 524288, SAMPLERATE_HZ, updateFilter);
  audioShield.eqFilter(2,updateFilter);
  calcBiquad(FILTER_PARAEQ, 1189, 3.3, 2.45, 524288, SAMPLERATE_HZ, updateFilter);
  audioShield.eqFilter(3,updateFilter);
  calcBiquad(FILTER_PARAEQ, 1645, -7.2, 0.64, 524288, SAMPLERATE_HZ, updateFilter);
  audioShield.eqFilter(4,updateFilter);
  calcBiquad(FILTER_PARAEQ, 6243, 1.4, 0.30, 524288, SAMPLERATE_HZ, updateFilter);
  audioShield.eqFilter(5,updateFilter);
  calcBiquad(FILTER_PARAEQ, 19864, -7.8, 0.38, 524288, SAMPLERATE_HZ, updateFilter);
  audioShield.eqFilter(6,updateFilter);

  // High pass filter adds noise for some reason. We don't need it.
  audioShield.adcHighPassFilterDisable();

  // Enable output
  audioShield.volume(0.6);
  audioShield.unmuteHeadphone();
}

// Begin usual Arduino code
void setup() {
  // Serial.begin(115200);

  // Set up I2S clocks
  SetupI2S();
  delay(100);
  
  // Set up SGTL5000
  SetupSGTL5000();
  delay(100);

  // Turn LED off
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, LOW);
}

void loop() {
  // Enter stand by to save power
  rtc.standbyMode();
}