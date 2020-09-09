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

//#define MPPT_TEST

#ifndef MPPT_TEST
#include "os.h"
#include "hal_uart.h"
#else // MPPT_TEST

#include <stdio.h>
#include <stdlib.h>
#include <memory.h>

#define MPPT_MODE_TEXT 1
#define MPPT_MODE_HEX 0
#define MPPT_AS_VOT 1
#define FIELD_SIZEOF(t, f) (sizeof(((t*)0)->f))
#define OS_MAX_SERIAL 1

#define osal_memset(a,b,c) memset(a,b,c)
#define uint8 unsigned char
#define uint16 unsigned short
#define int16 signed short
#define int32 signed int
#define int8 signed char
#define LO_UINT16(a) ((a)&0xff)
#define HI_UINT16(a) ((a)>>8 & 0xff)
#define BUILD_UINT16(lo,hi) (((hi)<<8)|(lo))
#define HalUARTRead(port, ptr, len) test_read(ptr,len)
uint16 test_read(void* ptr, uint16 len);
#endif // MPPT_TEST

// the douuble buffer is recommended by Victron in TEXT mode
// when assuming BASIC app fetches the data before the next Frame
// then its not needed, the frames comes in 1 second intervals
#define MPPT_DOUBLE_BUF FALSE

#if MPPT_MODE_TEXT
#define RECEIVE_MPPT(port) receive_text(port)
#else
#define RECEIVE_MPPT(port) receive_mppt(port)
#endif

#if !MPPT_AS_VOT
// send MPPT data
#define SEND_MPPT(a) send_app(a,(void*)&mppt,sizeof(mppt))
#define SCAN_INTERVAL 2000
#else
// send data like Votroic
#define SEND_MPPT(a) send_as_vot(a)
#define SCAN_INTERVAL 200
#endif

// interval for requesting a data set
#define REQUEST_COUNT (sizeof(requests)/sizeof(request_t))
#define REQUEST_INTERVAL (SCAN_INTERVAL/REQUEST_COUNT)

//#define IS_NIB(a) (((a) >= '0' && (a) <= '9' ) || ( (a) >= 'A' && (a) <= 'F' ) )
#define IS_NIB(a) (1)
#define NIB_VAL(a) ((a) >= 'A' ? (a) - 'A' + 10 : (a) - '0' )
#define SWAP_UINT16(a) BUILD_UINT16(HI_UINT16(a),LO_UINT16(a))

#if 1
#define DEBUG_LED_ON  P0|=0x10
#define DEBUG_LED_OFF P0&=~0x10
#endif

typedef struct
{
  uint8  size;
  uint16 batt_volt;   // 10mV per bit
  uint16 sol_volt;    // 10mV per bit
  int16 main_current;  // 10mA per bit
  int16 load_current; // 10mA per bit
  uint8  status;
} mppt_t;

static mppt_t mppt = { sizeof(mppt) - sizeof(mppt.size),0,0,0,0,0 };

#if MPPT_MODE_HEX
typedef struct
{
  uint8 len;
  uint8 *data;
} request_t;

