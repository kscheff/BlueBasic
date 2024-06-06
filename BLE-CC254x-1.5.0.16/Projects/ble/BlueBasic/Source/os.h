////////////////////////////////////////////////////////////////////////////////
// BlueBasic
////////////////////////////////////////////////////////////////////////////////
//
// Authors:
//      Tim Wilkinson <timjwilkinson@gmail.com>
//
// os.h
//


#define kVersion "v0.16"

#ifndef kProduct
#define kProduct "BlueBasic"
#endif

#ifndef kMfrName
#define kMfrName "https://blue-battery.com"
#endif

#if !__APPLE__
#define int32_t long int  // 32 bit on the IAR EW8051
#define uint32_t unsigned long int
#define int16_t short     // 16 bit on the IAR EW8051
#define uint16_t unsigned short
#endif

#if __APPLE__

#include <stdio.h>
#include <stdlib.h>
#include <memory.h>
#include <time.h>
#include <assert.h>

#define __data

#define BLUEBASIC_MEM 8192
#define FLASHSTORE_NRPAGES flashstore_nrpages

#define SIMULATE_PINS   1
#define ENABLE_PORT0    1
#define ENABLE_PORT1    1
#define SIMULATE_FLASH  1

#define OS_memset(A, B, C)    memset(A, B, C)
#define OS_memcpy(A, B, C)    memcpy(A, B, C)
#define OS_rmemcpy(A, B, C)   memmove(A, B, C)
#define OS_srand(A)           srandom(A)
#define OS_rand()             random()
#define OS_malloc(A)          malloc(A)
#define OS_free(A)            free(A)
#define OS_putchar(A)         putchar(A)
#define OS_breakcheck()       (0)
#define OS_reboot(F)
#define OS_set_millis(V)      do { } while ((void)(V), 0)
#define OS_interrupt_attach(A, B) 0
#define OS_interrupt_detach(A)    0
#define OS_delaymicroseconds(A) do { } while ((void)(A), 0)
#define OS_yield(A)

extern void OS_prompt_buffer(unsigned char* start, unsigned char* end);
extern char OS_prompt_available(void);
extern void OS_timer_stop(unsigned char id);
extern char OS_timer_start(unsigned char id, unsigned long timeout, unsigned char repeat, unsigned short lineno);
extern void OS_flashstore_init(void);
extern void OS_flashstore_write(unsigned long faddr, unsigned char* value, unsigned short sizeinwords);
extern void OS_flashstore_erase(unsigned long page);
extern void OS_init(void);
extern uint32_t OS_get_millis(void);

// command line option from main.c
extern unsigned char flashstore_nrpages;

#define OS_MAX_TIMER              4
#define BLUEBASIC_EVENT_TIMER     0x0001
#define DELAY_TIMER               3
#define OS_MAX_INTERRUPT          1
#define BLUEBASIC_EVENT_INTERRUPT 0x0100
#define OS_AUTORUN_TIMEOUT        5000

// Simulate various BLE structures and values

#define GATT_PERMIT_READ        1
#define GATT_PERMIT_WRITE       2
#define GATT_PERMIT_AUTHEN_READ 4
#define GATT_PERMIT_AUTHEN_WRITE 8

#define GATT_PROP_BCAST         1
#define GATT_PROP_READ          2
#define GATT_PROP_WRITE_NO_RSP  4
#define GATT_PROP_WRITE         8
#define GATT_PROP_NOTIFY        16
#define GATT_PROP_INDICATE      32
#define GATT_PROP_AUTHEN        64
#define GATT_PROP_EXTENDED      128

#define GATT_CLIENT_CFG_NOTIFY  1

#define SUCCESS 0
#define FAILURE 1
#define INVALID_TASK_ID 0
#define GATT_MAX_NUM_CONN 1

#define GAP_ADTYPE_FLAGS                      0x01

#define GAP_ADTYPE_16BIT_COMPLETE             0x03
#define GAP_ADTYPE_32BIT_COMPLETE             0x05
#define GAP_ADTYPE_128BIT_COMPLETE            0x07
#define GAP_ADTYPE_LOCAL_NAME_SHORT           0x08
#define GAP_ADTYPE_LOCAL_NAME_COMPLETE        0x09

