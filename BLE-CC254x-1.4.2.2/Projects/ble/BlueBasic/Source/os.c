////////////////////////////////////////////////////////////////////////////////
// BlueBasic
////////////////////////////////////////////////////////////////////////////////
//
// Authors:
//      Tim Wilkinson <timjwilkinson@gmail.com>
//
// os.c
//

#include "os.h"
#include "OSAL.h"
#include "OSAL_Clock.h"
#include "hal_uart.h"
#ifdef HAL_I2C
#include "hal_i2c.h"
#endif
#include "OSAL_PwrMgr.h"
#ifdef FEATURE_OAD_HEADER
#include "oad.h"
#include "oad_target.h"
#include "hal_flash.h"
#endif
#include "hal_uart.h"

#define FILE_HANDLE_PROGRAM     0
#define FILE_HANDLE_DATA        1
#define FILE_HANDLE_MAX         (OS_MAX_FILE)

#define	FILE_HEADER             (0x80)
#define FILE_PROGRAM_FIRST      (0x81)
#define FILE_PROGRAM_END        (FILE_PROGRAM_FIRST+0x20)
#define FILE_DATA_FIRST         (FILE_PROGRAM_END+1)
#define FILE_BLOCK_SIZE         (128)

extern void osalTimeUpdate(void);

extern uint8 blueBasic_TaskID;

os_interrupt_t blueBasic_interrupts[OS_MAX_INTERRUPT];
os_timer_t blueBasic_timers[OS_MAX_TIMER];
os_discover_t blueBasic_discover;

unsigned char sbuf[16];
uint8 sbuf_read_pos = 16;
uint8 uart_stop_polling = 1;

#ifdef PROCESS_SERIAL_DATA
unsigned char sflow;
#endif

struct program_header
{
  char autorun;
  unsigned short length[OS_MAX_FILE];
};

// Timers
#define NR_TIMERS ((OS_MAX_TIMER) - 1)
struct
{
  unsigned short lineno;
} timers[OS_MAX_TIMER];

// Serial
os_serial_t serial[OS_MAX_SERIAL];

#ifdef HAL_I2C
// I2C
os_i2c_t i2c[1];
unsigned char prevent_sleep_flags = 0; 
#endif

unsigned short bluebasic_yield_linenum;

enum {
  MODE_STARTUP = 0,
  MODE_NEED_INPUT,
  MODE_GOT_INPUT,
  MODE_RUNNING

};
static struct {
  char* start;
  char* end;
  char* ptr;
  char mode;
  char quote;
} input;

#ifdef ENABLE_BLE_CONSOLE

extern unsigned char ble_console_write(unsigned char ch);
extern unsigned char ble_console_enabled;

#endif

#define CR	'\r'
#define NL	'\n'
#define BELL	'\b'
#define SQUOTE  '\''
#define DQUOTE  '\"'
#define CTRLC	0x03
#define CTRLH	0x08


void OS_type(char c)
{
  switch (c)
  {
    case 0xff:
      break;
    case NL:
    case CR:
#ifdef ENABLE_CONSOLE_ECHO
      OS_putchar('\n');
#endif // ENABLE_CONSOLE_ECHO
      *input.ptr = NL;
      input.mode = MODE_GOT_INPUT;
      // Wake the interpreter now we have something new to do
      osal_set_event(blueBasic_TaskID, BLUEBASIC_INPUT_AVAILABLE);
      break;
    case CTRLH:
      if(input.ptr == input.start)
      {
        break;
      }
      if (*--input.ptr == input.quote)
      {
        input.quote = 0;
      }
#ifdef ENABLE_CONSOLE_ECHO
      OS_putchar(CTRLH);
      OS_putchar(' ');
      OS_putchar(CTRLH);
#endif // ENABLE_CONSOLE_ECHO
      break;
    default:
      if(input.ptr != input.end)
      {
        // Are we in a quoted string?
        if(c == input.quote)
        {
          input.quote = 0;
        }
        else if (c == DQUOTE || c == SQUOTE)
        {
          input.quote = c;
        }
        else if (input.quote == 0 && c >= 'a' && c <= 'z')
        {
          c = c + 'A' - 'a';
        }
        *input.ptr++ = c;
#ifdef ENABLE_CONSOLE_ECHO
        OS_putchar(c);
#endif
      }
      break;
  }
}