static const request_t requests[] =
{
  { 11, ":7D7ED008A\n" }, // solar current
  { 11, ":7BBED00A6\n" }, // solar panel voltage
  //  {11, ":7BCED00A5\n"}, // solar power
  { 11, ":7D5ED008C\n" }, // battery voltage
  { 11, ":70102004B\n" }  // status
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
static uint8 request = 0;
#endif // MPPT_MODE_HEX

//static long scan_time = 0;

#if MPPT_MODE_HEX
typedef enum
{
  MPPT_INIT,
  MPPT_IDLE,
  MPPT_START,
  MPPT_GET,
  MPPT_ADR,
  MPPT_DATA,
  MPPT_CHECKSUM,
  MPPT_END,
} state_mppt_t;
#endif

#if MPPT_MODE_TEXT
typedef enum
{
  MPPT_FRAME,
  MPPT_IDLE,
  MPPT_START,
  MPPT_CHECKSUM,
  MPPT_LABEL_0,
  MPPT_LABEL_1,
  MPPT_LABEL_2,
  MPPT_LABEL_3,
  MPPT_LABEL_4,
  MPPT_LABEL_5,
  MPPT_LABEL_6,
  MPPT_LABEL_7,
  MPPT_LABEL_END,
  MPPT_TAB,
  MPPT_DATA_0,
  MPPT_DATA_1,
  MPPT_DATA_2,
  MPPT_DATA_3,
  MPPT_DATA_4,
  MPPT_DATA_5,
  MPPT_DATA_END
} state_mppt_t;
#endif

#if defined(MPPT_MODE_TEXT) || defined (MPPT_MODE_HEX)
static state_mppt_t state;
#endif
static uint8 mppt_sum;

#if MPPT_MODE_HEX
static unsigned long mppt_adr;
static unsigned long mppt_data;
static uint8 mppt_nibs;
static uint8 mppt_checksum;
#endif

#if MPPT_MODE_HEX
// send a request to the mppt controller
static void request_mppt(uint8 port)
{
  HalUARTWrite(port, requests[request].data, requests[request].len);
  request++;
  request = request >= REQUEST_COUNT ? 0 : request;
}
#endif

#ifndef MPPT_TEST

#if !MPPT_AS_VOT
// send data to application
static void send_app(uint8 port, uint8 *buf, uint8 len)
{
  serial[port].sbuf_read_pos = 0;
  OS_memcpy(serial[port].sbuf, buf, len);
}
#endif

#else // MPPT_TEST

typedef struct
{
  unsigned short onread;
  unsigned short onwrite;
  unsigned char sbuf[16];
  uint8 sbuf_read_pos; // = 16;
#ifdef PROCESS_SERIAL_DATA
  unsigned char sflow;
#endif  
} os_serial_t;

os_serial_t serial[OS_MAX_SERIAL];


static void send_app(uint8 port, uint8 *buf, uint8 len)
{
  // dump data to console
  for (uint8 i = 0; i < len; i++)
    printf("%02x ", (unsigned int)buf[i]);
  printf("\n");
}

#endif //MPPT_TEST

#if MPPT_AS_VOT
#define VOT_STATUS_MPP_FLAG 0x01
#define VOT_STATUS_ACTIVE 0x8
#define VOT_STATUS_LIMIT 0x10

typedef struct
{
  uint8 start;
  uint8 id;
  uint8 u_batt_lsb;  // 10mV per bit
  uint8 u_batt_msb;
  uint8 u_panel_lsb; // 10mV per bit
  uint8 u_panel_msb;
  uint8 i_batt_lsb;  // 0.1A per bit
  uint8 i_batt_msb;
  uint8 res[6];
  uint8 status;
  uint8 parity;
} sol_frame_t;

static_assert(sizeof(sol_frame_t) <= FIELD_SIZEOF(os_serial_t, sbuf), "sol_frame_t bigger than serial buffer");

static void send_as_vot(uint8 port)
{
  // directly copy data to serial buffer
  sol_frame_t* sol_frame = (sol_frame_t*)serial[port].sbuf;
  sol_frame->start = 0xAA;
  sol_frame->id = 0x1A;
  sol_frame->u_batt_lsb = LO_UINT16(mppt.batt_volt);
  sol_frame->u_batt_msb = HI_UINT16(mppt.batt_volt);
  sol_frame->u_panel_lsb = LO_UINT16(mppt.sol_volt);
  sol_frame->u_panel_msb = HI_UINT16(mppt.sol_volt);
  int16 sol_current = (mppt.main_current + mppt.load_current + 5) / 10;
  if (sol_current >= 0)
  {
    sol_frame->i_batt_lsb = LO_UINT16(sol_current);
    sol_frame->i_batt_msb = HI_UINT16(sol_current);
  }
  else
  {
    sol_frame->i_batt_lsb = sol_frame->i_batt_msb = 0;
  }
  
  // Victron MPPT Status (0x0201)  
  // 0 not charging
  // 2 failure 
  // 3 bulk   (full current)
  // 4 absobtion (voltage limit)
  // 5 float  (voltage limit)
  // 7 equalize (voltage limit)
  // 252 ESS (voltage controlled from external)
  // 255 unavailable  
  sol_frame->status = VOT_STATUS_MPP_FLAG;
  switch (mppt.status)
  {
  case 3:
    sol_frame->status |= VOT_STATUS_ACTIVE;
    break;
  case 4:
  case 5:
  case 6:
  case 7:
  case 252:
    sol_frame->status |= VOT_STATUS_LIMIT | VOT_STATUS_ACTIVE;
    break;
  default:
    break;
  }
  serial[port].sbuf_read_pos = 0;
}
#endif

#if MPPT_MODE_HEX
// receive data from MPPT controller
static void receive_mppt(uint8 port)
{
  uint8 c;
  for (; HalUARTRead(port, &c, 1); )
  {
    switch (state)
    {
    case MPPT_IDLE:
      if (c == ':')
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
        mppt_adr = mppt_adr << 4 | NIB_VAL(c);
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
        if (mppt_nibs <= 8)
        {
          mppt_checksum = (uint8)mppt_data;
          mppt_data >>= 8;
        }
      }
      break;
    default:
      state = MPPT_IDLE;
    }
    if (state == MPPT_END)
    {
      state = MPPT_IDLE;
      mppt_sum += (uint8)((uint16)mppt_adr >> 8)
        + (uint8)mppt_adr
          + (uint8)(mppt_data >> 24)
            + (uint8)(mppt_data >> 16)
              + (uint8)(mppt_data >> 8)
                + (uint8)mppt_data
                  + mppt_checksum;
      if (mppt_sum == 0x55)
      {
        switch ((uint16)mppt_adr)
        {
        case 0xd7ed:  // solar current
          mppt.sol_current = SWAP_UINT16(mppt_data) / 100;
          break;
        case 0xbbed:  // solar panel voltage
          mppt.sol_volt = SWAP_UINT16(mppt_data)/10;
          break;
        case 0xd5ed:  // battery voltage
          mppt.batt_volt = SWAP_UINT16(mppt_data)/10;
          break;
        case 0x0102:  // status
          mppt.status = LO_UINT16(mppt_data);
          break;
        default:
#ifdef MPPT_FORWARD_UNKNOWN
          {
            app_buf_t tmp = { sizeof(tmp) - 1, (uint16)mppt_adr, mppt_data };
            send_app(port, (void*)&tmp, sizeof(tmp));
          }
#endif
          break;
        }
      }
    }
  }
}
#endif // MPPT_MODE_HEX

