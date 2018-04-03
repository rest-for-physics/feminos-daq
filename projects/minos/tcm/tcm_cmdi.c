/*******************************************************************************
                           Minos - Trigger Clock Module (TCM)
                           __________________________________

 File:        tcm_cmdi.c

 Description: A command interpreter to control the TCM.

 Author:      D. Calvet,        calvet@hep.saclay.cea.fr
        		  
 History:
  April 2012: created

  July 2012: added range and wildcard support for the "port" commands

  February 2013: added commands to read/clear inter event time histogram.
   added commands to set/show maximum event out time.

  May 2013: added commands "inv_tcm_sig" and "do_end_of_busy"

  January 2014: corrected error on calculation of datagram size that prevented
  END_OF_FRAME to be sent. Corrected content of cmd statistics frame

  February 2014: added commands dcbal_enc dcbal_dec and inv_tcm_clk

  March 2014: corrected error in the printout of which register is read for
  some of the commands acting on register 22.

  June 2014: added command "feminos mask"

  June 2014: added commands "mult_trig_ena", "mult_trig_dst",
  "mult_more_than" and "mult_less_than"

  June 2014: added command "tcm_bert"

*******************************************************************************/

#include "tcm_cmdi.h"
#include "busymeter.h"
#include "periodmeter.h"
#include "frame.h"
#include "error_codes.h"
#include "i2c.h"
#include "cmdi_i2c.h"
#include "cmdi_flash.h"

#include <stdio.h>
#include <string.h>

////////////////////////////////////////////////
// Pick compilation Date and Time
char server_date[] = __DATE__;
char server_time[] = __TIME__;
////////////////////////////////////////////////

/* Convert Multiplicity Trigger Destination to String  */
static char MultTrigDst2str[2][20] = {
		"Send to Feminos",  // 0
		"Send to External"  // 1
		};

/*******************************************************************************
 trig_range_rate2freq - converts trigger range and rate to frequency
*******************************************************************************/
float trig_range_rate2freq(unsigned int range, unsigned int rate)
{
	if (range == 0)
	{
		return (0.1 * (float) rate);
	}
	else if (range == 1)
	{
		return (10 * (float) rate);
	}
	else if (range == 2)
	{
		return (100 * (float) rate);
	}
	else if (range == 3)
	{
		return (1000 * (float) rate);
	}
	else
	{
		return (0.0);
	}
}

/*******************************************************************************
 isobus2str
*******************************************************************************/
void isobus2str(int iso, char *s)
{
	sprintf(s, " ");
	if (iso & R20_WCK_SYNCH)
	{
		strcat(s, "WCK_SYNCH ");
	}
	if (iso & R20_SCA_START)
	{
		strcat(s, "SCA_START ");
	}
	if (iso & R20_SCA_STOP)
	{
		strcat(s, "SCA_STOP ");
		if ((iso & R20_EV_TYPE_M) == R20_EV_TYPE_00)
		{
			strcat(s, "EV_TYPE:00 ");
		}
		else if ((iso & R20_EV_TYPE_M) == R20_EV_TYPE_01)
		{
			strcat(s, "EV_TYPE:01 ");
		}
		else if ((iso & R20_EV_TYPE_M) == R20_EV_TYPE_10)
		{
			strcat(s, "EV_TYPE:10 ");
		}
		else if ((iso & R20_EV_TYPE_M) == R20_EV_TYPE_11)
		{
			strcat(s, "EV_TYPE:11 ");
		}
	}
	if (iso & R20_CLR_EVCNT)
	{
		strcat(s, "CLR_EVCNT ");
	}
	if (iso & R20_CLR_TSTAMP)
	{
		strcat(s, "CLR_TSTAMP ");
	}
}

/*******************************************************************************
 status2str
*******************************************************************************/
void status2str(int sta, char *s)
{
	sprintf(s, " ");
	if (sta & R20_STANDBY)
	{
		strcat(s, "STANDBY ");
	}
	if (sta & R20_WAITING_TRIG)
	{
		strcat(s, "WAITING_TRIG ");
	}
	if (sta & R20_WAITING_LAT)
	{
		strcat(s, "WAITING_LAT ");
	}
	if (sta & R20_FEM_BUSY)
	{
		strcat(s, "FEM_BUSY ");
	}
	if (sta & R20_START_ACK_MISS)
	{
		strcat(s, "START_ACK_MISS ");
	}
	if (sta & R20_TRIG_ACK_MISS)
	{
		strcat(s, "TRIG_ACK_MISS ");
	}
	if (sta & R20_NO_BUSY_MISS)
	{
		strcat(s, "NO_BUSY_MISS ");
	}
}

/*******************************************************************************
 status2str
*******************************************************************************/
void int2binstr(int sta, char *s)
{
	int i;
	unsigned int mask;

	mask = 1 << (NB_OF_FEMINOS_PORTS-1);
	for (i=0; i<NB_OF_FEMINOS_PORTS; i++)
	{
		if (sta & mask)
		{
			strcat(s, "1");
		}
		else
		{
			strcat(s, "0");
		}
		if ((i==7) || (i==15) || (i==23))
		{
			strcat(s, ".");
		}
		mask >>=1;
	}
}

/*******************************************************************************
 TcmCmdi_CmdCommands
*******************************************************************************/
int TcmCmdi_CmdCommands(CmdiContext *c)
{
	int err;
	char *rep;
	unsigned short size;
	unsigned short *p;

	rep = (char *)( c->burep + CFRAME_ASCII_MSG_OFFSET); // Move to beginning of ASCII string in CFRAME
	err = 0;

	// Command to clear command statistics
	if (strncmp(c->cmd, "cmd clr", 7) == 0)
	{
		// We cannot completely clear the rx_cmd_cnt because we also count
		// the clear command itself when it is received
		c->rx_cmd_cnt     = 1;
		c->err_cmd_cnt    = 0;
		c->tx_rep_cnt     = 0;

		sprintf(rep, "0 Tcm(%d) command statistics cleared\n", c->fem->card_id);
	}
	// Put statistics
	else if (strncmp(c->cmd, "cmd stat", 8) == 0)
	{
		// Build response frame
		size = 0;
		p = (unsigned short *) (c->burep);
		c->reply_is_cframe = 0;

		// Put Start Of Frame + Version
		*p = PUT_FVERSION_FEMID(PFX_START_OF_MFRAME, CURRENT_FRAMING_VERSION, c->fem->card_id);
		p++;
		size+=2;

		// Leave space for size;
		p++;
		size+=sizeof(unsigned short);

		// Put Command statistics prefix
		*p = PFX_CMD_STATISTICS;
		p++;
		size+=sizeof(unsigned short);

		// Put Command statistics
		*p = (unsigned short) (c->rx_cmd_cnt & 0xFFFF);
		p++;
		*p = (unsigned short) ((c->rx_cmd_cnt & 0xFFFF0000) >> 16);
		p++;
		size+=sizeof(int);
		*p = 0x0000; // no rx_daq_count on TCM
		p++;
		*p = 0x0000; // no rx_daq_count on TCM
		p++;
		size+=sizeof(int);
		*p = 0x0000; // no rx_daq_timeout on TCM
		p++;
		*p = 0x0000; // no rx_daq_timeout on TCM
		p++;
		size+=sizeof(int);
		*p = 0x0000; // no rx_daq_delayed on TCM
		p++;
		*p = 0x0000; // no rx_daq_delayed on TCM
		p++;
		size+=sizeof(int);
		*p = 0x0000; // no daq_miss_cnt on TCM
		p++;
		*p = 0x0000; // no daq_miss_cnt on TCM
		p++;
		size+=sizeof(int);
		*p = (unsigned short) (c->err_cmd_cnt & 0xFFFF);
		p++;
		*p = (unsigned short) ((c->err_cmd_cnt & 0xFFFF0000) >> 16);
		p++;
		size+=sizeof(int);
		// We anticipate on the next value of tx_rep_cnt to take into account the
		// fact that when we fill the present data packet, we do not know yet if it
		// will be sent successfully. The problem is that if we increment tx_rep_cnt
		// after the packet has been really sent, the number of commands replies
		// is greater by one unit compared to the number of command received - which
		// is normal but non-intuitive.
		*p = (unsigned short) ((c->tx_rep_cnt+1) & 0xFFFF);
		p++;
		*p = (unsigned short) ((c->tx_rep_cnt & 0xFFFF0000) >> 16);
		p++;
		size+=sizeof(int);
		*p = 0x0000; // no tx_daq_cnt on TCM
		p++;
		*p = 0x0000; // no tx_daq_cnt on TCM
		p++;
		size+=sizeof(int);
		*p = 0x0000; // no tx_daq_resend_cnt on TCM
		p++;
		*p = 0x0000; // no tx_daq_resend_cnt on TCM
		p++;
		size+=sizeof(int);


		// Put End Of Frame
		*p = PFX_END_OF_FRAME;
		p++;
		size+=sizeof(unsigned short);

		// Put packet size after start of frame
		p = (unsigned short *) (c->burep);
		p++;
		*p = size;

		// Include in the size the 2 empty bytes kept at the beginning of the UDP datagram
		c->rep_size = size + 2;
	}
	else
	{
		err = -1;
		sprintf(rep, "%d syntax: cmd <clr | stat>\n", err);
	}

	c->do_reply = 1;
	return(err);
}