void OS_prompt_buffer(unsigned char* start, unsigned char* end)
{
  input.mode = MODE_NEED_INPUT;
  input.start = (char*)start;
  input.end = (char*)end;
  input.ptr = (char*)start;
  input.quote = 0;
}

char OS_prompt_available(void)
{
  if (input.mode == MODE_GOT_INPUT)
  {
    input.mode = MODE_RUNNING;
    return 1;
  }
  else
  {
    return 0;
  }
}

void OS_putchar(char ch)
{
#if ENABLE_BLE_CONSOLE  
  ble_console_write(ch);
#endif
}

void* OS_rmemcpy(void *dst, const void GENERIC *src, unsigned int len)
{
  uint8 *pDst;
  const uint8 GENERIC *pSrc;

  pSrc = len + (uint8*)src;
  pDst = len + (uint8*)dst;

  while ( len-- )
    *--pDst = *--pSrc;

  return ( pDst );
}

void OS_init(void)
{
#ifndef ENABLE_BLE_CONSOLE
  OS_openserial();
#endif
}

void OS_timer_stop(unsigned char id)
{
  blueBasic_timers[id].linenum = 0;
  osal_stop_timerEx(blueBasic_TaskID, BLUEBASIC_EVENT_TIMER << id);
}

char OS_timer_start(unsigned char id, unsigned long timeout, unsigned char repeat, unsigned short linenum)
{
  if (id >= OS_MAX_TIMER)
  {
    return 0;
  }
  blueBasic_timers[id].linenum = linenum;
  if (repeat)
  {
    osal_start_reload_timer(blueBasic_TaskID, BLUEBASIC_EVENT_TIMER << id, timeout);
  }
  else
  {
    osal_start_timerEx(blueBasic_TaskID, BLUEBASIC_EVENT_TIMER << id, timeout);
  }
  return 1;
}

void OS_yield(unsigned short linenum)
{
  bluebasic_yield_linenum = linenum;
  osal_set_event( blueBasic_TaskID, BLUEBASIC_EVENT_YIELD );
#if 0  
  extern void printnum(signed char fieldsize, long num);
  extern void printmsg(const char *msg);
  printnum(0, linenum);
  printmsg(" YLD");
#endif
}

char OS_interrupt_attach(unsigned char pin, unsigned short lineno)
{
  unsigned char i;

  for (i = 0; i < OS_MAX_INTERRUPT; i++)
  {
    if (blueBasic_interrupts[i].linenum == 0)
    {
      blueBasic_interrupts[i].pin = pin;
      blueBasic_interrupts[i].linenum = lineno;
      return 1;
    }
  }
  return 0;
}

char OS_interrupt_detach(unsigned char pin)
{
  unsigned char i;

  for (i = 0; i < OS_MAX_INTERRUPT; i++)
  {
    if (blueBasic_interrupts[i].pin == pin)
    {
      blueBasic_interrupts[i].pin = 0;
      blueBasic_interrupts[i].linenum = 0;
      return 1;
    }
  }
  return 0;
}

long OS_get_millis(void)
{
  osalTimeUpdate();
  return osal_getClock() * 1000  +  osal_getMSClock();
}

void OS_set_millis(long timems)
{
  osal_setClock(timems / 1000);
}

__no_init __data uint8 JumpToImageAorB @ 0x09;

