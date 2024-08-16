/*******************************************************************************
                           Minos / Feminos
                           _____________________

 File:        i2c.c

 Description: I2C device API implementation

  Used for controlling I2C devices of Enclustra Mars MX2 Module.
  Adapted from sample code provided by Enclustra

 Author:      D. Calvet,        calvet@hep.saclay.cea.fr


 History:
  September 2012: created

*******************************************************************************/

#include "i2c.h"
#include "MarsI2C.h"
#include "interrupt.h"
#include "platform_spec.h"
#include "spiflash.h"
#include <cstdio>

/* Conversion table for months in numeral to short string */
static char monthnum2monthstr[13][4] = {
        "???",
        "JAN",
        "FEB",
        "MAR",
        "APR",
        "MAY",
        "JUN",
        "JUL",
        "AUG",
        "SEP",
        "OCT",
        "NOV",
        "DEC"};

/*******************************************************************************
 I2C_Open
*******************************************************************************/
int I2C_Open(XIic* i2c_inst, XIntc* x_intc) {
    int Status;

    // Initialize the IIC Instance
    if ((Status = XIic_Initialize(i2c_inst, XPAR_IIC_0_DEVICE_ID)) != XST_SUCCESS) {
        printf("I2C_Open: XIic_Initialize failed (status:%d)\n", Status);
        return (-1);
    }

    /*
     * Connect the device driver handler that will be called when an
     * interrupt for the device occurs, the handler defined above performs
     *  the specific interrupt processing for the device.
     */
    if ((Status = XIntc_Connect(x_intc, XPAR_INTC_0_IIC_0_VEC_ID, (XInterruptHandler) XIic_InterruptHandler, (void*) i2c_inst)) != XST_SUCCESS) {
        printf("I2C_Open: XIntc_Connect failed (status:%d)\n", Status);
        return (-1);
    }

    // Enable the interrupts for the IIC device
    XIntc_Enable(x_intc, XPAR_INTC_0_IIC_0_VEC_ID);

    // Set the Transmit and Receive handlers
    XIic_SetSendHandler(i2c_inst, i2c_inst, (XIic_Handler) SendHandler);
    XIic_SetRecvHandler(i2c_inst, i2c_inst, (XIic_Handler) ReceiveHandler);
    XIic_SetStatusHandler(i2c_inst, i2c_inst, (XIic_StatusHandler) StatusHandler);

    return (0);
}

/*******************************************************************************
 I2C_GetModuleInfo
*******************************************************************************/
int I2C_GetModuleInfo(XIic* i2c_inst, char* str) {
    int Status;
    unsigned int SerialNo;
    unsigned int ProdNo;
    unsigned int ModConf;
    unsigned char MacAddr[6];
    unsigned char ReadBuffer[4];
    int Ddr2Size;
    int FlashSize;
    int mars;

    if ((Status = EEPROM_init(EEPROM_ADDR)) != XST_SUCCESS) {
        sprintf(str, "I2C_GetModuleInfo: EEPROM_init failed (status:%d)", Status);
        return (-1);
    }

    /* Read Module Serial Number */
    if ((Status = EEPROM_read(0x0, 4, &ReadBuffer[0])) != XST_SUCCESS) {
        sprintf(str, "I2C_GetModuleInfo: EEPROM_read failed (status:%d)", Status);
        return (-1);
    }
    SerialNo = (ReadBuffer[0] << 24) | (ReadBuffer[1] << 16) | (ReadBuffer[2] << 8) | ReadBuffer[3];

    /* Read Module Product Number */
    if ((Status = EEPROM_read(0x4, 4, &ReadBuffer[0])) != XST_SUCCESS) {
        sprintf(str, "I2C_GetModuleInfo: EEPROM_read failed (status:%d)", Status);
        return (-1);
    }
    ProdNo = (ReadBuffer[0] << 24) | (ReadBuffer[1] << 16) | (ReadBuffer[2] << 8) | ReadBuffer[3];

    /* Read Module Configuration */
    if ((Status = EEPROM_read(0x8, 4, &ReadBuffer[0])) != XST_SUCCESS) {
        sprintf(str, "I2C_GetModuleInfo: EEPROM_read failed (status:%d)", Status);
        return (-1);
    }
    ModConf = (ReadBuffer[0] << 24) | (ReadBuffer[1] << 16) | (ReadBuffer[2] << 8) | ReadBuffer[3];

    /* Read MAC Address */
    if ((Status = EEPROM_read(0x10, 6, &MacAddr[0])) != XST_SUCCESS) {
        sprintf(str, "I2C_GetModuleInfo: EEPROM_read failed (status:%d)", Status);
        return (-1);
    }

    /* Determine Module type */
    if ((ProdNo >> 16) == 0x320) {
        mars = 1;
    } else if ((ProdNo >> 16) == 0x321) {
        mars = 2;
    } else {
        mars = 0;
    }

    Ddr2Size = 8 * (0x1 << (((ModConf >> 4) & 0xf) - 1));
    FlashSize = (0x1 << ((ModConf & 0xf) - 1));

    /* Print Module information */
    if (mars) {
        sprintf(str, "Mars MX%d Rev:%d Serial:%d SDRAM:%d MB Flash:%d MB MAC0:%02X:%02X:%02X:%02X:%02X:%02X",
                mars,
                (ProdNo & 0xff),
                SerialNo,
                Ddr2Size,
                FlashSize,
                MacAddr[0],
                MacAddr[1],
                MacAddr[2],
                MacAddr[3],
                MacAddr[4],
                MacAddr[5]);
    } else {
        sprintf(str, "Unknown Module");
    }

    return (0);
}

