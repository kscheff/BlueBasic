/*
 * rapid.c getting serial data from WCS Rapid LPB 60 Ladebooster
 * BlueBattery BBX, kai@blue-battery.com
 * (C) 2022 Kai Scheffer, Switzerland
 */
 
#include "os.h"
#include "hal_uart.h"
#include <stddef.h>
#include <math.h>
   
#define RAPID_BUFFER 0
   
#define CRC16Polynom    0x1021
#define CRC16Startwert  0xFFFF
#define byte uint8

uint8 const crc16_Hi[256] = { 
 0, 16, 32, 48, 64, 80, 96, 112, 129, 145, 161, 177, 193, 209, 225, 241, 
 18, 2, 50, 34, 82, 66, 114, 98, 147, 131, 179, 163, 211, 195, 243, 227, 
 36, 52, 4, 20, 100, 116, 68, 84, 165, 181, 133, 149, 229, 245, 197, 213, 
 54, 38, 22, 6, 118, 102, 86, 70, 183, 167, 151, 135, 247, 231, 215, 199, 
 72, 88, 104, 120, 8, 24, 40, 56, 201, 217, 233, 249, 137, 153, 169, 185, 
 90, 74, 122, 106, 26, 10, 58, 42, 219, 203, 251, 235, 155, 139, 187, 171, 
 108, 124, 76, 92, 44, 60, 12, 28, 237, 253, 205, 221, 173, 189, 141, 157, 
 126, 110, 94, 78, 62, 46, 30, 14, 255, 239, 223, 207, 191, 175, 159, 143, 
 145, 129, 177, 161, 209, 193, 241, 225, 16, 0, 48, 32, 80, 64, 112, 96, 
 131, 147, 163, 179, 195, 211, 227, 243, 2, 18, 34, 50, 66, 82, 98, 114, 
 181, 165, 149, 133, 245, 229, 213, 197, 52, 36, 20, 4, 116, 100, 84, 68, 
 167, 183, 135, 151, 231, 247, 199, 215, 38, 54, 6, 22, 102, 118, 70, 86, 
 217, 201, 249, 233, 153, 137, 185, 169, 88, 72, 120, 104, 24, 8, 56, 40, 
 203, 219, 235, 251, 139, 155, 171, 187, 74, 90, 106, 122, 10, 26, 42, 58, 
 253, 237, 221, 205, 189, 173, 157, 141, 124, 108, 92, 76, 60, 44, 28, 12, 
 239, 255, 207, 223, 175, 191, 143, 159, 110, 126, 78, 94, 46, 62, 14, 30, 
 };

uint8 const crc16_Lo[256] = { 
 0, 33, 66, 99, 132, 165, 198, 231, 8, 41, 74, 107, 140, 173, 206, 239, 
 49, 16, 115, 82, 181, 148, 247, 214, 57, 24, 123, 90, 189, 156, 255, 222, 
 98, 67, 32, 1, 230, 199, 164, 133, 106, 75, 40, 9, 238, 207, 172, 141, 
 83, 114, 17, 48, 215, 246, 149, 180, 91, 122, 25, 56, 223, 254, 157, 188, 
 196, 229, 134, 167, 64, 97, 2, 35, 204, 237, 142, 175, 72, 105, 10, 43, 
 245, 212, 183, 150, 113, 80, 51, 18, 253, 220, 191, 158, 121, 88, 59, 26, 
 166, 135, 228, 197, 34, 3, 96, 65, 174, 143, 236, 205, 42, 11, 104, 73, 
 151, 182, 213, 244, 19, 50, 81, 112, 159, 190, 221, 252, 27, 58, 89, 120, 
 136, 169, 202, 235, 12, 45, 78, 111, 128, 161, 194, 227, 4, 37, 70, 103, 
 185, 152, 251, 218, 61, 28, 127, 94, 177, 144, 243, 210, 53, 20, 119, 86, 
 234, 203, 168, 137, 110, 79, 44, 13, 226, 195, 160, 129, 102, 71, 36, 5, 
 219, 250, 153, 184, 95, 126, 29, 60, 211, 242, 145, 176, 87, 118, 21, 52, 
 76, 109, 14, 47, 200, 233, 138, 171, 68, 101, 6, 39, 192, 225, 130, 163, 
 125, 92, 63, 30, 249, 216, 187, 154, 117, 84, 55, 22, 241, 208, 179, 146, 
 46, 15, 108, 77, 170, 139, 232, 201, 38, 7, 100, 69, 162, 131, 224, 193, 
 31, 62, 93, 124, 155, 186, 217, 248, 23, 54, 85, 116, 147, 178, 209, 240, 
 };

typedef struct
{
  uint8 start;
  uint8 id;
  uint8 u_output_lsb;  // 10mV per bit
  uint8 u_output_msb;
  uint8 u_input_lsb; // 10mV per bit
  uint8 u_input_msb;
  uint8 i_out_lsb;  // 0.1A per bit
  uint8 i_out_msb;
  uint8 error; // bit 7:error, 0..6 code
  uint8 res[4];
  uint8 phase; // bit 0:bulk, 1:Absorb, 2:Float; 3:Care
  uint8 status;
  uint8 parity;
} booster_frame_t;

static uint8 CRC16H;
static uint8 CRC16L;

