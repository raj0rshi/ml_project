#define F_CPU 16000000L		// run CPU at 16 MHz. Must be defined before including <delay.h>
#define BAUD 9600			// Use same setting for Serial/COM port

#define tskKEYPAD_PRIORITY 1
#define tskBLINKLED_PRIORITY 1
#define tskKEYPAD_STACK_SIZE (configMINIMAL_STACK_SIZE * 6)
#define tskLED_STACK_SIZE configMINIMAL_STACK_SIZE

#define MAX_LCD_LINE_CHARS 16	// Characters per line on LCD1602 module

// ---------------------------------------------------------------------------
// INCLUDES
#include "FreeRTOS.h"		// RTOS kernel
#include "task.h"			// Multitasking function definitions,

#include <avr/io.h>			// define MCU port registers
#include <util/delay.h>		// _delay_ms()
#include <stdio.h>			// snprintf()
#include <util/setbaud.h>	// definition of BAUD
#include <stdbool.h>		// definition of type bool
#include <projdefs.h>		// pdPASS
#include <stdlib.h>

// ---------------------------------------------------------------------------
// TYPEDEFS
typedef uint8_t byte;		// I just like byte better

// ---------------------------------------------------------------------------
// TASK HANDLES
TaskHandle_t xRedLED = NULL;		// Blink red LED task handle
TaskHandle_t xWhiteLED = NULL;		// Blink white LED task handle
TaskHandle_t xBlueLED = NULL;		// Blink red LED task handle
TaskHandle_t xGreenLED = NULL;		// Blink green LED task handle
TaskHandle_t xKeypadScan = NULL;	// Monitor keypad task handle
TaskHandle_t xMalProg = NULL;
TaskHandle_t xMonitor = NULL;

// ---------------------------------------------------------------------------
// GLOBAL VARIABLES

// ---------------------------------------------------------------------------
// MACROS
#define ClearBit(x,y) x &= ~_BV(y)	// equivalent to cbi(x,y)
#define SetBit(x,y) x |= _BV(y)		// equivalent to sbi(x,y)

// ---------------------------------------------------------------------------
// INITIALIZE KEYPAD AND LCD PORTS
/**************************************************************************************************
* PortB and PortD (digital pins) were chosen for the keypad row and column communications
* PortC (analog pins) was chosen for data communications with the LCD.
*
* Digital or analog pins could have been used for either device. I chose to set it up this way
* for simplicity when building the circuit and making the code more readable
*************************************************************************************************/
void SetupPorts()
{
	DDRB |= 0x3F;	// xx11 xxxx; set B0-B5 as LED and Keypad (rows)outputs , Leave B6-B7 alone
	DDRC |= 0x3F;	// 0011 1111; set C0-C5 as LCD outputs, leave C6-C7 alone
	DDRD |= 0xC0;	// 11xx xxxx; set D6-D7 as LED outputs, Leave D0-D5 alone
	DDRD &= 0xC3;   // xx00 00xx; set D2-D5 as Keypad inputs (columns), Leave D1-D2 and D6-D7 alone
}

// Keypad row pin definitions
#define keypadDirectionRegisterR DDRB
#define keypadPortControlR PORTB
#define keypadPortValueR PINB

// Keypad column pin definitions
#define keypadDirectionRegisterC DDRD
#define keypadPortControlC PORTD
#define keypadPortValueC PIND

// The LCD1602 display module requires 6 I/O pins: 2 control lines & 4 data lines.
// The following defines specify which port pins connect to the controller:
//
// ---------------------------------------------------------------------------
// LCD PORT PINS
#define LCD_RS 0	// LCD R/S driven by analog pin 0 (PC5)
#define LCD_E 1		// LCD enable driven by analog pin 1 (PC4)
#define DAT4 2		// LCD data line 4 driven by analog pin 2 (PC3)
#define DAT5 3		// LCD data line 5 driven by analog pin 3 (PC2)
#define DAT6 4		// LCD data line 6 driven by analog pin 4 (PC1)
#define DAT7 5		// LCD data line 7 driven by analog pin 5 (PC0)
#define mem_a 1
#define mem_b 50
// ---------------------------------------------------------------------------
// ONBOARD LED PORT PIN
#define UNO_LED	5	// Onboard LED driven by digital pin 13 (PB5)