/*******************************************************************************
 I2C_GetCurrentMonitor
*******************************************************************************/
int I2C_GetCurrentMonitor(XIic* i2c_inst, char* str) {
    /*
    int Status;
    unsigned short Voltage, Current, Power;
    */

    sprintf(str, "I2C_GetCurrentMonitor: not supported");
    return (0);

    /*
    CurrSenseInit(CURRSENS_STARTER);

    if ((Status = CurrSenseWriteConfig(0x019F)) != XST_SUCCESS)
	{
        sprintf(str, "I2C_GetCurrentMonitor: CurrSenseWriteConfig failed (status:%d)", Status);
        return (-1);
    }

	if ((Status = CurrSenseWriteCalib (0xC800)) != XST_SUCCESS)
    {
        sprintf(str, "I2C_GetCurrentMonitor: CurrSenseWriteConfig failed (status:%d)", Status);
        return (-1);
    }

    if ((Status = CurrSenseReadVBus(&Voltage)) != XST_SUCCESS)
    {
        sprintf(str, "I2C_GetCurrentMonitor: CurrSenseReadVBus failed (status:%d)", Status);
        return (-1);
    }

	if ((Status = CurrSenseReadCurrent(&Current)) != XST_SUCCESS)
    {
        sprintf(str, "I2C_GetCurrentMonitor: CurrSenseReadCurrent failed (status:%d)", Status);
        return (-1);
    }

    if ((Status = CurrSenseReadPower(&Power)) != XST_SUCCESS)
    {
        sprintf(str, "I2C_GetCurrentMonitor: CurrSenseReadPower failed (status:%d)", Status);
        return (-1);
    }

	sprintf(str, "Module Voltage: %d mV  Current: %d mA  Power:%d mW", Voltage, Current, Power);

    return(0);
    */
}

/*******************************************************************************
 I2C_GetDateTimeTemp
*******************************************************************************/
int I2C_GetDateTimeTemp(XIic* i2c_inst, char* str) {
    int Status;
    int Day, Month, Year, Hour, Min, Sec;
    int Temp;

    if ((Status = RTC_init()) != XST_SUCCESS) {
        sprintf(str, "I2C_GetDateTimeTemp: RTC_init failed (status:%d)", Status);
        return (-1);
    }

    /* Read Time */
    if ((Status = RTC_readTime(&Hour, &Min, &Sec)) != XST_SUCCESS) {
        sprintf(str, "I2C_GetDateTimeTemp: RTC_readTime failed (status:%d)", Status);
        return (-1);
    }

    /* Read Date */
    if ((Status = RTC_readDate(&Day, &Month, &Year)) != XST_SUCCESS) {
        sprintf(str, "I2C_GetDateTimeTemp: RTC_readTime failed (status:%d)", Status);
        return (-1);
    }
    if ((Month < 0) || (Month > 13)) {
        Month = 0;
    }

    /* Read Temperature */
    if ((Status = RTC_readTemp(&Temp)) != XST_SUCCESS) {
        sprintf(str, "I2C_GetDateTimeTemp: RTC_readTime failed (status:%d)", Status);
        return (-1);
    }

    sprintf(str, "Module Time: %02d:%02d:%02d Date: %02d:%s:20%02d Temperature: %d degC",
            Hour,
            Min,
            Sec,
            Day,
            monthnum2monthstr[Month],
            Year,
            Temp);

    return (0);
}

/*******************************************************************************
 I2C_SetDateTime
*******************************************************************************/
int I2C_SetDateTime(XIic* i2c_inst, char* str, int day, int month, int year, int hour, int min, int sec) {
    int Status;

    if (year >= 2000) {
        year -= 2000;
    }

    if ((Status = RTC_setTime(hour, min, sec)) != XST_SUCCESS) {
        sprintf(str, "I2C_SetDateTime: RTC_setTime failed (status:%d)", Status);
        return (-1);
    }

    if ((Status = RTC_setDate(day, month, year)) != XST_SUCCESS) {
        sprintf(str, "I2C_SetDateTime: RTC_setDate failed (status:%d)", Status);
        return (-1);
    }

    sprintf(str, "Date and Time set");

    return (0);
}
