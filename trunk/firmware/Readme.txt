************************************************************************
**
** Project......: Firmware USB AVR Si570 controler.
**
** Platform.....: ATtiny45
**
** Licence......: This software is freely available for non-commercial 
**                use - i.e. for research and experimentation only!
**                Copyright: (c) 2006 by OBJECTIVE DEVELOPMENT Software GmbH
**                Based on ObDev's AVR USB driver by Christian Starkjohann
**
** Programmer...: F.W. Krom, PE0FKO and
**                thanks to Tom Baier DG8SAQ for the initial program.
** 
** Description..: Control the Si570 Freq. PLL chip over the USB port.
**
** History......: V15.1 02/12/2008: First release of PE0FKO.
**                V15.2 19/12/2008: Change the Si570 code.
**                V15.3 02/01/2009: Add Automatich smooth tune.
**                V15.4 06/01/2009: Add Automatic Band Pass Filter Selection.
**                V15.5 14/01/2009: Add the Smooth tune and band pass filter 
**                                  to the "Set freq by Si570 registers" command.
**                V15.6 17/01/2009: Bug fix, no connection on boot from PC.
**                                  Used a FreqSmooth so the returned freq is
**                                  the real freq and not the smooth center freq.
**                V15.7 22/01/2009: Source change. Upgrade ObDev to 20081022.
**                                  FreqSmoothTune variable removed from eeprom.
**                                  Test errors in i2c code changed. 
**                                  Add cmd 0x00, return firmware version number.
**                                  Add cmd 0x20, Write Si570 register
**                                  Add cmd 0x0F, Reset by Watchdog
**                V15.8 10/02/2009: CalcFreqFromRegSi570() will use the fixed
**                                  xtal freq of 114.285 MHz. Change static 
**                                  variables to make some more free rom space.
**
**************************************************************************

Compiler: WinAVR-20071221
V14			3866 bytes (94.4% Full)
V15.1		3856 bytes (94.1% Full)
V15.2		3482 bytes (85.0% Full)
V15.3		3892 bytes (95.0% Full)
V15.4		3918 bytes (95.7% Full)
V15.5		4044 bytes (98.7% Full)
V15.6		4072 bytes (99.4% Full)
V15.7		4090 bytes (99.9% Full)
V15.8		3984 bytes (97.3% Full)


Fuse bit information:
Fuse high byte:
0xdd = 1 1 0 1   1 1 0 1     RSTDISBL disabled (SPI programming can be done)
0x5d = 0 1 0 1   1 1 0 1     RSTDISBL enabled (PB5 can be used as I/O pin)
       ^ ^ ^ ^   ^ \-+-/ 
       | | | |   |   +------ BODLEVEL 2..0 (brownout trigger level -> 2.7V)
       | | | |   +---------- EESAVE (preserve EEPROM on Chip Erase -> not preserved)
       | | | +-------------- WDTON (watchdog timer always on -> disable)
       | | +---------------- SPIEN (enable serial programming -> enabled)
       | +------------------ DWEN (debug wire enable)
       +-------------------- RSTDISBL (disable external reset -> disabled)

