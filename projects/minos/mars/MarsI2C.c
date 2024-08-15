/**************************************************************
 *  Project          : Mars EDK reference design
 *  File description : I2C suppot for Mars FPGA modules
 *  File name        : MarsI2C.c
 *  Author           : Christoph Glattfelder
 **************************************************************
 *  Copyright (c) 2011 by Enclustra GmbH, Switzerland
 *  All rights reserved.
 **************************************************************
 *  Notes:
 *
 **************************************************************
 *  File history:
 *
 *  Version | Date     | Author             | Remarks
 *  -----------------------------------------------------------
 *  1.0	    | 18.03.11 | Ch. Glattfelder    | Created
 *          |          |                    |
 *  ------------------------------------------------------------
 *
 **************************************************************/

#include "MarsI2C.h"
#include "mb_interface.h"
#include "xiic.h"
#include "xparameters.h"

XIic IicInstance;
volatile u8 TransmitComplete;
volatile u8 ReceiveComplete;

static int WriteAddr(u8 Addr);
static int ReadReg16(u8 Addr, u16* pValue);
static int WriteReg16(u8 Addr, u16 Value);
static int WriteReg8(u8 Addr, u8 Value);
static int ReadReg8(u8 Addr, u8* pValue);
static int SetI2cDevAddr(int DevAddr);

int CurrSensAddr;
int EepromAddr;

/*****************************************************************************
 * Set the I2C device address in the I2C controller
 ******************************************************************************/

static int SetI2cDevAddr(int DevAddr) {

    int Status;

    // Set the Address of the Slave.
    Status = XIic_SetAddress(&IicInstance, XII_ADDR_TO_SEND_TYPE,
                             DevAddr);
    if (Status != XST_SUCCESS) {
        return XST_FAILURE;
    }
    return XST_SUCCESS;
}

/*****************************************************************************
 * Generic register access functions
 ******************************************************************************/

static int WriteAddr(u8 Addr) {
    int Status;
    u8 WriteBuffer[3];

    // Set the defaults.
    TransmitComplete = 1;

    // Start the IIC device.
    Status = XIic_Start(&IicInstance);
    if (Status != XST_SUCCESS) {
        return XST_FAILURE;
    }

    // Set the Repeated Start option.
    IicInstance.Options = 0;

    // Send the data.
    WriteBuffer[0] = Addr;
    Status = XIic_MasterSend(&IicInstance, WriteBuffer, 1);
    if (Status != XST_SUCCESS) {
        return XST_FAILURE;
    }

    /*
     * Wait till data is transmitted.
     */
    while ((TransmitComplete) || (XIic_IsIicBusy(&IicInstance) == TRUE)) {
    }

    // Stop the IIC device.
    Status = XIic_Stop(&IicInstance);
    if (Status != XST_SUCCESS) {
        return XST_FAILURE;
    }

    return XST_SUCCESS;
}

static int WriteReg16(u8 Addr, u16 Value) {
    int Status;
    u8 WriteBuffer[3];

    // Set the defaults.
    TransmitComplete = 1;

    // Start the IIC device.
    Status = XIic_Start(&IicInstance);
    if (Status != XST_SUCCESS) {
        return XST_FAILURE;
    }

    // Set the Repeated Start option.
    IicInstance.Options = 0;

    // Send the data.
    WriteBuffer[0] = Addr;
    WriteBuffer[1] = ((Value >> 8) & 0xff);
    WriteBuffer[2] = (Value & 0xff);
    Status = XIic_MasterSend(&IicInstance, WriteBuffer, 3);
    if (Status != XST_SUCCESS) {
        return XST_FAILURE;
    }

    /*
     * Wait till data is transmitted.
     */
    while ((TransmitComplete) || (XIic_IsIicBusy(&IicInstance) == TRUE)) {
    }

    // Stop the IIC device.
    Status = XIic_Stop(&IicInstance);
    if (Status != XST_SUCCESS) {
        return XST_FAILURE;
    }

    return XST_SUCCESS;
}

static int ReadReg16(u8 Addr, u16* pValue) {
    int Status;
    u8 ReadBuffer[2] = {0, 0};

    // set Addr
    Status = WriteAddr(Addr);
    if (Status != XST_SUCCESS) {
        return XST_FAILURE;
    }

    // Set the defaults.
    ReceiveComplete = 1;

    // Start the IIC device.
    Status = XIic_Start(&IicInstance);
    if (Status != XST_SUCCESS) {
        return XST_FAILURE;
    }

    // Set the Repeated Start option.
    IicInstance.Options = 0; // XII_REPEATED_START_OPTION;

    // Receive the data.
    Status = XIic_MasterRecv(&IicInstance, ReadBuffer, 2);
    if (Status != XST_SUCCESS) {
        return XST_FAILURE;
    }

    // Wait till all the data is received.
    while (ReceiveComplete) {
    }
    *pValue = ReadBuffer[0] * 256 + ReadBuffer[1];

    // Stop the IIC device.
    Status = XIic_Stop(&IicInstance);
    if (Status != XST_SUCCESS) {
        return XST_FAILURE;
    }

    return XST_SUCCESS;
}