#define GAP_ADTYPE_FLAGS_LIMITED              0x01
#define GAP_ADTYPE_FLAGS_GENERAL              0x02
#define GAP_ADTYPE_FLAGS_BREDR_NOT_SUPPORTED  0x04

#define TGAP_GEN_DISC_ADV_MIN                 0
#define TGAP_LIM_ADV_TIMEOUT                  1
#define TGAP_GEN_DISC_SCAN                    2
#define TGAP_LIM_DISC_SCAN                    3
#define TGAP_LIM_DISC_ADV_INT_MIN             6
#define TGAP_LIM_DISC_ADV_INT_MAX             7
#define TGAP_GEN_DISC_ADV_INT_MIN             8
#define TGAP_GEN_DISC_ADV_INT_MAX             9
#define TGAP_FILTER_ADV_REPORTS               35

#define GAP_DEVICE_NAME_LEN                   21
#define GGS_DEVICE_NAME_ATT                   0

#define HCI_EXT_TX_POWER_MINUS_23_DBM         0
#define HCI_EXT_TX_POWER_MINUS_6_DBM          1
#define HCI_EXT_TX_POWER_0_DBM                2
#define HCI_EXT_TX_POWER_4_DBM                3

#define LINKDB_STATUS_UPDATE_NEW              0
#define LINKDB_STATUS_UPDATE_REMOVED          1
#define LINKDB_STATUS_UPDATE_STATEFLAGS       2

#define linkDB_Up(A)                          0
#define linkDB_State(A, B)                    0

#define GATT_MIN_ENCRYPT_KEY_SIZE          7  //!< GATT Minimum Encryption Key Size
#define GATT_MAX_ENCRYPT_KEY_SIZE          16 //!< GATT Maximum Encryption Key Size

typedef struct
{
  unsigned char len;
  const unsigned char *uuid;
} gattAttrType_t;

typedef struct
{
  gattAttrType_t type;
  unsigned char permissions;
  unsigned short handle;
  unsigned char* const pValue;
} gattAttribute_t;

typedef struct
{
  void* read;
  void* write;
  void* auth;
} gattServiceCBs_t;

typedef struct gattCharCfg
{
  unsigned short connhandle;
  unsigned char value;
} gattCharCfg_t;

#define bool unsigned char
#define uint8 unsigned char
#define uint16 unsigned short
#define int8 char
#define int16 short
#define FALSE 0
#define TRUE 1
#define linkDBNumConns 1
#define CONST
#define bStatus_t unsigned char
typedef unsigned char (*pfnGATTReadAttrCB_t)( uint16 connHandle, gattAttribute_t *pAttr,
                                         uint8 *pValue, uint8 *pLen, uint16 offset,
                                         uint8 maxLen, uint8 method );
typedef uint16 gapParamIDs_t;

typedef unsigned char halIntState_t;
#define HAL_ENTER_CRITICAL_SECTION(x) (void)x
#define HAL_EXIT_CRITICAL_SECTION(x)  (void)x
#define HAL_CRITICAL_STATEMENT(x)       (x;)

#define OS_enable_sleep(x)

extern unsigned char GATTServApp_RegisterService(gattAttribute_t *pAttrs, uint16 numAttrs, uint8 encKeySize, CONST gattServiceCBs_t *pServiceCBs);
extern unsigned char GATTServApp_DeregisterService(unsigned short handle, void* attr);
extern unsigned char GATTServApp_InitCharCfg(unsigned short handle, gattCharCfg_t* charcfgtbl);
extern unsigned char GATTServApp_ProcessCharCfg( gattCharCfg_t *charCfgTbl, uint8 *pValue,
                                            uint8 authenticated, gattAttribute_t *attrTbl,
                                            uint16 numAttrs, uint8 taskId,
                                            pfnGATTReadAttrCB_t pfnReadAttrCB );