Fuse low byte:
0xe1 = 1 1 1 0   0 0 0 1
       ^ ^ \+/   \--+--/
       | |  |       +------- CKSEL 3..0 (clock selection -> HF PLL)
       | |  +--------------- SUT 1..0 (BOD enabled, fast rising power)
       | +------------------ CKOUT (clock output on CKOUT pin -> disabled)
       +-------------------- CKDIV8 (divide clock by 8 -> don't divide) 


Modifications by Fred Krom, PE0FKO at Nov 2008
- Hang on no pull up of SCL line i2c to Si570 (or power down of Si590 in SR-V90)
- Compiler (WinAVR-20071221) optimized the i2c delay loop a way!
- Calculating the Si570 registers from a given frequency, returns a HIGH HS_DIV value
- Source cleanup and split in deferent files.
- Remove many debug USB command calls!
- Version usbdrv-20081022
- Add command 0x31, write only the Si570 registers (change freq max 3500ppm)
- Change the Si570 register calculation and now use the full 38 bits of the chip!
  Is is accurate, fast and small code! It cost only 350us (old 2ms) to calculate the new registers.
- Add command 0x3d, Read the actual used xtal frequency (4 bytes, 24 bits fraction, 8.24 bits)
- Add the "automatic smooth tune" functionality.
- Add the I/O function command 0x15
- Add the commands 0x34, 0x35, 0x3A, 0x3B, 0x3C, 0x3D
- Add the I/O function command 0x16
- Add read / write Filter cross over points 0x17
- Many code optimalization to make the small code.
- Calculation of the freq from the Si570 registers and call 0x32, command 0x30


Implemented functions:
----------------------

V15.8
+----+---+---+---+-----------------------------------------------------
|Cmd |SQA|FKO| IO| Function
+0x--+---+---+---+-----------------------------------------------------
| 00 | * |   | I | Echo value variable
| 00 |   | * | I | Get Firmware version number
| 01 | * | * | I | [DO NOT USE] set port directions
| 02 | * | * | I | [DO NOT USE] read ports
| 03 | * | * | I | [DO NOT USE] read port states 
| 04 | * | * | I | [DO NOT USE] set ports
| 05 | * |   | I | [DO NOT USE] send I2C start sequence
| 06 | * |   | I | [DO NOT USE] send I2C stop sequence
| 07 | * |   | I | [DO NOT USE] send byte to I2C
| 08 | * |   | I | [DO NOT USE] send word to I2C
| 09 | * |   | I | [DO NOT USE] send dword to I2C
| 0A | * |   | I | [DO NOT USE] send word to I2C with start and stop sequence
| 0B | * |   | I | [DO NOT USE] receive word from I2C with start and stop sequence
| 0C | * |   | I | [DO NOT USE] modify I2C clock
| 0D |   |   | I | [DO NOT USE] read OSCCAL to "value"
| 0E |   |   | I | [DO NOT USE] Write "value" to OSCCAL
| 0F | * | * | I | [DO NOT USE] Reset by Watchdog
| 10 | * |   | I | [DO NOT USE] EEPROM write byte value=address, index=data
| 11 | * |   | I | [DO NOT USE] EEPROM read byte "value"=address
| 13 |   |   | I | [DO NOT USE] return usb device address
| 15 |   | * | I | Set IO port with mask and data bytes, and perform cmd 0x16
| 16 |   | * | I | Return the I/O pin value
| 17 |   | * | I | Read the Filter cross over points and set one point
| 20 | * | * | I | Write byte to Si570 register
| 21 | * |   | I | [DO NOT USE] SI570: read byte to register index (Use command 0x3F)
| 22 | * |   | I | [DO NOT USE] SI570: freeze NCO (Use command 0x20)
| 23 | * |   | I | [DO NOT USE] SI570: unfreeze NCO (Use command 0x20)
| 30 | * | * | O | Set frequency by register and load Si570
| 32 | * | * | O | Set frequency by value and load Si570
| 33 | * | * | O | write new crystal frequency to EEPROM and use it.
| 34 |   | * | O | Write new startup frequency to eeprom
| 35 |   | * | O | Write new smooth tune to eeprom and use it.
| 3A |   | * | I | Return running frequency
| 3B |   | * | I | Return smooth tune ppm value
| 3C |   | * | I | Return the startup frequency
| 3D |   | * | I | Return the XTal frequency
| 3E |   |   | I | [DEBUG] read out calculated frequency control registers
| 3F | * | * | I | Read out frequency control registers
| 40 | * | * | I | Return I2C transmission error status
| 41 | * |   | I | [DO NOT USE] set/reset init freq status
| 41 |   | * | I | Set the new i2c address.
| 50 | * | * | I | [DO NOT USE] set USR_P1 and get cw-key status
| 51 | * | * | I | [DO NOT USE] read SDA and cw key level simultaneously


Commands:
---------
All the command are working with the "usb_control_msg" command from the LibUSB open source project.
I'm using "libusb-win32-device-bin-0.1.12.1" from http://sourceforge.net/projects/libusb-win32/

To use the library include the header file ./include/usb.h in your project and add the 
library ./lib/*/libusb.lib for your linker.

Open the device with usb_open(...) with the VID & PID to get a device handle.

  int usb_control_msg(usb_dev_handle *dev, int requesttype, int request,
                      int value, int index, char *bytes, int size,
                      int timeout);

    requesttype:    Data In or OUT command (table IO value)
        I = USB_TYPE_VENDOR | USB_RECIP_DEVICE | USB_ENDPOINT_IN
        O = USB_TYPE_VENDOR | USB_RECIP_DEVICE | USB_ENDPOINT_OUT

    request: The command number.
    value:   Word parameter 0
    index:   Word parameter 1
    bytes:   Array data send to the device (OUT) or from the device (IN)
    size:    length in bytes of the "bytes" array.


In case of a unknow command the firmware will return a 1 (one) if there is a bytes array specified it 
returns the byte 255.


Command 0x00:
-------------
This call will return the version number of the firmware. The high byte is the version major and the
low byte the version minor number.

It is a bid tricky for the previous versions because they used a "word echo command" on command 0x00.
If the call will be done with the "value" parameter is set to 0x0E00 it will return version 14.0 for the
original DG8SAQ software. (Also my previous software will return the version 14.0, the owner had to
upgrade or not use it). There is also a other way to check the type of software, use the USB Version 
string for that.

Code sample:
    uint16_t version;
    r = usb_control_msg(handle, USB_TYPE_VENDOR | USB_RECIP_DEVICE | USB_ENDPOINT_IN
                    0x00, 0x0E00, 0, &version, sizeof(version), 1000);
	// if the return value is 2, the variable version will give the major and minor
	// version number in the high and low byte.

Parameters:
    requesttype:    USB_ENDPOINT_IN
    request:         0x00
    value:           0x0E00
    index:           Don't care
    bytes:           Version word variable
    size:            2


Command 0x01:
-------------
Set port directions.
Do not use, use I/O function 0x15


Command 0x02:
-------------
Read ports.
Do not use, use I/O function 0x15


Command 0x03:
-------------
Read port states.
Do not use.


Command 0x04:
-------------
Set ports.
Do not use, use I/O function 0x15


Command 0x0F:
-------------
Restart the board (done by Watchdog timer).

Parameters:
    requesttype:    USB_ENDPOINT_IN
    request:         0x0F
    value:           Don't care
    index:           Don't care
    bytes:           NULL
    size:            0


Command 0x15:
-------------
Set the I/O bits of the device. The SoftRock V9 only had two I/O lines, bit0 and bit1.
It also returned the I/O value (like command 0x16).

There are two values for every I/O bit, the data direction and data bits.
+-----+------+----------------------
| DDR | DATA | PIN Function
+-----+------+----------------------
|  0  |   0  | input
|  0  |   1  | input internal pullup
|  1  |   0  | output 0
|  1  |   1  | output 1
+-----+------+----------------------

It only works if automatic filter is disabled!

Code sample:
    uint16_t INP;
    r = usb_control_msg(handle, USB_TYPE_VENDOR | USB_RECIP_DEVICE | USB_ENDPOINT_IN
                    0x15, 0x02, 0x02, &INP, sizeof(INP), 1000);
    // Set P2 to output and one!
    // Use P1 as input, no internal pull up R enabled.
    // Read the input in array INP[], only bit0 and bit1 used by this hardware.

Parameters:
    requesttype:    USB_ENDPOINT_IN
    request:         0x15
    value:           Data Direction Register
    index:           Data register
    bytes:           PIN status (returned)
    size:            2


Command 0x16:
-------------
Get the I/O values in the returned word, the SoftRock V9 only had two I/O lines, bit0 and bit1.

Code sample:
    uint16_t INP;
    r = usb_control_msg(handle, USB_TYPE_VENDOR | USB_RECIP_DEVICE | USB_ENDPOINT_IN
                    0x16, 0, 0, &INP, sizeof(INP), 1000);
    // Read the input word INP, only bit0 and bit1 used by SoftRock V9 hardware.

Parameters:
    requesttype:    USB_ENDPOINT_IN
    request:         0x16
    value:           Don't care
    index:           Don't care
    bytes:           PIN status (returned)
    size:            2


Command 0x17:
-------------
Read the Filter cross over points and set one point.
The call will read the three filter cross over points and set one of the points if specified.
The data format of the points is a 11.5 bits in MHz, that give a resolution of 1/32 MHz.
The fourth data entry is a boolean specifying if the had to be used!

Code sample:
    uint16_t FilterCrossOver[4];
    FilterCrossOver[0] = 4.1 * 4.0 * (1<<5);
    FilterCrossOver[1] = 8.0 * 4.0 * (1<<5);
    FilterCrossOver[2] = 16. * 4.0 * (1<<5);
    FilterCrossOver[4] = true;        // Enable

    usb_control_msg(handle, USB_TYPE_VENDOR | USB_RECIP_DEVICE | USB_ENDPOINT_IN
                    0x17, FilterCrossOver[0], 0, NULL, 0, 1000);

    usb_control_msg(handle, USB_TYPE_VENDOR | USB_RECIP_DEVICE | USB_ENDPOINT_IN
                    0x17, FilterCrossOver[1], 1, NULL, 0, 1000);

    usb_control_msg(handle, USB_TYPE_VENDOR | USB_RECIP_DEVICE | USB_ENDPOINT_IN
                    0x17, FilterCrossOver[2], 2, NULL, 0, 1000);

    usb_control_msg(handle, USB_TYPE_VENDOR | USB_RECIP_DEVICE | USB_ENDPOINT_IN
                    0x17, FilterCrossOver[3], 3, FilterCrossOver, sizeof(FilterCrossOver), 1000);

    // Specify filter cross over point for a softrock that divide the LO by 4!
    // And read the points back from the device.

Parameters: Setting one of the points
    requesttype:    USB_ENDPOINT_IN
    request:         0x17
    value:           FilterCrossOver[0]
    index:           0..2 index of the 'value' filter point.
    bytes:           Array of four 16bits integers for the filter points.
    size:            8

Parameters: Enable / disable the filter
    requesttype:    USB_ENDPOINT_IN
    request:         0x17
    value:           0 (disable) or 1 (enable)
    index:           3
    bytes:           Array of four 16bits integers for the filter points.
    size:            8


Command 0x20:
-------------
Write one byte to a Si570 register. Return value is the i2c error boolean in the buffer array.

Code sample:
	// Si570 RECALL function
	uint8_t i2cError;
    r = usb_control_msg(handle, USB_TYPE_VENDOR | USB_RECIP_DEVICE | USB_ENDPOINT_IN
                    0x20, 0x55 | (135<<8), 0x01, &i2cError, 1, 1000);
	if (r == 1 && i2cError == 0)
		// OK

Parameters:
    requesttype:    USB_ENDPOINT_IN
    request:         0x20
    value:           I2C Address low byte (only for the DG8SAQ firmware)
                     Si570 register high byte
    index:           Register value low byte
    bytes:           NULL
    size:            0


Command 0x30 & 0x31:
--------------------
Set the oscillator frequency by Si570 register. The real frequency will be 
calculated by the firmware and the called command 0x32

Default:    None

Parameters:
    requesttype:    USB_ENDPOINT_OUT
    request:         0x30
    value:           I2C Address (only for the DG8SAQ firmware), Don't care
    index:           7 (only for the DG8SAQ firmware), Don't care
    bytes:           pointer 48 bits register
    size:            6


Command 0x32:
-------------
Set the oscillator frequency by value. The frequency is formatted in MHz
as 11.21 bits value.

Default:    None

Parameters:
    requesttype:    USB_ENDPOINT_OUT
    request:         0x32
    value:           Don't care
    index:           Don't care
    bytes:           pointer 32 bits integer
    size:            4

Code sample:
    uint32_t iFreq;
    double   dFreq;

    dFreq = 30.123456; // MHz
    iFreq = (uint32_t)( dFreq * (1UL<<21) )
    r = usb_control_msg(handle, USB_TYPE_VENDOR | USB_RECIP_DEVICE | USB_ENDPOINT_OUT
                    0x32, 0, 0, (char *)&iFreq, sizeof(iFreq), 1000);
    if (r < 0) Error


Command 0x33:
-------------
Write new crystal frequency to EEPROM and use it. It can be changed to calibrate the device.
The frequency is formatted in MHz as a 8.24 bits value.

Default:    114.285 MHz

Parameters:
    requesttype:    USB_ENDPOINT_OUT
    request:         0x33
    value:           Don't care
    index:           Don't care
    bytes:           pointer 32 bits integer
    size:            4

Code sample:
    uint32_t iXtalFreq;
    double   dXtalFreq;

    dXtalFreq = 114.281;
    iXtalFreq = (uint32_t)( dXtalFreq * (1UL<<24) )
    r = usb_control_msg(handle, USB_TYPE_VENDOR | USB_RECIP_DEVICE | USB_ENDPOINT_OUT
                    0x33, 0, 0, (char *)&iXtalFreq, sizeof(iXtalFreq), 1000);
    if (r < 0) Error


Command 0x34:
-------------
Write new startup frequency to eeprom. When the device is started it will output
this frequency until a program set an other frequency.
The frequency is formatted in MHz as a 11.21 bits value.

Default:    4 * 7.050 MHz

Parameters:
    requesttype:    USB_ENDPOINT_OUT
    request:         0x34
    value:           Don't care
    index:           Don't care
    bytes:           pointer 32 bits integer
    size:            4

Code sample:
    uint32_t iXtalFreq;
    double   dXtalFreq;

    dFreq = 4.0 * 3.550; // MHz
    iFreq = (uint32_t)( dFreq * (1UL<<24) )
    r = usb_control_msg(handle, USB_TYPE_VENDOR | USB_RECIP_DEVICE | USB_ENDPOINT_OUT
                    0x34, 0, 0, (char *)&iFreq, sizeof(iFreq), 1000);
    if (r < 0) Error


Command 0x35:
-------------
Write new smooth tune to eeprom and use it.

Default:    3500 PPM

Parameters:
    requesttype:    USB_ENDPOINT_OUT
    request:         0x35
    value:           Don't care
    index:           Don't care
    bytes:           pointer 16 bits integer
    size:            2

Code sample:
    uint16_t Smooth;
    Smooth = 3400;
    r = usb_control_msg(handle, USB_TYPE_VENDOR | USB_RECIP_DEVICE | USB_ENDPOINT_OUT
                    0x35, 0, 0, (char *)&Smooth, sizeof(Smooth), 1000);
    if (r < 0) Error


Command 0x3A:
-------------
Return actual frequency of the device.
The frequency is formatted in MHz as a 11.21 bits value.

Parameters:
    requesttype:    USB_ENDPOINT_IN
    request:         0x3A
    value:           Don't care
    index:           Don't care
    bytes:           pointer 32 bits integer
    size:            4

Code sample:
    uint32_t iFreq;
    double   dFreq;
    r = usb_control_msg(handle, USB_TYPE_VENDOR | USB_RECIP_DEVICE | USB_ENDPOINT_IN
                    0x3A, 0, 0, (char *)&iFreq, sizeof(iFreq), 1000);
    if (r == 4)
        dFreq = (double)iFreq / (1UL<<21);


Command 0x3B:
-------------
Return the "Smooth tune" PPM (pulse per MHz) of the device.
The value is default 3500 (from data sheet) and can be changed. I do not know what 
happened with the chip if it is out of range (>3500).
If the value is set to zero it will disable the "Automatic Smooth tune" function.

Default:    3500 PPM

Parameters:
    requesttype:    USB_ENDPOINT_IN
    request:         0x3B
    value:           Don't care
    index:           Don't care
    bytes:           pointer 16 bits integer
    size:            2

Code sample:
    uint16_t Smooth;
    r = usb_control_msg(handle, USB_TYPE_VENDOR | USB_RECIP_DEVICE | USB_ENDPOINT_IN
                    0x3B, 0, 0, (char *)&Smooth, sizeof(Smooth), 1000);
    if (r == 2) ...


Command 0x3C:
-------------
Return device startup frequency.
The frequency is formatted in MHz as a 11.21 bits value.

Default:    4 * 7.050 MHz

Parameters:
    requesttype:    USB_ENDPOINT_IN
    request:         0x3C
    value:           Don't care
    index:           Don't care
    bytes:           pointer 32 bits integer
    size:            4

Code sample:
    uint32_t iFreq;
    double   dFreq;
    r = usb_control_msg(handle, USB_TYPE_VENDOR | USB_RECIP_DEVICE | USB_ENDPOINT_IN
                    0x3C, 0, 0, (char *)&iFreq, sizeof(iFreq), 1000);
    if (r == 4)
        dFreq = (double)iFreq / (1UL<<21);


Command 0x3D:
-------------
Return device crystal frequency.
The frequency is formatted in MHz as a 8.24 bits value.

Default:    114.285 MHz

Parameters:
    requesttype:    USB_ENDPOINT_IN
    request:         0x3D
    value:           Don't care
    index:           Don't care
    bytes:           pointer 32 bits integer
    size:            4

Code sample:
    uint32_t iFreqXtal;
    double   dFreqXtal;
    r = usb_control_msg(handle, USB_TYPE_VENDOR | USB_RECIP_DEVICE | USB_ENDPOINT_IN
                    0x3D, 0, 0, (char *)&iFreqXtal, sizeof(iFreqXtal), 1000);
    if (r == 4)
        dFreqXtal = (double)iFreqXtal / (1UL<<24);


Command 0x3F:
-------------
Return the Si570 frequency control registers (reg 7 .. 12). If there are I2C errors
the return length is 0.

Default:    None

Parameters:
    requesttype:     USB_ENDPOINT_IN
    request:         0x3F
    value:           Don't care
    index:           Don't care
    bytes:           pointer 6 byte register array
    size:            6


Command 0x41:
-------------
Set a new I2C address for the Si570 chip.
The function can also be used to reset the device to "factory default" by writing the
value 255. After a restart the device will initialize to all the default values.

Default:    0x55 (85 decimal)

Parameters:
    requesttype:    USB_ENDPOINT_IN
    request:         0x41
    value:           I2C address or 255
    index:           0
    bytes:           NULL
    size:            0


Command 0x50:
-------------
Set IO_P1 and read CW key level.

Parameters:
    requesttype:    USB_ENDPOINT_IN
    request:         0x50
    value:           Output bool to user output P1
    index:           0
    bytes:           pointer to 1 byte variable P2
    size:            1


Command 0x51:
-------------
Read CW key level.

Parameters:
    requesttype:    USB_ENDPOINT_IN
    request:         0x51
    value:           0
    index:           0
    bytes:           pointer to 1 byte variable P2
    size:            1