// ---------------------------------------------------------------------------
// LCD1602 MODULE COMMANDS
#define CLEARDISPLAY	0x01
#define SETCURSOR		0x80

// ---------------------------------------------------------------------------
// Keypad definitions
#define ROWS	4
#define COLUMNS	4
#define INVALID -1

// ---------------------------------------------------------------------------
// Max characters per task displayed from vTaskList - value found through trial and error
#define MAX_TASKLIST_CHARS 40

// ---------------------------------------------------------------------------
// enumerated values for terminal output strings
enum
{
	RED_TASK_CREATE = 0,
	WHITE_TASK_CREATE,
	BLUE_TASK_CREATE,
	TASK_ALREADY_RUNNING,
	TASK_NOT_RUNNING,
	TASK_CREATED,
	TASK_DELETED,
};



// ---------------------------------------------------------------------------
// MISC ROUTINES

/***********************************************************************
* Simple loop routine to create a delay without disabling interrupts
**********************************************************************/
void msDelay(int delay)
{								// put into a routine
	for (int i=0;i<delay;i++)	// to remove code inlining
	_delay_ms(1);				// at cost of timing accuracy
}

/*************************************************************
* Read data sent to LCD port
************************************************************/
void PulseEnableLine ()
{
	SetBit(PORTC,LCD_E);	// take LCD enable line high
	_delay_us(40);			// wait 40 microseconds
	ClearBit(PORTC,LCD_E);	// take LCD enable line low
}

/*************************************************************
* Set pins on LCD port
************************************************************/
void SendNibble(byte data)
{
	PORTC &= 0xC3;		// 1100 0011 = clear pins 4-7 on LCD port
	if (data & _BV(4)) SetBit(PORTC,DAT4);
	if (data & _BV(5)) SetBit(PORTC,DAT5);
	if (data & _BV(6)) SetBit(PORTC,DAT6);
	if (data & _BV(7)) SetBit(PORTC,DAT7);
	PulseEnableLine();	// clock 4 bits into controller
}

/*************************************************************
* Send byte to LCD port four bits at a time.
************************************************************/
void SendByte (byte data)
{
	SendNibble(data);		// send upper 4 bits
	SendNibble(data<<4);	// send lower 4 bits
}


// ---------------------------------------------------------------------------
// UARTC ROUTINES
void uart_init(void) {
	UBRR0H = UBRRH_VALUE;
	UBRR0L = UBRRL_VALUE;
	UCSR0C = _BV(UCSZ01) | _BV(UCSZ00); /* 8-bit data */
	UCSR0B = _BV(RXEN0) | _BV(TXEN0);   /* Enable RX and TX */
}
void delayLong()
{
	unsigned int delayvar;
	delayvar = 0;
	while (delayvar <=  65500U)
	{
		asm("nop");
		delayvar++;
	}
}


unsigned char serialCheckRxComplete(void)
{
	return( UCSR0A & _BV(RXC0)) ;		// nonzero if serial data is available to read.
}

unsigned char serialCheckTxReady(void)
{
	return( UCSR0A & _BV(UDRE0) ) ;		// nonzero if transmit register is ready to receive new data.
}

unsigned char serialRead(void)
{
	while (serialCheckRxComplete() == 0)		// While data is NOT available to read
	{;;}
	return UDR0;
}

void serialWrite(unsigned char DataOut)
{
	while (serialCheckTxReady() == 0)		// while NOT ready to transmit
	{;;}
	UDR0 = DataOut;
}

/*

void establishContact() {
while (serialCheckRxComplete() == 0) {
serialWrite('A');
//	serialWrite(65U);
delayLong();
delayLong();
delayLong();
delayLong();
delayLong();
delayLong();
delayLong();
}
}
*/



