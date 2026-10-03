# CHANGES
This file contains the log of changes of **EEPROM 28C Programmer** eXPerience.


## "mercury" 1.3.0 2026-10-03 Feature release
- Added page write mode ("-b" option) and end of write cycle detection by DATA polling, with per-block verify and retry
- Write cycle detection also requires two identical consecutive reads (Toggle Bit): no more false errors when a page load is interrupted
- "-w" verifies the whole written range again at the end; page mode raises the process priority (piHiPri)
- Ascii datafiles: invalid data or odd digit count is now an error (was a silent end of file); "XXXXX:" address labels of the stdout dump are skipped
- The optional LENGTH of "-w"/"-v" must start with a digit: "-wa", "-vf FILE" are rejected (were read as hex lengths 0xA, 0xF)
- Unexpected arguments ("-v FILE" without "-f") and multiple operations are now errors; getopt messages are no longer printed twice
- SDP command waits tBLC+tWC
- README: help as text (the screenshot was outdated), hardware notes


## "mercury" 1.2.1 2026-10-03 Bugfix release
- Fixed "-l 0" enabling SDP instead of disabling it (MODE was parsed as a character)
- Fixed "-w" without LENGTH writing nothing: now the DATAFILE length is used, as documented
- "-w"/"-v" LENGTH is now accepted also as separate argument ("-w 100" as well as "-w100")
- Fixed off-by-one at end of binary file: write duplicated the last byte, verify always failed
- Added fopen error check (was a segfault); write/verify read from stdin when no "-f" is given
- Dump to file now honors "-a" (binary by default, as write/verify); status messages go to stderr
- Verify now reports the expected value too; exit status is 1 on errors and test/verify mismatch
- Added validation of START, LENGTH, BLANK and MODE params and ROM size bound (32K)
- GPIOs are initialized only after options parsing ("-h" no longer touches the GPIOs)
- SDP command now waits for the write cycle to complete
- makefile: create obj/ and dist/ directories, link libraries after objects
- Added obj/ to .gitignore, minor header fixes


## "mercury" 1.2.0 2020-07-13 Feature release
- Added software "-a" option to set datafile as ascii vs binary


## "mercury" 1.1.0 2020-07-10 Feature release
- Added software "-p" option to print the bus pin number of the RasbperryPI GPIOs (wiringPI numbering)


## "mercury" 1.0.0 2020-07-08 Feature release
- Refactored software path-tree structure and code modules


## 0.1.0 2020-07-05 Draft release
- Created repository *xp-eeprom-28C-programmer* and
- Added README.md, CHANGES.md and TODO.md files
- Added software code for RaspberryPI-3
- Added schematic
- Tested on Atmel 28C16, 28C64 and 28C256
