//*****************************************************************************
// Written by Teresa Li
//            Kevin Ren
//
//*****************************************************************************

//*****************************************************************************
//
// Application Name     - Timer Count Capture
// Application Overview - This application showcases Timer's count capture 
//                        feature to measure frequency of an external signal.
// Application Details  -
// http://processors.wiki.ti.com/index.php/CC32xx_Timer_Count_Capture_Application
// or
// docs\examples\CC32xx_Timer_Count_Capture_Application.pdf
//
//*****************************************************************************

// Driverlib includes
#include "hw_types.h"
#include "hw_ints.h"
#include "hw_memmap.h"
#include "interrupt.h"
#include "prcm.h"
#include "gpio.h"
#include "utils.h"
#include "timer.h"
#include "spi.h"
#include "rom.h"
#include "rom_map.h"
#include "pin.h"
#include "math.h"
#include "string.h"
#include "uart.h"

// Common interface includes
#include "uart_if.h"
#include "timer_if.h"
#include "pin_mux_config.h"
#include "Adafruit_GFX.h"


#define APPLICATION_VERSION     "1.1.1"
#define APP_NAME        "Board to Board Texting"

#define BUTTON_ZERO     255
#define BUTTON_ONE      32895
#define BUTTON_TWO      16575
#define BUTTON_THREE    49215
#define BUTTON_FOUR     8415
#define BUTTON_FIVE     41055
#define BUTTON_SIX      24735
#define BUTTON_SEVEN    57375
#define BUTTON_EIGHT    4335
#define BUTTON_NINE     36975
#define BUTTON_LAST     765
#define BUTTON_MUTE     2295
// Color definitions
#define BLACK           0x0000
#define BLUE            0x001F
#define GREEN           0x07E0
#define CYAN            0x07FF
#define RED             0xF800
#define MAGENTA         0xF81F
#define YELLOW          0xFFE0
#define WHITE           0xFFFF

#define SPI_IF_BIT_RATE  100000
#define TR_BUFF_SIZE     100

typedef struct PinSetting {
    unsigned long port;
    unsigned int pin;
} PinSetting;

typedef struct {
    char message[160];
    int index;
} Msg;

typedef struct {
    int x, y;
} Coordinate;

//*****************************************************************************
//                 GLOBAL VARIABLES -- Start
//*****************************************************************************
static int buffer[100];
static int number[100];
static int value = 1;
static int i = 0;
static int detected = 0;
static int delta = 0;
static char lastkey = '\0';
static int start = 0;

static char keySet[10][4][2] = {{"","","",""},// space for 0
                                {"","","",""}, // nothing for 1
                                {"A","B","C",""}, // 2
                                {"D","E","F",""}, // 3
                                {"G","H","I",""}, // 4
                                {"J","K","L",""}, // 5
                                {"M","N","O",""}, // 6
                                {"P","Q","R","S"},// 7
                                {"T","U","V",""}, // 8
                                {"W","X","Y","Z"} // 9
                        };
static int keyBuffer[10] = {0,0,0,0,0,0,0,0,0,0};

static PinSetting OC = {.port = GPIOA3_BASE, .pin = 0x80};
static PinSetting Receiver = {.port = GPIOA1_BASE, .pin = 0x8};
static Coordinate top = {.x = 0, .y = 0};
static Coordinate bot = {.x = 0, .y = 120};
static Msg message;
static Msg rmsg;


#if defined(ccs) || defined(gcc)
extern void (* const g_pfnVectors[])(void);
#endif
#if defined(ewarm)
extern uVectorEntry __vector_table;
#endif
//*****************************************************************************
//                 GLOBAL VARIABLES -- End
//*****************************************************************************


//*****************************************************************************
//
//! Application startup display on UART
//!
//! \param  none
//!
//! \return none
//!
//*****************************************************************************
void
DisplayBanner(char * AppName)
{

    Report("\n\n\n\r");
    Report("\t\t *************************************************\n\r");
    Report("\t\t  CC3200 %s Application \n\r", AppName);
    Report("\t\t *************************************************\n\r");
    Report("\n\n\n\r");
}