static int WriteReg8(u8 Addr, u8 Value) {
    int Status;
    u8 WriteBuffer[2];

    // Set the defaults.
    TransmitComplete = 1;

    // Start the IIC device.
    Status = XIic_Start(&IicInstance);
    if (Status != XST_SUCCESS) {
        return XST_FAILURE;
    }

    // Set the Repeated Start option.
    IicInstance.Options = 0;

    // Send the data.
    WriteBuffer[0] = Addr;
    WriteBuffer[1] = Value;
    Status = XIic_MasterSend(&IicInstance, WriteBuffer, 2);
    if (Status != XST_SUCCESS) {
        return XST_FAILURE;
    }

    /*
     * Wait till data is transmitted.
     */
    while ((TransmitComplete) || (XIic_IsIicBusy(&IicInstance) == TRUE)) {
    }

    // Stop the IIC device.
    Status = XIic_Stop(&IicInstance);
    if (Status != XST_SUCCESS) {
        return XST_FAILURE;
    }

    return XST_SUCCESS;
}

static int ReadReg8(u8 Addr, u8* pValue) {
    int Status;
    u8 ReadBuffer[2] = {0, 0};

    // set Addr
    Status = WriteAddr(Addr);
    if (Status != XST_SUCCESS) {
        return XST_FAILURE;
    }

    // Set the defaults.
    ReceiveComplete = 1;

    // Start the IIC device.
    Status = XIic_Start(&IicInstance);
    if (Status != XST_SUCCESS) {
        return XST_FAILURE;
    }

    // Set the Repeated Start option.
    IicInstance.Options = 0; // XII_REPEATED_START_OPTION;

    // Receive the data.
    Status = XIic_MasterRecv(&IicInstance, ReadBuffer, 1);
    if (Status != XST_SUCCESS) {
        return XST_FAILURE;
    }

    // Wait till all the data is received.
    while (ReceiveComplete) {
    }
    *pValue = ReadBuffer[0];

    // Stop the IIC device.
    Status = XIic_Stop(&IicInstance);
    if (Status != XST_SUCCESS) {
        return XST_FAILURE;
    }

    return XST_SUCCESS;
}

/*****************************************************************************
 * Current monitor functions
 ******************************************************************************/

void CurrSenseInit(int DevAddr) {
    CurrSensAddr = DevAddr;
}

int CurrSenseReadConfig(u16* pValue) {
    SetI2cDevAddr(CurrSensAddr);
    return ReadReg16(0, pValue);
}

int CurrSenseReadVShunt(u16* pValue) {
    SetI2cDevAddr(CurrSensAddr);
    return ReadReg16(1, pValue);
}

int CurrSenseReadVBus(u16* pValue) {
    int Status;
    u16 Value;

    SetI2cDevAddr(CurrSensAddr);
    Status = ReadReg16(2, &Value);
    *pValue = Value / 2; // mV

    return Status;
}

int CurrSenseReadPower(u16* pValue) {
    u16 Voltage, Current;
    int Status;

    SetI2cDevAddr(CurrSensAddr);
    Status = ReadReg16(1, &Current);
    Status = ReadReg16(2, &Voltage);

    *pValue = (Current * Voltage / 2000);
    return Status;
}

int CurrSenseReadCurrent(u16* pValue) {
    SetI2cDevAddr(CurrSensAddr);
    return ReadReg16(1, pValue);
}

int CurrSenseReadCalib(u16* pValue) {
    SetI2cDevAddr(CurrSensAddr);
    return ReadReg16(5, pValue);
}

int CurrSenseWriteConfig(u16 Value) {
    SetI2cDevAddr(CurrSensAddr);
    return WriteReg16(0, Value);
}

int CurrSenseWriteCalib(u16 Value) {
    SetI2cDevAddr(CurrSensAddr);
    return WriteReg16(5, Value);
}

/*****************************************************************************
 * GPIO functions
 ******************************************************************************/

int GpioSetDir(u8 Value) {
    SetI2cDevAddr(GPIO_ADDR);
    return WriteReg8(1, Value);
}

int GpioOut(u8 Value) {
    SetI2cDevAddr(GPIO_ADDR);
    return WriteReg8(0, Value);
}

int GpioSetLed(u8 Value) {
    SetI2cDevAddr(GPIO_ADDR);
    WriteReg8(1, 0xF0); // set lower bytes as output
    return WriteReg8(0, Value);
}

/*****************************************************************************
 * RTC functions
 ******************************************************************************/

static u8 bcd(int dec) {
    return ((dec / 10) << 4) + (dec % 10);
}

static int decimal(u8 bcd) {
    return ((bcd >> 4) * 10) + bcd % 16;
}

int RTC_init() {
    u8 Value;

    SetI2cDevAddr(RTC_ADDR);
    // enable write access
    WriteReg8(8, 0x41);
    // enable Temp sense
    ReadReg8(0x0D, (u8*) &Value);
    WriteReg8(0x0D, (Value | 0x80));

    return 0;
}

