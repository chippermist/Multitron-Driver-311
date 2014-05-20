/*
 * Source file for the MultiTron driver.  This is where you should implement the
 * user-facing API (mtron_*) for the MultiTron, using the tronctl() hardware
 * interface to communicate with the device.
 */
#include <stdio.h>
#include <string.h>

#include "mtron.h"
#include "driver.h"

///////////////////////////////////////////////////////////////////////////////////////////////////////
//
//Usage: Sets the field of the bytes accordingly to the shifts required
//Arguments: takes in the displayID, Opcode, Reserved bits and ScanLine
//
//Returns: the set position and values of the fields
//
///////////////////////////////////////////////////////////////////////////////////////////////////////

uint32_t setFields(int d_id, int opcode, int res, int scan_line) 
{
	//returns the bits after settins the values passed in the arguments
	return ( d_id | (opcode << 7) | (res << 12) | (scan_line << 25) );
}

///////////////////////////////////////////////////////////////////////////////////////////////////////
//
//Usage: Used to calculate the display ID and change and store the values in the same address as scaled x and scaled y used in the functions
//
//Arguments: the struct elements, x coordinarte, y coordinate, *scaled_x, *scaled_y
//
//Returns: the calculated displayId from the x and y coordinate
//
///////////////////////////////////////////////////////////////////////////////////////////////////////

int getDisplayID(struct multitron *mtron, uint32_t x, uint32_t y, uint32_t *scaled_y, uint32_t *scaled_x)
{
	//scales the y coordinate according to the height
	*scaled_y = y%(SUBDISPLAY_HEIGHT);
	
	//scales the x coordinate according to the width
	*scaled_x = x%(SUBDISPLAY_WIDTH);

	int row = y/SUBDISPLAY_HEIGHT;	//calculates the number of row in which the pixel exists
	int col = x/SUBDISPLAY_WIDTH;	//calculates the number of col in which the pixel exists

	//calc the height and width
	int height = (mtron->rows)*SUBDISPLAY_HEIGHT;
	int width = (mtron->cols)*SUBDISPLAY_WIDTH;

	//checks if the things are in bounds and if not then returns -1 
	if((y >= height)|| (x >=width) || (y < 0) || (x< 0))
	{
		return -1;
	}

	//returns the number of the display in the number of displays
	return (((row)*(mtron->cols))+(col));
}

///////////////////////////////////////////////////////////////////////////////////////////////////////
//
//Usage: Initialize the display
//
//Arguments: takes in the struct pointer
//
//Returns: initializes screen and stores the number of rows and cols into the struct buffer
//
///////////////////////////////////////////////////////////////////////////////////////////////////////