#if MPPT_MODE_TEXT
typedef enum
{
  LABEL_NONE,
  LABEL_V,
  LABEL_I, 
  LABEL_VPV,
  LABEL_CS,
  LABEL_IL
} label_t;

static label_t label;

static void receive_text(uint8 port)
{
  static const uint8 sequence[] = { 'c','k','s','u','m' };
  static uint8 in_buf[16];
  static int32 mppt_data;
  static int8 mppt_sign;

  uint8 *ptr = in_buf;
  uint8 len = (uint8)HalUARTRead(port, ptr, sizeof(in_buf));
  while (len--)
  {
    uint8 c = *ptr++;
    mppt_sum += c;
    switch (state)
    {
    case MPPT_FRAME:
      mppt_sum = c;
      if (c == '\r')
        state = MPPT_START;
      break;
    case MPPT_IDLE:
      if (c == '\r')
        state = MPPT_START;
      break;
    case MPPT_START:
      if (c == '\n')
        state = MPPT_LABEL_0;
      else
        state = MPPT_IDLE;
      break;
    case MPPT_LABEL_0:
      state = MPPT_LABEL_1;
      switch (c)
      {
      case 'V':
        label = LABEL_V;
        break;
      case 'C':
        label = LABEL_CS;
//        state = MPPT_LABEL_1;
        break;
      case 'I':
//        state = MPPT_LABEL_1;
        label = LABEL_I;
        mppt_sign = 1;
        break;
      default:
        state = MPPT_IDLE;
      }
      break;
    case MPPT_LABEL_1:
      switch (c)
      {
      case 0x09:
        if ((label == LABEL_V) || (label == LABEL_I))
        {
          state = MPPT_DATA_0;
          mppt_data = 0;
        }
        else
        {
          state = MPPT_IDLE;
        }
        break;
      case 'L':
        state = MPPT_TAB;
        label = LABEL_IL;
        break;
      case 'S':
        state = MPPT_TAB;
//        label = LABEL_CS;
        break;
      case 'P':  // VPV?
      case 'h':  // checksum?
        state = MPPT_LABEL_2;
        break;
      default:
        state = MPPT_IDLE;
      }
      break;
    case MPPT_LABEL_2:
      switch (c)
      {
      case 'V':
        state = MPPT_TAB;
        label = LABEL_VPV;
        break;
      case 'e':
        state = MPPT_LABEL_3;
        break;
      default:
        state = MPPT_IDLE;
      }
      break;
    case MPPT_LABEL_3:
    case MPPT_LABEL_4:
    case MPPT_LABEL_5:
    case MPPT_LABEL_6:
    case MPPT_LABEL_7:
      if (c == sequence[state - MPPT_LABEL_3])
        state += 1;
      else
        state = MPPT_IDLE;
      break;
    case MPPT_LABEL_END:
      if (c == 9)
        state = MPPT_CHECKSUM;
      else
        state = MPPT_IDLE;
      break;
    case MPPT_CHECKSUM:
      if (mppt_sum == 0)
      {
        // valid frame received
        // indicate valid data
        if (serial[port].sbuf_read_pos == 16)
        {
          mppt.size = sizeof(mppt) - sizeof(mppt.size);
          // only copy data, don't stall RX for too long
          SEND_MPPT(port);
        }
      }
      state = MPPT_FRAME;
      break;
    case MPPT_TAB:
      if (c == 0x09)
      {
        state = MPPT_DATA_0;
        mppt_data = 0;
      }
      else
        state = MPPT_IDLE;
      break;
    case MPPT_DATA_0:
      if (c == '-')
      {
        mppt_sign = -1;
        break;
      }
    case MPPT_DATA_1:
    case MPPT_DATA_2:
    case MPPT_DATA_3:
    case MPPT_DATA_4:
    case MPPT_DATA_5:
      if (c >= '0' && c <= '9')
      {
        mppt_data = mppt_data * 10 + c - '0';
        state += 1;
        break;
      }
      // fall through
    case MPPT_DATA_END:
      if (c == 0xd)
      {
        switch (label)
        { 
        case LABEL_V:
          mppt.batt_volt = mppt_data / 10;
          break;
        case LABEL_I:
          mppt.main_current = mppt_sign * mppt_data / 10;
          break;
        case LABEL_IL:
          mppt.load_current = mppt_sign * mppt_data / 10;
          break;
        case LABEL_VPV:
          mppt.sol_volt = mppt_data / 10;
          break;
        case LABEL_CS:
          mppt.status = LO_UINT16(mppt_data);
          break;
        default:
          // empty
          break;
        }
        state = MPPT_START;
      }
      else
        state = MPPT_IDLE;
      break;
    default:
      state = MPPT_FRAME;
    }
  }
}
#endif // MPPT_MODE_TEXT