extern unsigned char GATTServApp_ProcessCCCWriteReq(unsigned short handle, gattAttribute_t* attr, unsigned char* value, unsigned char len, unsigned short offset, unsigned short validcfg);
extern unsigned char GAPRole_SetParameter( uint16 param, uint8 len, void *pValue );
extern unsigned char GAPRole_GetParameter( uint16 param, void *pValue );
extern unsigned char GAPRole_TerminateConnection(void);
extern unsigned char GGS_SetParameter(unsigned short param, unsigned char len, void* addr);
extern unsigned char GAPBondMgr_SetParameter(unsigned short param, unsigned long value, unsigned char len, void* addr);
extern unsigned char GAPBondMgr_GetParameter(unsigned short param, unsigned long* shortValue, unsigned char len, void* longValue);
extern unsigned char HCI_EXT_SetTxPowerCmd(unsigned char power);
extern unsigned char HCI_EXT_SetRxGainCmd(unsigned char gain);
extern unsigned char GAPObserverRole_StartDiscovery(unsigned char mode, unsigned char active, unsigned char whitelist);
extern unsigned char GAPObserverRole_CancelDiscovery(void);
extern bStatus_t GAP_SetParamValue( gapParamIDs_t paramID, uint16 paramValue );

#define osal_run_system()

#define SEMAPHORE_READ_WAIT()
#define SEMAPHORE_READ_SIGNAL()
#define SEMAPHORE_CONN_WAIT()
#define SEMAPHORE_CONN_SIGNAL()
#define SEMAPHORE_FLASH_WAIT()
#define SEMAPHORE_FLASH_SIGNAL()
#define SEMAPHORE_INPUT_WAIT()
#define SEMAPHORE_INPUT_SIGNAL()

#define OS_MAX_SERIAL 2
#define ENABLE_BLE_CONSOLE      1

#else /* __APPLE__ --------------------------------------------------------------------------- */

#include "OSAL.h"
#include "hal_board.h"
#include "gatt.h"
#include "gattservapp.h"
#include "gapgattserver.h"
#ifdef GAP_BOND_MGR
#include "gapbondmgr.h"
#endif
#include "osal_snv.h"
#include "OnBoard.h"
#include "gap.h"
#include "observer.h"
#include "peripheral.h"
#include "linkdb.h"
#include "hci.h"
#include "hal_flash.h"
#include "timestamp.h"

#ifdef PROCESS_MPPT
#include "victron_mppt.h"
#endif

#ifdef PROCESS_RAPID
#include "rapid.h"
#endif

#if HAL_I2C_MASTER
#include "hal_i2c.h"
#define OS_I2C_INIT HalI2CInit(i2cClock_267KHZ)
#define OS_I2C_READ(adr, len, pBuf) HalI2CRead(adr, len, pBuf)
#define OS_I2C_WRITE(adr, len, pBuf) HalI2CWrite(adr, len, pBuf)
#endif

// Configurations
#ifdef TARGET_PETRA

#define TARGET_CC2540           1
#define ENABLE_DEBUG_INTERFACE  1
#define ENABLE_LOWPOWER_CLOCK   1
#define ENABLE_BLE_CONSOLE      1
#define ENABLE_FAKE_OAD_PROFILE 1
#define ENABLE_PORT0            1
#define ENABLE_PORT1            1
#define FEATURE_BOOST_CONVERTER P2_0

#else // TARGET_PETRA

#ifndef RUNTIME_ONLY
#define ENABLE_BLE_CONSOLE      1
#define ENABLE_FAKE_CONSOLE_PROFILE 1
#else
#define ENABLE_BLE_CONSOLE      0
#endif
#define ENABLE_INTERRUPT        0
#define ENABLE_FAKE_OAD_PROFILE 0
#define ENABLE_PORT0            1
#define ENABLE_PORT1            1
#define ENABLE_PORT2            1
#define ENABLE_YIELD            1

#define YIELD_TIMEOUT_MS_SLOW   20
#define YIELD_TIMEOUT_MS_NORMAL 10
#define YIELD_TIMEOUT_MS_FAST 5