// ---------------------------------------------------------------------------
// LCD1602 DRIVER ROUTINES
/*************************************************************
* LCD_Cmd sends LCD controller command
* LCD_Char sends single ascii character to display
* LCD_Init initializes the LCD controller
* LCD_Clear clears the LCD display & homes cursor
* LCD_Home homes the LCD cursor
* LCD_Goto puts cursor at position (x,y)
* LCD_Line puts cursor at start of line (x)
* LCD_Message displays a string
* LCD_Hex displays a hexadecimal value - NOT USED YET
* LCD_Integer displays an integer value
*************************************************************/
void LCD_Cmd (byte cmd)
{
	ClearBit(PORTC,LCD_RS); // R/S line 0 = command data
	SendByte(cmd); // send it
}

void LCD_Char (byte ch)
{
	SetBit(PORTC,LCD_RS); // R/S line 1 = character data
	SendByte(ch); // send it
	serialWrite(ch);
}

void LCD_Init()
{
	LCD_Cmd(0x33); // initialize controller
	LCD_Cmd(0x32); // set to 4-bit input mode
	LCD_Cmd(0x28); // 2 line, 5x7 matrix
	LCD_Cmd(0x0C); // turn cursor off (0x0E to enable)
	LCD_Cmd(0x06); // cursor direction = right
	LCD_Cmd(0x01); // start with clear display
	msDelay(50); // wait for LCD to initialize
}

void LCD_Clear() // clear the LCD display
{
	LCD_Cmd(CLEARDISPLAY);
	msDelay(30); // wait for LCD to process command
}

void LCD_Home() // home LCD cursor (without clearing)
{
	LCD_Cmd(SETCURSOR);
	msDelay(30); // wait for LCD to process command
}

void LCD_Goto(byte x, byte y) // put LCD cursor on specified line
{
	byte addr = 0; // line 0 begins at addr 0x00
	switch (y)
	{
		case 1: addr = 0x40; break; // line 1 begins at addr 0x40
		case 2: addr = 0x14; break;
		case 3: addr = 0x54; break;
	}
	LCD_Cmd(SETCURSOR+addr+x); // update cursor with x,y position
}

void LCD_Line(byte row) // put cursor on specified line
{
	LCD_Goto(0,row);
	msDelay(10); // wait for LCD to process command
}

void LCD_Message(const char *text) // display string on LCD
{
	while (*text) // do until /0 character
	{
		LCD_Char(*text++); // send char & update char pointer
		//msDelay(5);
	}
	msDelay(50); // wait for LCD to finish writing
}

// ------------------------------------------------------------------------------
// TASK ROUTINES



void blinkGreenLED(void* parameter)
{
	DDRB |= (1 << PB5);		// PB5 as output
	msDelay(20);			// Give it a little time to set pin
	

	while (1)
	{
		

		srand(xTaskGetTickCount());
		int m=mem_a+rand()%mem_b;
		char * cp=pvPortMalloc(m* sizeof(char));
		
		PORTB &= ~(1 << PB5);	// Turn LED off
		msDelay(100);			// LED off for .1 seconds
		PORTB |= (1 << PB5);	// Turn LED on
		msDelay(100);		// LED off for .1 seconds
		vPortFree(cp);
	}
	

}
char * Itoa(int i) {
	char * res = pvPortMalloc(8*sizeof(int));
	sprintf(res, "%d", i);
	return res;
}


void Stats()
{

	while(1)
	{
		
		int x=xPortGetFreeHeapSize();
		LCD_Clear();
		char * xx=Itoa(x);
		LCD_Message(xx);
		vPortFree(xx);
		//msDelay(10);
		
		int y=uxTaskGetNumberOfTasks();
		LCD_Clear();
		xx=Itoa(y);
		LCD_Message(" ");
		LCD_Message(xx);
		vPortFree(xx);
		LCD_Message("\n");
		//msDelay(10);
	}

}


