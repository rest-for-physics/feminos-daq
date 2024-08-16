/*******************************************************************************
                           Minos / Feminos
                           _____________________

 File:        interrupt.c

 Description: Interrupt System.

 Adapted from code provided by Xilinx

 Author:      D. Calvet,        calvet@hep.saclay.cea.fr


 History:
  September 2012: created

*******************************************************************************/
#include "interrupt.h"
#include "platform_spec.h"
#include "xiic.h"
#include "xspi.h"
#include <cstdio>

/*
 * Global variable
 */
XIntc InterruptSystem;

/*****************************************************************************/
/**
 *
 * This function setups the interrupt system such that interrupts can occur
 * for the Spi device. This function is application specific since the actual
 * system may or may not have an interrupt controller. The Spi device could be
 * directly connected to a processor without an interrupt controller.  The
 * user should modify this function to fit the application.
 *
 * @param	SpiPtr is a pointer to the instance of the Spi device.
 *
 * @return	XST_SUCCESS if successful, otherwise XST_FAILURE.
 *
 * @note		None
 *
 ******************************************************************************/
int InterruptSystem_Setup(XIntc* x_intc) {
    int Status;

    /*
     * Initialize the interrupt controller driver so that
     * it's ready to use, specify the device ID that is generated in
     * xparameters.h
     */
    if ((Status = XIntc_Initialize(x_intc, XPAR_INTC_0_DEVICE_ID)) != XST_SUCCESS) {
        return (-1);
    }

    /*
     * Start the interrupt controller such that interrupts are enabled for
     * all devices that cause interrupts, specific real mode so that
     * the SPI can cause interrupts thru the interrupt controller.
     */
    if ((Status = XIntc_Start(x_intc, XIN_REAL_MODE)) != XST_SUCCESS) {
        return (-1);
    }

    /*
     * Enable MicroBlaze interrupts
     */
    microblaze_enable_interrupts();

    return XST_SUCCESS;
}