#ifndef MPPT_TEST
// manage the Victron MPPT controller
// should be called in regular intervals to drive the data pump
void process_mppt(uint8 port, uint8 len)
{
  if (len)
  {
    //DEBUG_LED_ON;
    RECEIVE_MPPT(port);
    if (mppt.size)
    {
      mppt.size = 0;
#if UART_USE_CALLBACK   
      osal_set_event(blueBasic_TaskID, BLUEBASIC_EVENT_SERIAL<<(port == HAL_UART_PORT_1));
#endif
    }
    //DEBUG_LED_OFF;
  }
#if MPPT_MODE_HEX    
  else
  {
    long now = OS_get_millis();
    if (now - scan_time > SCAN_INTERVAL)
    {
      SEND_MPPT(port);
      scan_time = now;
    }
    if (now - request_time > REQUEST_INTERVAL)
    {
      request_mppt(port);
      request_time = now;
    }
  }
#endif
}

#else // MPPT_TEST

static uint16 read_counter = 0;

// --- test frame ---
//     PID 0xA040
//     FW  111
//     SER#  HQ1337Z6DQA
//     V 26810
//     I 10200
//     VPV 30510
//     PPV 282
//     CS  3
//     ERR 0
//     H19 3461
//     H20 117
//     H21 1113
//     H22 568
//     H23 1374
//     Checksum  ?
// --- end test frame ---
//3010 WRITE #SERIAL,13,10,80,73,68,9,48,120,65,48,52,65,13,10,70,87,9,49,49,54,13,10,83,69,82,35,9,72,81,49,55,53,48,89,70,78,53,82,13,10,86,9,50,55,54,57,48,13,10,73,9,52,52,48,48,13,10,86,80,86,9,51,49,51,48,48,13,10,80,80,86,9,49,50,53,
//3020 WRITE #SERIAL,13,10,67,83,9,51,13,10,69,82,82,9,48,13,10,72,49,57,9,54,55,52,49,13,10,72,50,48,9,53,53,13,10,72,50,49,9,49,54,54,13,10,72,50,50,9,49,48,54,13,10,72,50,51,9,51,49,56,13,10,72,83,68,83,9,56,52,13,10,67,104,101,99,107,115,117,109,9,106,

