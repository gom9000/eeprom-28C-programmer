 # EEPROM 28C Programmer
**Type**: Programming Tool | **Status**: Completed

A simple parallel EEPROM programmer based on Raspberry PI (model 3).

It currently supports 28C16 (2Kx8), 28C64 (8Kx8) and 28C256 (32Kx8) EEPROMs, but it will probably work with all 28C-family (having all the necessary address-bus pins).


![eeprom-28C-programmer_built](resources/eeprom-28C-programmer_built.jpg)



## Specifications
The hardware uses a standard Raspberry Pi 3 for its abundant GPIOs, avoiding the complexity of serial-to-parallel shift registers on the address bus.

* **Voltage Translation:** The voltage gap between the Raspberry Pi GPIOs (3.3V) and the EEPROM data bus (5V) is safely managed by a TXS0108 8-bit bidirectional voltage-level translator. The address and control lines are driven directly by the 3.3V GPIOs (they are EEPROM inputs only, and 3.3V is above their 2.0V V<sub>IH</sub> threshold).

* **Timing Management:**  the end of each write cycle is detected with DATA polling (bit D7 reads inverted until the cycle completes) instead of a fixed delay. Writes can be done byte by byte (all 28C devices) or by pages with -b 40 (64-byte pages of 28C64/28C256). Every block is verified and failed bytes are retried.



### Prerequisites / Requirements
To build and run this project:

* **Hardware:** Raspberry Pi Model 3 (or any model with enough available GPIOs), TXS0108 translator.

* **Software:** WiringPI library installed, standard GCC toolchain.


### Hardware
Schematics and PCB layouts are designed with ExpressPCB free CAD software.


#### Schematic:
![board-schematic](resources/eeprom-28C-programmer_sch.jpg)


### Software
The software is written in C and uses WiringPI for GPIO access, and the GNU Getopt function to automate the parsing of command-line options.

Below is the list of options with the relative allowed params, as shown in the help:

![eeprom-28C-programmer_usage](resources/eeprom-28C-programmer_usage.jpg)

Notes:
* LENGTH, START and BLANK are hex values; the software checks them against the max ROM size (32K).
* Data files are binary by default, ascii hex with `-a` (dump, write and verify use the same format).
* The exit status is 1 on errors and on test/verify mismatch, so the tool can be used in scripts.
* A 28C16 is a 24-pin device, so it needs an adapter or rewiring. The 28C16 has no Software Data Protection: do not use `-l` with it.


## Changes
See file [CHANGES](CHANGES.md) for the project resources change logs.


## Future Plans
See file [TODO](TODO.md) for the project future plans.


## About & License
**Author**: Alessandro Fraschetti (gom9000).  
**Technical Notes**: The hardware design was supported by **ExpressPCB** and the custom **[expresspcb-goslib](https://github.com/gom9000/expresspcb-goslib)** libraries.  
**License**: This project is licensed under the [MIT License](LICENSE). 