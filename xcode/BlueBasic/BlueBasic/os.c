//
//  os.c
//  BlueBasic
//
//  Created by tim on 7/13/14.
//  Copyright (c) 2014 tim. All rights reserved.
//

#include <stdio.h>
#include <unistd.h>
#include <signal.h>
#include <time.h>
#include "os.h"

// see main.c
extern char *flash_file;

// Timers
#define NR_TIMERS ((OS_MAX_TIMER) - 1)
struct
{
  unsigned short lineno;
  unsigned char repeat;
  uint32_t periode;
  uint32_t fireTime;
} timers[OS_MAX_TIMER];

os_discover_t blueBasic_discover;

static char alarmfire;
static char alarm_active = 0;
static unsigned char* bstart;
static unsigned char* bend;

extern unsigned char __store[];

void OS_prompt_buffer(unsigned char* start, unsigned char* end)
{
  bstart = start;
  bend = end;
}

char OS_prompt_available(void)
{
  char quote = 0;
  unsigned char* ptr = bstart;

  for (;;)
  {
    char c = getchar();
    switch (c)
    {
      case -1:
        if (feof(stdin))
        {
          return 0;
        }
        if (alarmfire)
        {
          //alarmfire = 0;
          uint32_t millis = OS_get_millis();
          //printf("tick %d\n", millis);
          for (unsigned char id = 0; id < OS_MAX_TIMER; id++)
          {
            if ((timers[id].lineno != 0) && (timers[id].fireTime <= millis))
            {
              printf("run timer %d:  millis=%d, fireTime=%d, periode=%d, repeat=%d \n", id, millis, timers[id].fireTime, timers[id].periode, timers[id].repeat);
              unsigned short lineno = timers[id].lineno;
              if (id != DELAY_TIMER && timers[id].repeat)
              {
                timers[id].fireTime += timers[id].periode;
              }
              else
              {
                timers[id].lineno = 0;
              }
              interpreter_run(lineno, id == DELAY_TIMER ? 0 : INTERPRETER_CAN_YIELD | INTERPRETER_CAN_RETURN);
            }
          }
//          timers[0].lineno && interpreter_run(timers[0].lineno, 1);
//          timers[1].lineno && interpreter_run(timers[1].lineno, 0);
        }
        break;
      case '\n':
        OS_timer_stop(DELAY_TIMER); // Stop autorun
        OS_putchar('\n');
        *ptr = '\n';
        return 1;
      default:
        if(ptr == bend)
        {
          OS_putchar('\b');
        }
        else
        {
          // Are we in a quoted string?
          if(c == quote)
          {
            quote = 0;
          }
          else if (c == '"' || c == '\'')
          {
            quote = c;
          }
          else if (quote == 0 && c >= 'a' && c <= 'z')
          {
            c = c + 'A' - 'a';
          }
          *ptr++ = c;
          OS_putchar(c);
        }
        break;
    }
  }
}

static void alarmhandler(int sig)
{
  alarmfire = 1;
}


void OS_timer_stop(unsigned char id)
{
  unsigned char stop = 1;
  timers[id].lineno = 0;
  timers[id].fireTime = 0;
  timers[id].periode = 0;
  timers[id].repeat = 0;
  for (unsigned char id = 0; id < OS_MAX_TIMER; id++)
  {
    if (timers[id].lineno)
      stop = 0;
  }
  if (stop)
  {
    alarm(0);
    alarmfire = 0;
    alarm_active = 0;
  }
}

char OS_timer_start(unsigned char id, unsigned long timeout, unsigned char repeat, unsigned short lineno)
{
  if (id >= OS_MAX_TIMER)
  {
    return 0;
  }
  printf("setting timer %d: timeout=%ld, repeat=%d, lineno=%d\n", id, timeout, repeat, lineno);
  timers[id].lineno = lineno;
  timers[id].periode = id == DELAY_TIMER ? 0 : (int32_t) timeout;
  timers[id].repeat = id == DELAY_TIMER ? 0 :  repeat;
  timers[id].fireTime = OS_get_millis() + (int32_t) timeout;
  //ualarm((useconds_t)(timeout * 1000), repeat ? (useconds_t)(timeout * 1000) : 0);
  // schedule a alam evry 1 us
  if (alarm_active == 0)
  {
    alarm_active = 1;
    struct sigaction act = { alarmhandler, 0, 0 };
    sigaction(SIGALRM, &act, NULL);
    ualarm((useconds_t)(1000), (useconds_t)(1000));
  }
  return 1;
}

// -- BLE placeholders

unsigned char GATTServApp_RegisterService(gattAttribute_t *pAttrs, uint16 numAttrs, uint8 encKeySize, CONST gattServiceCBs_t *pServiceCBs)
{
  return SUCCESS;
}

unsigned char GATTServApp_DeregisterService(unsigned short handle, void* attr)
{
  return SUCCESS;
}

