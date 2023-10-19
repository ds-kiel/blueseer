# BlueSeer: AI-Driven Environment Detection via BLE Scans

v0.1 - DAC pre-release

> Disclaimer: This is an early release, the code will be updated in the coming days.

Environment Detection refers to the ability, for a device, to autonomously recognize and classify its surroundings into broad categories such as Home, Office, Street, Transport, etc.
With environment-detection capabilities, wireless headphones can adapt noise cancellation on-the-fly when a user exits a building and walks near traffic. Smartphones can rely on environment detection to automatically turn on silent mode on when entering cinemas and airplane mode on when boarding a plane.
Although smartphones have access to many sensors to perform environment detection, e.g., cameras, microphones and GPS, typical IoT devices lack this diversity of sensing options.
With BlueSeer, we show that a Bluetooth Low Energy radio is the only component required to accurately classify our environment on low-power, resource-limited IoT devices.
BlueSeer achieves 84% accuracy, distingishing between 7 categories.

## How to use BlueSeer

The repository is divided into three main content: The python code to train the BlueSeer model, a raw dataset that needs to be parsed before training, and the C++ code for on-device inference.

##### Setup

To train the model, you need to install tensorflow, tensorflow-model-optimization, sckikit-learn, seaborn, and numpy via ```pip```.

To run the C++ code, you need to install Zephyr ([How to install Zephyr](https://docs.zephyrproject.org/latest/develop/getting_started/index.html)) and Tensorflow Lite for Microcontrollers ([Official website](https://www.tensorflow.org/lite/microcontrollers)).

- Replace `zephyr/boards/arm/adafruit_feather_nrf52840/adafruit_feather_nrf52840.dts` with the device tree file (`adafruit_feather_nrf52840.dts`) of this repository.

## Abstract
IoT devices rely on environment detection to trigger specific actions, e.g., for headphones to adapt noise cancellation to the surroundings.
While phones feature many sensors, from GNSS to cameras, small wearables must rely on the few energy-efficient components they already incorporate.
In this paper, we demonstrate that a Bluetooth radio is the only component required to accurately classify environments and present BlueSeer, an environment-detection system that solely relies on received BLE packets and an embedded neural network.
BlueSeer achieves an accuracy of up to 84% differentiating between 7 environments on resource-constrained devices, and requires only 12 ms for inference on a 64 MHz microcontroller-unit.

To cite the paper, please use:
> V. Poirot, L. Harms, H. Martens, O. Landsiedel. "BlueSeer: AI-Driven Environment Detection via BLE Scans", in the Proceedings of the Design Automation Conference (DAC), 2022.

## Disclaimer 
> Although we tested the code extensively, BlueSeer is a research prototype that likely contain bugs. We take no responsibility for and give no warranties in respect of using the code.

Unless explicitly stated otherwise, all BlueSeer sources are distributed under the terms of the [3-clause BSD license](license). This license gives everyone the right to use and distribute the code, either in binary or source code format, as long as the copyright license is retained in the source code.