void deleteChar(int x, int y) {
    fillRect(x, y, 6, 8, BLACK);
}

//*****************************************************************************
//
//! Board Initialization & Configuration
//!
//! \param  None
//!
//! \return None
//
//*****************************************************************************
void
BoardInit(void)
{
/* In case of TI-RTOS vector table is initialize by OS itself */
#ifndef USE_TIRTOS
  //
  // Set vector table base
  //
#if defined(ccs) || defined(gcc)
    MAP_IntVTableBaseSet((unsigned long)&g_pfnVectors[0]);
#endif
#if defined(ewarm)
    MAP_IntVTableBaseSet((unsigned long)&__vector_table);
#endif
#endif
    //
    // Enable Processor
    //
    MAP_IntMasterEnable();
    MAP_IntEnable(FAULT_SYSTICK);

    PRCMCC3200MCUInit();
}


int sameArray(int a[], int b[]) {
    int index;
    for (index = 0; index < 16; index++) {
        if (a[index] != b[index]) {
            return 0;
        }
    }
    return 1;
}

void findPattern()
{
    int pattern[16] = {0,0,0,0,
                       0,0,1,0,
                       1,1,1,1,
                       1,1,0,1};
    int index, find = 0;
    for (index = 0; index < i-16; index++) {
        if (sameArray(buffer+index, pattern)) {
            find = 1;
            break;
        }
    }
    if (find) {
        int j, k;
        for (k = 0, j = index + 16; j < i; j++, k++) {
            number[k] = buffer[j];
        }
        detected = 1;
        i = k;
    }
}


static void ResetButton()
{
    Timer_IF_InterruptClear(TIMERA0_BASE);
    //
    // Turn off the timer
    //
    TimerDisable(TIMERA0_BASE, TIMER_A);
    // reset the key, so the same key can be enter again
    lastkey = '\0';
    Report("RESET\n\r");
}

void Process(char key, int keyIndex, int numKeys)
{
    if (lastkey != key) {
         keyBuffer[keyIndex] = 0;
         if (start)
             message.index++;
         else
             start = 1;
     }
     if (keyBuffer[keyIndex] == numKeys) {
         keyBuffer[keyIndex] = 0;
         top.x -= 6;
         deleteChar(top.x, top.y);
         setCursor(top.x, top.y);
     }
     if (numKeys == 3) {
         switch (keyBuffer[keyIndex]) {
         case 0:
             Outstr(keySet[keyIndex][keyBuffer[keyIndex]]);
             message.message[message.index] = keySet[keyIndex][keyBuffer[keyIndex]][0];
             break;
         case 1:
         case 2:
             top.x -= 6;
             deleteChar(top.x, top.y);
             setCursor(top.x, top.y);
             Outstr(keySet[keyIndex][keyBuffer[keyIndex]]);
             message.message[message.index] = keySet[keyIndex][keyBuffer[keyIndex]][0];
             break;
         default:
             break;
         }
     }
     else {
         switch (keyBuffer[keyIndex]) {
         case 0:
             Outstr(keySet[keyIndex][keyBuffer[keyIndex]]);
             message.message[message.index] = keySet[keyIndex][keyBuffer[keyIndex]][0];
             break;
         case 1:
         case 2:
         case 3:
             top.x -= 6;
             deleteChar(top.x, top.y);
             setCursor(top.x, top.y);
             Outstr(keySet[keyIndex][keyBuffer[keyIndex]]);
             message.message[message.index] = keySet[keyIndex][keyBuffer[keyIndex]][0];
             break;
         default:
             break;
         }
     }
     keyBuffer[keyIndex]++;
     lastkey = key;
}