void OS_reboot(char flash)
{
 #ifdef FEATURE_OAD_HEADER
  if (flash)
  {
#if 0    
    short zero = 0;
    uint16 addr = OAD_IMG_B_PAGE * (HAL_FLASH_PAGE_SIZE / HAL_FLASH_WORD_SIZE);
    HalFlashWrite(addr, (uint8*)&zero, sizeof(zero));
#else
    JumpToImageAorB = 0;
    // Simulate a reset for the Application code by an absolute jump to the expected INTVEC addr.
    asm("LJMP 0x0830");
#endif    
  }
#else
  VOID flash;
#endif
  SystemReset();
}

#pragma optimize=none
void OS_delaymicroseconds(short micros)
{
  // An empty loop is pretty accurate it turns out
  while (micros-- > 0)
    ;
}

void OS_flashstore_init(void)
{
  // If flashstore is uninitialized, deleting all the pages will set it up correctly.
  if (*(unsigned long*)FLASHSTORE_CPU_BASEADDR == 0xFFFFFFFF)
  {
    flashstore_deleteall();
  }
}


void OS_enable_sleep(unsigned char enable)
{
#ifdef POWER_SAVING
  extern uint8 Hal_TaskID;
  if (enable) {
    osal_pwrmgr_task_state(Hal_TaskID, PWRMGR_CONSERVE);
  } else {
    CLEAR_SLEEP_MODE();
    osal_pwrmgr_task_state(Hal_TaskID, PWRMGR_HOLD);
  }
#endif
}

#if UART_USE_CALLBACK

#if !defined(PROCESS_SERIAL_DATA)
static void _uartCallback(uint8 port, uint8 event)
{
#ifdef HAL_UART_RX_WAKEUP
  // UART woken up. This happens in the interrupt handler so we really
  // don't want to do anything else.
  if (event == HAL_UART_RX_WAKEUP)
  {
    return;
  }
#endif
  if (port != HAL_UART_PORT_0)
    return;
  if (event & (HAL_UART_RX_ABOUT_FULL | HAL_UART_RX_FULL)
      && serial[0].onread
        && sbuf_read_pos == 16 )
  {
    uint8 len = Hal_UART_RxBufLen(HAL_UART_PORT_0);
    if (len >= 16)
    {
      HalUARTRead(HAL_UART_PORT_0, &sbuf[0], 16);
      sbuf_read_pos = 0;
      osal_set_event(blueBasic_TaskID, BLUEBASIC_EVENT_SERIAL);
    }
  }
  else if (event & HAL_UART_TX_EMPTY
             && serial[0].onwrite )
  {
    osal_set_event(blueBasic_TaskID, BLUEBASIC_EVENT_SERIAL);
  }
}

#else

static void _uartCallback(uint8 port, uint8 event)
{
#ifdef HAL_UART_RX_WAKEUP
  // UART woken up. This happens in the interrupt handler so we really
  // don't want to do anything else.
  if (event == HAL_UART_RX_WAKEUP )
  {
    return;
  }
#endif
  if (port != HAL_UART_PORT_0)
    return;
  if (event & (HAL_UART_RX_ABOUT_FULL | HAL_UART_RX_FULL)
      && serial[0].onread
        && sbuf_read_pos == 16 )
  {
    uint8 len = Hal_UART_RxBufLen(HAL_UART_PORT_0);
    if ( len >= 16 )
    {
      if (sflow == 'V')
      {
        HalUARTRead(HAL_UART_PORT_0, &sbuf[0], 1);
        if (sbuf[0] == 0xAA)
        {
          uint8 parity = 0;
          uint8 cnt;
          HalUARTRead(HAL_UART_PORT_0, &sbuf[1], 15);
          for (cnt=1; cnt < 15; )
          {
            parity ^= sbuf[cnt++];
          }
          //only send serial data when frame has no parity error
          if (parity == sbuf[15])
          {
            sbuf_read_pos = 0;
            osal_set_event(blueBasic_TaskID, BLUEBASIC_EVENT_SERIAL);
          }
        }
      }
      else
      {
        HalUARTRead(HAL_UART_PORT_0, &sbuf[0], 16);
        sbuf_read_pos = 0;
        osal_set_event(blueBasic_TaskID, BLUEBASIC_EVENT_SERIAL);
      }
    } else if (event & HAL_UART_TX_EMPTY
               && serial[0].onwrite )
    {
      osal_set_event(blueBasic_TaskID, BLUEBASIC_EVENT_SERIAL);
    }
  }
}
#endif