void MalProg()
{
	while(1)
	{
	//	int x=mem_a+rand()%mem_b;
	//	char * m=pvPortMalloc(x*sizeof(char));
		//if(m==0)
		//{
			//msDelay(500);
		//}else
		//{
			//msDelay(500);
		//}
		//
		//if((rand()%100)>50)
		//{
			//vPortFree(m);
		//}
		if((rand()%100)<=20)
		{
			TaskHandle_t xMalProg = NULL;
			xTaskCreate(MalProg, "MalProg", tskLED_STACK_SIZE, NULL, tskBLINKLED_PRIORITY, &xMalProg);
		}
		msDelay(500);
	}
	
	

	msDelay(500);
}
int uart_putchar(char c, FILE *stream) {
	if (c == '\n') {
		uart_putchar('\r', stream);
	}
	loop_until_bit_is_set(UCSR0A, UDRE0);
	UDR0 = c;
	return 0;
}

int uart_getchar(FILE *stream) {
	loop_until_bit_is_set(UCSR0A, RXC0);
	return UDR0;
}
void blinkRedLED(void* parameter)
{
	DDRD |= (1 << PB7);		// Set PD7 as output
	msDelay(20);			// Give it a little time to set pin
	while (1)
	{
		int m=mem_a+rand()%mem_b;
		char * cp=pvPortMalloc(m* sizeof(char));
		
		PORTD &= ~(1 << PB7);	// Turn LED off
		msDelay(50);			// LED off for 2 seconds
		PORTD |= (1 << PB7);	// Turn LED on
		msDelay(50);
		vPortFree(cp);			// LED On for 1/4 second
	}
}

void blinkWhiteLED(void* parameter)
{
	DDRD |= (1 << PD6);		// PD6 as output
	msDelay(20);			// Give it a little time to set pin
	while (1)
	{
		int m=mem_a+rand()%mem_b;
		char * cp=pvPortMalloc(m* sizeof(char));
		
		PORTD &= ~(1 << PD6);	// Turn LED off
		msDelay(50);			// LED off for 2 seconds
		PORTD |= (1 << PD6);	// Turn LED on
		msDelay(50);
		vPortFree(cp);			// LED On for 1/4 second
	}
}

void blinkBlueLED(void* parameter)
{
	DDRB |= (1 << PB4);		// PB4 as output
	msDelay(20);			// Give it a little time to set pin
	while (1)
	{
		
		int m=mem_a+rand()%mem_b;
		char * cp=pvPortMalloc(m* sizeof(char));
		
		PORTB &= ~(1 << PB4);	// Turn LED off
		msDelay(50);			// LED off for 2 seconds
		PORTB |= (1 << PB4);	// Turn LED on
		msDelay(50);
		vPortFree(cp);			// LED On for 1/4 second
	}
}

/************************************************************************************************
* The blinkGreenLED task is started in main(). The LED On/Off pattern was chosen to emulate
* a heartbeat to symbolize the program is healthy (just for fun). If the heartbeat stops or goes
* into tachycardia it indicates a problem has occurred. Typically this happens if the device runs
* out of heap or stack space.
*
*_______/\  ____/\  __________/\  ____/\  __________/\  ____/\  __________/\  ____/\  __________
*         \/	   \/	         \/	     \/            \/      \/	         \/      \/
*
************************************************************************************************/

/*********************************************************************************************
* The keypadScan task is started in main(). It performs some initial steps, then prompts the
* user for keypad input on the LCD and terminal. Then starts an endless loop that continually
* scans for keypad input. Additional tasks are created and deleted based on the keypad input.
*********************************************************************************************/