#endif // TARGET_PETRA

#ifndef ENABLE_FAKE_CONSOLE_PROFILE
#define ENABLE_FAKE_CONSOLE_PROFILE 0
#endif

#ifndef ENABLE_INTERRUPT
#define ENABLE_INTERRUPT 1
#endif

#if TARGET_CC2540 || TARGET_CC2541
#define TARGET_CC254X   1
#else
#error "Unknown TARGET_XXX"
#endif

extern unsigned char blueBasic_TaskID;

__no_init __data uint8 JumpToImageAorB @ 0x09;

// Task Events
#define BLUEBASIC_START_DEVICE_EVT 0x0001
#define BLUEBASIC_CONNECTION_EVENT 0x0002
#define BLUEBASIC_INPUT_AVAILABLE 0x0004

#if HAL_UART 
#if (HAL_UART_ISR == 2 || HAL_UART_DMA == 2) && (HAL_UART_DMA == 1 || HAL_UART_ISR == 1)
#define OS_MAX_SERIAL             2
#else
#define OS_MAX_SERIAL             1
#endif
#endif // HAL_UART

#define BLUEBASIC_EVENT_SERIAL    0x0008
#define BLUEBASIC_EVENT_SERIALS   0x0018
#define OS_MAX_TIMER              4
#define DELAY_TIMER               3
#define BLUEBASIC_EVENT_TIMER     0x0020
#define BLUEBASIC_EVENT_TIMERS    0x01E0 // Num bits == OS_MAX_TIMER
#define OS_MAX_INTERRUPT          4
#define BLUEBASIC_EVENT_INTERRUPT 0x0200
#define BLUEBASIC_EVENT_INTERRUPTS 0x1E00 // Num bits == OS_MAX_INTERRUPT
#define BLUEBASIC_EVENT_I2C        0x2000
#define BLUEBASIC_EVENT_YIELD      0x4000
#define BLUEBASIC_EVENT_CON        0x8000

#ifndef OS_AUTORUN_TIMEOUT
#define OS_AUTORUN_TIMEOUT        5000
#endif

#define OS_MAX_FILE               16

#if HAL_UART
// Serial
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
extern os_serial_t serial[OS_MAX_SERIAL];
#endif

#ifdef HAL_I2C
// I2C
typedef struct
{
  unsigned short onread;
  unsigned short onwrite;
  unsigned char available_bytes;
} os_i2c_t;
extern os_i2c_t i2c[1];
#endif

typedef struct
{
  unsigned short pin;
  unsigned short linenum;
} os_interrupt_t;
extern os_interrupt_t blueBasic_interrupts[OS_MAX_INTERRUPT];

typedef struct
{
  unsigned short linenum;
} os_timer_t;
extern os_timer_t blueBasic_timers[OS_MAX_TIMER];

#ifdef FEATURE_SAMPLING
typedef struct {
#ifdef FEATURE_TRUE_RMS
  int32 adc[8];
#else
  int16 adc[8];
#endif
  uint8 map;
  uint8 timer;
  uint16 timeout;
  uint16 duty;
  uint8 pin;
  uint8 polarity;
  uint8 mode;
} os_sampling_t;
extern os_sampling_t sampling;
#endif

extern unsigned short bluebasic_yield_linenum;
extern unsigned char bluebasic_block_execution;

//#define FLASHSTORE_DMA_BASEADDR (__segment_begin("FLASHSTORE"))
#ifndef FLASHSTORE_1STPAGE
#define FLASHSTORE_1STPAGE 72
#endif
#define FLASHSTORE_DMA_BASEADDR ((unsigned long)FLASHSTORE_1STPAGE * FLASHSTORE_PAGESIZE)
#define FLASHSTORE_CPU_BASEADDR ((unsigned char*)(FLASHSTORE_DMA_BASEADDR & 0x7FFF | 0x8000))

#define OS_memset(A, B, C)     osal_memset(A, B, C)
#define OS_memcpy(A, B, C)     osal_memcpy(A, B, C)
#define OS_srand(V)            VOID V
#define OS_rand()              osal_rand()
#define OS_malloc(A)           osal_mem_alloc(A)
#define OS_free(A)             osal_mem_free(A)