#endif
  
  
unsigned char OS_serial_open(unsigned char port, unsigned long baud, unsigned char parity, unsigned char bits, unsigned char stop, unsigned char flow, unsigned short onread, unsigned short onwrite)
{
  halUARTCfg_t config;
  int cbaud;
  
  switch (baud)
  {
    case 1000:
      cbaud = HAL_UART_BR_1000;
      break;
    case 9600:
      cbaud = HAL_UART_BR_9600;
      break;
    case 19200:
      cbaud = HAL_UART_BR_19200;
      break;
    case 38400:
      cbaud = HAL_UART_BR_38400;
      break;
    case 57600:
      cbaud = HAL_UART_BR_57600;
      break;
    case 115200:
      cbaud = HAL_UART_BR_115200;
      break;
    default:
      return 2;
  }
 
  // Only support port 0, no-parity, 8-bits, 1 stop bit
#ifndef PROCESS_SERIAL_DATA 
  if (port != 0 || parity != 'N' || bits != 8 || stop != 1 || (flow != 'H' && flow != 'N'))
#else
  // additional option 'V' means preprocessing needs to be enabled
  sflow = flow;
  if (port != 0 || parity != 'N' || bits != 8 || stop != 1 || (flow != 'H' && flow != 'N' && flow != 'V'))
#endif
  {
    return 3;
  }

  config.configured = 1;
  config.baudRate = cbaud;
  config.flowControl = flow == 'H' ? HAL_UART_FLOW_ON : HAL_UART_FLOW_OFF;
  config.flowControlThreshold = flow == 'H' ? 64 : 0;
  config.idleTimeout = 0;
  config.rx.maxBufSize = HAL_UART_DMA_RX_MAX;
  config.tx.maxBufSize = HAL_UART_DMA_TX_MAX;
  config.intEnable = 1;
#if UART_USE_CALLBACK  
  config.callBackFunc = _uartCallback;
#else
  config.callBackFunc = NULL;
#endif  

#ifdef POWER_SAVING  
  // suggestion taken from here:
  // http://e2e.ti.com/support/wireless_connectivity/bluetooth_low_energy/f/538/p/431990/1668088#1668088
  extern uint8 Hal_TaskID;
//  CLEAR_SLEEP_MODE();
//  osal_pwrmgr_task_state(Hal_TaskID, PWRMGR_HOLD);
#ifdef HAL_I2C
  prevent_sleep_flags |= 0x01;
#endif
#endif
  HalUARTInit();
  P0SEL |= 0x3c;  // select peripheral mode
  if (HalUARTOpen(HAL_UART_PORT_0, &config) == HAL_UART_SUCCESS)
  {
    serial[0].onread = onread;
    serial[0].onwrite = onwrite;
#ifdef PROCESS_SERIAL_DATA
    sbuf_read_pos = 16;
#endif
#if !(UART_USE_CALLBACK)  
    uint32 periode = 160000UL / baud;
    if (periode < 10)
      periode = 10;
    osal_start_reload_timer(blueBasic_TaskID, BLUEBASIC_EVENT_SERIAL, periode);
#endif
    uart_stop_polling = 0;
    return 0;
  }
  return 1;
}

