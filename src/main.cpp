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
#include "Adafruit_ZeroI2S.h"

#define SAMPLERATE_HZ 48000
#define BITWIDTH I2S_16_BIT

AudioControlSGTL5000 audioShield;
Adafruit_ZeroI2S i2s;

void SetupI2S() {
  
  if (!i2s.begin(BITWIDTH, SAMPLERATE_HZ)) {
    while (1);            
  }

  // This works around a quirk in the hardware (errata 1.2.1) -
  // the DFLLCTRL register must be manually reset to this value before
  // configuration.
  while ( (SYSCTRL->PCLKSR.reg & SYSCTRL_PCLKSR_DFLLRDY) == 0 );
  SYSCTRL->DFLLCTRL.reg = SYSCTRL_DFLLCTRL_ENABLE;
  while ( (SYSCTRL->PCLKSR.reg & SYSCTRL_PCLKSR_DFLLRDY) == 0 );

  // Overclock to 49.152MHz so that divide by 4 gives 12.288MHz for MCLK
  // https://github.com/arduino/ArduinoCore-samd/blob/master/cores/arduino/startup.c
  // Arduino core already sets DFLL clock source as internal OSC32K 32.768kHz oscillator
  // So just need to adjust the multiplier to 1500 * 32.768kHz = 49.152MHz
  SYSCTRL->DFLLMUL.reg =
      SYSCTRL_DFLLMUL_MUL(1500) |
      /* The coarse and fine step are used by the DFLL to lock
       on to the target frequency. These are set to half
       of the maximum value. Lower values mean less overshoot,
       whereas higher values typically result in some overshoot but
       faster locking. */
      SYSCTRL_DFLLMUL_FSTEP(511) |
      SYSCTRL_DFLLMUL_CSTEP(31);
  while(!SYSCTRL->PCLKSR.bit.DFLLRDY);

  // Setting up the DFLL is to set it to closed loop mode and turn it on
  SYSCTRL->DFLLCTRL.reg |= 
    SYSCTRL_DFLLCTRL_MODE |
    SYSCTRL_DFLLCTRL_WAITLOCK |
    SYSCTRL_DFLLCTRL_RUNSTDBY |
    SYSCTRL_DFLLCTRL_ENABLE;
  while (!SYSCTRL->PCLKSR.bit.DFLLLCKC || !SYSCTRL->PCLKSR.bit.DFLLLCKF) {}

  // WIP: block to reference DFLL to USB start of frame for better 12MHz accuracy
  // If USB not attached then just output factory calibrated 12MHz
  // Potentially uses more power but seems to have the same current draw
  // {
  //   // Enable USB clock recovery mode
  //   SYSCTRL->DFLLCTRL.reg |=
  //         SYSCTRL_DFLLCTRL_USBCRM |
  //         /* Disable chill cycle as per datasheet to speed up locking.
  //           This is specified in section 17.6.7.2.2, and chill cycles
  //           are described in section 17.6.7.2.1. */
  //         SYSCTRL_DFLLCTRL_CCDIS |
  //         SYSCTRL_DFLLCTRL_RUNSTDBY |
  //         SYSCTRL_DFLLCTRL_ENABLE;
  //   while ( (SYSCTRL->PCLKSR.reg & SYSCTRL_PCLKSR_DFLLRDY) == 0 );

  //   // Configure the DFLL to multiply the 1 kHz clock to 48 MHz
  //   SYSCTRL->DFLLMUL.reg |=
  //         /* This value is output frequency / reference clock frequency,
  //             so 48 MHz / 1 kHz */
  //         SYSCTRL_DFLLMUL_MUL(48000);
  //         // SYSCTRL_DFLLMUL_MUL(49152);

  //   // Closed loop mode
  //   SYSCTRL->DFLLCTRL.bit.MODE = 1;
  //   while ( !SYSCTRL->PCLKSR.bit.DFLLRDY );
  // }

  // Change I2S clocks to output correct freq
  // For SAMD21, Adafruit_ZeroI2S divides DFLL48M clock equally for MCLK and BCLK
  // Need to change so that MCLK = 49.152MHz / 4 = 12.288Mhz
  GCLK->GENDIV.reg = GCLK_GENDIV_DIV(4) |             // Divide by 4
                     GCLK_GENDIV_ID(I2S_GCLK_ID_0);   // Divide the clock for I2S
  while (GCLK->STATUS.bit.SYNCBUSY);
  
  // Then change BCLK to match required for sampling rate
  I2S->CLKCTRL[0].bit.MCKDIV = 7;

  // Enable MCLK on Pin PA09
  I2S->CLKCTRL[0].bit.MCKEN = 1;
  PORT->Group[PORTA].PINCFG[9].bit.PMUXEN = 1;
  PORT->Group[PORTA].PMUX[9 >> 1].reg |= PORT_PMUX_PMUXO_G; 

  // Configure the regulator to run in normal mode when in standby mode
  // Otherwise it defaults to low power mode and can only supply 50 uA
  SYSCTRL->VREG.bit.RUNSTDBY = 1;

  // Enable the DFLL48M clock in standby mode
  // SYSCTRL->DFLLCTRL.bit.ONDEMAND = 0;
  SYSCTRL->DFLLCTRL.bit.RUNSTDBY = 1;

  // DFLL48M is clocked from OSC32K when crystalless
  // Enable the OSC32K clock in standby mode
  // SYSCTRL->OSC32K.bit.ONDEMAND = 0;
  SYSCTRL->OSC32K.bit.RUNSTDBY = 1;

  // Run generic clock in deep sleep
  GCLK->GENCTRL.bit.RUNSTDBY = 1;

  // Enable I2S to receive mode
  i2s.enableRx();
}

void SetupSGTL5000() {

  audioShield.enable();
  audioShield.muteHeadphone();
  audioShield.volume(0);
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
  audioShield.volume(0.7);
  audioShield.unmuteHeadphone();
}

void EnterDeepSleep() {
  // Enter deep sleep / stand by to reduce power
  SCB->SCR |= SCB_SCR_SLEEPDEEP_Msk;
  __DSB();
  __WFI();
}

void setup() {

  // Turn off LED
  REG_PORT_OUTCLR0 = PORT_PA15;

  // Set up I2S clocks
  SetupI2S();
  delay(100);
  
  // Set up SGTL5000
  SetupSGTL5000();
  delay(100);
}

void loop() {
  EnterDeepSleep();
}