#define OS_flashstore_write(A, V, L)  HalFlashWrite(A, V, L)
#define OS_flashstore_erase(P)        HalFlashErase(P)

// block execution bits
// bit 0: block when bluetooth read request blob
// bit 1: block during ble connection
// bit 2: block durig flash compact
extern unsigned char bluebasic_block_execution;
#define BLOCK(x) x
//#define BLOCK(x) HAL_CRITICAL_STATEMENT(x)
#define BLUEBASIC_BLOCK_SET(A)   BLOCK(bluebasic_block_execution |= (A))
#define BLUEBASIC_BLOCK_CLR(A)   BLOCK(bluebasic_block_execution &= ~(A))
#define SEMAPHORE_READ_WAIT()    BLUEBASIC_BLOCK_SET(0x01)
#define SEMAPHORE_READ_SIGNAL()  BLUEBASIC_BLOCK_CLR(0x01)
#define SEMAPHORE_CONN_WAIT()    //BLUEBASIC_BLOCK_SET(0x02)
#define SEMAPHORE_CONN_SIGNAL()  //BLUEBASIC_BLOCK_CLR(0x02)
#define SEMAPHORE_FLASH_WAIT()   BLUEBASIC_BLOCK_SET(0x04)
#define SEMAPHORE_FLASH_SIGNAL() BLUEBASIC_BLOCK_CLR(0x04)
#define SEMAPHORE_YIELD_WAIT()   BLUEBASIC_BLOCK_SET(0x08)
#define SEMAPHORE_YIELD_SIGNAL() BLUEBASIC_BLOCK_CLR(0x08)
#define SEMAPHORE_INPUT_WAIT()   BLUEBASIC_BLOCK_SET(0x10)
#define SEMAPHORE_INPUT_SIGNAL() BLUEBASIC_BLOCK_CLR(0x10)

#if ENABLE_BLE_CONSOLE
#define OS_PUTCHAR(c) OS_putchar(c)
#define OS_TYPE(c) OS_type(c)
extern void OS_putchar(char ch);
extern void OS_type(char ch);
#else
#define OS_PUTCHAR(c)
#define OS_TYPE(c)
#endif

extern void OS_init(void);
extern void OS_openserial(void);
extern void OS_prompt_buffer(unsigned char* start, unsigned char* end);
extern char OS_prompt_available(void);
extern void* OS_rmemcpy(void *dst, const void *src, unsigned int len);
extern void OS_timer_stop(unsigned char id);
extern char OS_timer_start(unsigned char id, unsigned long timeout, unsigned char repeat, unsigned short lineno);
extern void OS_yield(unsigned short linenum);
extern char OS_interrupt_attach(unsigned char pin, unsigned short lineno);
extern char OS_interrupt_detach(unsigned char pin);
extern long OS_get_millis(void);
extern void OS_set_millis(long time);
extern void OS_delaymicroseconds(short micros);
extern void OS_reboot(char flash);
extern void OS_flashstore_init(void);
extern void OS_enable_sleep(unsigned char enable);

extern void interpreter_devicefound(unsigned char addtype, unsigned char* address, signed char rssi, unsigned char eventtype, unsigned char len, unsigned char* data);

#endif /* __APPLE__ */

// bit field for interpreter modes
#define INTERPRETER_CAN_RETURN 1
#define INTERPRETER_CAN_YIELD  2

//definitions of field length in bytes to process
#define _GAP_ARRAY 0x8000
#define _GAP_NOVAL 0x0000
#define _GAP_UINT8 0x1000
#define _GAP_UINT16 0x2000
#define _GAP_UINT32 0x4000