static void BerechneCRC16(byte* dataPTR, int size)
{
  byte uIndex;
  for (int i = 0; i < size; i++)
  {
    uIndex = (byte)(CRC16H ^ *(dataPTR + i));
    CRC16H = (byte)(CRC16L ^ crc16_Hi[uIndex]);
    CRC16L = crc16_Lo[uIndex];
  }
}

//static bool PruefeCRC16(byte* dataPTR, int size)
//{
//  CRC16H = HI_UINT16(CRC16Startwert);
//  CRC16L = LO_UINT16(CRC16Startwert);
//  BerechneCRC16(dataPTR, size - 2);
//  byte* bytePTR = dataPTR + size - 2;
//  if (*bytePTR != CRC16L)
//    return false;
//  bytePTR++;
//  if (*bytePTR != CRC16H)
//    return false;
//  return true;
//}
//
//void SetzeCRC16(byte* dataPTR, int size)
//{
//  BerechneCRC16(dataPTR, size - 2);
//  byte* bytePTR = dataPTR + size;
//  bytePTR--;
//  *bytePTR = CRC16H;
//  bytePTR--;
//  *bytePTR = CRC16L;
//}

 
typedef enum  __attribute__ ((__packed__))
{
  RAPID_IDLE,
  RAPID_SKIP,
  RAPID_FLOAT,
  RAPID_CRCL,
  RAPID_CRCH
} state_rapid_t;

static state_rapid_t state;

typedef union
{
  float f;
  uint8 b[4];
} convert_float_t;

typedef struct
{
  convert_float_t i_slave;
  convert_float_t u_in;
  convert_float_t u_out;
  convert_float_t i_out;
} messwerte_t;

static union
{
  messwerte_t messwerte;
  convert_float_t f[sizeof(messwerte_t) / sizeof(float)];
} data;

static void send_as_vot(uint8 port)
{
  // directly copy data to serial buffer
  booster_frame_t* booster_frame = (booster_frame_t*)serial[port].sbuf;
  booster_frame->start = 0xAA;
  booster_frame->id = 0x7A;
  
  // messwerte alread limited to positive numbers
  uint16 input = (uint16)(data.messwerte.u_in.f * 100); // should always be >= 0
  uint16 output = (uint16)(data.messwerte.u_out.f * 100); // should always be >= 0
  int16 current = (int16)((data.messwerte.i_out.f + data.messwerte.i_slave.f) * 10);
  // current could be negative when Rapid reverse charges the starter battery
  // so we clip negative values 
  //current = current > 0 ? current : 0;
  
  booster_frame->u_input_lsb = LO_UINT16(input);
  booster_frame->u_input_msb = HI_UINT16(input);
  booster_frame->u_output_lsb = LO_UINT16(output);
  booster_frame->u_output_msb = HI_UINT16(output);
  booster_frame->i_out_lsb = LO_UINT16(current);
  booster_frame->i_out_msb = HI_UINT16(current);

  // Victron MPPT Status (0x0201)  
  // 0 not charging
  // 2 failure 
  // 3 bulk   (full current)
  // 4 absobtion (voltage limit)
  // 5 float  (voltage limit)
  // 7 equalize (voltage limit)
  // 252 ESS (voltage controlled from external)
  // 255 unavailable  
  
  serial[port].sbuf_read_pos = 0;
#if UART_USE_CALLBACK   
  osal_set_event(blueBasic_TaskID, BLUEBASIC_EVENT_SERIAL<<(port == HAL_UART_PORT_1));
#endif
}

void process_rapid(uint8 port, uint8 len)
{
  static uint8 cnt;
  static uint8 fcnt;
  static uint8 bcnt;
  uint8 c;
  while ( HalUARTRead(port, &c, sizeof(c)) )
  {
    cnt += 1;
    switch (state)
    {
    case RAPID_IDLE:
      if (c == 'S')
      {
        state = RAPID_SKIP;
        cnt = 0;
        CRC16H = 0xff;
        CRC16L = 0xff;
        fcnt = 0;
      }
      break;
    case RAPID_SKIP:
      if ( cnt == offsetof(TypMesswerte, SlaveStromAufbaubatterie) ||
           cnt == offsetof(TypMesswerte, SpannungStartbatterieMessleitung) ||
           cnt == offsetof(TypMesswerte, SpannungAufbaubatterieMessleitung) ||
           cnt == offsetof(TypMesswerte, StromAufbaubatterie) )
      {
        state = RAPID_FLOAT;
        bcnt = 0;
        data.f[fcnt].b[bcnt] = c;
      }
      if ( cnt == offsetof(TypMesswerte, CRC16) - 1 )
      {
        state = RAPID_CRCL;
      }
      break;
    case RAPID_FLOAT:
      {
        data.f[fcnt].b[++bcnt] = c;
        if (bcnt >= sizeof(float) - 1)
        {
          state = RAPID_SKIP;
          fcnt++;
          // limit to positive numbers to save later the compares
          //*pmesswert = pnumber->f > 0 ? pnumber->f : 0;
        }
      }
      break;
    case RAPID_CRCL:
      state = (c == CRC16L) ? RAPID_CRCH : RAPID_IDLE;
      break;
    case RAPID_CRCH:
      if ( c == CRC16H )
      {
        send_as_vot(port);
      }
      state = RAPID_IDLE;
      break;
    }
    if ( state != RAPID_CRCH )
    {
      BerechneCRC16(&c, sizeof(c));
    }
  }
}

//Daten vom LBR anfordern mit 0x53 (1 Byte)