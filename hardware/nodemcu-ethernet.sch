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
LIBS:siliconi
LIBS:opto
LIBS:atmel
LIBS:contrib
LIBS:valves
LIBS:esp12e
LIBS:nodemcu-ethernet-cache
EELAYER 27 0
EELAYER END
$Descr User 5315 4724
encoding utf-8
Sheet 1 1
Title "NodeMCU 10-Base-T Wiring Diagram"
Date "2 apr 2016"
Rev "-"
Comp "(C) 2016 <>< Charles Lohr"
Comment1 "CC 3.0 BY"
Comment2 ""
Comment3 ""
Comment4 ""
$EndDescr
Text GLabel 1750 750  0    60   Input ~ 0
NODEMCU_D1(GPIO5)
Text Notes 900  2350 0    60   ~ 0
WARNING: Using ethernet without magnetic isolation is dangerous.\nDo so with caution only once you are absolutely positive the other\nside of the ethernet is isolated.\n\nWARNING: The ESP will be competing against the serial driver.  This\nwill cause degradation in signal and may damage the serial driver.
Text Notes 900  3000 0    60   ~ 0
NOTE:  Using ethernet without a driver is fraught with peril.\n\nNOTE:  The resistors and capacitors selected here were ones\nI found happened to kind of work (<60% packet loss) you may\nfind better ones.  If you do, please tell me.
Text GLabel 1750 950  0    60   Input ~ 0
NODEMCU_D2(GPIO4)
Text GLabel 1750 2100 0    60   Input ~ 0
NODEMCU_GROUND
Text GLabel 1750 1150 0    60   Input ~ 0
NODEMCU_D4(GPIO2)
Text GLabel 1750 1350 0    60   Input ~ 0
NODEMCU_D6(GPIO12)
$Comp
L R R1
U 1 1 56FB3CA8
P 2250 1750
F 0 "R1" H 2100 1700 79  0000 C CNN
F 1 "1k" H 2100 1850 79  0000 C CNN
F 2 "~" V 2180 1750 30  0000 C CNN
F 3 "~" H 2250 1750 30  0000 C CNN
	1    2250 1750
	1    0    0    -1  
$EndComp
$Comp
L R R2
U 1 1 56FB3CB5
P 2400 1750
F 0 "R2" H 2550 1700 79  0000 C CNN
F 1 "1k" H 2550 1850 79  0000 C CNN
F 2 "~" V 2330 1750 30  0000 C CNN
F 3 "~" H 2400 1750 30  0000 C CNN
	1    2400 1750
	1    0    0    -1  
$EndComp
$Comp
L C C1
U 1 1 56FB3CCF
P 2850 1150
F 0 "C1" V 3000 1100 79  0000 L CNN
F 1 "1nF" V 2700 1050 79  0000 L CNN
F 2 "~" H 2888 1000 30  0000 C CNN
F 3 "~" H 2850 1150 60  0000 C CNN
	1    2850 1150
	0    -1   -1   0   
$EndComp
$Comp
L C C2
U 1 1 56FB3CE8
P 3300 1350
F 0 "C2" V 3200 1450 79  0000 L CNN
F 1 "27p" V 3400 1000 79  0000 L CNN
F 2 "~" H 3338 1200 30  0000 C CNN
F 3 "~" H 3300 1350 60  0000 C CNN
	1    3300 1350
	0    -1   -1   0   
$EndComp
$Comp
L R R3
U 1 1 56FB3CEE
P 2900 1750
F 0 "R3" H 3050 1700 79  0000 C CNN
F 1 "10k" H 3050 1850 79  0000 C CNN
F 2 "~" V 2830 1750 30  0000 C CNN
F 3 "~" H 2900 1750 30  0000 C CNN
	1    2900 1750
	1    0    0    -1  
$EndComp
Wire Wire Line
	1750 1350 3100 1350
Wire Wire Line
	2650 1150 1750 1150
$Comp
L LED D1
U 1 1 56FB3D69
P 2000 750
F 0 "D1" H 1900 900 79  0000 C CNN
F 1 "LED" H 2150 900 79  0000 C CNN
F 2 "~" H 2000 750 60  0000 C CNN
F 3 "~" H 2000 750 60  0000 C CNN
	1    2000 750 
	1    0    0    -1  
$EndComp
$Comp
L LED D2
U 1 1 56FB3D76
P 2000 950
F 0 "D2" H 1900 800 79  0000 C CNN
F 1 "LED" H 2150 800 79  0000 C CNN
F 2 "~" H 2000 950 60  0000 C CNN
F 3 "~" H 2000 950 60  0000 C CNN
	1    2000 950 
	1    0    0    -1  
$EndComp
Wire Wire Line
	1750 2100 3800 2100
Wire Wire Line
	2400 2000 2400 2100
Connection ~ 2400 2100
Wire Wire Line
	2250 2000 2250 2100
Connection ~ 2250 2100
Wire Wire Line
	3050 1150 3800 1150
Wire Wire Line
	3500 1350 3800 1350
Text GLabel 3800 1350 2    60   Input ~ 0
ET_PIN1_RX+
Text Notes 3450 1500 0    60   ~ 0
<<<DATA FLOW<<<
Text Notes 3450 1100 0    60   ~ 0
>>>DATA FLOW>>>
Text GLabel 3800 1150 2    60   Input ~ 0
ET_PIN3_TX+
Text GLabel 3800 2100 2    60   Input ~ 0
ET_PIN2_RX-
Text GLabel 3800 2200 2    60   Input ~ 0
ET_PIN2_TX-
Wire Wire Line
	1800 950  1750 950 
Wire Wire Line
	1750 750  1800 750 
Wire Wire Line
	2900 2000 2900 2100
Connection ~ 2900 2100
Wire Wire Line
	2900 1500 2900 1350
Connection ~ 2900 1350
Wire Wire Line
	2200 950  2250 950 
Wire Wire Line
	2250 950  2250 1500
Wire Wire Line
	2400 1500 2400 750 
Wire Wire Line
	2400 750  2200 750 
Wire Wire Line
	3650 2100 3650 2200
Wire Wire Line
	3650 2200 3800 2200
Connection ~ 3650 2100
$Comp
L R R4
U 1 1 56FF541F
P 3550 1750
F 0 "R4" H 3700 1700 79  0000 C CNN
F 1 "680" H 3700 1850 79  0000 C CNN
F 2 "~" V 3480 1750 30  0000 C CNN
F 3 "~" H 3550 1750 30  0000 C CNN
	1    3550 1750
	1    0    0    -1  
$EndComp
Wire Wire Line
	3550 1500 3550 1350
Connection ~ 3550 1350
Wire Wire Line
	3550 2000 3550 2100
Connection ~ 3550 2100
$EndSCHEMATC