#define BLE_PROFILEROLE         0x1300  //!< Reading this parameter will return GAP Role type. Read Only. Size is uint8.
#define BLE_IRK                 0x8301  //!< Identity Resolving Key. Read/Write. Size is uint8[KEYLEN]. Default is all 0, which means that the IRK will be randomly generated.
#define BLE_SRK                 0x8302  //!< Signature Resolving Key. Read/Write. Size is uint8[KEYLEN]. Default is all 0, which means that the SRK will be randomly generated.
#define BLE_SIGNCOUNTER         0x4303  //!< Sign Counter. Read/Write. Size is uint32. Default is 0.
#define BLE_BD_ADDR             0x8304  //!< Device's Address. Read Only. Size is uint8[B_ADDR_LEN]. This item is read from the controller.
#define BLE_ADVERT_ENABLED      0x1305  //!< Enable/Disable Advertising. Read/Write. Size is uint8. Default is TRUE=Enabled.
#define BLE_ADVERT_OFF_TIME     0x2306  //!< Advertising Off Time for Limited advertisements (in milliseconds). Read/Write. Size is uint16. Default is 30 seconds.
#define BLE_ADVERT_DATA         0x8307  //!< Advertisement Data. Read/Write. Max size is uint8[B_MAX_ADV_LEN].  Default is "02:01:01", which means that it is a Limited Discoverable Advertisement.
#define BLE_SCAN_RSP_DATA       0x8308  //!< Scan Response Data. Read/Write. Max size is uint8[B_MAX_ADV_LEN]. Defaults to all 0.
#define BLE_ADV_EVENT_TYPE      0x1309  //!< Advertisement Type. Read/Write. Size is uint8.  Default is GAP_ADTYPE_ADV_IND (defined in GAP.h).
#define BLE_ADV_DIRECT_TYPE     0x130A  //!< Direct Advertisement Address Type. Read/Write. Size is uint8. Default is ADDRTYPE_PUBLIC (defined in GAP.h).
#define BLE_ADV_DIRECT_ADDR     0x830B  //!< Direct Advertisement Address. Read/Write. Size is uint8[B_ADDR_LEN]. Default is NULL.
#define BLE_ADV_CHANNEL_MAP     0x130C  //!< Which channels to advertise on. Read/Write Size is uint8. Default is GAP_ADVCHAN_ALL (defined in GAP.h)
#define BLE_ADV_FILTER_POLICY   0x130D  //!< Filter Policy. Ignored when directed advertising is used. Read/Write. Size is uint8. Default is GAP_FILTER_POLICY_ALL (defined in GAP.h).
#define BLE_CONNHANDLE          0x230E  //!< Connection Handle. Read Only. Size is uint16.
#define BLE_RSSI_READ_RATE      0x230F  //!< How often to read the RSSI during a connection. Read/Write. Size is uint16. The value is in milliseconds. Default is 0 = OFF.
#define BLE_PARAM_UPDATE_ENABLE 0x1310  //!< Slave Connection Parameter Update Enable. Read/Write. Size is uint8. If TRUE then automatic connection parameter update request is sent. Default is FALSE.
#define BLE_MIN_CONN_INTERVAL   0x2311  //!< Minimum Connection Interval to allow (n * 1.25ms).  Range: 7.5 msec to 4 seconds (0x0006 to 0x0C80). Read/Write. Size is uint16. Default is 7.5 milliseconds (0x0006).
#define BLE_MAX_CONN_INTERVAL   0x2312  //!< Maximum Connection Interval to allow (n * 1.25ms).  Range: 7.5 msec to 4 seconds (0x0006 to 0x0C80). Read/Write. Size is uint16. Default is 4 seconds (0x0C80).
#define BLE_SLAVE_LATENCY       0x2313  //!< Update Parameter Slave Latency. Range: 0 - 499. Read/Write. Size is uint16. Default is 0.
#define BLE_TIMEOUT_MULTIPLIER  0x2314  //!< Update Parameter Timeout Multiplier (n * 10ms). Range: 100ms to 32 seconds (0x000a - 0x0c80). Read/Write. Size is uint16. Default is 1000.
#define BLE_CONN_BD_ADDR        0x8315  //!< Address of connected device. Read only. Size is uint8[B_MAX_ADV_LEN]. Set to all zeros when not connected.
#define BLE_CONN_INTERVAL       0x2316  //!< Current connection interval.  Read only. Size is uint16.  Range is 7.5ms to 4 seconds (0x0006 to 0x0C80).  Default is 0 (no connection).
#define BLE_CONN_LATENCY        0x2317  //!< Current slave latency.  Read only.  Size is uint16.  Range is 0 to 499. Default is 0 (no slave latency or no connection).
#define BLE_CONN_TIMEOUT        0x2318  //!< Current timeout value.  Read only.  size is uint16.  Range is 100ms to 32 seconds.  Default is 0 (no connection).
#define BLE_PARAM_UPDATE_REQ    0x1319  //!< Slave Connection Parameter Update Request. Write. Size is uint8. If TRUE then connection parameter update request is sent.
#define BLE_STATE               0x131A  //!< Reading this parameter will return GAP Peripheral Role State. Read Only. Size is uint8.
#define BLE_ADV_NONCONN_ENABLED 0x131B  //!< Enable/Disable Non-Connectable Advertising.  Read/Write.  Size is uint8.  Default is FALSE=Disabled.

