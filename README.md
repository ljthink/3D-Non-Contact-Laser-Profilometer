3D non-contact Laser Profilometer
==================

Code for a stm32f4 microcontroller (dev board) to control an XY stage and interact with Keyence LT-9010 laser profilometer to create 3D scans of surfaces. The device and its associated code was created as part of a bachelor's thesis at the University of Warwick.

Uses the C++ framework xpcc (see https://github.com/roboterclubaachen/xpcc for code and license). A copy is included because a modified UartHandler is used.

Uses cascaded PID controller to control DC brushed motors equipped with optical rotary encoders.

A GUI using QT 5.2 can display the current position and measurement. A command line program can be used to manually control the stage and define vertices for a rectangular area.


Surface scan of a bolt's (damaged) thread:
![Surface scan of a M1.6 bolt's damaged thread](https://raw.githubusercontent.com/jrahlf/Laser-Profilometer/master/images/bolt.png)

Orange peel:
![Surface of an orange peel](https://raw.githubusercontent.com/jrahlf/3D-Non-Contact-Laser-Profilometer/master/images/orangePeel.png)

Coin:
![Surface scan of an english 1 pence coin visualized in MATLAB](https://raw.githubusercontent.com/jrahlf/Laser-Profilometer/master/images/coinScan.png)

![Photograph of the built system](https://raw.githubusercontent.com/jrahlf/3D-Non-Contact-Laser-Profilometer/master/images/1.jpg)

![Screenshot of the PC side GUI](https://raw.githubusercontent.com/jrahlf/Laser-Profilometer/master/images/pcGui.png)