int RTC_readTime(int* hour, int* min, int* sec) {
    int Status;
    u8 Value;

    SetI2cDevAddr(RTC_ADDR);
    Status = ReadReg8(0x00, (u8*) &Value);
    *sec = decimal(Value);
    Status += ReadReg8(0x01, (u8*) &Value);
    *min = decimal(Value);
    Status += ReadReg8(0x02, (u8*) &Value);
    *hour = decimal(Value & 0x3f);

    return Status;
}

int RTC_setTime(int hour, int min, int sec) {
    int Status;
    u8 h = bcd(hour), m = bcd(min), s = bcd(sec);
    h |= 0x80; // enable 24h format

    SetI2cDevAddr(RTC_ADDR);
    Status = WriteReg8(0x0, s);
    Status += WriteReg8(0x1, m);
    Status += WriteReg8(0x2, h);

    return Status;
}

int RTC_readDate(int* day, int* month, int* year) {
    int Status;
    u8 Value;

    SetI2cDevAddr(RTC_ADDR);
    Status = ReadReg8(0x3, (u8*) &Value);
    *day = decimal(Value);
    Status += ReadReg8(0x4, (u8*) &Value);
    *month = decimal(Value);
    Status += ReadReg8(0x5, (u8*) &Value);
    *year = decimal(Value);

    return Status;
}

int RTC_setDate(int day, int month, int year) {
    int Status;
    u8 d = bcd(day), m = bcd(month), y = bcd(year);

    SetI2cDevAddr(RTC_ADDR);
    Status = WriteReg8(0x3, d);
    Status += WriteReg8(0x4, m);
    Status += WriteReg8(0x5, y);

    return Status;
}

int RTC_readTemp(int* temp) {
    int Status;
    u8 value0, value1;

    SetI2cDevAddr(RTC_ADDR);
    Status = ReadReg8(0x28, (u8*) &value0);
    Status += ReadReg8(0x29, (u8*) &value1);
    *temp = (value0 + value1 * 256) / 2 - 273;

    return Status;
}

/*****************************************************************************
 * EEPROM functions
 ******************************************************************************/

int EEPROM_init(int DevAddr) {
    int Status;

    EepromAddr = DevAddr;
    SetI2cDevAddr(EepromAddr);
    // set mode
    Status = WriteReg8(0xA8, 0x0);

    return Status;
}

int EEPROM_read(u32 addr, u32 len, u8* readbuffer) {
    int Status;
    ///	u8 ReadBuffer[2] = {0,0};

    // select EEPORM
    Status = SetI2cDevAddr(EepromAddr);
    if (Status != XST_SUCCESS) {
        return XST_FAILURE;
    }

    // set Addr
    Status = WriteAddr(addr);
    if (Status != XST_SUCCESS) {
        return XST_FAILURE;
    }

    // Set the defaults.
    ReceiveComplete = 1;

    // Start the IIC device.
    Status = XIic_Start(&IicInstance);
    if (Status != XST_SUCCESS) {
        return XST_FAILURE;
    }

    // Set the Repeated Start option.
    IicInstance.Options = 0; // XII_REPEATED_START_OPTION;

    // Receive the data.
    Status = XIic_MasterRecv(&IicInstance, readbuffer, len);
    if (Status != XST_SUCCESS) {
        return XST_FAILURE;
    }

    // Wait till all the data is received.
    while (ReceiveComplete);

    // Stop the IIC device.
    Status = XIic_Stop(&IicInstance);
    if (Status != XST_SUCCESS) {
        return XST_FAILURE;
    }

    return XST_SUCCESS;
}

/*****************************************************************************/
/**
 * This Send handler is called asynchronously from an interrupt context and
 * indicates that data in the specified buffer has been sent.
 *
 * @param	InstancePtr is a pointer to the IIC driver instance for which
 * 		the handler is being called for.
 *
 * @return	None.
 *
 * @note		None.
 *
 ******************************************************************************/
void SendHandler(XIic* InstancePtr) {
    TransmitComplete = 0;
}

/*****************************************************************************/
/**
 * This Receive handler is called asynchronously from an interrupt context and
 * indicates that data in the specified buffer has been Received.
 *
 * @param	InstancePtr is a pointer to the IIC driver instance for which
 * 		the handler is being called for.
 *
 * @return	None.
 *
 * @note		None.
 *
 ******************************************************************************/
void ReceiveHandler(XIic* InstancePtr) {
    ReceiveComplete = 0;
}

/*****************************************************************************/
/**
 * This Status handler is called asynchronously from an interrupt
 * context and indicates the events that have occurred.
 *
 * @param	InstancePtr is a pointer to the IIC driver instance for which
 *		the handler is being called for.
 * @param	Event indicates the condition that has occurred.
 *
 * @return	None.
 *
 * @note		None.
 *
 ******************************************************************************/
void StatusHandler(XIic* InstancePtr, int Event) {
}