//
// GAPBOND
//
#define BLE_PAIRING_MODE        0x1400  //!< Pairing Mode: @ref  BLE_PAIRING_MODE_DEFINES. Read/Write. Size is uint8. Default is BLE_PAIRING_MODE_WAIT_FOR_REQ.
#define BLE_INITIATE_WAIT       0x2401  //!< Pairing Mode Initiate wait timeout.  This is the time it will wait for a Pairing Request before sending the Slave Initiate Request. Read/Write. Size is uint16. Default is 1000(in milliseconds).
#define BLE_MITM_PROTECTION     0x1402  //!< Man-In-The-Middle (MITM) basically turns on Passkey protection in the pairing algorithm. Read/Write. Size is uint8. Default is 0(disabled).
#define BLE_IO_CAPABILITIES     0x1403  //!< I/O capabilities.  Read/Write. Size is uint8. Default is BLE_IO_CAP_DISPLAY_ONLY @ref BLE_IO_CAP_DEFINES.
#define BLE_OOB_ENABLED         0x1404  //!< OOB data available for pairing algorithm. Read/Write. Size is uint8. Default is 0(disabled).
#define BLE_OOB_DATA            0x8405  //!< OOB Data. Read/Write. size uint8[16]. Default is all 0's.
#define BLE_BONDING_ENABLED     0x1406  //!< Request Bonding during the pairing process if enabled.  Read/Write. Size is uint8. Default is 0(disabled).
#define BLE_KEY_DIST_LIST       0x1407  //!< The key distribution list for bonding.  size is uint8.  @ref BLE_KEY_DIST_DEFINES. Default is sEncKey, sIdKey, mIdKey, mSign enabled.
#define BLE_DEFAULT_PASSCODE    0x4408  //!< The default passcode for MITM protection. size is uint32. Range is 0 - 999,999. Default is 0.
#define BLE_ERASE_ALLBONDS      0x0409  //!< Erase all of the bonded devices. Write Only. No Size.
#define BLE_KEYSIZE             0x140C  //!< Key Size used in pairing. Read/Write. size is uint8. Default is 16.
#define BLE_AUTO_SYNC_WL        0x140D  //!< Clears the White List adds to it each unique address stored by bonds in NV. Read/Write. Size is uint8. Default is FALSE.
#define BLE_BOND_COUNT          0x140E  //!< Gets the total number of bonds stored in NV. Read Only. Size is uint8. Default is 0 (no bonds).
#define BLE_BOND_FAIL_ACTION    0x140F  //!< Possible actions Central may take upon an unsuccessful bonding. Write Only. Size is uint8. Default is 0x02 (Terminate link upon unsuccessful bonding).
#define BLE_ERASE_SINGLEBOND    0x8410  //!< Erase a single bonded device. Write only. Must provide address type followed by device address.

#define BLE_RXGAIN              0x2F00 // length?
#define BLE_TXPOWER             0x2F01 // length?

#define _GAPROLE(V)   (0x0FFF & (V))
#define _GAPLEN(L)    ((0x7000 & (L))>>12) // length in bytes for the access