unsigned char OS_serial_close(unsigned char port)
{
  serial[0].onread = 0;
  serial[0].onwrite = 0;
#if !(UART_USE_CALLBACK)  
  osal_stop_timerEx(blueBasic_TaskID, BLUEBASIC_EVENT_SERIAL);
#endif
#ifdef PROCESS_SERIAL_DATA
    sbuf_read_pos = 16;
#endif  
  // HalUARTClose(0); - In the hal_uart.h include file, but not actually in the code
  HalUARTSuspend();
  P0SEL &= ~0x3c;  // select GPIO mode
  uart_stop_polling = 1;
//  P0DIR &= ~0x08;
//  P0 &= ~0x0c;
//  P0INP |= 0x0c;   
#ifdef POWER_SAVING
#if HAL_UART_DMA
  extern uint8 Hal_TaskID;
#ifndef HAL_I2C
  osal_pwrmgr_task_state(Hal_TaskID, PWRMGR_CONSERVE);
#else
  prevent_sleep_flags &= 0xFE;
  if (!prevent_sleep_flags) {
    osal_pwrmgr_task_state(Hal_TaskID, PWRMGR_CONSERVE);    
  }
#endif
#endif
#endif  
  return 1;
}

#ifndef PROCESS_SERIAL_DATA
short OS_serial_read(unsigned char port)
{
  unsigned char ch;
  if (HalUARTRead(HAL_UART_PORT_0, &ch, 1) == 1)
  {
    return ch;
  }
  else
  {
    return -1;
  }
}
#else
short OS_serial_read(unsigned char port)
{
  if (sbuf_read_pos < 16)
  {
    return sbuf[sbuf_read_pos++];
  }
  else
  {
    return -1;
  }
}
#endif


unsigned char OS_serial_write(unsigned char port, unsigned char ch)
{
  return HalUARTWrite(HAL_UART_PORT_0, &ch, 1) == 1 ? 1 : 0;
}

#ifndef PROCESS_SERIAL_DATA
unsigned char OS_serial_available(unsigned char port, unsigned char ch)
{
  return ch == 'R' ? Hal_UART_RxBufLen(HAL_UART_PORT_0) : Hal_UART_TxBufLen(HAL_UART_PORT_0);
}
#else
unsigned char OS_serial_available(unsigned char port, unsigned char ch)
{
  return ch == 'R' ? (16 - sbuf_read_pos) : Hal_UART_TxBufLen(HAL_UART_PORT_0);
}
#endif

#ifdef HAL_I2C
uint8 _i2cCallback(uint8 cnt)
{
  if (cnt) {
    // some bytes are available to read
    i2c[0].available_bytes = cnt;
    osal_set_event(blueBasic_TaskID, BLUEBASIC_EVENT_I2C);
  } else if (i2c[0].available_bytes == 0) {
    osal_set_event(blueBasic_TaskID, BLUEBASIC_EVENT_I2C);
  }
  return FALSE;
}

unsigned char OS_i2c_open(unsigned char address, unsigned short onread, unsigned short onwrite)
{
  
#if defined POWER_SAVING 
  // suggestion taken from here:
  // http://e2e.ti.com/support/wireless_connectivity/bluetooth_low_energy/f/538/p/431990/1668088#1668088
  extern uint8 Hal_TaskID;
  CLEAR_SLEEP_MODE();
  osal_pwrmgr_task_state(Hal_TaskID, PWRMGR_HOLD);
  prevent_sleep_flags |= 0x02;
#endif
  
  HalI2CInit(address, _i2cCallback);
  i2c[0].onread = onread;
  i2c[0].onwrite = onwrite;
  i2c[0].available_bytes = 0;
  return 0;
}