static void UARTIntHandler()
{
    TimerIntClear(TIMERA1_BASE, TIMER_A);
    TimerDisable(TIMERA1_BASE, TIMER_A);
    UARTIntClear(UARTA1_BASE, UART_INT_RX);
    Report("Interrupt\n\r");
    char tmp = UARTCharGet(UARTA1_BASE);
    rmsg.message[rmsg.index++] = tmp;
    TimerEnable(TIMERA1_BASE, TIMER_A);
}

static void GPIOEdgeHandler(void) {
    unsigned long ulStatus;
    TimerDisable(TIMERA0_BASE, TIMER_A);
    TimerDisable(TIMERA2_BASE, TIMER_A);
    Timer_IF_InterruptClear(TIMERA2_BASE);
    ulStatus = MAP_GPIOIntStatus (Receiver.port, true);
    MAP_GPIOIntClear(Receiver.port, ulStatus);

    if(delta > 3)
        value = 1;
    else
        value = 0;

    delta = 0;

    if (i >= 16 && !detected) {
        findPattern();
    }
    if (!detected)
        buffer[i] = value;
    else
        number[i] = value;


    i++;
    TimerEnable(TIMERA2_BASE, TIMER_A);
    TimerEnable(TIMERA0_BASE, TIMER_A);

}
static void PrintBottom()
{
    TimerIntClear(TIMERA1_BASE, TIMER_A);
    TimerDisable(TIMERA1_BASE, TIMER_A);
    Report("End\n\r");
    rmsg.message[rmsg.index] = '\0';
    setCursor(bot.x, bot.y);
    fillRect(bot.x, bot.y, 128, 8, BLACK);
    Outstr(rmsg.message);
    rmsg.message[0] = '\0';
    rmsg.index = 0;
}

static void TimerCountHandler(void)
{
    Timer_IF_InterruptClear(TIMERA2_BASE);
    delta++;
}

