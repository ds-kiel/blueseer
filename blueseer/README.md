## Setup

1. Install Zephyr (https://www.zephyrproject.org)
2. Put this folder into zephyr/samples/ 
3. Install Tensorflow Lite for Microcontrollers - I used the instructions in this repository https://github.com/nicknameBOB/TF_NCS_dev
4. Change the TF_SRC_DIR variable in CMakeLists.txt to the path to your tensorflow folder
5. Replace the adafruit feather bboard definition with: zephyr/boards/arm/adafruit_feather_nrf52840/adafruit_feather_nrf52840.dts` with the device tree file (`adafruit_feather_nrf52840.dts`) of this repository.