static uint8 test_frame[] = {
  13,10,80,73,68, 9,48,120,65,48,52,65,13,10,70,87,
  9,49,49,54,13,10,83, 69,82,35, 9,72,81,49,55,53,
  48,89,70,78,53,82,13, 10,86, 9,50,55,54,57,48,13,
  10,73, 9,52,52,48,48, 13,10,86,80,86, 9,51,49,51,
  48,48,13,10,80,80,86,  9,49,50,53,13,10,67,83, 9,
  51,13,10,69,82,82, 9, 48,13,10,72,49,57, 9,54,55,
  52,49,13,10,72,50,48,  9,53,53,13,10,72,50,49, 9,
  49,54,54,13,10,72,50, 50, 9,49,48,54,13,10,72,50,
  51, 9,51,49,56,13,10, 72,83,68,83, 9,56,52,13,10,
  67,104,101,99,107,115,117,109,9,106,
  // frame with async data
  58,65,53,48,49,48,48,48,48,48,48,50,48,48,48,48,
  48,48,48,48,48,48,48,48,48,48,51,50,48,53,49,68,
  48,53,48,48,48,48,48,48,48,48,48,48,57,55,48,48,
  48,48,48,48,48,48,48,48,49,52,48,48,48,48,48,48,
  48,70,48,48,66,57,48,55,49,49,48,48,48,53,10,13,
  10,80,73,68,9,48,120,65,48,53,51,13,10,70,87,9,
  49,52,50,13,10,83,69,82,35,9,72,81,49,57,48,53,
  50,51,77,72,65,13,10,86,9,49,51,51,48,48,13,10,
  73,9,49,52,57,48,13,10,86,80,86,9,49,56,54,48,
  48,13,10,80,80,86,9,50,49,13,10,67,83,9,51,13,
  10,77,80,80,84,9,50,13,10,69,82,82,9,48,13,10,
  76,79,65,68,9,79,78,13,10,73,76,9,48,13,10,72,
  49,57,9,49,51,50,13,10,72,50,48,9,50,13,10,72,
  50,49,9,50,48,13,10,72,50,50,9,49,50,13,10,72,
  50,51,9,56,50,13,10,72,83,68,83,9,49,55,13,10,
  67,104,101,99,107,115,117,109,9,171,13,10,80,73,68,9
};

uint16 test_read(void* ptr, uint16 len)
{
  uint16 max = read_counter + len > sizeof(test_frame) ? sizeof(test_frame) - read_counter : len;
  printf("reading %d\n", read_counter);
  if (max)
    memcpy(ptr, test_frame + read_counter, max);
  read_counter += max;
  return max;
}

int main(void)
{
  printf("Starting to parse data...\n");
  for (read_counter = 0; read_counter < sizeof(test_frame); )
  {
    RECEIVE_MPPT(0);
  }
  SEND_MPPT(0);
  printf("v:   %d mV\n", mppt.batt_volt * 10);
  printf("vpv: %d mV\n", mppt.sol_volt * 10);
  printf("i:   %d mA\n", mppt.sol_current * 100);
  printf("cs:  %02x\n", mppt.status);
  return 0;
}

#endif // MPPT_TEST