//*****************************************************************************
//
//! Main  Function
//
//*****************************************************************************
int main()
{
    int k = 0;
    int sum = 0;
    message.index = 0;
    BoardInit();
    PinMuxConfig();

    // Configuring the timers
    //
    Timer_IF_Init(PRCM_TIMERA0, TIMERA0_BASE, TIMER_CFG_PERIODIC, TIMER_A, 0);
    Timer_IF_Init(PRCM_TIMERA1, TIMERA1_BASE, TIMER_CFG_PERIODIC, TIMER_A, 0);
    Timer_IF_Init(PRCM_TIMERA2, TIMERA2_BASE, TIMER_CFG_PERIODIC, TIMER_A, 0);
    //
    // Setup the interrupts for the timer timeouts.
    //
    Timer_IF_IntSetup(TIMERA0_BASE, TIMER_A, ResetButton);
    Timer_IF_IntSetup(TIMERA1_BASE, TIMER_A, PrintBottom);
    Timer_IF_IntSetup(TIMERA2_BASE, TIMER_A, TimerCountHandler);

    TimerLoadSet(TIMERA0_BASE, TIMER_A, MILLISECONDS_TO_TICKS(1000));
    TimerLoadSet(TIMERA1_BASE, TIMER_A, MILLISECONDS_TO_TICKS(1000));
    TimerLoadSet(TIMERA2_BASE, TIMER_A, MILLISECONDS_TO_TICKS(0.5));


    GPIOIntRegister(Receiver.port, GPIOEdgeHandler);
    GPIOIntTypeSet(Receiver.port, Receiver.pin, GPIO_FALLING_EDGE);
    int ulStatus = MAP_GPIOIntStatus(Receiver.port, false);
    GPIOIntClear(Receiver.port, ulStatus);

    GPIOIntEnable(Receiver.port, Receiver.pin);

    ClearTerm();
    InitTerm();
    DisplayBanner(APP_NAME);

    MAP_PRCMPeripheralClkEnable(PRCM_GSPI,PRCM_RUN_MODE_CLK);

    //
    // Reset the peripheral
    //
    MAP_PRCMPeripheralReset(PRCM_GSPI);
    MAP_SPIReset(GSPI_BASE);

    //
    // Configure SPI interface
    //
    MAP_SPIConfigSetExpClk(GSPI_BASE,MAP_PRCMPeripheralClockGet(PRCM_GSPI),
                     SPI_IF_BIT_RATE,SPI_MODE_MASTER,SPI_SUB_MODE_0,
                     (SPI_SW_CTRL_CS |
                     SPI_4PIN_MODE |
                     SPI_TURBO_OFF |
                     SPI_CS_ACTIVEHIGH |
                     SPI_WL_8));

    //
    // Enable SPI for communication
    //
    MAP_SPIEnable(GSPI_BASE);
    MAP_SPICSEnable(GSPI_BASE); // Enables chip select
    MAP_GPIOPinWrite(OC.port, OC.pin, OC.pin);
    Adafruit_Init();
    fillScreen(BLACK);

    //Enable and set up the UARTA1
    MAP_UARTConfigSetExpClk(UARTA1_BASE,MAP_PRCMPeripheralClockGet(PRCM_UARTA1),
                             UART_BAUD_RATE,
                             (UART_CONFIG_WLEN_8 | UART_CONFIG_STOP_ONE |
                             UART_CONFIG_PAR_NONE));
    MAP_UARTIntEnable(UARTA1_BASE, UART_INT_RX);
    MAP_UARTIntRegister(UARTA1_BASE, UARTIntHandler);
    MAP_UARTFIFODisable(UARTA1_BASE);



    while (1) {
        if(!(detected && i >= 16)){
            continue;
        }

        sum = 0;
        for(k = 0; k < 16; k++){
            sum += (int)number[k]*pow(2, (15-k));
        }

        TimerLoadSet(TIMERA0_BASE, TIMER_A, MILLISECONDS_TO_TICKS(1000));
        TimerEnable(TIMERA0_BASE, TIMER_A);
        TimerDisable(TIMERA2_BASE, TIMER_A);

        setCursor(top.x, top.y);

        switch(sum){
            case(BUTTON_ZERO):
                Outstr(" ");
                message.message[++message.index] = ' ';
                lastkey = '0';
                break;
            case(BUTTON_ONE):
                Report("1");
                lastkey = '1';
                break;
            case(BUTTON_TWO):
                Process('2', 2, 3);
                break;
            case(BUTTON_THREE):
                Process('3', 3, 3);
                break;
            case(BUTTON_FOUR):
                Process('4', 4, 3);
                break;
            case(BUTTON_FIVE):
                Process('5', 5, 3);
                break;
            case(BUTTON_SIX):
                Process('6', 6, 3);
                break;
            case(BUTTON_SEVEN):
                Process('7', 7, 4);
                break;
            case(BUTTON_EIGHT):
                Process('8', 8, 3);
                break;
            case(BUTTON_NINE):
                Process('9', 9, 4);
                break;
            case(BUTTON_LAST):
                Report("Delete\n\r");
                top.x -= 6;
                deleteChar(top.x, top.y);
                top.x -= 6;
                lastkey = 'd';
                message.message[message.index--] = '\0';
                break;
            case(BUTTON_MUTE):
                Report("Enter\n\r");
                lastkey = 'e';
                // print message for now
                message.message[++message.index] = '\0';
                Report("message: %s\n\r", message.message);
                int index;
                for (index = 0; index < message.index; index++) {
                    top.x -= 6;
                    deleteChar(top.x, top.y);
                    UARTCharPut(UARTA1_BASE, message.message[index]);
                }
                message.index = 0;
                message.message[message.index] = '\0';
                TimerEnable(TIMERA2_BASE, TIMER_A);
                TimerDisable(TIMERA0_BASE, TIMER_A);
                start = 0;
                top.x -= 6;
                break;
            default:
                Report("Unknown code %d\n\r", sum);
                top.x -= 6;
                lastkey = 'u';
                break;
        }
        Report("Pressed\n\r");
        i = 0;
        detected = 0;
        top.x += 6;
        if (top.x > 122) {
            top.x = 0;
            top.y += 8;
        }
    }

    MAP_SPICSDisable(GSPI_BASE);
}
