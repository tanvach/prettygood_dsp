# Prettygood DSP

A self contained, Arduino compatible board for applying audio DSP. The intended purpose is to equalize and apply bass boost to [BMR VR off ear headphones](https://prettygood3d.com/post/999028410814/quest-2-off-ear-bmr-v01), but can be adapted for any other light DSP tasks by SGTL5000 codec.

Supports:
-   7 band prarametric or 5 band graphic EQ.
-   Psycho-acoustic bass boost to over come limited bass response of small headphone drivers.
-   Stereo surround expansion.
-   Automatic volume control.

Please see the SGTL5000 [documentation](https://www.pjrc.com/teensy/SGTL5000.pdf) for all DSP features.

TODO:
-   Make use of the USER button for switching between presets.
-   Refactor code and put configurations in a header file.

# Usage
- Plug in a USB power source (anything from computer, phone, Quest/Quest 2 or USB power bank).
- Plug in 3.5mm stereo jack from audio out to LINEIN.
- Plug your headphones into OUT.

NOTE: the OUT port is designed to drive headphones. It may not work as intended as an input into another audio device.

# Board Design
![Board Layout](board/board_layout_v1.png)

The board is designed based on [Adafruit SAMD21 QtPy](https://learn.adafruit.com/adafruit-qt-py). The default bootloader is vanilla SAMD21 QtPy and can be download from [Adafruit SAMD UF2 repo](https://github.com/adafruit/uf2-samdx1/releases). The only difference is that we've replaced the NeoPixel with a blue LED.

The schematic can be found [here](board/schematic_v1.pdf).

The advantage of using this bootloader is that any projects that can run on SAMD21 boards (Aduino Zero, QtPy, Trinket M0, Feather M0) can theoretically run on Prettygood DSP.

One limitation - the I2S peripheral needs to be on to provide clock signals to the SGTL5000 codec. So the I2S/SPI pins are not available as GPIO. Otherwise the TX and A0-A3 pins are free and available.

# Installation
Open the repo in VSCode with PlatformIO installed. It should automatically download all dependencies. Compile and upload main.cpp through USB port. 



## Dependencies
 * [Adafruit Zero I2S](https://www.arduino.cc/reference/en/libraries/adafruit-zero-i2s-library/)

# Contributing
Contributions are welcome!