unsigned char GATTServApp_InitCharCfg(unsigned short handle, gattCharCfg_t* charcfgtbl)
{
  return SUCCESS;
}

unsigned char GATTServApp_ProcessCharCfg( gattCharCfg_t *charCfgTbl, uint8 *pValue,
                                                uint8 authenticated, gattAttribute_t *attrTbl,
                                                uint16 numAttrs, uint8 taskId,
                                                pfnGATTReadAttrCB_t pfnReadAttrCB )
{
  return SUCCESS;
}

unsigned char GATTServApp_ProcessCCCWriteReq(unsigned short handle, gattAttribute_t* attr, unsigned char* value, unsigned char len, unsigned short offset, unsigned short validcfg)
{
  return SUCCESS;
}

unsigned char GAPRole_SetParameter( uint16 param, uint8 len, void *pValue )
{
  return SUCCESS;
}

unsigned char GAPRole_GetParameter( uint16 param, void *pValue )
{
  if (pValue)
  {
    *(unsigned short*)pValue = 0;
  }
  return SUCCESS;
}

unsigned char GAPBondMgr_SetParameter(unsigned short param, unsigned long value, unsigned char len, void* addr)
{
  return SUCCESS;
}

unsigned char GAPBondMgr_GetParameter(unsigned short param, unsigned long* shortValue, unsigned char len, void* longValue)
{
  if (shortValue)
  {
    *(unsigned short*)shortValue = 0;
  }
  else
  {
    memset(longValue, 0, len);
  }
  return SUCCESS;
}

unsigned char GGS_SetParameter(unsigned short param, unsigned char len, void* addr)
{
  return SUCCESS;
}

unsigned char HCI_EXT_SetTxPowerCmd(unsigned char power)
{
  return SUCCESS;
}

unsigned char HCI_EXT_SetRxGainCmd(unsigned char gain)
{
  return SUCCESS;
}

unsigned char GAPObserverRole_StartDiscovery(unsigned char mode, unsigned char active, unsigned char whitelist)
{
  return SUCCESS;
}

unsigned char GAPObserverRole_CancelDiscovery(void)
{
  return SUCCESS;
}

unsigned char GAPRole_TerminateConnection(void)
{
  return SUCCESS;
}

extern bStatus_t GAP_SetParamValue( gapParamIDs_t paramID, uint16 paramValue )
{
  return SUCCESS;
}

void OS_flashstore_init(void)
{
  FILE* fp = fopen(flash_file, "r");
  if (fp)
  {
    fread(__store, FLASHSTORE_LEN, sizeof(char), fp);
    fclose(fp);
  }
  else
  {
    int lastage = 1;
    const unsigned char* ptr;
    memset(__store, 0xFF, FLASHSTORE_LEN);
    for (ptr = __store; ptr < &__store[FLASHSTORE_LEN]; ptr += FLASHSTORE_PAGESIZE)
    {
      *(int*)ptr = lastage++;
    }
    
  }
}

void OS_flashstore_write(unsigned long faddr, unsigned char* value, unsigned short sizeinwords)
{
  memcpy(&__store[faddr << 2], value, sizeinwords << 2);
  FILE* fp = fopen(flash_file, "w");
  fwrite(__store, FLASHSTORE_LEN, sizeof(char), fp);
  fclose(fp);
}

void OS_flashstore_erase(unsigned long page)
{
  memset(&__store[page << 11], 0xFF, FLASHSTORE_PAGESIZE);
  FILE* fp = fopen(flash_file, "w");
  fwrite(__store, FLASHSTORE_LEN, sizeof(char), fp);
  fclose(fp);
}

unsigned char OS_serial_open(unsigned char port, unsigned long baud, unsigned char parity, unsigned char bits, unsigned char stop, unsigned char flow, unsigned short onread, unsigned short onwrite)
{
  return 0;
}

unsigned char OS_serial_close(unsigned char port)
{
  return 0;
}

short OS_serial_read(unsigned char port)
{
  return 255;
}

unsigned char OS_serial_write(unsigned char port, unsigned char ch)
{
  return 0;
}

unsigned char OS_serial_available(unsigned char port, unsigned char ch)
{
  return 0;
}

int16 OS_get_temperature(uint8 wait) {
  return 2000;
}

uint32_t OS_get_millis(void) {
  static uint32_t start_millis = 0xffffffff;
  struct timespec t;
  clock_gettime(CLOCK_MONOTONIC, &t);
  uint32_t millis = (uint32_t)(t.tv_sec * 1000 + t.tv_nsec / 1000 / 1000);
  if (start_millis == 0xffffffff) {
    start_millis = millis;
  }
  return millis - start_millis;
}

void OS_init(void) {
  // initialize start time
  OS_get_millis();
}

int8 OS_get_vdd_7(void) {
  //  mV * 127 / 3720
  return (3300 * 127 / 3720);
}