/*******************************************************************************
 TcmCmdi_Cmd_Usage
  fill a string with command syntax outline
*******************************************************************************/
void TcmCmdi_Cmd_Usage(char *r, int sz)
{
	unsigned int r_capa = sz - 8;
	
	sprintf(r, "0 Commands:\n");
	if (strlen(r) < r_capa) strcat(r, "help verbose version\n");
	if (strlen(r) < r_capa) strcat(r, "\n");
}

/*******************************************************************************
 TcmCmdi_Context_Init
*******************************************************************************/
void TcmCmdi_Context_Init(CmdiContext *c)
{
	c->burep           = (void *) 0;
	c->frrep             = (void *) 0;
	c->burep             = (void *) 0;
	c->reply_is_cframe = 0;
	c->rep_size        = 0;
	c->do_reply        = 0;
	c->bp              = (BufPool *) 0;
	c->et              = (Ethernet *) 0;
	c->i2c_inst        = (XIic    *) 0;
	c->lst_socket      = 0;
	c->daq_socket      = 0;
	c->rx_cmd_cnt      = 0;
	c->err_cmd_cnt     = 0;
}

/*******************************************************************************
 TcmCmdi_Cmd_Interpret
*******************************************************************************/
int TcmCmdi_Cmd_Interpret(CmdiContext *c)
{
	unsigned int param[10];
	int err = 0;
	char *rep;
	unsigned int val_rdi;
	char str[80];
	char str_bis[80];
	short *sh;
	int rep_ascii_len;
	int arg_cnt;
	int port_beg;
	int port_end;
	int port_tmp;
	char *p;

	c->do_reply = 0;
	c->rep_size = 0;

	// Get a buffer for response if we do not have already one
	if (c->frrep == (void *) 0)
	{
		// Ask a buffer to the buffer manager
		if ((err = BufPool_GiveBuffer(c->bp, &(c->frrep), AUTO_RETURNED)) < 0)
		{
			c->err_cmd_cnt++;
			return (err);
		}
		// Leave space in the buffer for Ethernet IP and UDP headers
		p = (char *) c->frrep + USER_OFFSET_ETH_IP_UDP_HDR;
		// Zero the next two bytes which are reserved for future protocol
		*p = 0x00; p++;
		*p = 0x00; p++;
		// Set the effective address of the beginning of buffer for the user
		c->burep = (void *) p;
	}
	rep = (char *)( c->burep + CFRAME_ASCII_MSG_OFFSET); // Move to beginning of ASCII string in CFRAME
	c->reply_is_cframe = 1;

	// Command to read/write a configuration register in the TCM
	if (strncmp(c->cmd, "reg", 3) == 0)
	{
		c->rx_cmd_cnt++;
		if (sscanf(c->cmd, "reg %i 0x%x", &param[0], &param[1]) == 2)
		{
			if ( (err = Tcm_ActOnRegister(c->fem, TCM_REG_WRITE, param[0], 0xFFFFFFFF, &param[1])) < 0)
			{
				sprintf(rep, "%d Tcm(%d): Tcm_ActOnRegister %d failed\n", err, c->fem->card_id, param[0]);
			}
			else
			{
				sprintf(rep, "0 Tcm(%d) Reg(%d) <- 0x%08x\n", c->fem->card_id, param[0], param[1]);
			}
		}
		else
		{
			if ( (err = Tcm_ActOnRegister(c->fem, TCM_REG_READ, param[0], 0xFFFFFFFF, &val_rdi)) < 0)
			{
				sprintf(rep, "%d Tcm(%d): Tcm_ActOnRegister %d failed\n", err, c->fem->card_id, param[0]);
			}
			else
			{
				sprintf(rep, "0 Tcm(%d) Reg(%d) = 0x%08x (%d)\n", c->fem->card_id, param[0], val_rdi, val_rdi);
			}
		}
		c->do_reply = 1;
	}

	// Command to send a synchronous command to all Feminos
	else if (strncmp(c->cmd, "isobus", 6) == 0)
	{
		c->rx_cmd_cnt++;
		if (sscanf(c->cmd, "isobus 0x%x", &param[0]) == 1)
		{
			if ( (err = Tcm_ActOnRegister(c->fem, TCM_REG_WRITE, 20, R20_ISOBUS_M, &param[0])) < 0)
			{
				sprintf(rep, "%d Tcm(%d): Tcm_ActOnRegister %d failed\n", err, c->fem->card_id, 20);
			}
			else
			{
				param[1] = 0x0;
				if ( (err = Tcm_ActOnRegister(c->fem, TCM_REG_WRITE, 20, R20_ISOBUS_M, &param[1])) < 0)
				{
					sprintf(rep, "%d Tcm(%d): Tcm_ActOnRegister %d failed\n", err, c->fem->card_id, 20);
				}
				else
				{
					isobus2str(param[0], &str[0]);
					sprintf(rep, "0 Tcm(%d) Reg(%d) <- 0x%08x (%s auto-clear)\n", c->fem->card_id, 20, param[0], &str[0]);
				}
			}
		}
		else
		{
			err = -1;
			sprintf(rep, "%d Tcm(%d): missing argument in command %s\n", err, c->fem->card_id, c->cmd);
		}
		c->do_reply = 1;
	}

	// Command to restart TCM
	else if (strncmp(c->cmd, "restart", 7) == 0)
	{
		c->rx_cmd_cnt++;
		param[0] = R20_RESTART;
		if ( (err = Tcm_ActOnRegister(c->fem, TCM_REG_WRITE, 20, R20_RESTART, &param[0])) < 0)
		{
			sprintf(rep, "%d Tcm(%d): Tcm_ActOnRegister %d failed\n", err, c->fem->card_id, 20);
		}
		else
		{
			param[0] = 0x0;
			if ( (err = Tcm_ActOnRegister(c->fem, TCM_REG_WRITE, 20, R20_RESTART, &param[0])) < 0)
			{
				sprintf(rep, "%d Tcm(%d): Tcm_ActOnRegister %d failed\n", err, c->fem->card_id, 20);
			}
			else
			{
				sprintf(rep, "0 Tcm(%d) Reg(%d) <- restart done\n", c->fem->card_id, 20);
			}
		}
		c->do_reply = 1;
	}

	// Command to show state of TCM
	else if (strncmp(c->cmd, "state", 5) == 0)
	{
		c->rx_cmd_cnt++;
		if ( (err = Tcm_ActOnRegister(c->fem, TCM_REG_READ, 20, 0xFFFFFFFF, &val_rdi)) < 0)
		{
			sprintf(rep, "%d Tcm(%d): Tcm_ActOnRegister %d failed\n", err, c->fem->card_id, 20);
		}
		else
		{
			status2str(val_rdi, &str[0]);
			sprintf(rep, "0 Tcm(%d) Reg(%d) = 0x%x (%s)\n",
				c->fem->card_id,
				20,
				val_rdi,
				str);
		}
		c->do_reply = 1;
	}

	// Command to show the state of Feminos/Ports
	else if (strncmp(c->cmd, "feminos", 7) == 0)
	{
		c->rx_cmd_cnt++;

		// Command to mask Feminos
		if (sscanf(c->cmd, "feminos mask %i", &param[0]) == 1)
		{
			if ( (err = Tcm_ActOnRegister(c->fem, TCM_REG_WRITE, 28, R28_FEM_MASK_M, &param[0])) < 0)
			{
				sprintf(rep, "%d Tcm(%d): Tcm_ActOnRegister failed\n", err, c->fem->card_id);
			}
			else
			{
				sprintf(rep, "0 Tcm(%d) Reg(%d) <- 0x%x\n", c->fem->card_id, 28, param[0]);
			}
		}
		else
		{
			if (strncmp(c->cmd, "feminos enabled", 15) == 0)
			{
				param[0] = 16; // Register 16
				sprintf(&str[0], "Fem(%d..0).Enabled = ", (NB_OF_FEMINOS_PORTS-1));
			}
			else if (strncmp(c->cmd, "feminos detected", 16) == 0)
			{
				param[0] = 17; // Register 17
				sprintf(&str[0], "Fem(%d..0).Detected = ", (NB_OF_FEMINOS_PORTS-1));
			}
			else if (strncmp(c->cmd, "feminos sampling", 16) == 0)
			{
				param[0] = 18; // Register 18
				sprintf(&str[0], "Fem(%d..0).Sampling = ", (NB_OF_FEMINOS_PORTS-1));
			}
			else if (strncmp(c->cmd, "feminos busy", 12) == 0)
			{
				param[0] = 19; // Register 19
				sprintf(&str[0], "Fem(%d..0).Busy = ", (NB_OF_FEMINOS_PORTS-1));
			}
			else if (strncmp(c->cmd, "feminos mask", 12) == 0)
			{
				param[0] = 28; // Register 28
				sprintf(&str[0], "Fem(%d..0).Mask = ", (NB_OF_FEMINOS_PORTS-1));
			}
			else
			{
				param[0] = 17; // Irrelevant
				err      = -1;
			}
			// Read the requested register
			if (err == 0)
			{
				if ( (err = Tcm_ActOnRegister(c->fem, TCM_REG_READ, param[0], 0xFFFFFFFF, &val_rdi)) < 0)
				{
					sprintf(rep, "%d Tcm(%d): Tcm_ActOnRegister %d failed\n", err, c->fem->card_id, param[0]);
				}
				else
				{
					int2binstr(val_rdi, &str[0]);
					sprintf(rep, "0 Tcm(%d) %s (Reg(%d) = 0x%08x)\n",
						c->fem->card_id,
						str,
						param[0],
						val_rdi);
				}
			}
			else
			{
				sprintf(rep, "%d Tcm(%d): syntax error in %s\n", err, c->fem->card_id, c->cmd);
			}
		}
		c->do_reply = 1;
	}

	// Command to show Ports RX stat or clear them
	else if (strncmp(c->cmd, "port", 4) == 0)
	{
		c->rx_cmd_cnt++;
		if (sscanf(c->cmd, "port %s %s", &str_bis[0], &str[0]) == 2)
		{
			// Init
			err      = 0;
			port_tmp = 0;
			port_beg = 0;
			port_end = 0;

			// See if we have a wildcard
			if (strncmp(str_bis, "*", 1) == 0)
			{
				port_beg = 0;
				port_end = NB_OF_FEMINOS_PORTS-1;
			}
			// Maybe we have a range
			else if (sscanf(str_bis, "%i:%i", &port_beg, &port_end) == 2)
			{
				// Swap begin and end if out of order
				if (port_beg > port_end)
				{
					port_tmp = port_beg;
					port_beg = port_end;
					port_end = port_tmp;
				}
			}
			// Have we a single port specified
			else if (sscanf(str_bis, "%i", &port_beg) == 1)
			{
				port_end = port_beg;
			}
			// Incorrect syntax for port argument
			else
			{
				err = -1;
				sprintf(rep, "%d Tcm(%d) illegal port %s\n",
					err,
					c->fem->card_id,
					str_bis);
			}

			// Check port index boundaries
			if ((port_beg >= NB_OF_FEMINOS_PORTS) || (port_end >= NB_OF_FEMINOS_PORTS))
			{
				err = -1;
				sprintf(rep, "%d Tcm(%d) port %d:%d out of range [0;%d]\n",
					err,
					c->fem->card_id,
					port_beg,
					port_end,
					(NB_OF_FEMINOS_PORTS-1));
			}

			// Continue if no previous error
			if (err == 0)
			{
				// Clear counter
				if (strncmp(&str[0], "clr", 3) == 0)
				{
					// Set the clear bit for each of the ports to be acted on
					param[1] = 0;
					for (port_tmp = port_beg; port_tmp <= port_end; port_tmp++)
					{
						param[1] |= (1 << port_tmp);
					}

					param[0] = 21; // Register 21: RX Counter Clear
					// Write register
					if ( (err = Tcm_ActOnRegister(c->fem, TCM_REG_WRITE, param[0], 0xFFFFFFFF, &param[1])) < 0)
					{
						sprintf(rep, "%d Tcm(%d): Tcm_ActOnRegister %d failed\n", err, c->fem->card_id, param[0]);
					}
					else
					{
						// Write zeroes to de-activate all clear bits
						param[1] = 0;
						if ( (err = Tcm_ActOnRegister(c->fem, TCM_REG_WRITE, param[0], 0xFFFFFFFF, &param[1])) < 0)
						{
							sprintf(rep, "%d Tcm(%d): Tcm_ActOnRegister %d failed\n", err, c->fem->card_id, param[0]);
						}
						else
						{
							sprintf(rep, "0 Tcm(%d) port %d..%d counters cleared\n", c->fem->card_id, port_beg, port_end);
						}
					}
				}
				// Read RX count or RX error count
				else if ((strncmp(&str[0], "rx", 2) == 0) || (strncmp(&str[0], "err", 3) == 0))
				{
					// Check single port argument was supplied
					if (port_end != port_beg)
					{
						err = -1;
						sprintf(rep, "%d Tcm(%d) command %s only supported with single port argument\n",
							err,
							c->fem->card_id,
							str);
					}
					else
					{
						param[0] = port_beg >> 1; // Register address is port_index / 2
						// Read the requested register
						if ( (err = Tcm_ActOnRegister(c->fem, TCM_REG_READ, param[0], 0xFFFFFFFF, &val_rdi)) < 0)
						{
							sprintf(rep, "%d Tcm(%d): Tcm_ActOnRegister %d failed\n", err, c->fem->card_id, param[0]);
						}
						else
						{
							// Extract read count
							if (strncmp(&str[0], "rx", 2) == 0)
							{
								// even port number
								if (port_beg & 0x1)
								{
									val_rdi = (val_rdi >> 16) & 0xFF;
								}
								// odd port number
								else
								{
									val_rdi = (val_rdi >>  0) & 0xFF;
								}
								sprintf(str, "RX count");
							}
							// Extract rx error count
							else
							{
								// even port number
								if (port_beg & 0x1)
								{
									val_rdi = (val_rdi >> 24) & 0xFF;
								}
								// odd port number
								else
								{
									val_rdi = (val_rdi >>  8) & 0xFF;
								}
								sprintf(str, "RX Error count");
							}

							sprintf(rep, "0 Tcm(%d) Port(%d) %s: %d\n",
								c->fem->card_id,
								port_beg,
								str,
								val_rdi);
						}
					}
				}
				else
				{
					err = -1;
					sprintf(rep, "%d Tcm(%d) syntax: port <p> <rx | err | clr>\n", err, c->fem->card_id);
				}
			}
		}
		else
		{
			err = -1;
			sprintf(rep, "%d Tcm(%d) syntax: port <* | beg:end | p> <rx | err | clr>\n", err, c->fem->card_id);
		}

		c->do_reply = 1;
	}

	// Command to set/show trigger generator rate
	else if (strncmp(c->cmd, "trig_rate", 9) == 0)
	{
		c->rx_cmd_cnt++;
		if (sscanf(c->cmd, "trig_rate %i %i", &param[0], &param[1]) == 2)
		{
			param[0] = ((param[0] & 0x3) << 7) | ((param[1] & 0x7F));
			param[0] = R22_PUT_TRIG_RATE(0, param[0]);
			if ( (err = Tcm_ActOnRegister(c->fem, TCM_REG_WRITE, 22, R22_TRIG_RATE_M, &param[0])) < 0)
			{
				sprintf(rep, "%d Tcm(%d): Tcm_ActOnRegister failed\n", err, c->fem->card_id);
			}
			else
			{
				sprintf(rep, "0 Tcm(%d) Reg(%d) <- 0x%x\n", c->fem->card_id, 22, param[0]);
			}
		}
		else
		{
			if ( (err = Tcm_ActOnRegister(c->fem, TCM_REG_READ, 22, 0xFFFFFFFF, &val_rdi)) < 0)
			{
				sprintf(rep, "%d Tcm(%d): Tcm_ActOnRegister failed\n", err, c->fem->card_id);
			}
			else
			{
				param[0] = ((R22_GET_TRIG_RATE(val_rdi) >> 7) & 0x3);
				param[1] = ((R22_GET_TRIG_RATE(val_rdi) >> 0) & 0x7F);

				sprintf(rep, "0 Tcm(%d) Reg(%d) = 0x%x (%d) Range: %d  Rate: %d (%.2f Hz)\n",
					c->fem->card_id,
					22,
					val_rdi,
					val_rdi,
					param[0],
					param[1],
					trig_range_rate2freq(param[0], param[1])
					);
			}
		}
		c->do_reply = 1;
	}

	// Command to set/show event limit
	else if (strncmp(c->cmd, "event_limit", 11) == 0)
	{
		c->rx_cmd_cnt++;
		if (sscanf(c->cmd, "event_limit %i", &param[0]) == 1)
		{
			param[0] = R22_PUT_EVENT_LIMIT(0, param[0]);
			if ( (err = Tcm_ActOnRegister(c->fem, TCM_REG_WRITE, 22, R22_EVENT_LIMIT_M, &param[0])) < 0)
			{
				sprintf(rep, "%d Tcm(%d): Tcm_ActOnRegister failed\n", err, c->fem->card_id);
			}
			else
			{
				sprintf(rep, "0 Tcm(%d) Reg(%d) <- 0x%x\n", c->fem->card_id, 22, param[0]);
			}
		}
		else
		{
			if ( (err = Tcm_ActOnRegister(c->fem, TCM_REG_READ, 22, 0xFFFFFFFF, &val_rdi)) < 0)
			{
				sprintf(rep, "%d Tcm(%d): Tcm_ActOnRegister failed\n", err, c->fem->card_id);
			}
			else
			{
				param[0] = R22_GET_EVENT_LIMIT(val_rdi);
				if (param[0] == 0)
				{
					sprintf(&str[0], "infinity");
				}
				else if (param[0] == 1)
				{
					sprintf(&str[0], "1");
				}
				else if (param[0] == 2)
				{
					sprintf(&str[0], "10");
				}
				else if (param[0] == 3)
				{
					sprintf(&str[0], "100");
				}
				else if (param[0] == 4)
				{
					sprintf(&str[0], "1000");
				}
				else if (param[0] == 5)
				{
					sprintf(&str[0], "10000");
				}
				else if (param[0] == 6)
				{
					sprintf(&str[0], "100000");
				}
				else if (param[0] == 7)
				{
					sprintf(&str[0], "1000000");
				}
				else
				{
					sprintf(&str[0], "unknown");
				}
				sprintf(rep, "0 Tcm(%d) Reg(%d) = 0x%x (%d) Event_Limit: %s\n",
					c->fem->card_id,
					22,
					val_rdi,
					val_rdi,
					&str[0]);
			}
		}
		c->do_reply = 1;
	}

	// Command to set/show trigger delay
	else if (strncmp(c->cmd, "trig_delay", 10) == 0)
	{
		c->rx_cmd_cnt++;
		if ((arg_cnt = sscanf(c->cmd, "trig_delay %i %i", &param[0], &param[1])) == 2)
		{
			// Check arguments and prepare data
			param[3] = 0x0;
			if (param[0] == 0)
			{
				param[1] = R24_PUT_TRIG_DELAY_L(0, param[1]);
				param[2] = 0xFFFF;
				param[3] = 24;
			}
			else if (param[0] == 1)
			{
				param[1] = R24_PUT_TRIG_DELAY_H(0, param[1]);
				param[2] = 0xFFFF0000;
				param[3] = 24;
			}
			else if (param[0] == 2)
			{
				param[1] = R25_PUT_TRIG_DELAY_L(0, param[1]);
				param[2] = 0xFFFF;
				param[3] = 25;
			}
			else if (param[0] == 3)
			{
				param[1] = R25_PUT_TRIG_DELAY_H(0, param[1]);
				param[2] = 0xFFFF0000;
				param[3] = 25;
			}

			// Check first argument validity
			if (param[3] == 0)
			{
				err = -1;
				sprintf(rep, "%d Tcm(%d): trig_delay illegal argument %d\n", err, c->fem->card_id, param[0]);
			}
			else
			{
				if ( (err = Tcm_ActOnRegister(c->fem, TCM_REG_WRITE, param[3], param[2], &param[1])) < 0)
				{
					sprintf(rep, "%d Tcm(%d): Tcm_ActOnRegister failed\n", err, c->fem->card_id);
				}
				else
				{
					sprintf(rep, "0 Tcm(%d) Reg(%d) <- 0x%x\n", c->fem->card_id, param[3], param[1]);
				}
			}
		}
		// Do a read if only one argument was supplied
		else if (arg_cnt == 1)
		{
			// Check arguments and prepare data
			param[3] = 0x0;
			if ((param[0] == 0) || (param[0] == 1))
			{
				param[3] = 24;
			}
			else if ((param[0] == 2) || (param[0] == 3))
			{
				param[3] = 25;
			}

			// Check first argument validity
			if (param[3] == 0)
			{
				err = -1;
				sprintf(rep, "%d Tcm(%d): trig_delay illegal argument %d\n", err, c->fem->card_id, param[0]);
			}
			else
			{
				if ( (err = Tcm_ActOnRegister(c->fem, TCM_REG_READ, param[3], 0xFFFFFFFF, &val_rdi)) < 0)
				{
					sprintf(rep, "%d Tcm(%d): Tcm_ActOnRegister failed\n", err, c->fem->card_id);
				}
				else
				{
					if (param[0] == 0)
					{
						param[0] = R24_GET_TRIG_DELAY_L(val_rdi);
					}
					else if (param[0] == 1)
					{
						param[0] = R24_GET_TRIG_DELAY_H(val_rdi);
					}
					else if (param[0] == 2)
					{
						param[0] = R25_GET_TRIG_DELAY_L(val_rdi);
					}
					else if (param[0] == 3)
					{
						param[0] = R25_GET_TRIG_DELAY_H(val_rdi);
					}
					sprintf(rep, "0 Tcm(%d) Reg(%d) = 0x%x (%d) Trig_Delay: 0x%x (%d)\n",

							c->fem->card_id,
							param[3],
							val_rdi,
							val_rdi,
							param[0],
							param[0]);
				}
			}
		}
		// Give syntax error if no argument was supplied
		else
		{
			err = -1;
			sprintf(rep, "%d Tcm(%d): usage: trig_delay <type> <value>\n", err, c->fem->card_id);
		}
		c->do_reply = 1;
	}

	// Command to show the event rx/tx counters
	else if (strncmp(c->cmd, "event", 5) == 0)
	{
		c->rx_cmd_cnt++;
		if (strncmp(c->cmd, "event rx", 8) == 0)
		{
			param[0] = 26; // Register 26
			sprintf(&str[0], "Event Received =");
		}
		else
		{
			param[0] = 27; // Register 27
			sprintf(&str[0], "Event Transmitted =");
		}

		// Read the requested register
		if ( (err = Tcm_ActOnRegister(c->fem, TCM_REG_READ, param[0], 0xFFFFFFFF, &val_rdi)) < 0)
		{
			sprintf(rep, "%d Tcm(%d): Tcm_ActOnRegister %d failed\n", err, c->fem->card_id, param[0]);
		}
		else
		{
			sprintf(rep, "0 Tcm(%d) %s %d (Reg(%d) = 0x%08x)\n",
				c->fem->card_id,
				str,
				val_rdi,
				param[0],
				val_rdi);
		}
		c->do_reply = 1;
	}

	// Command to set/show sampling edge of Feminos->TCM data link
	else if (strncmp(c->cmd, "sel_edge", 8) == 0)
	{
		c->rx_cmd_cnt++;
		if (sscanf(c->cmd, "sel_edge %i", &param[0]) == 1)
		{
			param[0] = R22_PUT_SEL_EDGE(0, param[0]);
			if ( (err = Tcm_ActOnRegister(c->fem, TCM_REG_WRITE, 22, R22_SEL_EDGE, &param[0])) < 0)
			{
				sprintf(rep, "%d Tcm(%d): Tcm_ActOnRegister failed\n", err, c->fem->card_id);
			}
			else
			{
				sprintf(rep, "0 Tcm(%d) Reg(%d) <- 0x%x\n", c->fem->card_id, 22, param[0]);
			}
		}
		else
		{
			if ( (err = Tcm_ActOnRegister(c->fem, TCM_REG_READ, 22, 0xFFFFFFFF, &val_rdi)) < 0)
			{
				sprintf(rep, "%d Tcm(%d): Tcm_ActOnRegister failed\n", err, c->fem->card_id);
			}
			else
			{
				sprintf(rep, "0 Tcm(%d) Reg(%d) = 0x%x (%d) Sel_Edge: %d\n",
					c->fem->card_id,
					22,
					val_rdi,
					val_rdi,
					R22_GET_SEL_EDGE(val_rdi));
			}
		}
		c->do_reply = 1;
	}

	// Command to show/set state of inverter for TCM->Feminos signals
	else if (strncmp(c->cmd, "inv_tcm_sig", 11) == 0)
	{
		c->rx_cmd_cnt++;
		if (sscanf(c->cmd, "inv_tcm_sig %i", &param[0]) == 1)
		{
			param[0] = R22_PUT_INV_TCM_SIG(0, param[0]);
			if ( (err = Tcm_ActOnRegister(c->fem, TCM_REG_WRITE, 22, R22_INV_TCM_SIG, &param[0])) < 0)
			{
				sprintf(rep, "%d Tcm(%d): Tcm_ActOnRegister failed\n", err, c->fem->card_id);
			}
			else
			{
				sprintf(rep, "0 Tcm(%d) Reg(%d) <- 0x%x\n", c->fem->card_id, 22, param[0]);
			}
		}
		else
		{
			if ( (err = Tcm_ActOnRegister(c->fem, TCM_REG_READ, 22, 0xFFFFFFFF, &val_rdi)) < 0)
			{
				sprintf(rep, "%d Tcm(%d): Tcm_ActOnRegister failed\n", err, c->fem->card_id);
			}
			else
			{
				sprintf(rep, "0 Tcm(%d) Reg(%d) = 0x%x (%d) Inv_tcm_sig: %d\n",
					c->fem->card_id,
					22,
					val_rdi,
					val_rdi,
					R22_GET_INV_TCM_SIG(val_rdi));
			}
		}
		c->do_reply = 1;
	}

	// Command to show/set state of inverter for TCM->Feminos clock
	else if (strncmp(c->cmd, "inv_tcm_clk", 11) == 0)
	{
		c->rx_cmd_cnt++;
		if (sscanf(c->cmd, "inv_tcm_clk %i", &param[0]) == 1)
		{
			param[0] = R22_PUT_INV_TCM_CLK(0, param[0]);
			if ( (err = Tcm_ActOnRegister(c->fem, TCM_REG_WRITE, 22, R22_INV_TCM_CLK, &param[0])) < 0)
			{
				sprintf(rep, "%d Tcm(%d): Tcm_ActOnRegister failed\n", err, c->fem->card_id);
			}
			else
			{
				sprintf(rep, "0 Tcm(%d) Reg(%d) <- 0x%x\n", c->fem->card_id, 22, param[0]);
			}
		}
		else
		{
			if ( (err = Tcm_ActOnRegister(c->fem, TCM_REG_READ, 22, 0xFFFFFFFF, &val_rdi)) < 0)
			{
				sprintf(rep, "%d Tcm(%d): Tcm_ActOnRegister failed\n", err, c->fem->card_id);
			}
			else
			{
				sprintf(rep, "0 Tcm(%d) Reg(%d) = 0x%x (%d) Inv_tcm_clk: %d\n",
					c->fem->card_id,
					22,
					val_rdi,
					val_rdi,
					R22_GET_INV_TCM_CLK(val_rdi));
			}
		}
		c->do_reply = 1;
	}

	// Command to read/set DC balanced encoding
	else if (strncmp(c->cmd, "dcbal_enc", 9) == 0)
	{
		c->rx_cmd_cnt++;
		// Act on Register #22
		param[0] = 22;

		if (sscanf(c->cmd, "dcbal_enc %x", &param[1]) == 1)
		{
			param[1] = R22_PUT_DCBAL_ENC(0, param[1]);
			if ( (err = Tcm_ActOnRegister(c->fem, TCM_REG_WRITE, param[0], R22_DCBAL_ENC, &param[1])) < 0)
			{
				sprintf(rep, "%d Tcm(%02d): Tcm_ActOnRegister %d failed\n", err, c->fem->card_id, param[0]);
			}
			else
			{
				sprintf(rep, "0 Tcm(%02d) Reg(%d) <- 0x%x\n", c->fem->card_id, param[0], param[1]);
			}
		}
		else
		{
			// Read register #22
			if ( (err = Tcm_ActOnRegister(c->fem, TCM_REG_READ, param[0], 0xFFFFFFFF, &val_rdi)) < 0)
			{
				sprintf(rep, "%d Tcm(%02d): Tcm_ActOnRegister %d failed\n", err, c->fem->card_id, param[0]);
			}
			else
			{
				sprintf(rep, "0 Tcm(%02d) Reg(%d) = 0x%x (DC_Bal_Encoding=%d)\n", c->fem->card_id, param[0], val_rdi, R22_GET_DCBAL_ENC(val_rdi));
			}
		}
		c->do_reply = 1;
	}

	// Command to read/set DC balanced decoding
	else if (strncmp(c->cmd, "dcbal_dec", 9) == 0)
	{
		c->rx_cmd_cnt++;
		// Act on Register #22
		param[0] = 22;

		if (sscanf(c->cmd, "dcbal_dec %x", &param[1]) == 1)
		{
			param[1] = R22_PUT_DCBAL_DEC(0, param[1]);
			if ( (err = Tcm_ActOnRegister(c->fem, TCM_REG_WRITE, param[0], R22_DCBAL_DEC, &param[1])) < 0)
			{
				sprintf(rep, "%d Tcm(%02d): Tcm_ActOnRegister %d failed\n", err, c->fem->card_id, param[0]);
			}
			else
			{
				sprintf(rep, "0 Tcm(%02d) Reg(%d) <- 0x%x\n", c->fem->card_id, param[0], param[1]);
			}
		}
		else
		{
			// Read register #22
			if ( (err = Tcm_ActOnRegister(c->fem, TCM_REG_READ, param[0], 0xFFFFFFFF, &val_rdi)) < 0)
			{
				sprintf(rep, "%d Tcm(%02d): Tcm_ActOnRegister %d failed\n", err, c->fem->card_id, param[0]);
			}
			else
			{
				sprintf(rep, "0 Tcm(%02d) Reg(%d) = 0x%x (DC_Bal_Decoding=%d)\n", c->fem->card_id, param[0], val_rdi, R22_GET_DCBAL_DEC(val_rdi));
			}
		}
		c->do_reply = 1;
	}

	// Command to show/set state of BUSY signal adapter
	else if (strncmp(c->cmd, "do_end_of_busy", 14) == 0)
	{
		c->rx_cmd_cnt++;
		if (sscanf(c->cmd, "do_end_of_busy %i", &param[0]) == 1)
		{
			param[0] = R22_PUT_DO_END_OF_BUSY(0, param[0]);
			if ( (err = Tcm_ActOnRegister(c->fem, TCM_REG_WRITE, 22, R22_DO_END_OF_BUSY, &param[0])) < 0)
			{
				sprintf(rep, "%d Tcm(%d): Tcm_ActOnRegister failed\n", err, c->fem->card_id);
			}
			else
			{
				sprintf(rep, "0 Tcm(%d) Reg(%d) <- 0x%x\n", c->fem->card_id, 22, param[0]);
			}
		}
		else
		{
			if ( (err = Tcm_ActOnRegister(c->fem, TCM_REG_READ, 22, 0xFFFFFFFF, &val_rdi)) < 0)
			{
				sprintf(rep, "%d Tcm(%d): Tcm_ActOnRegister failed\n", err, c->fem->card_id);
			}
			else
			{
				sprintf(rep, "0 Tcm(%d) Reg(%d) = 0x%x (%d) Do_End_Of_Busy: %d\n",
					c->fem->card_id,
					22,
					val_rdi,
					val_rdi,
					R22_GET_DO_END_OF_BUSY(val_rdi));
			}
		}
		c->do_reply = 1;
	}

	// Command to set/show the maximum event readout latency until a timeout error occurs
	else if (strncmp(c->cmd, "max_readout_time", 16) == 0)
	{
		c->rx_cmd_cnt++;
		if (sscanf(c->cmd, "max_readout_time %i", &param[0]) == 1)
		{
			param[0] = R22_PUT_MAX_READOUT_TIME(0, param[0]);
			if ( (err = Tcm_ActOnRegister(c->fem, TCM_REG_WRITE, 22, R22_MAX_READOUT_TIME_M, &param[0])) < 0)
			{
				sprintf(rep, "%d Tcm(%d): Tcm_ActOnRegister failed\n", err, c->fem->card_id);
			}
			else
			{
				sprintf(rep, "0 Tcm(%d) Reg(%d) <- 0x%x\n", c->fem->card_id, 22, param[0]);
			}
		}
		else
		{
			if ( (err = Tcm_ActOnRegister(c->fem, TCM_REG_READ, 22, 0xFFFFFFFF, &val_rdi)) < 0)
			{
				sprintf(rep, "%d Tcm(%d): Tcm_ActOnRegister failed\n", err, c->fem->card_id);
			}
			else
			{
				sprintf(rep, "0 Tcm(%d) Reg(%d) = 0x%x (%d) Max_Event_Readout_Time: %d (seconds)\n",
					c->fem->card_id,
					22,
					val_rdi,
					val_rdi,
					(R22_GET_MAX_READOUT_TIME(val_rdi) + 1) );
			}
		}
		c->do_reply = 1;
	}

	// Command to set/show enabled triggers
	else if (strncmp(c->cmd, "trig_ena", 8) == 0)
	{
		c->rx_cmd_cnt++;
		if (sscanf(c->cmd, "trig_ena %i", &param[0]) == 1)
		{
			param[0] = R22_PUT_TRIG_ENABLE(0, param[0]);
			if ( (err = Tcm_ActOnRegister(c->fem, TCM_REG_WRITE, 22, R22_TRIG_ENABLE_M, &param[0])) < 0)
			{
				sprintf(rep, "%d Tcm(%d): Tcm_ActOnRegister failed\n", err, c->fem->card_id);
			}
			else
			{
				sprintf(rep, "0 Tcm(%d) Reg(%d) <- 0x%x\n", c->fem->card_id, 22, param[0]);
			}
		}
		else
		{
			if ( (err = Tcm_ActOnRegister(c->fem, TCM_REG_READ, 22, 0xFFFFFFFF, &val_rdi)) < 0)
			{
				sprintf(rep, "%d Tcm(%d): Tcm_ActOnRegister failed\n", err, c->fem->card_id);
			}
			else
			{
				if ((val_rdi & R22_TRIG_ENABLE_M) == 0)
				{
					sprintf(&str[0], "none");
				}
				else
				{
					str[0] = '\0';
					if (val_rdi & R22_AUTO_TRIG_ENABLE)
					{
						strcat(&str[0], "AUTO ");
					}
					if (val_rdi & R22_NIM_TRIG_ENABLE)
					{
						strcat(&str[0], "NIM ");
					}
					if (val_rdi & R22_LVDS_TRIG_ENABLE)
					{
						strcat(&str[0], "LVDS ");
					}
					if (val_rdi & R22_TTL_TRIG_ENABLE)
					{
						strcat(&str[0], "TTL ");
					}
				}
				sprintf(rep, "0 Tcm(%d) Reg(%d) = 0x%x (%d) Trig_Enable: 0x%x %s\n",
					c->fem->card_id,
					22,
					val_rdi,
					val_rdi,
					R22_GET_TRIG_ENABLE(val_rdi),
					&str[0]);
			}
		}
		c->do_reply = 1;
	}

	// Command to set/show multiplicity trigger enable flag
	else if (strncmp(c->cmd, "mult_trig_ena", 13) == 0)
	{
		c->rx_cmd_cnt++;
		if (sscanf(c->cmd, "mult_trig_ena %i", &param[0]) == 1)
		{
			param[0] = R22_PUT_MULT_TRIG_ENA(0, param[0]);
			if ( (err = Tcm_ActOnRegister(c->fem, TCM_REG_WRITE, 22, R22_MULT_TRIG_ENA, &param[0])) < 0)
			{
				sprintf(rep, "%d Tcm(%d): Tcm_ActOnRegister failed\n", err, c->fem->card_id);
			}
			else
			{
				sprintf(rep, "0 Tcm(%d) Reg(%d) <- 0x%x\n", c->fem->card_id, 22, param[0]);
			}
		}
		else
		{
			if ( (err = Tcm_ActOnRegister(c->fem, TCM_REG_READ, 22, 0xFFFFFFFF, &val_rdi)) < 0)
			{
				sprintf(rep, "%d Tcm(%d): Tcm_ActOnRegister failed\n", err, c->fem->card_id);
			}
			else
			{
				sprintf(rep, "0 Tcm(%d) Reg(%d) = 0x%x (%d) Multiplicity_Trigger_Enable: %d\n",
					c->fem->card_id,
					22,
					val_rdi,
					val_rdi,
					R22_GET_MULT_TRIG_ENA(val_rdi));
			}
		}
		c->do_reply = 1;
	}

	// Command to set/show multiplicity trigger destination flag
	else if (strncmp(c->cmd, "mult_trig_dst", 13) == 0)
	{
		c->rx_cmd_cnt++;
		if (sscanf(c->cmd, "mult_trig_dst %i", &param[0]) == 1)
		{
			param[0] = R22_PUT_MULT_TRIG_DST(0, param[0]);
			if ( (err = Tcm_ActOnRegister(c->fem, TCM_REG_WRITE, 22, R22_MULT_TRIG_DST, &param[0])) < 0)
			{
				sprintf(rep, "%d Tcm(%d): Tcm_ActOnRegister failed\n", err, c->fem->card_id);
			}
			else
			{
				sprintf(rep, "0 Tcm(%d) Reg(%d) <- 0x%x\n", c->fem->card_id, 22, param[0]);
			}
		}
		else
		{
			if ( (err = Tcm_ActOnRegister(c->fem, TCM_REG_READ, 22, 0xFFFFFFFF, &val_rdi)) < 0)
			{
				sprintf(rep, "%d Tcm(%d): Tcm_ActOnRegister failed\n", err, c->fem->card_id);
			}
			else
			{
				sprintf(rep, "0 Tcm(%d) Reg(%d) = 0x%x (%d) Multiplicity_Trigger_Destination: (%d) %s\n",
					c->fem->card_id,
					22,
					val_rdi,
					val_rdi,
					R22_GET_MULT_TRIG_DST(val_rdi),
					&MultTrigDst2str[R22_GET_MULT_TRIG_DST(val_rdi)][0]);
			}
		}
		c->do_reply = 1;
	}

	// Command to set/show multiplicity trigger lower limit threshold
	else if (strncmp(c->cmd, "mult_more_than", 14) == 0)
	{
		c->rx_cmd_cnt++;
		if (sscanf(c->cmd, "mult_more_than %i", &param[0]) == 1)
		{
			param[0] = R29_PUT_MULT_MORE_THAN(0, param[0]);
			if ( (err = Tcm_ActOnRegister(c->fem, TCM_REG_WRITE, 29, R29_MULT_MORE_THAN_M, &param[0])) < 0)
			{
				sprintf(rep, "%d Tcm(%d): Tcm_ActOnRegister failed\n", err, c->fem->card_id);
			}
			else
			{
				sprintf(rep, "0 Tcm(%d) Reg(%d) <- 0x%x\n", c->fem->card_id, 29, param[0]);
			}
		}
		else
		{
			if ( (err = Tcm_ActOnRegister(c->fem, TCM_REG_READ, 29, 0xFFFFFFFF, &val_rdi)) < 0)
			{
				sprintf(rep, "%d Tcm(%d): Tcm_ActOnRegister failed\n", err, c->fem->card_id);
			}
			else
			{
				sprintf(rep, "0 Tcm(%d) Reg(%d) = 0x%x (%d) Multiplicity_More_Than: %d\n",
					c->fem->card_id,
					29,
					val_rdi,
					val_rdi,
					R29_GET_MULT_MORE_THAN(val_rdi));
			}
		}
		c->do_reply = 1;
	}

	// Command to set/show multiplicity trigger upper limit threshold
	else if (strncmp(c->cmd, "mult_less_than", 14) == 0)
	{
		c->rx_cmd_cnt++;
		if (sscanf(c->cmd, "mult_less_than %i", &param[0]) == 1)
		{
			param[0] = R29_PUT_MULT_LESS_THAN(0, param[0]);
			if ( (err = Tcm_ActOnRegister(c->fem, TCM_REG_WRITE, 29, R29_MULT_LESS_THAN_M, &param[0])) < 0)
			{
				sprintf(rep, "%d Tcm(%d): Tcm_ActOnRegister failed\n", err, c->fem->card_id);
			}
			else
			{
				sprintf(rep, "0 Tcm(%d) Reg(%d) <- 0x%x\n", c->fem->card_id, 29, param[0]);
			}
		}
		else
		{
			if ( (err = Tcm_ActOnRegister(c->fem, TCM_REG_READ, 29, 0xFFFFFFFF, &val_rdi)) < 0)
			{
				sprintf(rep, "%d Tcm(%d): Tcm_ActOnRegister failed\n", err, c->fem->card_id);
			}
			else
			{
				sprintf(rep, "0 Tcm(%d) Reg(%d) = 0x%x (%d) Multiplicity_Less_Than: %d\n",
					c->fem->card_id,
					29,
					val_rdi,
					val_rdi,
					R29_GET_MULT_LESS_THAN(val_rdi));
			}
		}
		c->do_reply = 1;
	}

	// Command to set/show bit error rate tester mode
	else if (strncmp(c->cmd, "tcm_bert", 8) == 0)
	{
		c->rx_cmd_cnt++;
		if (sscanf(c->cmd, "tcm_bert %i", &param[0]) == 1)
		{
			param[0] = R22_PUT_TCM_BERT(0, param[0]);
			if ( (err = Tcm_ActOnRegister(c->fem, TCM_REG_WRITE, 22, R22_TCM_BERT, &param[0])) < 0)
			{
				sprintf(rep, "%d Tcm(%d): Tcm_ActOnRegister failed\n", err, c->fem->card_id);
			}
			else
			{
				sprintf(rep, "0 Tcm(%d) Reg(%d) <- 0x%x\n", c->fem->card_id, 22, param[0]);
			}
		}
		else
		{
			if ( (err = Tcm_ActOnRegister(c->fem, TCM_REG_READ, 22, 0xFFFFFFFF, &val_rdi)) < 0)
			{
				sprintf(rep, "%d Tcm(%d): Tcm_ActOnRegister failed\n", err, c->fem->card_id);
			}
			else
			{
				sprintf(rep, "0 Tcm(%d) Reg(%d) = 0x%x (%d) TCM_Bit_Error_Rate_Tester: %d\n",
					c->fem->card_id,
					22,
					val_rdi,
					val_rdi,
					R22_GET_TCM_BERT(val_rdi));
			}
		}
		c->do_reply = 1;
	}

	// Command to print version, compilation date and time
	else if (strncmp(c->cmd, "version", 7) == 0)
	{
		c->rx_cmd_cnt++;
		c->do_reply = 1;
		sprintf(rep, "%d Server: Software V%d.%d Compiled %s at %s\n",
			0,
			SERVER_VERSION_MAJOR,
			SERVER_VERSION_MINOR,
			server_date,
			server_time);
	}

	// Command to print help message
	else if (strncmp(c->cmd, "help", 4) == 0)
	{
		c->rx_cmd_cnt++;
		TcmCmdi_Cmd_Usage(rep, 256);
		c->do_reply = 1;
	}

	// Command to act on command statistics
	else if (strncmp(c->cmd, "cmd", 3) == 0)
	{
		c->rx_cmd_cnt++;
		err = TcmCmdi_CmdCommands(c);
	}

	// Command to clear/get Busy (i.e. dead-time) Histogram
	else if (strncmp(c->cmd, "hbusy", 5) == 0)
	{
		c->rx_cmd_cnt++;
		if (strncmp(c->cmd, "hbusy clr", 9) == 0)
		{
			HistoInt_Clear(&busy_histogram);
			sprintf(rep, "0 Tcm(%d) cleared busy histogram\n", c->fem->card_id);
			err = 0;
		}
		else if (strncmp(c->cmd, "hbusy get", 9) == 0)
		{
			HistoInt_ComputeStatistics(&busy_histogram);
			err = Busymeter_PacketizeHisto(&busy_histogram, (void *) c->burep, POOL_BUFFER_SIZE, &(c->rep_size), c->fem->card_id);
			c->reply_is_cframe = 0;
		}
		else
		{
				err = -1;
				sprintf(rep, "%d Tcm(%d) illegal command %s\n", err, c->fem->card_id, c->cmd);
		}
		c->do_reply = 1;
	}

	// Command to clear/get Inter Event Time (i.e. event period) Histogram
	else if (strncmp(c->cmd, "hevper", 6) == 0)
	{
		c->rx_cmd_cnt++;
		if (strncmp(c->cmd, "hevper clr", 10) == 0)
		{
			HistoInt_Clear(&evper_histogram);
			sprintf(rep, "0 Tcm(%d) cleared event period histogram\n", c->fem->card_id);
			err = 0;
		}
		else if (strncmp(c->cmd, "hevper get", 10) == 0)
		{
			HistoInt_ComputeStatistics(&evper_histogram);
			err = Periodmeter_PacketizeHisto(&evper_histogram, (void *) c->burep, POOL_BUFFER_SIZE, &(c->rep_size), c->fem->card_id);
			c->reply_is_cframe = 0;
		}
		else
		{
				err = -1;
				sprintf(rep, "%d Tcm(%d) illegal command %s\n", err, c->fem->card_id, c->cmd);
		}
		c->do_reply = 1;
	}

	// Command family "flash"
	else if (strncmp(c->cmd, "mars", 4) == 0)
	{
		c->rx_cmd_cnt++;
		err = Cmdi_I2CCommands(c);
	}

	// Command family "flash"
	else if (strncmp(c->cmd, "flash", 5) == 0)
	{
		c->rx_cmd_cnt++;
		err = Cmdi_FlashCommands(c);
	}

	// Unknown command
	else
	{
		err = ERR_UNKNOWN_COMMAND;
		c->rx_cmd_cnt++;
		c->do_reply = 1;
		sprintf(rep, "%d Tcm(%d) Unknown command: %s", err, c->fem->card_id, c->cmd);
	}

	// Format frame to be a configuration reply frame
	if (c->reply_is_cframe)
	{
		sh = (short *) c->burep;
		c->rep_size = 0;

		// Put CFrame prefix
		*sh = PUT_FVERSION_FEMID(PFX_START_OF_CFRAME, CURRENT_FRAMING_VERSION, c->fem->card_id);
		sh++;
		c->rep_size+=2;

		// Put error code
		*sh = err;
		sh++;
		c->rep_size+=2;

		// Get string size (excluding string null terminating character)
		rep_ascii_len = strlen((char *) (c->burep + CFRAME_ASCII_MSG_OFFSET));
		// Include the size of the null character in Frame size
		c->rep_size = c->rep_size + rep_ascii_len + 1;

		// Put string size
		*sh = PUT_ASCII_LEN(rep_ascii_len);
		sh++;
		c->rep_size+=2;

		// If response size is odd, add one more null word and make even response size
		if (c->rep_size & 0x1)
		{
			*((char *) (c->burep + CFRAME_ASCII_MSG_OFFSET + rep_ascii_len)) = '\0';
			c->rep_size++;
		}

		// Put end of frame
		sh = (short *) (c->burep + c->rep_size);
		*sh = PFX_END_OF_FRAME;
		c->rep_size+=2;

		// Include in the size the 2 empty bytes kept at the beginning of the UDP datagram
		c->rep_size +=2;
	}

	// The response (if any) is sent to whom sent the last command

	// Increment command error count if needed
	if (err < 0)
	{
		c->err_cmd_cnt++;
	}

	return (err);
}