int mtron_init(struct multitron *mtron)
{
	char buffer[2];

	//sets the displayID and scanline to 0
	//reserve bits to 0 
	//sets opcode to POWERON mode
	uint32_t op = setFields(0,MTRON_POWERON,0,0);
	
	//passes the new opcode to the function
	tronctl(op,buffer);
	
	//sets the row and the col from the buffer
	mtron->cols = (int)buffer[0];
	mtron->rows = (int)buffer[1];

//	printf("%d, %d", mtron->rows, mtron->cols);
	return 0;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////
//
//Usage: destroys the screens
//
//Arguments: takes in the struct pointer
//
//Returns: quits and destroys the screen and sets the reserved bits to 0.
//
///////////////////////////////////////////////////////////////////////////////////////////////////////

void mtron_destroy(struct multitron *mtron)
{
	//sets the reserved bits to POWEROFF and reserved to 0
	uint32_t op = setFields(0,MTRON_POWEROFF,0,0);
	
	//passes into the funtion
	tronctl(op,NULL);
}

///////////////////////////////////////////////////////////////////////////////////////////////////////
//
//Usage:retrives the pixel color at certain location
//
//Arguments:struct, x coordinate, y coordinate, color pointer
//
//Returns:retrives the color at certain pixel and saves it into the color pointer
//
///////////////////////////////////////////////////////////////////////////////////////////////////////

int mtron_getpixel(struct multitron *mtron, uint32_t x, uint32_t y,
		uint8_t *color)
{
	char scanline[256];	//scanline buffer

	int scaled_y = 0;

	int scaled_x = 0;

	//retrives the display ID of the pixel location
	int DisplayID = getDisplayID(mtron,x,y,&scaled_y,&scaled_x);

	//checks if the display id is valid
	if(DisplayID == -1)
        {
	        return -1;
	}

	//sets the opcode to read_line according to the displayID and the scaled_y pixel
	uint32_t op = setFields(DisplayID, MTRON_READ_LINE, 0 ,scaled_y );

	//sends into the function
	tronctl(op,scanline);
	
	//memcpy from the string.h library
	memcpy(color, &scanline[scaled_x],1);

	return 0;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////
//
//Usage:gets the information about the pixel and then writes the certain data to the coordinate
//
//Arguments:struct pointer, x coordinate, y coordinate, color
//
//Returns:writes the data at certain pixel location
//
///////////////////////////////////////////////////////////////////////////////////////////////////////

int mtron_putpixel(struct multitron *mtron, uint32_t x, uint32_t y,
		uint8_t color)
{
	char scanline[256];

	int scaled_y = 0;

	int scaled_x = 0;

	//calculates the displayID using the function
	int DisplayID = getDisplayID(mtron,x,y,&scaled_y,&scaled_x);

	//if the displayID is not valid returns -1
	if(DisplayID == -1)
	{
		return -1;
	}

	//reads 256 bytes using the MTRON_READ_LINE function of the correct scaled y 
	//and the display ID
	uint32_t op = setFields(DisplayID, MTRON_READ_LINE, 0 ,scaled_y );

	//a simple step check for debugging if there is any error
	if(tronctl(op,scanline) != 0)
	{
		printf("error 1");
	}
		
	//copies the pixel data to certain location
	memcpy(&scanline[scaled_x],&color,1);
	
//	printf("%d, scan = %d", color, (int)scanline[col]);

	//writes the data on the certain displayID and scaled y
	op = setFields(DisplayID, MTRON_WRITE_LINE, 0, scaled_y);
	
	//step check 2
	if(tronctl(op,scanline) != 0)
	{
		printf("error 2");
	}

	return 0;
}

///////////////////////////////////////////////////////////////////////////////////////
//
//Usage: Retrives the value of the pixels in the given rectangular area
//
//Arguments: struct mtron, x coordinate, y coordinate, width, height, and the data pointer
//
//Returns: Returns 0 if successful. Saves the values of the pixels into the data buffer
//
///////////////////////////////////////////////////////////////////////////////////////

int mtron_getrect(struct multitron *mtron, uint32_t x, uint32_t y,
		uint32_t w, uint32_t h, uint8_t *data)
{
	char scanline[256];
	int bytes_to_read =0;
	int row =0, col=0;
	int scaled_x =0, scaled_y =0;
	uint32_t op =0;
	int i=0,j=0;
	int displayID=0, displayID_end=0;
	int read_data =0;

	int start_pos = 0;
	int end_pos = 0;

	//bound check if the height and width of the rectangle
	//if the rectangle extends outside the screen scope it returns -1
	if(((x+w) > ((mtron->cols)*SUBDISPLAY_WIDTH)) || ((y+h) > ((mtron->rows)*SUBDISPLAY_HEIGHT)))
	{
			return -1;
	}

	//for loop for storing pixel data into the data buffer
	//goes through every horizontal line in the rectangle
	for(i = y; i< (y+h) ;i++)
	{
		//calculates the displayID for the first (x,y) coordinate
		displayID = getDisplayID(mtron,x,i,&scaled_y,&scaled_x);
		
		//calculates the display ID for the (x+w,y) coordinate where y is dynamic according to the size
		//of the rectangle
		displayID_end = getDisplayID(mtron,(x+w - 1),i,&row,&col);
		
		//printf("%d to %d \n",displayID, displayID_end);
		
		//bound check for displayID
		if((displayID < 0) || (displayID_end < 0))
		{
			return -1;
		}



		//for loop which changes the displayID upto the (x+w,y+h) of the rectangle
		for(j = displayID; j<= displayID_end; j++)
		{
			//passes the opcode for READ_LINE to the multi-tron
			op = setFields(j, MTRON_READ_LINE,0, i % SUBDISPLAY_HEIGHT);
			//passes argument to the call
			tronctl(op,scanline);

			//if the rectangle is on the same screen
			if(displayID == displayID_end) 
			{
				start_pos = x % SUBDISPLAY_WIDTH;
				end_pos = (x+w-1) % SUBDISPLAY_WIDTH;
			}
			//if the offset is on a different display
			else if( j == displayID ) 
			{
			
				start_pos = x % SUBDISPLAY_WIDTH;
				end_pos = 255;
			}
			//if the offset is at the last display
			else if( j == displayID_end) 
			{
			
				start_pos = 0;
				end_pos = (x+w-1) % SUBDISPLAY_WIDTH;
			}
			//if the offset is none of the above
			else 
			{
				
				start_pos = 0;
				end_pos = 255;
			}

			
			//calculates and maintains a record of amount of bytes to read into the buffer
			bytes_to_read = end_pos - start_pos + 1;
			//printf("\ndisplay:%d, height: %d, start: %d, end = %d, bytes written = %d\n",j,i, start_pos, end_pos, bytes_to_read);


			//copies the pixel data from the scanline to the data buffer
			memcpy(&data[read_data], &scanline[start_pos], bytes_to_read);

			//maintains a record of how much data is read
			read_data += bytes_to_read;

			//passes arguments to call
			tronctl(op,scanline);
			
		}
	}


	return 0;
}

///////////////////////////////////////////////////////////////////////////////////////
//
//Usage: Retrives the value of the pixels in the given rectangular area and 
//draws the given pixels into the rectangle specified
//
//Arguments: struct mtron, x coordinate, y coordinate, width, height, and the data pointer
//
//Returns: Returns 0 if successful. Saves the values of the pixels into the rectangle
//
///////////////////////////////////////////////////////////////////////////////////////

int mtron_putrect(struct multitron *mtron, uint32_t x, uint32_t y,
		uint32_t w, uint32_t h, uint8_t *data)
{
	char scanline[256];
	int row =0, col=0;
	int scaled_x =0, scaled_y =0;
	uint32_t op =0;
	int i=0,j=0;
	int displayID=0, displayID_end=0;
	int written_data =0, bytes_to_write =0;

	int start_pos = 0;
	int end_pos = 0;


	//bound check if the height and width of the rectangle
	//if the rectangle extends outside the screen scope it returns -1
	if(((x+w) > ((mtron->cols)*SUBDISPLAY_WIDTH)) || ((y+h) > ((mtron->rows)*SUBDISPLAY_HEIGHT)))
	{
			return -1;
	}


	//for loop for storing pixel data into the data buffer
	//goes through every horizontal line in the rectangle
	for(i = y; i< (y+h) ;i++)
	{
		//calculates the displayID for the first (x,y) coordinate
		displayID = getDisplayID(mtron,x,i,&scaled_y,&scaled_x);
		
		//calculates the displayID for the first (x+w,y) coordinate where y is dynamic according to the size
		displayID_end = getDisplayID(mtron,(x+w - 1),i,&row,&col);
		
		//printf("%d to %d \n",displayID, displayID_end);
		

		//bound check for displayID
		if((displayID < 0) || (displayID_end < 0))
		{
			return -1;
		}



		//for loop which changes the displayID upto the (x+w,y+h) of the rectangle
		for(j = displayID; j<= displayID_end; j++)
		{
			//passes the opcode for READ_LINE to the multi-tron
			op = setFields(j, MTRON_READ_LINE,0, i % SUBDISPLAY_HEIGHT);
			//passes argument to the call
			tronctl(op,scanline);

			//if the rectangle is on the same screen
			if(displayID == displayID_end) 
			{
				start_pos = x % SUBDISPLAY_WIDTH;
				end_pos = (x+w-1) % SUBDISPLAY_WIDTH;
			}
			//if the offset is on a different display
			else if( j == displayID ) 
			{
			
				start_pos = x % SUBDISPLAY_WIDTH;
				end_pos = 255;
			}
			//if the offset is at the last display
			else if( j == displayID_end) 
			{
			
				start_pos = 0;
				end_pos = (x+w-1) % SUBDISPLAY_WIDTH;
			}
			//if the offset is none of the above
			else 
			{
				start_pos = 0;
				end_pos = 255;
			}

			//maintains the amont of data written
			//it can be a max of 256 so end - start +1
			bytes_to_write = end_pos - start_pos + 1;
			//printf("\ndisplay:%d, height: %d, start: %d, end = %d, bytes written = %d\n",j,i, start_pos, end_pos, bytes_to_write);

			//memory copies from data to the scanline
			memcpy(&scanline[start_pos], &data[written_data], bytes_to_write);

			//maintaints how much data is already written
			written_data += bytes_to_write; 

			//passes and sets the arguments to the fields and writes it in the rectangle on appropriate pixels
			op = setFields(j, MTRON_WRITE_LINE, 0, i % SUBDISPLAY_HEIGHT);
			//passes arguments to the call
			tronctl(op,scanline);
			
		}
	}
	return 0;
}