#define FIELD_SIZEOF(t, f) (sizeof(((t*)0)->f))



typedef struct
{
  unsigned short linenum;
} os_discover_t;
extern os_discover_t blueBasic_discover;

extern unsigned short timeSlice;

#define LINKDB_STATUS_UPDATE_RSSI 16

extern bool interpreter_setup(void);
extern void interpreter_banner(void);
extern void interpreter_loop(void);
extern unsigned char interpreter_run(unsigned short gofrom, unsigned char canreturn);
extern void interpreter_timer_event(unsigned short id);

#ifdef FEATURE_SAMPLING
extern void interpreter_sampling(void);
#endif

#define PIN_MAKE(A,I) (((A) << 6) | ((I) << 3))
#define PIN_MAJOR(P)  ((P) >> 6)
#define PIN_MINOR(P)  (((P) >> 3) & 7)

#ifndef FLASHSTORE_NRPAGES
#define FLASHSTORE_NRPAGES 8  // defaults to 16K BASIC program
#endif
#define FLASHSTORE_PAGESIZE   2048
#define FLASHSTORE_LEN        (FLASHSTORE_NRPAGES * FLASHSTORE_PAGESIZE)

enum
{
  FLASHID_INVALID = 0x0000,
  FLASHID_SPECIAL = 0xFFFE,
  FLASHID_FREE    = 0xFFFF,
};

enum
{
  FLASHSPECIAL_AUTORUN = 0x00000001,
  FLASHSPECIAL_SNV     = 0x00000100,
  FLASHSPECIAL_FILE0   = 0x00100000,
  FLASHSPECIAL_FILE25  = 0x00290000,
};

#define FS_NR_FILE_HANDLES 4
#define FS_MAKE_FILE_SPECIAL(NAME,OFF)  (FLASHSPECIAL_FILE0+(((unsigned long)((NAME)-'A'))<<16)|(OFF))
#define FLASHSPECIAL_NR_FILE_RECORDS 0xFFFF
#define FLASHSPECIAL_DATA_LEN       2
#define FLASHSPECIAL_ITEM_ID        3
#define FLASHSPECIAL_DATA_OFFSET    (FLASHSPECIAL_ITEM_ID + sizeof(unsigned long))

#define SNV_MAKE_ID(FILENAME) ((FILENAME) - '0' + BLE_NVID_CUST_START)

extern unsigned char** flashstore_init(unsigned char** startmem);
extern unsigned char** flashstore_addline(unsigned char* line);
extern unsigned char** flashstore_deleteline(unsigned short id);
extern unsigned char** flashstore_deleteall(void);
extern unsigned short** flashstore_findclosest(unsigned short id);
extern unsigned int flashstore_freemem(void);
extern void flashstore_compact(unsigned char asklen, unsigned char* tempmemstart, unsigned char* tempmemend);
extern unsigned char flashstore_addspecial(unsigned char* item);
extern unsigned char flashstore_deletespecial(unsigned long specialid);
extern unsigned char* flashstore_findspecial(unsigned long specialid);

extern unsigned char OS_serial_open(unsigned char port, unsigned long baud, unsigned char parity, unsigned char bits, unsigned char stop, unsigned char flow, unsigned short onread, unsigned short onwrite);
extern unsigned char OS_serial_close(unsigned char port);
extern short OS_serial_read(unsigned char port);
extern unsigned char OS_serial_write(unsigned char port, unsigned char ch);
extern unsigned char OS_serial_available(unsigned char port, unsigned char ch);
unsigned char OS_i2c_open(unsigned char address, unsigned short onread, unsigned short onwrite);
unsigned char OS_i2c_close(unsigned char port);
short OS_i2c_read(unsigned char port);
unsigned char OS_i2c_write(unsigned char port, unsigned char ch);
unsigned char OS_i2c_available(unsigned char port, unsigned char ch);
int16 OS_get_temperature(uint8 wait);
extern unsigned char get_snv_length(unsigned char id);
int8 OS_get_vdd_7(void);

