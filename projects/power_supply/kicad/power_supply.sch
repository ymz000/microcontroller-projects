EESchema Schematic File Version 2
LIBS:power
LIBS:device
LIBS:transistors
LIBS:conn
LIBS:linear
LIBS:regul
LIBS:74xx
LIBS:cmos4000
LIBS:adc-dac
LIBS:memory
LIBS:xilinx
LIBS:special
LIBS:microcontrollers
LIBS:dsp
LIBS:microchip
LIBS:analog_switches
LIBS:motorola
LIBS:texas
LIBS:intel
LIBS:audio
LIBS:interface
LIBS:digital-audio
LIBS:philips
LIBS:display
LIBS:cypress
LIBS:siliconi
LIBS:opto
LIBS:atmel
LIBS:contrib
LIBS:valves
LIBS:custom
EELAYER 27 0
EELAYER END
$Descr User 8000 6000
encoding utf-8
Sheet 1 7
Title "Adjustable Modular Power Supply"
Date "24 jan 2015"
Rev "1"
Comp "Digital Cave"
Comment1 ""
Comment2 ""
Comment3 ""
Comment4 ""
$EndDescr
$Sheet
S 2100 1550 900  400 
U 54C2A0D4
F0 "Transformer" 50
F1 "transformer.sch" 50
F2 "MAINS1" I L 2100 1650 60 
F3 "MAINS2" I L 2100 1850 60 
F4 "EARTH" I L 2100 1750 60 
F5 "AC1" O R 3000 1650 60 
F6 "AC2" O R 3000 1750 60 
$EndSheet
$Sheet
S 3200 2350 750  2050
U 54C2B08D
F0 "Controller" 50
F1 "controller.sch" 50
F2 "SDA" B R 3950 2500 60 
F3 "SCL" B R 3950 2600 60 
$EndSheet
$Sheet
S 4250 1550 1050 1300
U 54C2B0D8
F0 "Regulator" 50
F1 "regulator.sch" 50
F2 "V+" I L 4250 1650 60 
F3 "V-" I L 4250 1750 60 
F4 "REGULATED_V" O R 5300 1650 60 
F5 "SDA" B L 4250 2500 60 
F6 "SCL" B L 4250 2600 60 
F7 "TEMP_OUT" O R 5300 2750 60 
$EndSheet
$Sheet
S 3300 1550 650  400 
U 54C2A519
F0 "Rectifier" 50
F1 "rectifier.sch" 50
F2 "AC1" I L 3300 1650 60 
F3 "AC2" I L 3300 1750 60 
F4 "V+" O R 3950 1650 60 
F5 "V-" O R 3950 1750 60 
$EndSheet
Wire Wire Line
	4250 1650 3950 1650
Wire Wire Line
	3300 1650 3000 1650
Wire Wire Line
	3950 1750 4250 1750
Wire Wire Line
	3300 1750 3000 1750
Wire Wire Line
	3950 2500 4250 2500
Wire Wire Line
	4250 2600 3950 2600
$Sheet
S 4250 3250 700  1150
U 54C2F9D6
F0 "User Interface" 50
F1 "interface.sch" 50
F2 "DS_E" I L 4250 3450 60 
F3 "DS_RS" I L 4250 3550 60 
F4 "DS_D4" I L 4250 3650 60 
F5 "DS_D5" I L 4250 3750 60 
F6 "DS_D6" I L 4250 3850 60 
F7 "DS_D7" I L 4250 3950 60 
F8 "DS_VE" I L 4250 3350 60 
F9 "ENC_1" I L 4250 4100 60 
F10 "ENC_2" I L 4250 4200 60 
F11 "ENC_3" I L 4250 4300 60 
$EndSheet
$Sheet
S 5600 2650 800  200 
U 54C32205
F0 "Fan Controller" 50
F1 "fan.sch" 50
F2 "TEMP_IN" I L 5600 2750 60 
$EndSheet
Wire Wire Line
	5300 2750 5600 2750
$EndSCHEMATC