static const char *taskString[] =
{
	"Press 'A' to create Red LED task, '1' to delete it.",
	"Press 'B' to create White LED task, '2' to delete it.",
	"Press 'C' to create Blue LED task, '3' to delete it.",
	"task already running.",
	"task not running.",
	"task created.",
	"task deleted.",
};
void keypadScan(void* parameter)
{
	int Row = INVALID;
	int Column = INVALID;

	FILE uart_output = FDEV_SETUP_STREAM(uart_putchar, NULL, _FDEV_SETUP_WRITE);
	FILE uart_input = FDEV_SETUP_STREAM(NULL, uart_getchar, _FDEV_SETUP_READ);

	stdout = &uart_output;
	stdin  = &uart_input;
	
	LCD_Clear();
	//LCD_Message("Enter Key:");	// Write prompt to LCD

	// Prompt for user input to create and delete Red, White and Blue tasks
	puts(" ");
	puts(taskString[RED_TASK_CREATE]);
	puts(taskString[WHITE_TASK_CREATE]);
	puts(taskString[BLUE_TASK_CREATE]);
	puts(" ");
	
	const char keyVal[COLUMNS][ROWS] =
	{
		{'1', '2', '3', 'A'},
		{'4', '5', '6', 'B'},
		{'7', '8', '9', 'C'},
		{'*', '0', '#', 'D'}
	};
	
	while (1)
	{
		// Store value for column
		byte keyPressCodeC = (keypadPortValueC >> 2);

		taskENTER_CRITICAL();
		keypadDirectionRegisterC ^= (1<<2) | (1<<3) | (1<<4) | (1<<5);
		keypadDirectionRegisterR ^= (1<<0) | (1<<1) | (1<<2) | (1<<3);
		taskEXIT_CRITICAL();
		
		msDelay(20);
		
		// Store value for row
		int temp = keypadPortValueR;
		byte keyPressCodeR = temp << 4;
		
		// Add value for column and row
		byte keyPressCode = keyPressCodeC | keyPressCodeR;
		
		if (keyPressCode != 0x0F)	// A keypad press was detected
		{
			// Find the column of the key that was pressed
			Column = (((keyPressCode & 0x0F) == 0x07) ? 0 :
			((keyPressCode & 0x0F) == 0x0B) ? 1 :
			((keyPressCode & 0x0F) == 0x0D) ? 2 :
			((keyPressCode & 0x0F) == 0x0E) ? 3 : INVALID); // Defensive coding. Should never happen
			
			if (Column != INVALID)
			{
				// If the column was found, now find the row
				Row = (((keyPressCode & 0xF0) == 0x80) ? 0 :
				((keyPressCode & 0xF0) == 0x40) ? 1 :
				((keyPressCode & 0xF0) == 0x20) ? 2 :
				((keyPressCode & 0xF0) == 0x10) ? 3 : INVALID);// Defensive coding. Should never happen

				if (Row != INVALID)
				{
					bool taskCreated = false;	// Have we created a task?
					bool taskDeleted = false;	// Have we deleted a task?
					bool getList = true;		// Do we want to display info for running tasks?
					char KeyIn = '\0';			// Key input from keypad
					char KeyOut[8] = {""};		// Key value string to be displayed on the terminal
					char lcdDisplay[MAX_LCD_LINE_CHARS] = {0};
					BaseType_t err = 0;
					
					LCD_Clear();
					LCD_Home();		// Set the LCD starting point to first line, first character
					LCD_Line(1);	// Display key on the second line (0 based line numbers)
					snprintf(lcdDisplay, sizeof(lcdDisplay),"Key = %c", keyVal[Row][Column]);
					LCD_Message(lcdDisplay);
					msDelay(1000);	// Display key for a second before prompting for a new key
					LCD_Home();
					//	LCD_Message("Enter Key:");
					
					KeyIn = keyVal[Row][Column];
					snprintf(KeyOut, sizeof(KeyOut), "Key = %c", KeyIn);
					puts(KeyOut);
					
					switch (KeyIn)
					{
						case 'A':
						if( xRedLED == NULL ) // Task not already running
						{
							err = xTaskCreate(blinkRedLED, "Red", tskLED_STACK_SIZE, NULL, tskBLINKLED_PRIORITY, &xRedLED);
							if (err == pdPASS)
							{
								taskCreated = true;
							}
						}
						else
						{
							puts(taskString[TASK_ALREADY_RUNNING]);
						}
						break;

						case 'B':
						if( xWhiteLED == NULL ) // Task not already running
						{
							err = xTaskCreate(blinkWhiteLED, "White", tskLED_STACK_SIZE, NULL, tskBLINKLED_PRIORITY, &xWhiteLED);
							if (err == pdPASS)
							{
								taskCreated = true;
							}
						}
						else
						{
							puts(taskString[TASK_ALREADY_RUNNING]);
						}
						break;

						case 'C':
						if( xBlueLED == NULL ) // Task not already running
						{
							err = xTaskCreate(blinkBlueLED, "Blue", tskLED_STACK_SIZE, NULL, tskBLINKLED_PRIORITY, &xBlueLED);
							if (err == pdPASS)
							{
								taskCreated = true;
							}
						}
						else
						{
							puts(taskString[TASK_ALREADY_RUNNING]);
						}
						break;
						
						case '1':
						if( xRedLED != NULL ) // Task already running
						{
							vTaskDelete(xRedLED);
							vTaskDelay(100/portTICK_PERIOD_MS); // give idle task a chance to run to clean up deleted task.
							taskDeleted = true;
							xRedLED = NULL;
						}
						else
						{
							puts(taskString[TASK_NOT_RUNNING]);
						}
						break;

						case '2':
						if( xWhiteLED != NULL )  // Task already running
						{
							vTaskDelete(xWhiteLED);
							vTaskDelay(100/portTICK_PERIOD_MS); // give idle task a chance to run to clean up deleted task.
							taskDeleted = true;
							xWhiteLED = NULL;
						}
						else
						{
							puts(taskString[TASK_NOT_RUNNING]);
						}
						break;

						case '3':
						if( xBlueLED != NULL ) // Task already running
						{
							vTaskDelete(xBlueLED);
							vTaskDelay(100/portTICK_PERIOD_MS); // give idle task a chance to run to clean up deleted task.
							taskDeleted = true;
							xBlueLED = NULL;
						}
						else
						{
							puts(taskString[TASK_NOT_RUNNING]);
						}
						break;

						default:
						puts(" ");
						getList = false;
						break;

					}
					if (getList)
					{
						if (taskCreated)
						{
							puts(taskString[TASK_CREATED]);
							taskCreated = false;
						}
						if (taskDeleted)
						{
							puts(taskString[TASK_DELETED]);
							taskDeleted = false;
						}
						UBaseType_t numTasks = uxTaskGetNumberOfTasks();
						char szOutBuf[numTasks * MAX_TASKLIST_CHARS];			// Task List
						size_t heapSize = xPortGetFreeHeapSize();
						snprintf(szOutBuf, sizeof(szOutBuf), "Number of Tasks = %d", numTasks);
						puts(szOutBuf);
						snprintf(szOutBuf, sizeof(szOutBuf), "Free Heap = 0x%x\n", heapSize);
						puts(szOutBuf);
						vTaskList(szOutBuf);
						puts(szOutBuf);
					}
				}
			}
			Row = INVALID;
			Column = INVALID;
			keyPressCode = 0x0F; // Reset code after displaying value
		}
	}
}

