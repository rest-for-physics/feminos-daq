/*******************************************************************************
                           Minos 
                           _____

 File:        hexreader.c

 Description: functions to extract data from a file in Intel HEX format.


 Author:      D. Calvet,        calvet@hep.saclay.cea.fr
              

 History:
  October 2012: adapted from T2K version. Added support for 32-bit addressing
  Note that the number of data bytes returned by HexReader_ReadBytes can be 0
  even if EOF is not reached
 
*******************************************************************************/
#include "hexreader.h"
#include <string.h>

/*******************************************************************************
 HexReader_StructClear
*******************************************************************************/
void HexReader_StructClear(HexContext *hc)
{
	int i;
	
	hc->hexf = (FILE *) 0;
	sprintf(hc->rep, "");
	sprintf(hc->cur_line, "");
	for (i=0; i<HEX_FILE_MAX_DATA_BYTES_PER_LINE; i++)
	{
		hc->cur_line_data[i]=0;
	}
	hc->cur_line_start_address  = 0;
	hc->cur_line_offset_address = 0;
	hc->cur_line_sz             = 0;
	hc->cur_line_ix             = 0;
	hc->line_count              = 0;
	hc->byte_count              = 0;
	hc->is_eof                  = 0;
}

/*******************************************************************************
 HexReader_Open
*******************************************************************************/
int HexReader_Open(HexContext *hc, char *hex_file_name)
{
	int err = 0;

	HexReader_StructClear(hc);

	// Open HEX file
	if (!(hc->hexf = fopen(hex_file_name, "r")))
	{
		err = -1;
		sprintf(hc->rep, "%d could not open file %s.\n", err, hex_file_name);
	}

	return(err);
}

/*******************************************************************************
 HexReader_Close
*******************************************************************************/
int HexReader_Close(HexContext *hc)
{
	int err;
	
	err = 0;

	// Close file
	if (hc->hexf) fclose(hc->hexf);

	return(err);
}

/*******************************************************************************
 ASCII_to_Byte
*******************************************************************************/
int ASCII_to_Byte(unsigned char *chex, char hq, char lq)
{
	// Convert the MSB quartet
	if ((hq >= '0') && (hq <= '9'))
	{
		*chex = (hq - '0') << 4;
	}
	else if ((hq >= 'A') && (hq <= 'F'))
	{
		*chex = (hq - 'A' + 10) << 4;
	}
	else if ((hq >= 'a') && (hq <= 'f'))
	{
		*chex = (hq - 'a' + 10) << 4;
	}
	else
	{
		return (-1);
	}

	// Convert the LSB quartet
	if ((lq >= '0') && (lq <= '9'))
	{
		*chex |= (lq - '0');
	}
	else if ((lq >= 'A') && (lq <= 'F'))
	{
		*chex |= (lq - 'A' + 10);
	}
	else if ((lq >= 'a') && (lq <= 'f'))
	{
		*chex |= (lq - 'a' + 10);
	}
	else
	{
		return (-1);
	}

	return (0);
}

