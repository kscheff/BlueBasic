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

// set how many MPPT devices should be supported
#ifndef MPPT_DEVICES
#define MPPT_DEVICES OS_MAX_SERIAL
#endif

#if MPPT_MODE_TEXT
#define RECEIVE_MPPT(port, len) receive_text(port, len)
#else
#define RECEIVE_MPPT(port) receive_mppt(port)
#endif

#if !MPPT_AS_VOT
// send MPPT data
#define SEND_MPPT(a) send_app(a)
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

#if 0
#define DEBUG_LED_ON  P1 |=  0x08
#define DEBUG_LED_OFF P1 &= ~0x08
#else
#define DEBUG_LED_ON 
#define DEBUG_LED_OFF
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

static mppt_t mppt[MPPT_DEVICES]; // = { sizeof(mppt[0]) - sizeof(mppt[0].size),0,0,0,0,0 };

#if 0
// report UART input buffer size high level as solar voltage 1V = 1 byte 
static uint16 test_high_watermark = 0;
#define TEST_WATERMARK_SET(len) if ((100U*len) > test_high_watermark) test_high_watermark = (100U*len)
#define TEST_WATERMARK_CLEAR if (test_high_watermark) test_high_watermark -= 10
#define TEST_WATERMARK_REPORT mppt.sol_volt = test_high_watermark
#else
#define TEST_WATERMARK_SET(len)
#define TEST_WATERMARK_CLEAR
#define TEST_WATERMARK_REPORT
#endif

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
  MPPT_IDLE,
  MPPT_CHECKSUM,
  MPPT_HEX_RECORD,
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

