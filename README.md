# 555Keychain Firmware

> Board in keychain form factor demonstrating the concept of PWM by generating and blinking an LED with a very slow (0.5-2Hz) PWM signal

It would have been ideal to use an authentic 555 timer circuit on the keychain board, but it was deemed unreliable due to the low voltage of operation. 

Instead, this iteration uses an ultra low-cost but capable microcontroller, the `CH32V003` by WCH. It is a basic RISC-V core clocked at 48MHz,
with a generous selection of peripherals. Coming in at an extremely approachable cost of $0.10 per package and being available in
the same SOC-8 package as the 555 makes it a tantalising substitution for a compact PWM signal visualisation solution.

## Project

This is a standard PlatformIO project - build and run it as you would any other.

> [!IMPORTANT]
> You will first have to install the CH32V PlatformIO platform [here](https://github.com/Community-PIO-CH32V/platform-ch32v) if you
> haven't already.

> [!TIP]
> The CH32V003 series only supports single wire debugging - not the 2 wire variant offered by most other chips in the CH32V series.
>
> As such, make sure to use the WCH-Link**E**, the older WCH-Link doesn't support the single wire debug protocol.

## Hardware

The KiCad project for the latest hardware revision is available [in its own repository](https://github.com/cryptoAlgorithm/555Sandbox/tree/4aebc4c284124fea7acbbd1c1e920401fa80e4b7).