/*******************************************************************************
 HexReader_ParseLine
*******************************************************************************/
int HexReader_ParseLine(HexContext *hc)
{
	int err;
	unsigned int exp_line_len;
	unsigned char i;
	unsigned char adrh;
	unsigned char adrl;
	unsigned char rec_type;
	unsigned char exp_chksum;
	unsigned char cur_line_chksum = 0;
	
	// Check that the line starts with a ':'
	if (hc->cur_line[0] != ':')
	{
		err = -1;
		sprintf(hc->rep, "%d unexpected character line %d column 1 (must be ':').\n", err, hc->line_count);	
		return (err);
	}

	// Get byte count
	if ((err = ASCII_to_Byte(&(hc->cur_line_sz), hc->cur_line[1], hc->cur_line[2])) < 0)
	{
		sprintf(hc->rep, "%d ASCII to hexadecimal conversion failed at line %d.\n", err, hc->line_count);	
		return (err);
	}
	// Update checksum
	cur_line_chksum = cur_line_chksum + hc->cur_line_sz;

	// Check byte count value
	if (hc->cur_line_sz > 32)
	{
		err = -1;
		sprintf(hc->rep, "%d byte count (%d) exceeded at line %d (maximum supported is 32).\n", err, hc->cur_line_sz, hc->line_count);	
		return (err);
	}

	// Check size of the string of the current line
	exp_line_len = 1 + 2 + 4 + 2 + (2 * hc->cur_line_sz) + 2 + 1;
	if (strlen(hc->cur_line) != exp_line_len)
	{
		err = -1;
		sprintf(hc->rep, "%d line %d length is %d while %d was expected.\n", err, hc->line_count, strlen(hc->cur_line), exp_line_len);
		return (err);
	}

	// Get address
	if ((err = ASCII_to_Byte(&adrh, hc->cur_line[3], hc->cur_line[4])) < 0)
	{
		sprintf(hc->rep, "%d ASCII to hexadecimal conversion failed at line %d.\n", err, hc->line_count);	
		return (err);
	}
	if ((err = ASCII_to_Byte(&adrl, hc->cur_line[5], hc->cur_line[6])) < 0)
	{
		sprintf(hc->rep, "%d ASCII to hexadecimal conversion failed at line %d.\n", err, hc->line_count);	
		return (err);
	}
	hc->cur_line_start_address = (adrh << 8) | adrl;

	// Update checksum
	cur_line_chksum = cur_line_chksum + adrh;
	// Update checksum
	cur_line_chksum = cur_line_chksum + adrl;

	// Get record type
	if ((err = ASCII_to_Byte(&rec_type, hc->cur_line[7], hc->cur_line[8])) < 0)
	{
		sprintf(hc->rep, "%d ASCII to hexadecimal conversion failed at line %d.\n", err, hc->line_count);	
		return (err);
	}
	// Update checksum
	cur_line_chksum = cur_line_chksum + rec_type;

	// Get data bytes
	for (i=0; i< hc->cur_line_sz; i++)
	{
		if ((err = ASCII_to_Byte(&(hc->cur_line_data[i]), hc->cur_line[(2*i)+9], hc->cur_line[(2*i)+10])) < 0)
		{
			sprintf(hc->rep, "%d ASCII to hexadecimal conversion failed at line %d.\n", err, hc->line_count);	
			return (err);
		}
		// Update checksum
		cur_line_chksum = cur_line_chksum + hc->cur_line_data[i];
	}

	// Get checksum
	if ((err = ASCII_to_Byte(&exp_chksum, hc->cur_line[(2*hc->cur_line_sz)+9], hc->cur_line[(2*hc->cur_line_sz)+10])) < 0)
	{
		sprintf(hc->rep, "%d ASCII to hexadecimal conversion failed at line %d.\n", err, hc->line_count);	
		return (err);
	}
	// Update checksum
	cur_line_chksum = ~cur_line_chksum + 1; 

	// Verify checksum
	if (cur_line_chksum != exp_chksum)
	{
		err = -1;
		sprintf(hc->rep, "%d checksum mismatch: computed=0x%x expected=0x%x.\n", err, cur_line_chksum, exp_chksum);	
		return (err);
	}

	// Check for EOF
	if (rec_type == HEX_FILE_REC_TYPE_EOF)
	{
		hc->is_eof = 1;
		if (hc->cur_line_sz == 0)
		{
			err = 0;
			sprintf(hc->rep, "%d EOF Record found at line %d.\n", err, hc->line_count);
			return (err);
		}
		else
		{
			err = -1;
			sprintf(hc->rep, "%d EOF Record found at line %d has non-null value (0x%x) in byte count field.\n", err, hc->line_count, hc->cur_line_sz);
			return (err);
		}
	}
	else if (rec_type == HEX_FILE_REC_TYPE_DATA)
	{
		err = 0;
	}
	// If it is an extended address offset record the data is the offset address
	else if (rec_type == HEX_FILE_REC_TYPE_ELAR)
	{
		hc->cur_line_offset_address = (hc->cur_line_data[0] <<8) + hc->cur_line_data[1];
		hc->cur_line_sz = 0;
		//printf ("cur_line_offset_address: 0x%x\n", hc->cur_line_offset_address);
		err = 0;
	}
	else
	{
		err = -1;
		sprintf(hc->rep, "%d unsupported value of Record Type (0x%x) at line %d.\n", err, rec_type, hc->line_count);
		return (err);
	}

	return(err);
}

/*******************************************************************************
 HexReader_ReadBytes
*******************************************************************************/
int HexReader_ReadBytes(HexContext *hc, int cnt, unsigned int *adr, unsigned char *data)
{
	int i;
	int byte_ret;

	// Check if no data are available from local buffer
	if (hc->cur_line_ix == hc->cur_line_sz)
	{
		// Get one line from HEX file
		if (fgets(&(hc->cur_line[0]), HEX_FILE_MAX_LINE_SIZE, hc->hexf) == 0)
		{
			byte_ret = -1;
			sprintf(hc->rep, "%d unexpected end of file.\n", byte_ret);
			return(byte_ret);
		}
		else
		{
			hc->line_count++;
			
			// Parse the line that was read
			if ((byte_ret = HexReader_ParseLine(hc)) < 0)
			{
				return (byte_ret);
			}

			hc->cur_line_ix = 0;
		}
	}

	// If EOF was reached
	if (hc->is_eof)
	{
		return (0);
	}
	else
	{
		byte_ret = 0;
		
		// Set the target address
		*adr = (hc->cur_line_offset_address << 16) + hc->cur_line_start_address + hc->cur_line_ix;

		// Get bytes up to desired number reached or no more bytes from the current line
		for (i=0; i<cnt; i++)
		{
			if (hc->cur_line_ix == hc->cur_line_sz)
			{
				break;
			}
			*(data+i) = hc->cur_line_data[hc->cur_line_ix];
			hc->cur_line_ix++;
			byte_ret++;
		}
	}
	return (byte_ret);
}