int main(void)
{
	uart_init();
	

	msDelay(500);	// wait for things to settle after power cycle
	SetupPorts();	// using PortC (analog ports) for LCD interface, Ports B and D (digital) for keypad
	msDelay(50);	// wait for LCD port to initialize


	LCD_Init(); // initialize LCD controller
	LCD_Clear();
	//LCD_Message(".....");
	//LCD_Message("Let's Begin.\n");	// Display welcome message on LCD device
	msDelay(2000);					// give user time to see message

	//xTaskCreate(blinkGreenLED, "Green", tskLED_STACK_SIZE, NULL, tskBLINKLED_PRIORITY, &xGreenLED);

	//xTaskCreate(blinkRedLED, "Blue", tskLED_STACK_SIZE, NULL, tskBLINKLED_PRIORITY, &xRedLED);
	xTaskCreate(MalProg, "MalProg", tskLED_STACK_SIZE, NULL, tskBLINKLED_PRIORITY, &xMalProg);
	xTaskCreate(Stats, "Stats", tskLED_STACK_SIZE, NULL, tskBLINKLED_PRIORITY, &xMonitor);

	//xTaskCreate(blinkBlueLED, "Stats", tskLED_STACK_SIZE, NULL, tskBLINKLED_PRIORITY, &xBlueLED);

	//xTaskCreate(blinkWhiteLED, "White", tskLED_STACK_SIZE, NULL, tskBLINKLED_PRIORITY, &xWhiteLED);

	// START SCHELUDER
	vTaskStartScheduler();
	return 0;
}

// IDLE TASK
void vApplicationIdleHook(void){
	// THIS RUNS WHILE NO OTHER TASK RUNS
}