#if MPPT_MODE_HEX
static state_mppt_t state;
#endif
//static uint8 mppt_sum;

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
static void send_app(uint8 port)
{
#if MPPT_DEVICES > 1  
  mppt_t* pmppt = &mppt[port];
#else
  mppt_t* pmppt = &mppt[0];
#endif  
  pmppt->size = sizeof(mppt[0]) - sizeof(mppt[0].size);
  serial[port].sbuf_read_pos = 0;
  OS_memcpy(serial[port].sbuf, pmppt, sizeof(mppt_t));
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
#if MPPT_DEVICES > 1  
  mppt_t* pmppt = &mppt[port];
#else
  mppt_t* pmppt = &mppt[0];
#endif  
  // directly copy data to serial buffer
  sol_frame_t* sol_frame = (sol_frame_t*)serial[port].sbuf;
  sol_frame->start = 0xAA;
  sol_frame->id = 0x1A;
  sol_frame->u_batt_lsb = LO_UINT16(pmppt->batt_volt);
  sol_frame->u_batt_msb = HI_UINT16(pmppt->batt_volt);
  sol_frame->u_panel_lsb = LO_UINT16(pmppt->sol_volt);
  sol_frame->u_panel_msb = HI_UINT16(pmppt->sol_volt);
  int16 sol_current = (pmppt->main_current + pmppt->load_current + 5) / 10;
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
  switch (pmppt->status)
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
          pmppt->sol_current = SWAP_UINT16(mppt_data) / 100;
          break;
        case 0xbbed:  // solar panel voltage
          pmppt->sol_volt = SWAP_UINT16(mppt_data)/10;
          break;
        case 0xd5ed:  // battery voltage
          pmppt->batt_volt = SWAP_UINT16(mppt_data)/10;
          break;
        case 0x0102:  // status
          pmppt->status = LO_UINT16(mppt_data);
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

//  static label_t label;

typedef struct 
{
  label_t label;
  int32 data;
  int8 sign;
  state_mppt_t state; // = MPPT_IDLE;
  uint8 cksum;  
} mppt_device_t;


static void receive_text(uint8 port, uint8 len)
{
  static const uint8 sequence[] = { 'c','k','s','u','m' };
  uint8 in_buf[16];
#if MPPT_DEVICES > 1
  static mppt_device_t mppt_devices[MPPT_DEVICES] = {
    {LABEL_NONE,0,0,MPPT_IDLE},
    {LABEL_NONE,0,0,MPPT_IDLE}
  };
  mppt_device_t text = mppt_devices[port]; 
#else
  static mppt_device_t text = {LABEL_NONE,0,0,MPPT_IDLE};
#endif
  
  while (len = (uint8)HalUARTRead(port, in_buf, sizeof(in_buf)) )
  {
    uint8 *ptr = in_buf;
    while (len--)
    {
      uint8 c = *ptr++;
      if ( (c == ':') && (text.state != MPPT_CHECKSUM) )
      {
        DEBUG_LED_ON;
        text.state = MPPT_HEX_RECORD;
      }
      if (text.state != MPPT_HEX_RECORD)
      {
        text.cksum += c;
      }
      switch (text.state)
      {
      case MPPT_HEX_RECORD:
        switch (c)
        {
        case '\n':
        case '\r':
          text.state = MPPT_IDLE;
          text.cksum = 0;
          DEBUG_LED_OFF;
        }
        break;
      case MPPT_IDLE:
        DEBUG_LED_OFF;
        if (c == '\n')
          text.state = MPPT_LABEL_0;
        break;
      case MPPT_LABEL_0:
        text.state = MPPT_LABEL_1;
        switch (c)
        {
        case 'V':
          text.label = LABEL_V;
          break;
        case 'C':
          text.label = LABEL_CS;
          //        state = MPPT_LABEL_1;
          break;
        case 'I':
          //        state = MPPT_LABEL_1;
          text.label = LABEL_I;
          text.sign = 1;
          break;
        default:
          text.state = MPPT_IDLE;
        }
        break;
      case MPPT_LABEL_1:
        switch (c)
        {
        case '\t':
          if ((text.label == LABEL_V) || (text.label == LABEL_I))
          {
            text.state = MPPT_DATA_0;
            text.data = 0;
          }
          else
          {
            text.state = MPPT_IDLE;
          }
          break;
        case 'L':
          text.state = MPPT_TAB;
          text.label = LABEL_IL;
          break;
        case 'S':
          text.state = MPPT_TAB;
          //        label = LABEL_CS;
          break;
        case 'P':  // VPV?
        case 'h':  // checksum?
          text.state = MPPT_LABEL_2;
          break;
        default:
          text.state = MPPT_IDLE;
        }
        break;
      case MPPT_LABEL_2:
        switch (c)
        {
        case 'V':
          text.state = MPPT_TAB;
          text.label = LABEL_VPV;
          break;
        case 'e':
          text.state = MPPT_LABEL_3;
          break;
        default:
          text.state = MPPT_IDLE;
        }
        break;
      case MPPT_LABEL_3:
      case MPPT_LABEL_4:
      case MPPT_LABEL_5:
      case MPPT_LABEL_6:
      case MPPT_LABEL_7:
        if (c == sequence[text.state - MPPT_LABEL_3])
          text.state += 1;
        else
          text.state = MPPT_IDLE;
        break;
      case MPPT_LABEL_END:
        if (c == '\t') {
          text.state = MPPT_CHECKSUM;
          DEBUG_LED_ON;
        }
        else
          text.state = MPPT_IDLE;
        break;
      case MPPT_CHECKSUM:
        text.state = MPPT_IDLE;
        if (text.cksum == 0)
        {
          // valid frame received
          // copy valid data if there is space
          if (serial[port].sbuf_read_pos == 16)
          {
            TEST_WATERMARK_REPORT;
            // only copy data, don't stall RX for too long
            SEND_MPPT(port);
          }
        }
        text.cksum = 0;
        break;
      case MPPT_TAB:
        if (c == '\t')
        {
          text.state = MPPT_DATA_0;
          text.data = 0;
        }
        else
          text.state = MPPT_IDLE;
        break;
      case MPPT_DATA_0:
        if (c == '-')
        {
          text.sign = -1;
          break;
        }
        // fallthrough
      case MPPT_DATA_1:
      case MPPT_DATA_2:
      case MPPT_DATA_3:
      case MPPT_DATA_4:
      case MPPT_DATA_5:
        if (c >= '0' && c <= '9')
        {
          text.data = text.data * 10 + c - '0';
          text.state += 1;
          break;
        }
        // fall through
      case MPPT_DATA_END:
        if (c == '\r')
        {
#if MPPT_DEVICES > 1
          mppt_t *m = &mppt[port];
#else
          mppt_t *m = &mppt[0];
#endif          
          DEBUG_LED_ON;
          switch (text.label)
          { 
          case LABEL_V:
            m->batt_volt = text.data / 10;
            break;
          case LABEL_I:
            m->main_current = text.sign * text.data / 10;
            break;
          case LABEL_IL:
            m->load_current = text.sign * text.data / 10;
            break;
          case LABEL_VPV:
            m->sol_volt = text.data / 10;
            break;
          case LABEL_CS:
            m->status = LO_UINT16(text.data);
            break;
          default:
            // empty
            break;
          }
        }
        text.state = MPPT_IDLE;
        break;
      default:
        text.state = MPPT_IDLE;
      }
    }
  }  
#if MPPT_DEVICES > 1
  mppt_devices[port] = text;
#endif
}
#endif // MPPT_MODE_TEXT

#ifndef MPPT_TEST
// manage the Victron MPPT controller
// should be called in regular intervals to drive the data pump
void process_mppt(uint8 port, uint8 len)
{
  if (len)
  {
    TEST_WATERMARK_SET(len);
    RECEIVE_MPPT(port, len);

    if (serial[port].sbuf_read_pos == 0)
    {
      TEST_WATERMARK_CLEAR;
#if UART_USE_CALLBACK   
      osal_set_event(blueBasic_TaskID, BLUEBASIC_EVENT_SERIAL<<(port == HAL_UART_PORT_1));
#endif
    }
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

