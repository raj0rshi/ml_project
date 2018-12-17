//int uart_putchar(char c, FILE *stream);
//int uart_getchar(FILE *stream);

// #define WHITE_TASK_CREATE	"Press 'A' to create blinkWhiteLED task."
/*
#define WHITE_TASK_CREATED	"blinkWhiteLED task created. Press 'B' to delete it."
#define WHITE_TASK_RUNNING	"blinkWhiteLED task already running. Press 'B' to delete it."
#define WHITE_TASK_DELETED	"blinkWhiteLED task deleted."
#define RED_TASK_CREATE		"Press 'B' to create blinkRedLED task."
#define RED_TASK_CREATED	"blinkRedLED task created. Press 'D' to delete it."
#define RED_TASK_RUNNING	"blinkRedLED task already running. Press 'D' to delete it."
#define RED_TASK_DELETED	"blinkRedLED task deleted."
*/

enum
{
	WHITE_TASK_CREATE = 0,
	WHITE_TASK_CREATED,
	WHITE_TASK_RUNNING,
	WHITE_TASK_DELETED,
	RED_TASK_CREATE,
	RED_TASK_CREATED,
	RED_TASK_RUNNING,
	RED_TASK_DELETED,
};

const char *taskString[] =
{
	"blinkWhiteLED task created. Press 'B' to delete it.",
	"blinkWhiteLED task already running. Press 'B' to delete it.",
	"blinkWhiteLED task deleted.",
	"Press 'B' to create blinkRedLED task.",
	"blinkRedLED task created. Press 'D' to delete it.",
	"blinkRedLED task already running. Press 'D' to delete it.",
	"blinkRedLED task deleted.",
};


//void uart_init(void);

/* http://www.ermicro.com/blog/?p=325 */

//extern FILE uart_output;
//extern FILE uart_input;
