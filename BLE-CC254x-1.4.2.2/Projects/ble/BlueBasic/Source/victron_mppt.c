////////////////////////////////////////////////////////////////////////////////
// BlueBasic driver for Victron VE.Direct connected MPPT controller
////////////////////////////////////////////////////////////////////////////////
//
// Authors:
//      Kai Scheffer <kai@scheffer.ch>
//
// (C) 2019 Kai Scheffer, BlueBattery.com
//
// victron_mppt.c
//

#include "os.h"
#include "hal_uart.h"
#include "victron_mppt.h"


//#define MPPT_FORWARD_ASYNC
//#define MPPT_FORWARD_UNKNOWN

// interval for requesting a data set
#define SCAN_INTERVAL 2000
#define REQUEST_COUNT (sizeof(requests)/sizeof(request_t))
#define REQUEST_INTERVAL (SCAN_INTERVAL/REQUEST_COUNT)

//#define IS_NIB(a) (((a) >= '0' && (a) <= '9' ) || ( (a) >= 'A' && (a) <= 'F' ) )
#define IS_NIB(a) (1)
#define NIB_VAL(a) ((a) >= 'A' ? (a) - 'A' + 10 : (a) - '0' )

static struct
{
  uint8  size;
  uint16 batt_volt;
  uint16 sol_volt;
  uint16 sol_current;
  uint8  status;
} mppt = {sizeof(mppt)-sizeof(mppt.size),0,0,0,0};

typedef struct
{
   uint8 len;
   uint8 *data;
} request_t;

static const request_t requests[] =
{
  {11, ":7D7ED008A\n"}, // solar current
  {11, ":7BBED00A6\n"}, // solar panel voltage
//  {11, ":7BCED00A5\n"}, // solar power
  {11, ":7D5ED008C\n"}, // battery voltage
  {11, ":70102004B\n"}  // status
};

#ifdef MPPT_FORWARD_UNKNOWN
typedef struct
{
  uint8 len;
  uint16 adr;
  unsigned long data;
} app_buf_t;
#endif

static long request_time = 0;
static long scan_time = 0;
static uint8 request = 0;

typedef enum
{
  MPPT_IDLE,
  MPPT_START,
  MPPT_GET,
  MPPT_ADR,
  MPPT_DATA,
  MPPT_CHECKSUM,
  MPPT_END
} state_mppt_t;

static state_mppt_t state = MPPT_IDLE;
static uint8 mppt_sum;
static unsigned long mppt_adr;
static unsigned long mppt_data;
static uint8 mppt_nibs;
static uint8 mppt_checksum;

// send a request to the mppt controller
static void request_mppt(uint8 port)
{
  HalUARTWrite(port, requests[request].data, requests[request].len);
  request++;
  request = request >= REQUEST_COUNT ? 0 : request;
}

// send data to application
static void send_app(uint8 port, uint8 *buf, uint8 len)
{
  // check if buffer has space
  if (serial[port].sbuf_read_pos >= len)
  {
    serial[port].sbuf_read_pos = 0;
    OS_memcpy(serial[port].sbuf, buf, len);    
  }
  if (serial[port].onread)
  {    
    interpreter_run(serial[port].onread, INTERPRETER_CAN_RETURN);    
  }  
}

// send MPPT data
#define send_mppt(a) send_app(a,(void*)&mppt,sizeof(mppt))

// receive data from MPPT controller
static void receive_mppt(uint8 port)
{
  uint8 c;
  for ( ; HalUARTRead(port, &c, 1); )
  {
    switch (state)
    {
    case MPPT_IDLE:
      if (c==':')
        state = MPPT_START;
      break;
    case MPPT_START:
#ifndef MPPT_FORWARD_ASYNC
      if (c == '7')
#else
      // check for Get or Asyn messages
      if (c == '7' || c == 'A') 
#endif
      {
        state = MPPT_ADR;
        mppt_sum = NIB_VAL(c);
        mppt_adr = 0;
        mppt_nibs = 0;
      }
      else
      {
        state = MPPT_IDLE;
      }
      break;
    case MPPT_ADR:
      {
        mppt_adr = mppt_adr<<4 | NIB_VAL(c);
        if (++mppt_nibs == 6)
        {
          if (((uint8)mppt_adr & 0xff) == 0)
          {
            state = MPPT_DATA;
            mppt_adr >>= 8;
            mppt_data = 0;
            mppt_nibs = 0;
            mppt_checksum = 0;
          }
          else
          {
            state = MPPT_IDLE;
          }
        }
      }      
      break;      
    case MPPT_DATA:
      if (c != '\n')
      {
        if (++mppt_nibs <= 8)
        {
          mppt_data = mppt_data << 4 | NIB_VAL(c);
        }
        else
        {
          mppt_checksum = mppt_checksum << 4 | NIB_VAL(c);
        }
      }
      else
      {
        // now we got all ...
        state = MPPT_END;
        // when less nibs arrived we extract the checksum byte
        if (mppt_nibs<=8)
        {
          mppt_checksum = (uint8)mppt_data;
          mppt_data >>= 8;
        }
      }
      break;
    }
    if (state == MPPT_END)
    {
      state = MPPT_IDLE;
      mppt_sum += (uint8)((uint16)mppt_adr>>8)
          + (uint8) mppt_adr
          + (uint8)(mppt_data>>24)
          + (uint8)(mppt_data>>16)
          + (uint8)(mppt_data>>8)
          + (uint8) mppt_data
          + mppt_checksum;
      if (mppt_sum  == 0x55 )
      {
        switch ((uint16)mppt_adr)
        {
        case 0xd7ed:  // solar current
          mppt.sol_current = (uint16) mppt_data;
          break;
        case 0xbbed:  // solar panel voltage
          mppt.sol_volt = (uint16) mppt_data;
          break;
        case 0xd5ed:  // battery voltage
          mppt.batt_volt = (uint16) mppt_data;
          break;
        case 0x0102:  // status
          mppt.status = (uint8) mppt_data;
          break;
        default:
#ifdef MPPT_FORWARD_UNKNOWN
          {
            app_buf_t tmp = {sizeof(tmp)-1, (uint16)mppt_adr, mppt_data};
            send_app(port, (void*)&tmp, sizeof(tmp));
          }
#endif
          break;
        }
      }
    }
  }
}

// manage the Victron MPPT controller in HEX mode
// should be called in regular intervals to drive the data pump
void process_mppt(uint8 port, uint8 len)
{
  long now = OS_get_millis();
  if (len) 
  {
    receive_mppt(port);
  }
  else if (now - scan_time > SCAN_INTERVAL)
  {
    send_mppt(port);
    scan_time = now;
  }
  else if (now - request_time > REQUEST_INTERVAL)
  {
    request_mppt(port);
    request_time = now;
  }
}