unsigned char OS_i2c_close(unsigned char port)
{
  // see errata: http://www.ti.com/lit/er/swrz049/swrz049.pdf
  // disable I2C pins to prevent glitch
  I2CWC |= 0x8c;
  i2c[0].onread = 0;
  i2c[0].onwrite = 0;
  i2c[0].available_bytes = 0;
#if defined POWER_SAVING
  extern uint8 Hal_TaskID;
  prevent_sleep_flags &= 0xFD;
  if (!prevent_sleep_flags) {
    osal_pwrmgr_task_state(Hal_TaskID, PWRMGR_CONSERVE);
  }
#endif  
  return 1;
}

short OS_i2c_read(unsigned char port)
{
  unsigned char ch;
  if (HalI2CRead(1, &ch) == 1)    
  {
    i2c[0].available_bytes -= 1;
    return ch;
  }
  else
  {
    return -1;
  }
}

unsigned char OS_i2c_write(unsigned char port, unsigned char ch)
{
  // not supported
  return 0;
}

unsigned char OS_i2c_available(unsigned char port, unsigned char ch)
{
  return ch == 'R' ? i2c[0].available_bytes : 0;  
}
#endif

// read temperature adc
int16 read_temperature_adc(void)
{
  int32 top;
  halIntState_t intState;
  
  HAL_ENTER_CRITICAL_SECTION(intState);
  TR0 = 0x01; // connect temperture sensor to ADC
  ATEST = 0x01;
  ADCCON3 = 0x0E | 0x30 | 0x00; // temperature sensor, 12-bit, internal voltage reference
  while ((ADCCON1 & 0x80) == 0)
    ;
  top = ADCL;
  top |= ADCH << 8;  
  ATEST = 0;
  TR0 = 0;
  HAL_EXIT_CRITICAL_SECTION(intState);
    
  // data sheet says 1480 @ 25C
  // and 4.5 bits per 1C
  // we use the factory data stored in info word 7
#define INFOPAGE_WORD7 0x781C
  //now we access the calibration data from the TI factory
  //according to https://e2e.ti.com/support/wireless_connectivity/f/538/p/396260/1450855#1450855
  // addr     DWORD   Data    Decription    
  // 0x781C   7       0x96    3V temperature sensor reading       
  // 0x781D           0x17    0x17 = 23, test temp RT
  // 0x781E           ADCH    temperature sensor, Vdd = 3.0v        
  // 0x781F           ADCL    temperature sensor, Vdd = 3.0v
  // 0x7820   8       0x32    2V temperature sensor reading
  // 0x7821           0x17    0x17 = 23, test temp RT        
  // 0x7822           ADCH    temperature sensor, Vdd = 2.0v
  // 0x7823           ADCL    temperature sensor, Vdd = 2.0v
  //
  // Attention: the RT temperature is not controlled in the factory
  // TI claims it has a lot to lt variantion depending on the factory temp
  uint16 factoryAdc;
  uint8  factoryTemp;
  if ( *((uint8 *)INFOPAGE_WORD7 + 0) == 0x96 )
  {
    factoryAdc  = *((uint8 *)INFOPAGE_WORD7 + 2)<<8
      | *((uint8 *)INFOPAGE_WORD7 + 3);
    factoryTemp = *((uint8 *)INFOPAGE_WORD7 + 1);
  }
  else
  {
    // use data sheet values
    factoryAdc = 1480<<4;
    factoryTemp = 25;
  }
  return ((top - factoryAdc) * 10 + 3) / 7 + 100 * factoryTemp;
}

// return temperture in °C * 100
// wait 0: return immediately
// wait 1: return lowest of last 8 samples
int16 OS_get_temperature(uint8 wait)
{
  // initialize to 20°C
  static int16 samples[8] = {2000,2000,2000,2000,2000,2000,2000,2000};
  int16 temp;
  if (wait)
  {
    temp = samples[0] = read_temperature_adc();
    for (uint8 i = sizeof(samples) - 1 ; --i; )
    {
      if (samples[i] < temp)
        temp = samples[i];
      samples[i] = samples[i-1];
    }
  }
  else
  {
    temp = read_temperature_adc();
  }
  return temp;
}
