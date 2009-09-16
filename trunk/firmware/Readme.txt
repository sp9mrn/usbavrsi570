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
**
**************************************************************************

* Compiler: WinAVR-20071221
* V1.4		3866 bytes (94.4% Full)
* V1.5.1	3856 bytes (94.1% Full)
* V1.5.2	3482 bytes (85.0% Full)
* V1.5.3	3892 bytes (95.0% Full)
* V1.5.4	3918 bytes (95.7% Full)
* V1.5.5	4044 bytes (98.7% Full)

* Modifications by Fred Krom, PE0FKO at Nov 2008
* - Hang on no pull up of SCL line i2c to Si570 (or power down of Si590 in SR-V90)
* - Compiler (WinAVR-20071221) optimized the i2c delay loop a way!
* - Calculating the Si570 registers from a given frequency, returns a HIGH HS_DIV value
* - Source cleanup and split in deferent files.
* - Remove many debug USB command calls!
* - Version usbdrv-20081022
* - Add command 0x31, write only the Si570 registers (change freq max 3500ppm)
* - Change the Si570 register calculation and now use the full 38 bits of the chip!
*   Is is accurate, fast and small code! It cost only 350us (old 2ms) to calculate the new registers.
* - Add command 0x3d, Read the actual used xtal frequency (4 bytes, 24 bits fraction, 8.24 bits)
* - Add the "automatic smooth tune" functionality.
* - Add the I/O function command 0x15
* - Add the commands 0x34, 0x35, 0x3A, 0x3B, 0x3C, 0x3D
* - Add the I/O function command 0x16
* - Add read / write Filter cross over points 0x17
* - Many code optimalization to make the small code.
* - Calculation of the freq from the Si570 registers and call 0x32, command 0x30



Implemented functions:
----------------------

V15.5
+---+---+---+---+-----------------------------------------------------
|Cmd|SQA|FKO| IO| Function
+---+---+---+---+-----------------------------------------------------
| 1 | * | * | I | [DO NOT USE] set port directions
| 2 | * | * | I | [DO NOT USE] read ports
| 3 | * | * | I | [DO NOT USE] read port states 
| 4 | * | * | I | [DO NOT USE] set ports
| 15|   | * | I | Set IO port with mask and data bytes, and perform cmd 0x16
| 16|   | * | I | Return the I/O pin value
| 17|   | * | I | Read the Filter cross over points and set one point
| 30| * | * | O | Set frequency by register and load Si570
| 32| * | * | O | Set frequency by value and load Si570
| 33| * | * | O | write new crystal frequency to EEPROM and use it.
| 34|   | * | O | Write new startup frequency to eeprom
| 35|   | * | O | Write new smooth tune to eeprom and use it.
| 3A|   | * | I | Return running frequency
| 3B|   | * | I | Return smooth tune ppm value
| 3C|   | * | I | Return the startup frequency
| 3D|   | * | I | Return the XTal frequency
| 3E| * | * | I | read out calculated frequency control registers
| 3F| * | * | I | [DEBUG] SI570: read out frequency control registers
| 41| * | * | I | Set the new i2c address! (pe0fko: function changed)
| 50| * | * | I | [DO NOT USE] set USR_P1 and get cw-key status
| 51| * | * | I | [DO NOT USE] read SDA and cw key level simultaneously


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
    uint8_t INP[2];
    r = usb_control_msg(handle, USB_TYPE_VENDOR | USB_RECIP_DEVICE | USB_ENDPOINT_IN
                    0x15, 0x02, 0x02, INP, sizeof(INP), 1000);
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
    uint8_t INP[2];
    r = usb_control_msg(handle, USB_TYPE_VENDOR | USB_RECIP_DEVICE | USB_ENDPOINT_IN
                    0x16, 0, 0, INP, sizeof(INP), 1000);
    // Read the input in array INP[], only bit0 and bit1 used by this hardware.

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


Command 0x3E:
-------------
Return the calculated Si570 frequency control registers.
Debug only.

Default:    None

Parameters:
    requesttype:     USB_ENDPOINT_IN
    request:         0x3E
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

