# Prettygood DSP

A self contained, Arduino compatible board for applying audio DSP. The intended purpose is to equalize and apply bass boost to [BMR VR off ear headphones](https://prettygood3d.com/post/999028410814/quest-2-off-ear-bmr-v01), but can be adapted for any other light DSP tasks by SGTL5000 codec.

Supports:
-   7 band prarametric or 5 band graphic EQ.
-   Psycho-acoustic bass boost to over come limited bass response of small headphone drivers.
-   Stereo surround expansion.
-   Automatic volume control.

Please see the SGTL5000 [documentation](https://www.pjrc.com/teensy/SGTL5000.pdf) for all DSP features. Credit to [Paul Stoffregen of PJRC.com](https://github.com/PaulStoffregen/Audio) for creating the Teensy Audio library (we're reusing a SGTL5000 component in the code).

TODO:
-   Make use of the USER button for switching between presets.
-   Refactor code and put configurations in a header file.
- Periodically draw large current to keep USB power bank on.

# Usage
- Plug in a USB power source. Anything from computer, phone, Quest/Quest 2 or some USB power bank (see below) should work.
- Plug in 3.5mm stereo jack from audio out to LINEIN.
- Plug your headphones into OUT.

NOTE: the OUT port is designed to drive headphones. It may not work as intended as an input into another audio device.

# USB Power Bank Compatibility
Most USB power banks will turn off when powering the board, since the current draw is too low.

- Some brands will allow 'low power draw' mode by holding the on button for few seconds (i.e. Xiaomi).
- A list of tested power banks that support low power draw https://www.candlepowerforums.com/vb/showthread.php?440476-USB-powerbank-WITHOUT-auto-off
- Use a USB splitter cable to power both the headset and DSP board (see below).

# USB Splitter Cable for Oculus Link
This splitter cable is confirmed to work with Oculus Link and from power bank. It provides power and USB 2.0 data to the headset (USB-C) and just power to DSP board (Micro USB): https://www.amazon.com/gp/product/B078MFCVLF/ref=ppx_yo_dt_b_asin_title_o00_s00?ie=UTF8&psc=1

!()[images/micro_usb_splitter.png]

# Board Design
![Board Layout](board/board_layout_v1.png)

The board is designed based on [Adafruit SAMD21 QtPy](https://learn.adafruit.com/adafruit-qt-py). The default bootloader is vanilla SAMD21 QtPy and can be download from [Adafruit SAMD UF2 repo](https://github.com/adafruit/uf2-samdx1/releases). The only difference is that we've replaced the NeoPixel with a blue LED.

The schematic can be found [here](board/schematic_v1.pdf).

The advantage of using this bootloader is that any projects that can run on SAMD21 boards (Aduino Zero, QtPy, Trinket M0, Feather M0) can theoretically run on Prettygood DSP.

One limitation - the I2S peripheral needs to be on to provide clock signals to the SGTL5000 codec. So the I2S/SPI pins are not available as GPIO. Otherwise the TX and A0-A3 pins are free and available.

# Update firmware
The board firmare is updatable via USB. Follow these steps to compile and upload new firmware to the board. 

*This board comes with a fail-safe bootloader mode, making it almost impossible to brick by updating the firmware through USB.*

## 1. Install Platform IO
You need to first install VSCode, then Platform IO by following [these instructions](https://platformio.org/install/ide?install=vscode).

## 2. Open in Platform IO
Download this repository using git command, or download and extract the zip file on your computer.

![download](images/download_button.png)

In VSCode (with Platform IO installed), choose 'Open...' or 'Open Folder...' and select this repo's 'prettygood_dsp' folder.

## 3. Make changes and upload
Once you're happy making changes, click the checkmark button at the bottom of VSCode to build the new firmware.

![build](images/build_button.png)

Platform IO will fetch all the dependencies automatically for you.

**Connect the DSP board via USB and double clicking the RESET button. This triggers the boot loader mode. Check that the blue LED stays on and QT_BOOT drive is mounted on your desktop.**

Click upload button. It may take a minute or so for Platform IO to automatically detect the serial / COM port and upload new firmware.


![upload](images/upload_button.png)

If upload hangs or fails, try disconnecting the USB and putting the board in boot loader mode again.

Once sucessfully uploaded, you can disconnect the USB cable.

## Dependencies
 * [Adafruit Zero I2S](https://www.arduino.cc/reference/en/libraries/adafruit-zero-i2s-library/)

# Contributing
Contributions are welcome!

