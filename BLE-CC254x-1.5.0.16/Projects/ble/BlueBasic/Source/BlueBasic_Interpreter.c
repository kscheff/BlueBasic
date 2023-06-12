////////////////////////////////////////////////////////////////////////////////
// BlueBasic
////////////////////////////////////////////////////////////////////////////////
//
// Authors:
//      Tim Wilkinson <tim.j.wilkinson@gmail.com>
//      Kai Scheffer <kai.scheffer@gmail.com>
//
//      Original TinyBasicPlus
//        Mike Field <hamster@snap.net.nz>
//	      Scott Lawrence <yorgle@gmail.com>
//
// v0.7 2016-11-15
//      added ANALOG REFERENCE, AVDD option
//      bugfix hanging printnum() 0x80000000 
//      changed urlmsg to point to GitHub
//
// v0.6: 2014-12-01
//      Finalize Petra version.
//
// v0.5: 2014-09-09
//      Flash storage system for programs.
//
// v.04: 2014-09-05
//      New version time again. Mostly this is support for over-the-air updates.
//
// v0.3: 2014-08-19
//      New version time. Lots more I/O options available, including SPI, I2C and a general WIRE signalling protocol
//      for talking to random other devices. Also, a new non-recursive expression evaluator which is a bit faster and
//      much more flexible. Speed improvements and general bug fixes.
//
// v0.2: 2014-08-01
//      Seems like a good place to declare 0.2. Work is complete enough to have a pre-flashed device which can
//      be connected to over BLE and then program, including setting up services and characteristics, and attaching
//      these to the general i/o on the chip.
//
// v0.1: 2014-07-10
//      Start of Blue Basic. Heavily modified from Tiny Basic Plus (https://github.com/BleuLlama/TinyBasicPlus).
//      The goal is to put a Basic interpreter onto the TI CC254x Bluetooth LE chip.

#include "os.h"
#include "math.h"


////////////////////////////////////////////////////////////////////////////////

#ifndef BLUEBASIC_MEM
#define kRamSize   2048 /* arbitrary */
#else // BLUEBASIC_MEM
#define kRamSize   (BLUEBASIC_MEM)
#endif // BLUEBASIC_MEM

#ifndef FEATURE_LAZY_INDEX
#define FEATURE_LAZY_INDEX TRUE
#endif

////////////////////////////////////////////////////////////////////////////////
// ASCII Characters
#define CR	'\r'
#define NL	'\n'
#define WS_TAB	'\t'
#define WS_SPACE   ' '
#define SQUOTE  '\''
#define DQUOTE  '\"'
#define CTRLC	0x03
#define CTRLH	0x08

typedef short unsigned LINENUM;

enum
{
  ERROR_OK = 0,
  ERROR_GENERAL,
  ERROR_EXPRESSION,
  ERROR_DIV0,
  ERROR_OOM,
  ERROR_TOOBIG,
  ERROR_BADPIN,
  ERROR_DIRECT,
  ERROR_EOF,
};

#if ENABLE_BLE_CONSOLE
static const char* const error_msgs[] =
{
  "OK",
  "Error",
  "Bad expression",
  "Divide by zero",
  "Out of memory",
  "Too big",
  "Bad pin",
  "Not in direct",
  "End of file",
};
#endif

#if defined(MPPT_AS_VOT) && MPPT_AS_VOT
#define MPPT_EMU_MSG " as Vot"
#else
#define MPPT_EMU_MSG ""
#endif

#if defined(MPPT_MODE_HEX) && MPPT_MODE_HEX
#define MPPT_MODE_MSG " VE.Hex"
#endif
#if defined (MPPT_MODE_TEXT) && MPPT_MODE_TEXT
#define MPPT_MODE_MSG " VE.Text"
#endif  
#ifndef MPPT_MODE_MSG
#define MPPT_MODE_MSG ""
#endif

#define STR_EXPAND(number) #number
#define STR(number) STR_EXPAND(number)

#if HAL_UART
#define MSG_EXT MPPT_MODE_MSG MPPT_EMU_MSG "\nUARTs:" STR(OS_MAX_SERIAL)
#else
#define MSG_EXT MPPT_MODE_MSG MPPT_EMU_MSG "\nno UART"
#endif

#if ENABLE_BLE_CONSOLE
#ifdef BUILD_TIMESTAMP
#ifdef FEATURE_OAD_HEADER
static const char initmsg[]           = "BlueBasic " BUILD_TIMESTAMP "/OAD " kVersion MSG_EXT;
#else
static const char initmsg[]           = "BlueBasic " BUILD_TIMESTAMP " " kVersion MSG_EXT;
#endif
#else
static const char initmsg[]           = "BlueBasic " kVersion MSG_EXT;
#endif
//static const char urlmsg[]            = "http://blog.xojs.org/bluebasic";
static const char urlmsg[]            = kMfrName;
static const char memorymsg[]         = " bytes free.";
#endif

#define VAR_TYPE    long int
#define VAR_SIZE    (sizeof(VAR_TYPE))


// Start enum at 128 so we get instant token values.
// By moving the command list to an enum, we can easily remove sections
// above and below simultaneously to selectively obliterate functionality.
enum
{
  // -----------------------
  // Keywords
  //

  KW_CONSTANT = 0x80,
  KW_LIST,
  KW_MEM,
  KW_NEW,
  KW_RUN,
  KW_NEXT,
  KW_IF,
  KW_ELIF,
  KW_ELSE,
  KW_GOTO,
  KW_GOSUB,
  KW_RETURN,
  KW_REM,
  KW_SLASHSLASH,
  KW_FOR,
  KW_PRINT,
  KW_REBOOT,
  KW_END,
  KW_DIM,
  KW_TIMER,
  KW_DELAY,
  KW_AUTORUN,
  KW_PIN_P0,
  KW_PIN_P1,
  KW_PIN_P2,
  KW_GATT,
  KW_ADVERT,
  KW_SCAN,
  KW_BTPOKE,
  KW_PINMODE,
  KW_INTERRUPT,
  KW_SERIAL,
  KW_SPI,
  KW_ANALOG,
  KW_CONFIG,
  KW_WIRE,
  KW_I2C,
  KW_OPEN,
  KW_CLOSE,
  KW_READ,
  KW_WRITE,   // 168
  
  // -----------------------
  // Keyword spacers - to add main keywords later without messing up the numbering below
  //

  KW_SPACE0, // 169
  KW_SPACE1,
  KW_SPACE2,
  KW_SPACE3,
  KW_SPACE4,
  KW_SPACE5,
  KW_SPACE6,
  KW_SPACE7, // 176

  // -----------------------
  // Operators
  //

  OP_ADD,  // 177
  OP_SUB,
  OP_MUL,
  OP_DIV,
  OP_REM,
  OP_AND,
  OP_OR,
  OP_XOR,
  OP_GE,
  OP_NE,
  OP_GT,
  OP_EQEQ,
  OP_EQ,
  OP_LE,
  OP_LT,
  OP_NE_BANG,
  OP_LSHIFT,
  OP_RSHIFT,
  OP_UMINUS,
  
  OP_SPACE0,
  OP_SPACE1,
  OP_SPACE2,
  OP_SPACE3,
  
  // -----------------------
  // Functions
  //
  
  FUNC_ABS,
  FUNC_LEN,
  FUNC_RND,
  FUNC_MILLIS,
  FUNC_BATTERY,
  FUNC_HEX,
  FUNC_EOF,
  
  // -----------------------
  // Funciton & operator spacers - to add main keywords later without messing up the numbering below
  //
  FUNC_POW,
  FUNC_TEMP,
//  FUNC_SPACE0,
//  FUNC_SPACE1,
  FUNC_SPACE2,
  FUNC_SPACE3,
  
  // -----------------------

  ST_TO,
  ST_STEP,
  TI_STOP,
  TI_REPEAT,

  PM_PULLUP,
  PM_PULLDOWN,
  PM_INPUT,
  PM_ADC,
  PM_OUTPUT,
  PM_RISING,
  PM_FALLING,
  PM_SPACE0,
  PM_SPACE1,
  PM_TIMEOUT,
  PM_WAIT,
  PM_PULSE,

  BLE_ONREAD,
  BLE_ONWRITE,
  BLE_ONCONNECT,
  BLE_ONDISCOVER,
  BLE_SERVICE,
  BLE_CHARACTERISTIC,
  BLE_WRITENORSP,
  BLE_NOTIFY,
  BLE_INDICATE,
  BLE_GENERAL,
  BLE_LIMITED,
  BLE_MORE,
  BLE_NAME,
  BLE_CUSTOM,
  BLE_FUNC_BTPEEK,
  BLE_ACTIVE,
  BLE_DUPLICATES,

  SPI_TRANSFER,
  SPI_MSB,
  SPI_LSB,
  SPI_MASTER,
  SPI_SLAVE,

  FS_TRUNCATE,
  FS_APPEND,

  IN_ATTACH,
  IN_DETACH,
  
  BLE_AUTH,

  LAST_KEYWORD
};

enum
{
  CO_TRUE = 1,
  CO_FALSE,
  CO_ON,
  CO_OFF,
  CO_YES,
  CO_NO,
  CO_HIGH,
  CO_LOW,
  CO_ADVERT_ENABLED,
  CO_MIN_CONN_INTERVAL,
  CO_MAX_CONN_INTERVAL,
  CO_SLAVE_LATENCY,
  CO_TIMEOUT_MULTIPLIER,
  CO_DEV_ADDRESS,
  CO_TXPOWER,
  CO_RXGAIN,
  CO_LIM_DISC_INT_MIN,
  CO_LIM_DISC_INT_MAX,
  CO_GEN_DISC_INT_MIN,
  CO_GEN_DISC_INT_MAX,
  CO_GEN_DISC_ADV_MIN,
  CO_LIM_ADV_TIMEOUT,
  CO_RESOLUTION,
  CO_REFERENCE,
  CO_POWER,
  CO_INTERNAL,
  CO_EXTERNAL,
  CO_AVDD,
  CO_DEFAULT_PASSCODE,
  CO_BONDING_ENABLED,
};

// Constant map (so far all constants are <= 16 bits)
static const unsigned short constantmap[] =
{
  1, // TRUE
  0, // FALSE
  1, // ON
  0, // OFF
  1, // YES
  0, // NO
  1, // HIGH
  0, // LOW
  BLE_ADVERT_ENABLED,
  BLE_MIN_CONN_INTERVAL,
  BLE_MAX_CONN_INTERVAL,
  BLE_SLAVE_LATENCY,
  BLE_TIMEOUT_MULTIPLIER,
  BLE_BD_ADDR,
  BLE_TXPOWER,
  BLE_RXGAIN,
  TGAP_LIM_DISC_ADV_INT_MIN,
  TGAP_LIM_DISC_ADV_INT_MAX,
  TGAP_GEN_DISC_ADV_INT_MIN,
  TGAP_GEN_DISC_ADV_INT_MAX,
  TGAP_GEN_DISC_ADV_MIN,
  TGAP_LIM_ADV_TIMEOUT,
  CO_RESOLUTION,
  CO_REFERENCE,
  CO_POWER,
  CO_INTERNAL,
  CO_EXTERNAL,
  CO_AVDD,
  BLE_DEFAULT_PASSCODE,
  BLE_BONDING_ENABLED,
};

//
// See http://en.wikipedia.org/wiki/Operator_precedence
//
static const unsigned char operator_precedence[] =
{
   4, // OP_ADD
   4, // OP_SUB
   3, // OP_MUL
   3, // OP_DIV
   3, // OP_REM
   8, // OP_AND
  10, // OP_OR
   9, // OP_XOR
   6, // OP_GE,
   7, // OP_NE,
   6, // OP_GT,
   7, // OP_EQEQ,
   7, // OP_EQ,
   6, // OP_LE,
   6, // OP_LT,
   7, // OP_NE_BANG,
   5, // OP_LSHIFT,
   5, // OP_RSHIFT,
   2, // OP_UMINUS
};

enum
{
  EXPR_NORMAL = 0,
  EXPR_BRACES,
  EXPR_COMMA,
};

// WIRE commands
enum
{
  WIRE_CMD_MASK = 0x07,

  WIRE_PIN_OUTPUT = 1,
  WIRE_PIN_INPUT = 2,
  WIRE_PIN_HIGH = 3,
  WIRE_PIN_LOW = 4,
  WIRE_PIN_READ = 5,
  
  WIRE_PIN_GENERAL = 0,
  
  WIRE_PIN = 0x00,
  WIRE_OUTPUT = 0x08,
  WIRE_INPUT = 0x10,
  WIRE_INPUT_NORMAL = 0x18,
  WIRE_INPUT_PULLUP = 0x20,
  WIRE_INPUT_PULLDOWN = 0x28,
  WIRE_INPUT_ADC = 0x30,
  WIRE_TIMEOUT = 0x38,
  WIRE_WAIT_TIME = 0x48,
  WIRE_HIGH = 0x50,
  WIRE_LOW = 0x58,
  WIRE_INPUT_PULSE = 0x60,
  WIRE_WAIT_HIGH = 0x68,
  WIRE_WAIT_LOW = 0x70,
  WIRE_INPUT_READ = 0x78,
  WIRE_INPUT_READ_ADC = 0x80,
  WIRE_INPUT_SET = 0x88,
};
#define WIRE_CASE(C)  ((C) >> 3)

#include "keyword_tables.h"

// Interpreter exit statuses
enum
{
  IX_BYE,
  IX_PROMPT,
  IX_OUTOFMEMORY
};

typedef struct
{
  char frame_type;
  uint16_t frame_size;
} frame_header;

typedef struct
{
  frame_header header;
  char for_var;
  VAR_TYPE terminal;
  VAR_TYPE step;
  unsigned char** line;
} for_frame;

typedef struct
{
  frame_header header;
  unsigned char** line;
} gosub_frame;

typedef struct
{
  frame_header header;
} event_frame;

typedef struct
{
  frame_header header;
  char type;
  char name;
  char oflags;
  VAR_TYPE ovalue;
  struct gatt_variable_ref* ble;
  // .... bytes ...
} variable_frame;

typedef struct
{
  frame_header header;
  gattAttribute_t* attrs;
  LINENUM connect;
} service_frame;

// Frame types
enum
{
  FRAME_GOSUB_FLAG = 1,
  FRAME_FOR_FLAG,
  FRAME_VARIABLE_FLAG,
  FRAME_EVENT_FLAG,
  FRAME_SERVICE_FLAG
};

// Stack clean up return types
enum
{
  RETURN_GOSUB = 1,
  RETURN_EVENT,
  RETURN_FOR,
  RETURN_ERROR_OOM,
  RETURN_ERROR_OR_OK,
  RETURN_QWHAT
};

// Variable types
enum
{
  VAR_INT = 1,
  VAR_DIM_BYTE
};

static __data unsigned char** lineptr;
static __data unsigned char* txtpos;
static __data unsigned char** program_end;
static __data unsigned char error_num;
static __data unsigned char* variables_begin;

__data unsigned char* sp;   // 
__data unsigned char* heap; // Access in flashstore

#if ENABLE_BLE_CONSOLE
static unsigned char* list_line;
#endif
static unsigned char** program_start;
static LINENUM linenum;

static variable_frame normal_variable = { { FRAME_VARIABLE_FLAG, 0 }, VAR_INT, 0, 0, 0, NULL };

#if defined ENABLE_YIELD && ENABLE_YIELD
static VAR_TYPE yield_time;
unsigned short timeSlice = 20;
#endif

#define VAR_COUNT 26
#define VARIABLE_INT_ADDR(F)    (((VAR_TYPE*)variables_begin) + ((F) - 'A'))
#define VARIABLE_INT_GET(F)     (*VARIABLE_INT_ADDR(F))
#define VARIABLE_INT_SET(F,V)   (*VARIABLE_INT_ADDR(F) = (V))

#define VARIABLE_IS_EXTENDED(F)  (vname = (F) - 'A', (*(variables_begin + VAR_COUNT * VAR_SIZE + (vname >> 3)) & (1 << (vname & 7))))
#define VARIABLE_SAVE(V) \
  do { \
    unsigned char vname = (V)->name - 'A'; \
    unsigned char* v = variables_begin + VAR_COUNT * VAR_SIZE + (vname >> 3); \
    VAR_TYPE* p = ((VAR_TYPE*)variables_begin) + vname; \
    vname = 1 << (vname & 7); \
    (V)->oflags = *v & vname; \
    *v |= vname; \
    (V)->ovalue = *p; \
    *(unsigned char**)p = (unsigned char*)(V); \
  } while(0)
#define VARIABLE_RESTORE(V) \
  do { \
    unsigned char vname = (V)->name - 'A'; \
    unsigned char* v = variables_begin + VAR_COUNT * VAR_SIZE + (vname >> 3); \
    *v = (*v & (255 - (1 << (vname & 7)))) | (V)->oflags; \
    ((VAR_TYPE*)variables_begin)[vname] = (V)->ovalue; \
  } while(0)
 
// define for getting min heap size statistic in MEM command    
//#define REPORT_MIN_MEMORY
#ifdef REPORT_MIN_MEMORY
static unsigned short minMemory;  
#define CHECK_MIN_MEMORY() minMemory = (sp - heap < minMemory) ? (sp - heap) : minMemory
#define SET_MIN_MEMORY(A) minMemory = (A)
#else
#define CHECK_MIN_MEMORY()
#define SET_MIN_MEMORY(A)
#endif

// define for getting source code line numbers to the errors... 
// eats ca. 1172 bytes CODE and 2 bytes XDATA
//#define REPORT_ERROR_LINE
#ifdef REPORT_ERROR_LINE
static unsigned short err_line = 0;
#define SET_ERR_LINE err_line = __LINE__
#else
#define SET_ERR_LINE
#endif

#define GOTO_QWHAT {SET_ERR_LINE; goto qwhat;}

#define CHECK_SP_OOM(S,E)   if (sp - (S) < heap) {SET_MIN_MEMORY(0); SET_ERR_LINE ; goto E;} else {sp -= (S); CHECK_MIN_MEMORY();}
#define CHECK_HEAP_OOM(S,E) if (heap + (S) > sp) {SET_MIN_MEMORY(0); SET_ERR_LINE ; goto E;} else {heap += (S); CHECK_MIN_MEMORY();}

static unsigned char cleanup_stack(void);

#ifdef SIMULATE_PINS
static unsigned char P0DIR, P1DIR, P2DIR;
static unsigned char P0SEL, P1SEL, P2SEL;
static unsigned char P0INP, P1INP, P2INP;
static unsigned char P0, P1, P2;
static unsigned char APCFG, ADCH, ADCL, ADCCON1, ADCCON3;
static unsigned char PICTL, P0IEN, P1IEN, P2IEN;
static unsigned char IEN1, IEN2;
static unsigned char U0BAUD, U0GCR, U0CSR, U0DBUF;
static unsigned char U1BAUD, U1GCR, U1CSR, U1DBUF;
static unsigned char PERCFG;
#endif

#if !defined(ENABLE_SPI) || ENABLE_SPI
static unsigned char spiChannel;
static unsigned char spiWordsize;
#endif
static unsigned char analogReference;
static unsigned char analogResolution = 0x30; // 14-bits
#if HAL_I2C_SLAVE
static unsigned char i2cScl;
static unsigned char i2cSda;
#endif

static VAR_TYPE pin_read(unsigned char major, unsigned char minor);
static unsigned char pin_parse(void);

static void pin_wire(unsigned char* start, unsigned char* end);
#if !defined(ENABLE_WIRE) || ENABLE_WIRE
static void pin_wire_parse(void);
static unsigned char pinParseCurrent;
static unsigned char* pinParsePtr;
static unsigned char* pinParseReadAddr;
#endif

static VAR_TYPE expression(unsigned char mode);
#define EXPRESSION_STACK_SIZE 8
#define EXPRESSION_QUEUE_SIZE 8

unsigned char  ble_adbuf[31];
unsigned char* ble_adptr;
unsigned char  ble_isadvert;

typedef struct gatt_variable_ref
{
  unsigned char var;
  gattAttribute_t* attrs;
  gattCharCfg_t* cfg;
  LINENUM read;
  LINENUM write;
} gatt_variable_ref;

#define INVALID_CONNHANDLE 0xFFFF

static short find_quoted_string(void);
static char ble_build_service(void);
static char ble_get_uuid(void);
static unsigned char ble_read_callback(unsigned short handle, gattAttribute_t* attr, unsigned char* value, unsigned char* len, unsigned short offset, unsigned char maxlen, uint8 method);
static unsigned char ble_write_callback(unsigned short handle, gattAttribute_t* attr, unsigned char* value, unsigned char len, unsigned short offset, uint8 method);
static void ble_notify_assign(gatt_variable_ref* vref);

#ifdef TARGET_CC254X

#include "gatt_uuid.h"

#define ble_primary_service_uuid                primaryServiceUUID
#define ble_characteristic_uuid                 characterUUID
#define ble_characteristic_description_uuid     charUserDescUUID
#define ble_client_characteristic_config_uuid   clientCharCfgUUID

#else // TARGET_CC254X

static const unsigned char ble_primary_service_uuid[] = { 0x00, 0x28 };
static const unsigned char ble_characteristic_uuid[] = { 0x03, 0x28 };
static const unsigned char ble_characteristic_description_uuid[] = { 0x01, 0x29 };
static const unsigned char ble_client_characteristic_config_uuid[] = { 0x02, 0x29 };

#endif // TARGET_CC254X

static LINENUM servicestart;
static unsigned short servicecount;
static unsigned char ble_uuid[16];
static unsigned char ble_uuid_len;

static const gattServiceCBs_t ble_service_callbacks =
{
  ble_read_callback,
  ble_write_callback,
  NULL
};

//
// File system handles.
//
typedef struct
{
  unsigned char filename;
  unsigned char action;
  unsigned short record;
  unsigned char poffset;
  unsigned short modulo;
} os_file_t;
static os_file_t files[FS_NR_FILE_HANDLES];

static unsigned char addspecial_with_compact(unsigned char* item);

#ifdef FEATURE_BOOST_CONVERTER
//
// Battery management.
//
unsigned short BlueBasic_rawBattery;
unsigned char BlueBasic_powerMode;
#endif

//
// Skip whitespace
//
static inline void ignore_blanks(void)
{
  if (*txtpos == WS_SPACE)
  {
    txtpos++;
  }
}

#if ENABLE_BLE_CONSOLE

//
// Tokenize the human readable command line into something easier, smaller and faster.
//  Note. The tokenized form must always be smaller than the human form otherwise this
//  will break because it overwrites the buffer as it goes along.
//
static void tokenize(void)
{
  unsigned char c;
  unsigned char* writepos;
  unsigned char* readpos;
  unsigned char* scanpos;
  const unsigned char* table;
  
  writepos = txtpos;
  scanpos = txtpos;
  for (;;)
  {
    readpos = scanpos;
    table = KEYWORD_TABLE(readpos);
    for (;;)
    {
      c = *readpos;
      if (c == SQUOTE || c == DQUOTE)
      {
        *writepos++ = c;
        readpos++;
        for (;;)
        {
          const char nc = *readpos++;
          *writepos++ = nc;
          if (nc == c)
          {
            break;
          }
          else if (nc == NL)
          {
            writepos--;
            readpos--;
            break;
          }
        }
        scanpos = readpos;
        break;
      }
      else if (c == NL)
      {
        *writepos = NL;
        return;
      }
      while (c == *table)
      {
        table++;
        c = *++readpos;
      }
      if (*table >= 0x80)
      {
        // Match found
        if (writepos > txtpos && writepos[-1] == WS_SPACE)
        {
          writepos--;
        }
        *writepos++ = *table;
        if (*table == KW_CONSTANT)
        {
          *writepos++ = table[1];
        }
        // Skip whitespace
        while ((void)(c = *readpos), c == WS_SPACE || c == WS_TAB)
        {
          readpos++;
        }
        scanpos = readpos;
        break;
      }
      else
      {
        // No match found
        // Move to next possibility
        while (*table++ < 0x80)
          ;
        if (table[-1] == KW_CONSTANT)
        {
          table++;
        }
        if (*table == 0)
        {
          // Not found
          // Advance until next likely token start
          c = *scanpos;
          if (c >= 'A' && c <= 'Z')
          {
            do
            {
              *writepos++ = c;
              c = *++scanpos;
            } while (c >= 'A' && c <= 'Z');
          }
          else if (c == WS_TAB || c == WS_SPACE)
          {
            if (writepos > txtpos && writepos[-1] != WS_SPACE)
            {
              *writepos++ = WS_SPACE;
            }
            do
            {
              c = *++scanpos;
            } while (c == WS_SPACE || c == WS_TAB);
          }
          else
          {
            *writepos++ = c;
            scanpos++;
          }
          break;
        }
        readpos = scanpos;
      }
    }
  }
}
#endif

//
// Print a number (base 10)
//
#if !ENABLE_BLE_CONSOLE
#define printnum(a,b)
#else
void printnum(signed char fieldsize, VAR_TYPE num)
{
  VAR_TYPE size = 1;
  signed char sign = 10;

  if (num < 0)
  {
    OS_putchar('-');
    sign = -sign;
  }
  for (; size <= num / sign ; size *= 10, fieldsize--)
    ;
  while (fieldsize-- > 0)
  {
    OS_putchar(WS_SPACE);
  }
  if (sign < 0)
    size = -size;
  for (/*size /= 10*/; size != 0; size /= 10)
  {
    OS_putchar('0' + num / size);
    num -= num/size*size;
  }
}
#endif

#if ENABLE_BLE_CONSOLE
static void testlinenum(void)
{
  unsigned char ch;

  ignore_blanks();

  linenum = 0;
  for (ch = *txtpos; ch >= '0' && ch <= '9'; ch = *++txtpos)
  {
    // Trap overflows
    if (linenum >= 0xFFFF / 10)
    {
      linenum = 0xFFFF;
      break;
    }
    linenum = linenum * 10 + ch - '0';
  }
}
#endif

//
// Find the beginning and limit of a quoted string.
//  Returns the length, of -1 is invalid.
//
static short find_quoted_string(void)
{
  short i = 0;
  unsigned char delim = *txtpos;
  if (delim != '"' && delim != '\'')
  {
    return -1;
  }
  txtpos++;
  for(; txtpos[i] != delim; i++)
  {
    if (txtpos[i] == NL)
    {
      return -1;
    }
  }
  return i;
}

//
// Print the string between quotation marks.
//
#if ENABLE_BLE_CONSOLE  

static unsigned char print_quoted_string(void)
{
  short i = find_quoted_string();
  if (i == -1)
  {
    return 0;
  }
  else
  {
    // Print the characters
    for (; i; i--)
    {
      OS_putchar(*txtpos++);
    }
    txtpos++;

    return 1;
  }
}
#endif

//
// Print a message + newline
//
#if !ENABLE_BLE_CONSOLE
#define printmsg(a)
#else
void printmsg(const char *msg)
{
  while (*msg != 0)
  {
    OS_putchar(*msg++);
  }
  OS_putchar(NL);
}
#endif

//
// Find pointer in the lineref for the given linenum.
//
static unsigned char** findlineptr(void)
{
  return (unsigned char**)flashstore_findclosest(linenum);
}

#if !ENABLE_BLE_CONSOLE
#define printline(a,b)
#else
//
// Print the current BASIC line. The line is tokenized so
// it is expanded as it's printed, including the addition of whitespace
// to make it more readable.
//
static void printline(unsigned char nindent, unsigned char indent)
{
  LINENUM line_num;
  unsigned char lc = WS_SPACE;

  line_num = *(LINENUM*)list_line;
  list_line += sizeof(LINENUM) + sizeof(char);

  // Output the line */
  printnum(nindent, line_num);
  while (indent-- != 0)
  {
    OS_putchar(WS_SPACE);
  }
  for (unsigned char c = *list_line++; c != NL; c = *list_line++)
  {
    if (c < 0x80)
    {
      OS_putchar(c);
    }
    else
    {
      // Decode the token (which is a bit non-trival and slow)
      const unsigned char* begin;
      const unsigned char* ptr;
      const unsigned char** k;

      for (k = keywords; *k; k++)
      {
        begin = *k;
        ptr = begin;
        while (*ptr != 0)
        {
          while (*ptr < 0x80)
          {
            ptr++;
          }
          if (*ptr != c || (c == KW_CONSTANT && ptr[1] != list_line[0]))
          {
            if (*ptr == KW_CONSTANT)
            {
              ptr++;
            }
            begin = ++ptr;
          }
          else
          {
            if (c == KW_CONSTANT)
            {
              list_line++;
            }
            if (lc != WS_SPACE)
            {
              OS_putchar(WS_SPACE);
            }
            for (c = *begin++; c < 0x80; c = *begin++)
            {
              OS_putchar(c);
            }
            if (*list_line != '(' && *list_line != ',' && begin[-1] != FUNC_HEX)
            {
              OS_putchar(WS_SPACE);
              c = WS_SPACE;
            }
            else
            {
              c = begin[-2];
            }
            goto found;
          }
        }
      }
    }
found:
    lc = c;
  }
  OS_putchar(NL);
}
#endif

//
// Parse a string into an integer.
// We limit the number of characters scanned and specific a base (upto 16)
//
static VAR_TYPE parse_int(unsigned char maxlen, unsigned char base)
{
  VAR_TYPE v = 0;

  while (maxlen-- > 0)
  {
    char ch = *txtpos++;
    if (ch >= '0' && ch <= '9')
    {
      ch -= '0';
    }
    else if (ch >= 'A' && ch <= 'F')
    {
      ch -= 'A' - 10;
    }
    else if (ch >= 'a' && ch <= 'f')
    {
      ch -= 'a' - 10;
    }
    else
    {
error:
      error_num = ERROR_EXPRESSION;
      txtpos--;
      break;
    }
    if (ch >= base)
    {
      goto error;
    }
    v = v * base + ch;
  }
  return v;
}

//
// Find the information on the stack about the given variable. Return the info in
// the passed argument, and return the address of the variable.
// This method handles the complexites of finding variables which are on the stack (DIMs)
// as well as the simple, preset, variables. Regardless, we return appropriate information
// so the caller doesn't need to worry about where the variable is stored.
//
static unsigned char* get_variable_frame(char name, variable_frame** frame)
{
  unsigned char vname;

  if (VARIABLE_IS_EXTENDED(name))
  {
    unsigned char* ptr = *(unsigned char**)VARIABLE_INT_ADDR(name);
    *frame = (variable_frame*)ptr;
    return ptr + sizeof(variable_frame);
  }
  else
  {
    *frame = &normal_variable;
    return (unsigned char*)VARIABLE_INT_ADDR(name);
  }
}

//
// Parse the variable name and return a pointer to its memory and its size.
//
static unsigned char* parse_variable_address(variable_frame** vframe)
{
  ignore_blanks();

  const unsigned char name = *txtpos;

  if (name < 'A' || name > 'Z')
  {
    return NULL;
  }
  txtpos++;
  unsigned char* ptr = get_variable_frame(name, vframe);
  if ((*vframe)->type == VAR_DIM_BYTE)
  {
    VAR_TYPE index;   
#if defined(FEATURE_LAZY_INDEX) && FEATURE_LAZY_INDEX
    // check if we have an index without braces to save program space
    unsigned char* otxtpos = txtpos;
    if (*txtpos >= '0' && *txtpos <= '9')
    {
      index = parse_int(255, 10);
      error_num = ERROR_OK;
    }
    else
#endif    
    {
      index = expression(EXPR_BRACES);
    }
    if (error_num || index < 0 || index >= (*vframe)->header.frame_size - sizeof(variable_frame))
    {
#if defined(FEATURE_LAZY_INDEX) && FEATURE_LAZY_INDEX
      txtpos = otxtpos;
#endif
      return NULL;
    }      
    ptr += index;
  }
  return ptr;
}

//
// copy variable content to dst and return its size
//
static unsigned char copy_dim(unsigned char *dst)
{
  variable_frame* vframe = NULL;
  unsigned char* otxtpos = txtpos;
  unsigned char* ptr = parse_variable_address(&vframe);
  unsigned char len = 0;
  if (ptr)
  {
    // only interested in variables without index
    txtpos = otxtpos;
    return 0;
  }
  else if (vframe)
  {
    // No address, but we have a vframe - this is a full array
    if (error_num == ERROR_EXPRESSION)
      error_num = ERROR_OK; // clear parsing error due to missing index braces
    len = vframe->header.frame_size - sizeof(variable_frame);
    CHECK_HEAP_OOM(len, qoom);
    OS_memcpy(dst, ((unsigned char*)vframe) + sizeof(variable_frame), len);
  }	
  return len;
qoom:
  error_num = ERROR_OOM;
  return 0;	  
}

//
// Create an array
//
static void create_dim(unsigned char name, VAR_TYPE size, unsigned char* data)
{
  variable_frame* f;
  CHECK_SP_OOM(sizeof(variable_frame) + size, qoom);
  f = (variable_frame*)sp;
  f->header.frame_type = FRAME_VARIABLE_FLAG;
  f->header.frame_size = sizeof(variable_frame) + size;
  f->type = VAR_DIM_BYTE;
  f->name = name;
  f->ble = NULL;
  VARIABLE_SAVE(f);
  if (data)
  {
    OS_memcpy(sp + sizeof(variable_frame), data, size);
  }
  else
  {
    OS_memset(sp + sizeof(variable_frame), 0, size);
  }
  return;
qoom:
  error_num = ERROR_OOM;
  return;
}

//
// Clean the heap and stack
//
static void clean_memory(void)
{
  // Use the 'lineptr' to track if we've done this before (so we dont keep doing it)
  if (!lineptr)
  {
    return;
  }
  lineptr = NULL;
  
#if HAL_UART  
  // Stop serial interfaces
  for (unsigned char i = OS_MAX_SERIAL; i--; )
  {
    OS_serial_close(i);
  }
#endif

#if HAL_I2C_SLAVE  
  // Stop i2c interface
  OS_i2c_close(0);
#endif
  
  // Reset variables to 0 and remove all types
  OS_memset(variables_begin, 0, VAR_COUNT * VAR_SIZE + 4);
  
  // Reset file handles
  OS_memset(files, 0, sizeof(files));
  
  // Stop timers
  for (unsigned char i = 0; i < OS_MAX_TIMER; i++)
  {
    OS_timer_stop(i);
  }
  
#ifdef FEATURE_SAMPLING
  sampling.map = 0;  // switch sampling off
  sampling.mode = 0;  // set default mode
#endif
  
#if ENABLE_YIELD  
  // Stop pending yield event
  OS_yield(0);
#endif
  
  // Remove any persistent info from the stack.
  sp = (unsigned char*)variables_begin;
  
  // Remove any persistent info from the heap.
  for (unsigned char* ptr = (unsigned char*)program_end; ptr < heap; )
  {
    switch (((frame_header*)ptr)->frame_type)
    {
      case FRAME_SERVICE_FLAG:
      {
        gattAttribute_t* attr;
        GATTServApp_DeregisterService(((service_frame*)ptr)->attrs[0].handle, &attr);
        break;
      }
    }
    ptr += ((frame_header*)ptr)->frame_size;
  }
  heap = (unsigned char*)program_end;
  SET_MIN_MEMORY(sp - heap);
}

// -------------------------------------------------------------------------------------------
//
// Expression evaluator
//
// -------------------------------------------------------------------------------------------

static VAR_TYPE* expression_operate(unsigned char op, VAR_TYPE* queueptr)
{
  if (op == OP_UMINUS)
  {
    queueptr[-1] = -queueptr[-1];
  }
  else
  {
    queueptr--;
    switch (op)
    {
      case OP_ADD:
        queueptr[-1] += queueptr[0];
        break;
      case OP_SUB:
        queueptr[-1] -= queueptr[0];
        break;
      case OP_MUL:
        queueptr[-1] *= queueptr[0];
        break;
      case OP_DIV:
        if (queueptr[0] == 0)
        {
          goto expr_div0;
        }
        queueptr[-1] /= queueptr[0];
        break;
      case OP_REM:
        if (queueptr[0] == 0)
        {
          goto expr_div0;
        }
        queueptr[-1] %= queueptr[0];
        break;
      case OP_AND:
        queueptr[-1] &= queueptr[0];
        break;
      case OP_OR:
        queueptr[-1] |= queueptr[0];
        break;
      case OP_XOR:
        queueptr[-1] ^= queueptr[0];
        break;
      case OP_GE:
        queueptr[-1] = queueptr[-1] >= queueptr[0];
        break;
      case OP_NE:
      case OP_NE_BANG:
        queueptr[-1] = queueptr[-1] != queueptr[0];
        break;
      case OP_GT:
        queueptr[-1] = queueptr[-1] > queueptr[0];
        break;
      case OP_EQEQ:
      case OP_EQ:
        queueptr[-1] = queueptr[-1] == queueptr[0];
        break;
      case OP_LE:
        queueptr[-1] = queueptr[-1] <= queueptr[0];
        break;
      case OP_LT:
        queueptr[-1] = queueptr[-1] < queueptr[0];
        break;
      case OP_LSHIFT:
        queueptr[-1] <<= queueptr[0];
        break;
      case OP_RSHIFT:
        queueptr[-1] >>= queueptr[0];
        break;
      default:
        error_num = ERROR_EXPRESSION;
        return NULL;
    }
  }
  return queueptr;
expr_div0:
  error_num = ERROR_DIV0;
  return NULL;
}

static VAR_TYPE expression(unsigned char mode)
{
  VAR_TYPE queue[EXPRESSION_QUEUE_SIZE];
  VAR_TYPE* queueend = &queue[EXPRESSION_QUEUE_SIZE];
  struct stack_t
  {
    unsigned char op;
    unsigned char depth;
  };
  struct stack_t stack[EXPRESSION_STACK_SIZE];
  struct stack_t* stackend = &stack[EXPRESSION_STACK_SIZE];
  unsigned char lastop = 1;

  // Done parse if we have a pending error
  if (error_num)
  {
    return 0;
  }
  
  VAR_TYPE* queueptr = queue;
  struct stack_t* stackptr = stack;

  for (;;)
  {
    unsigned char op = *txtpos++;
    switch (op)
    {
      // Ignore whitespace
      case WS_SPACE:
        continue;

      case NL:
        txtpos--;
        goto done;
      
      default:
        // Parse number
        if (op >= '0' && op <= '9')
        {
          if (queueptr == queueend)
          {
            goto expr_oom;
          }
          txtpos--;
          *queueptr++ = parse_int(255, 10);
          error_num = ERROR_OK;
          lastop = 0;
        }
        else if (op >= 'A' && op <= 'Z')
        {
          variable_frame* frame;
          unsigned char* ptr = get_variable_frame(op, &frame);
          
          if (frame->type == VAR_DIM_BYTE)
          {
            if (stackptr + 1 >= stackend)
            {
              goto expr_oom;
            }
#if defined(FEATURE_LAZY_INDEX) && FEATURE_LAZY_INDEX
            if (*txtpos >= '0' && *txtpos <= '9')
            {
              unsigned char* otxtpos = txtpos;
              VAR_TYPE index = parse_int(255, 10);
              error_num = ERROR_OK;
              if (index < 0 || index >= frame->header.frame_size - sizeof(variable_frame))
              {
                txtpos = otxtpos;
                error_num = ERROR_EXPRESSION;
                goto expr_error;
              }
              ptr += index;
              *queueptr++ = *ptr;
              lastop = 0;
              break;
            }
#endif
            (stackptr++)->op = op;
            lastop = 1;
          }
          else
          {
            if (queueptr == queueend)
            {
              goto expr_oom;
            }
            *queueptr++ = *(VAR_TYPE*)ptr;
            lastop = 0;
          }
        }
        else
        {
          txtpos--;
          goto done;
        }
        break;
        
      case KW_CONSTANT:
        if (queueptr == queueend)
        {
          goto expr_oom;
        }
        *queueptr++ = constantmap[*txtpos++ - CO_TRUE];
        lastop = 0;
        break;

      case FUNC_HEX:
        if (queueptr == queueend)
        {
          goto expr_oom;
        }
        *queueptr++ = parse_int(255, 16);
        error_num = ERROR_OK;
        lastop = 0;
        break;
        
      case FUNC_LEN:
      {
        unsigned char ch = *txtpos;
        if (ch != '(')
        {
          goto expr_error;
        }
        ch = *++txtpos;
        if (ch == KW_SERIAL)
#if HAL_UART          
        {
          unsigned char port = 0;
          ignore_blanks();
          if (!(txtpos[1] & KW_CONSTANT))
          {
            port = *++txtpos - '0';
            if (port > (OS_MAX_SERIAL-1))
            {
              goto expr_error;
            }
            ignore_blanks();
          }
          ch = txtpos[1];
          if (!(ch == KW_READ || ch == KW_WRITE) || txtpos[2] != ')')
          {
            goto expr_error;
          }
          txtpos += 3;
          if (queueptr == queueend)
          {
            goto expr_oom;
          }
          *queueptr++ = OS_serial_available(port, ch == KW_READ ? 'R' : 'W');
        }
#else   // HAL_UART
          goto expr_error;
#endif  // HAL_UART
#if HAL_I2C_SLAVE
        else if (ch == KW_I2C)
        {
          ch = txtpos[1];
          if (!(ch == KW_READ || ch == KW_WRITE) || txtpos[2] != ')')
          {
            goto expr_error;
          }
          txtpos += 3;
          if (queueptr == queueend)
          {
            goto expr_oom;
          }
          *queueptr++ = OS_i2c_available(0, ch == KW_READ ? 'R' : 'W');
        }
#endif        
        else if (ch == '"' && txtpos[2] == '"') {
          if (txtpos[1] >= 'A' && txtpos[1] <= 'Z')
          {
            // figure out the length of the named file
            // tbd
            // goto expr_error;
          }
#if ENABLE_SNV      
          else if (txtpos[1] >= '0' && txtpos[1] <= '9')
          {
            // figure out the length of the named SNV item
            if (queueptr == queueend)
            {
              goto expr_oom;
            }
            unsigned char len = get_snv_length(SNV_MAKE_ID(txtpos[1]));
            *queueptr++ =  len > 0 ? len - 1 : 0;
            txtpos += 3;
            if (*txtpos++ != ')') goto expr_error;
          }
#endif
          else
          {
            goto expr_error;
          }
        }
        else if (ch < 'A' || ch > 'Z' || txtpos[1] != ')')
        {
          goto expr_error;
        }
        else if (queueptr == queueend)
        {
          goto expr_oom;
        }
        else
        {
          variable_frame* frame;
          txtpos += 2;
          get_variable_frame(ch, &frame);
          if (frame->type != VAR_DIM_BYTE)
          {
            goto expr_error;
          }
          *queueptr++ = frame->header.frame_size - sizeof(variable_frame);
        }
        lastop = 0;
        break;
      }

      case '(':
        if (stackptr == stackend)
        {
          goto expr_oom;
        }
        stackptr->depth = queueptr - queue;
        (stackptr++)->op = op;
        lastop = 1;
        break;

      case FUNC_MILLIS:
      case FUNC_BATTERY:
      case FUNC_ABS:
      case FUNC_RND:
      case FUNC_EOF:
#ifdef ENABLE_PORT0
      case KW_PIN_P0:
#endif
#ifdef ENABLE_PORT1
      case KW_PIN_P1:
#endif
#ifdef ENABLE_PORT2
      case KW_PIN_P2:
#endif
      case BLE_FUNC_BTPEEK:
      case FUNC_POW:
      case FUNC_TEMP:
      case KW_MEM:
        if (stackptr == stackend)
        {
          goto expr_oom;
        }
        (stackptr++)->op = op;
        lastop = 1;
        break;

      case ',':
        for (;;)
        {
          if (stackptr == stack)
          {
            if (mode == EXPR_COMMA)
            {
              goto done;
            }
            goto expr_error;
          }
          unsigned const op2 = (--stackptr)->op;
          if (op2 == '(')
          {
            stackptr++;
            break;
          }
          if ((queueptr = expression_operate(op2, queueptr)) <= queue)
          {
            goto expr_error;
          }
        }
        lastop = 0;
        break;

      case ')':
      {
        signed char depth = -1;
        for (;;)
        {
          unsigned const op2 = (--stackptr)->op;
          if (op2 == '(')
          {
            depth = (queueptr - queue) - stackptr->depth;
            if (stackptr == stack && mode == EXPR_BRACES)
            {
              goto done;
            }
            break;
          }
          if ((queueptr = expression_operate(op2, queueptr)) <= queue)
          {
            goto expr_error;
          }
        }
        if (stackptr > stack)
        {
          op = (--stackptr)->op;
          if (depth == 0)
          {
            switch (op)
            {
              case FUNC_BATTERY:
              {
#ifdef FEATURE_BOOST_CONVERTER
                VAR_TYPE top = BlueBasic_rawBattery;
               *queueptr++ = (top * 3720L) / 511L;
#else
               *queueptr++ = (VAR_TYPE)( OS_get_vdd_7() * 3720L / 127);
#endif // FEATURE_BOOST_CONVERTER
                break;
              }
              case FUNC_MILLIS:
                *queueptr++ = (VAR_TYPE)OS_get_millis();
                break;
              case FUNC_TEMP:
                //return temperature in degree celsius * 100
                *queueptr++ = OS_get_temperature(0);
                break;
            case KW_MEM:
                //return free heap space
                *queueptr++ = sp - heap;
                break;
              default:
                goto expr_error;
            }
          }
          else if (depth == 1)
          {
            const VAR_TYPE top = queueptr[-1];
            switch (op)
            {
              case FUNC_ABS:
                queueptr[-1] = top < 0 ? -top : top;
                break;
              case FUNC_RND:
                queueptr[-1] = OS_rand() % top;
                break;
              case FUNC_EOF:
                if (top < 0 || top >= FS_NR_FILE_HANDLES || !files[top].action)
                {
                  goto expr_error;
                }
                else
#if ENABLE_SNV                  
                  if (files[top].filename >= 'A')
#endif
                {
                  unsigned char* special = flashstore_findspecial(FS_MAKE_FILE_SPECIAL(files[top].filename, files[top].record));
                  if (special)
                  {
                    // check if poffset is at the end, so we look ahead into next record
                    unsigned char len = special[FLASHSPECIAL_DATA_LEN];
                    if (files[top].poffset == len)
                    {
                      unsigned short record = (files[top].record + 1) % files[top].modulo;
                      special = flashstore_findspecial(FS_MAKE_FILE_SPECIAL(files[top].filename, record));
                    }
                  }
                  queueptr[-1] = special ? 0 : 1;
                }
#if ENABLE_SNV
                else
                {
                  // check for SNV id
                  osalSnvId_t id = SNV_MAKE_ID(files[top].filename);
                  queueptr[-1] = osal_snv_read(id,0,0) == SUCCESS ? 0 : 1;
                }
#endif
                break;
#ifdef ENABLE_PORT0
              case KW_PIN_P0:
                queueptr[-1] = pin_read(0, top);
                break;
#endif
#ifdef ENABLE_PORT1
              case KW_PIN_P1:
                queueptr[-1] = pin_read(1, top);
                break;
#endif
#ifdef ENABLE_PORT2
              case KW_PIN_P2:
                queueptr[-1] = pin_read(2, top);
                break;
#endif
              case BLE_FUNC_BTPEEK:
                if (top & 0x8000)
                {
                  goto expr_error; // Not supported
                }
                if (_GAPROLE(top) >= _GAPROLE(BLE_PAIRING_MODE) && top <= _GAPROLE(BLE_ERASE_SINGLEBOND))
                {
#if GAP_BOND_MGR
                  if (GAPBondMgr_GetParameter(_GAPROLE(top), (unsigned long*)&queueptr[-1]) != SUCCESS)
                  {
                    goto expr_error;
                  }
#else
                  goto expr_error;
#endif
                }
                else
                {
                  if (GAPRole_GetParameter(_GAPROLE(top), (unsigned long*)&queueptr[-1]) != SUCCESS)
                  {
                    goto expr_error;
                  }
                }
                break;

              //return temperature in degree celsius * 100 
              //TEMP(1) will wait for P0(2) to be 0.
              case FUNC_TEMP:
                queueptr[-1] = OS_get_temperature(top ? 1:0);
                break;
                
//            case KW_MEM:
//                switch (top)
//                {
//                case 0:  // return free heap
//                  queueptr[-1] = sp - heap;
//                case 1:  // return free flash
//                  queueptr[-1] =  flashstore_freemem();
//                default:
//                  goto expr_error;
//                }
//                break;

              default:
                if (op >= 'A' && op <= 'Z')
                {
                  variable_frame* frame;
                  unsigned char* ptr = get_variable_frame(op, &frame);
                  if (frame->type != VAR_DIM_BYTE || top < 0 || top >= frame->header.frame_size - sizeof(variable_frame))
                  {
                    goto expr_error;
                  }
                  queueptr[-1] = ptr[top];
                }
                else
                {
                  stackptr++;
                }
                break;
            }
          }
          else if (depth == 2)
          {
            double base = (double)(queueptr[-2]) / 0x10000;
            double exp  = (double)(queueptr[-1]) / 0x10000;
            (--queueptr)[-1] = (VAR_TYPE)(pow(base, exp) * 0x10000);
          }            
          else
          {
            goto expr_error;
          }
          lastop = 1;
        }
        break;
      }

      case OP_SUB:
        if (lastop)
        {
          // Unary minus
          op = OP_UMINUS;
        }
        // Fall through
      case OP_ADD:
      case OP_MUL:
      case OP_DIV:
      case OP_REM:
      case OP_AND:
      case OP_OR:
      case OP_XOR:
      case OP_GE:
      case OP_NE:
      case OP_GT:
      case OP_EQEQ:
      case OP_EQ:
      case OP_LE:
      case OP_LT:
      case OP_NE_BANG:
      case OP_LSHIFT:
      case OP_RSHIFT:
      {
        const unsigned char op1precedence = operator_precedence[op - OP_ADD];
        //for (;;)
        while(stackptr != stack)
        {
          const unsigned char op2 = stackptr[-1].op - OP_ADD;
          //if (stackptr == stack || op2 >= sizeof(operator_precedence) || op1precedence < operator_precedence[op2])
          if (op2 >= sizeof(operator_precedence) || op1precedence < operator_precedence[op2])
          {
            break;
          }
          if ((queueptr = expression_operate((--stackptr)->op, queueptr)) <= queue)
          {
            goto expr_error;
          }
        }
        if (stackptr == stackend)
        {
          goto expr_oom;
        }
        (stackptr++)->op = op;
        lastop = 1;
        break;
      }
    }
  }
done:
  while (stackptr > stack)
  {
    if ((queueptr = expression_operate((--stackptr)->op, queueptr)) <= queue)
    {
      goto expr_error;
    }
  }
  if (queueptr != &queue[1])
  {
    goto expr_error;
  }
  return queueptr[-1];
expr_error:
  if (!error_num)
  {
    error_num = ERROR_EXPRESSION;
  }
  return 0;
expr_oom:
  error_num = ERROR_OOM;
  return 0;
}

// -------------------------------------------------------------------------------------------
//
// The main interpreter
//
// -------------------------------------------------------------------------------------------

void interpreter_init()
{
#ifdef DEBUG_P20_SET
  P2DIR |= 1;
#endif
#ifndef TARGET_CC254X
  assert(LAST_KEYWORD < 256);
#else
  static_assert(LAST_KEYWORD < 256, "keyword table too large");
#endif
  program_start = OS_malloc(kRamSize);
  OS_memset(program_start, 0, kRamSize);
  variables_begin = (unsigned char*)program_start + kRamSize - VAR_COUNT * VAR_SIZE - 4; // 4 bytes = 32 bits of flags
  sp = variables_begin;
  program_end = flashstore_init(program_start);
  heap = (unsigned char*)program_end;
  SET_MIN_MEMORY(sp - heap);
  //interpreter_banner();
}

#if ENABLE_BLE_CONSOLE
void interpreter_banner(void)
{
  printmsg(initmsg);
  printmsg(urlmsg);
  printnum(0, flashstore_freemem());
  printmsg(memorymsg);
#if defined (FEATURE_CALIBRATION) && (!defined (XOSC32K_INSTALLED) || (defined (XOSC32K_INSTALLED) && (XOSC32K_INSTALLED == TRUE)))
  // report 32 KHz crystal periode
  extern uint16 periode_32k;
  printnum (0, periode_32k);
  printmsg(" clks XOSC32K");
#endif  
  printmsg(error_msgs[ERROR_OK]);
}
#endif

//
// Setup the interpreter.
//  Initialize everything we need to run the interpreter, and process any
//  'autorun' which may have been set.
//  return TRUE when autostart was scheduled, otherwise FALSE
bool interpreter_setup(void)
{
  OS_init();
  interpreter_init();
#if ENABLE_BLE_CONSOLE  
  OS_prompt_buffer(heap + sizeof(LINENUM), sp);
#endif  
  if (flashstore_findspecial(FLASHSPECIAL_AUTORUN))
  {
    if (program_end > program_start)
    {
      OS_timer_start(DELAY_TIMER, OS_AUTORUN_TIMEOUT, 0, *(LINENUM*)*program_start);
      return TRUE;
    }
  }
  return FALSE;
}

#if ENABLE_BLE_CONSOLE
//
// Run the main interpreter while we have new commands to execute
//
void interpreter_loop(void)
{
  while (OS_prompt_available())
  {
    interpreter_run(0, 0);
    OS_prompt_buffer(heap + sizeof(LINENUM), sp);
  }
}
#endif

//
// The main interpreter engine
//
unsigned char interpreter_run(LINENUM gofrom, unsigned char canreturn)
{
#if defined ENABLE_YIELD && ENABLE_YIELD 
  if (canreturn & INTERPRETER_CAN_YIELD)
  {
    yield_time = OS_get_millis();
  }
#endif  
  error_num = ERROR_OK;

  if (gofrom)
  {
    event_frame *f;

    linenum = gofrom;
    if (canreturn & INTERPRETER_CAN_RETURN)
    {
      CHECK_SP_OOM(sizeof(event_frame), qoom);
      f = (event_frame *)sp;
      f->header.frame_type = FRAME_EVENT_FLAG;
      f->header.frame_size = sizeof(event_frame);
    }
    lineptr = findlineptr();
    txtpos = *lineptr + sizeof(LINENUM) + sizeof(char);
    if (lineptr >= program_end)
    {
      goto print_error_or_ok;
    }
    goto interperate;
  }
  
#if ENABLE_BLE_CONSOLE  
  
#if !defined(ENABLE_WIRE) || ENABLE_WIRE  
  // Remove any pending WIRE parsing.
  pinParsePtr = NULL;
#endif

  txtpos = heap + sizeof(LINENUM);
  tokenize();

  {
    unsigned char linelen;

    // Move it to the end of program_memory
    // Find the end of the freshly entered line
    for(linelen = 0; txtpos[linelen++] != NL;)
      ;
    OS_rmemcpy(sp - linelen, txtpos, linelen);
    txtpos = sp - linelen;

    // Now see if we have a line number
    testlinenum();
    ignore_blanks();
    if (linenum == 0)
    {
      lineptr = program_end;
      goto direct;
    }
    if (linenum == 0xFFFF)
    {
      GOTO_QWHAT;
    }
    
    // Clean the memory (heap & stack) if we're modifying the code
    clean_memory();

    // Allow space for line header
    txtpos -= sizeof(LINENUM) + sizeof(char);

    // Calculate the length of what is left, including the (yet-to-be-populated) line header
    linelen = sp - txtpos;

    // Now we have the number, add the line header.
    *((LINENUM*)txtpos) = linenum;
    txtpos[sizeof(LINENUM)] = linelen;
    
    if (txtpos[sizeof(LINENUM) + sizeof(char)] == NL) // If the line has no txt, it was just a delete
    {
      program_end = flashstore_deleteline(linenum);
    }
    else
    {
      SEMAPHORE_FLASH_WAIT();
      unsigned char** newend = flashstore_addline(txtpos);
      if (!newend)
      {
        // No space - attempt to compact flash
        flashstore_compact(linelen, (unsigned char*)program_start, sp);
        newend = flashstore_addline(txtpos);
        if (!newend)
        {
          // Still no space
          SEMAPHORE_FLASH_SIGNAL();
          return IX_OUTOFMEMORY;
        }
      }
      SEMAPHORE_FLASH_SIGNAL();
      program_end = newend;
    }
    heap = (unsigned char*)program_end;
  }
#endif

  
prompt:
#if ENABLE_BLE_CONSOLE  
  OS_prompt_buffer(heap + sizeof(LINENUM), sp);
#endif  
  return IX_PROMPT;

// -- Commands ---------------------------------------------------------------

#if ENABLE_BLE_CONSOLE
direct:
  txtpos = heap + sizeof(LINENUM);
  if (*txtpos == NL)
  {
    goto prompt;
  }
  else
  {
    goto interperate;
  }
#endif
  
// ---------------------------------------------------------------------------

  
run_next_statement:
  txtpos = *++lineptr + sizeof(LINENUM) + sizeof(char);
  if (lineptr >= program_end) // Out of lines to run
  {
    goto print_error_or_ok;
  }
  // check how long the programs already runs
  // and yield processing if time is up
#if defined ENABLE_YIELD && ENABLE_YIELD
  if (canreturn & INTERPRETER_CAN_YIELD)
  {
    if ( (OS_get_millis() - yield_time) >= timeSlice) 
    {
      unsigned short line = *(LINENUM*)lineptr[0];
      OS_yield(line);
      goto prompt;
    }
  }
#endif  
  interperate:
  switch (*txtpos++)
  {
    default:
      if (*--txtpos < 0x80)
      {
        goto assignment;
      }
      break;
    case KW_CONSTANT:
      GOTO_QWHAT;
#if ENABLE_BLE_CONSOLE      
    case KW_LIST:
      goto list;
#endif
    case KW_MEM:
      goto mem;
    case KW_NEW:
      if (*txtpos != NL)
      {
        GOTO_QWHAT;
      }
      clean_memory();
      program_end = flashstore_deleteall();
      heap = (unsigned char*)program_end;
      goto print_error_or_ok;
    case KW_RUN:
      clean_memory();
      lineptr = program_start;
      if (lineptr >= program_end)
      {
        goto print_error_or_ok;
      }
      txtpos = *lineptr + sizeof(LINENUM) + sizeof(char);
      goto interperate;
    case KW_NEXT:
      goto next;
    case KW_IF:
    case KW_ELIF:
      goto cmd_elif;
    case KW_ELSE:
      goto cmd_else;
    case KW_GOTO:
      linenum = expression(EXPR_NORMAL);
      if (error_num || *txtpos != NL)
      {
        GOTO_QWHAT;
      }
      lineptr = findlineptr();
      if (lineptr >= program_end)
      {
        goto print_error_or_ok;
      }
      txtpos = *lineptr + sizeof(LINENUM) + sizeof(char);
      goto interperate;
    case KW_GOSUB:
      goto cmd_gosub;
    case KW_RETURN:
      goto gosub_return;
    case KW_REM:
    case KW_SLASHSLASH:
      goto run_next_statement;
    case KW_FOR:
      goto forloop;
    case KW_PRINT:
      goto print;
    case KW_REBOOT:
      goto cmd_reboot;
    case KW_END:
      goto run_next_statement;
    case KW_DIM:
      goto cmd_dim;
    case KW_TIMER:
      goto cmd_timer;
    case KW_DELAY:
      goto cmd_delay;
    case KW_AUTORUN:
      goto cmd_autorun;
#ifdef ENABLE_PORT0
    case KW_PIN_P0:
#endif
#ifdef ENABLE_PORT1
    case KW_PIN_P1:
#endif
#ifdef ENABLE_PORT2
    case KW_PIN_P2:
#endif
      txtpos--;
      goto assignpin;
    case KW_GATT:
      goto ble_gatt;
    case KW_ADVERT:
      ble_isadvert = 1;
      goto ble_advert;
    case KW_SCAN:
      ble_isadvert = 0;
      goto ble_scan;
    case KW_BTPOKE:
      goto cmd_btpoke;
    case KW_PINMODE:
      goto cmd_pinmode;
    case KW_INTERRUPT:
      goto cmd_interrupt;
    case KW_SERIAL:
      goto cmd_serial;
#if !defined(ENABLE_SPI) || ENABLE_SPI      
    case KW_SPI:
      goto cmd_spi;
#endif      
    case KW_ANALOG:
      goto cmd_analog;
    case KW_CONFIG:
      goto cmd_config;
#if !defined(ENABLE_WIRE) || ENABLE_WIRE      
    case KW_WIRE:
      goto cmd_wire;
#endif      
#if HAL_I2C     
    case KW_I2C:
      goto cmd_i2c;
#endif
    case KW_OPEN:
      goto cmd_open;
    case KW_CLOSE:
      goto cmd_close;
    case KW_READ:
      goto cmd_read;
    case KW_WRITE:
      goto cmd_write;
  }
  GOTO_QWHAT;

// -- Errors -----------------------------------------------------------------
  
qoom:
  error_num = ERROR_OOM;
  goto print_error_or_ok;
  
qtoobig:
  error_num = ERROR_TOOBIG;
  goto print_error_or_ok;

#if !defined(ENABLE_INTERRUPT) || (ENABLE_INTERRUPT)  
qbadpin:
  error_num = ERROR_BADPIN;
  goto print_error_or_ok;
#endif
  
qdirect:
  error_num = ERROR_DIRECT;
  goto print_error_or_ok;

qeof:
  error_num = ERROR_EOF;
  goto print_error_or_ok;

qwhat:
  if (!error_num)
  {
    error_num = ERROR_GENERAL;
  }
  // Fall through ...

print_error_or_ok:
#ifdef REPORT_ERROR_LINE  
  if (err_line)
  {
    printnum(0, err_line);
    printmsg(" : BlueBasic_Interpreter.c"); 
    err_line = 0;
  }
#endif  
#if ENABLE_BLE_CONSOLE    
  printmsg(error_msgs[error_num]);
  if (lineptr < program_end && error_num != ERROR_OK)
  {
    list_line = *lineptr;
    OS_putchar('>');
    OS_putchar('>');
    OS_putchar(WS_SPACE);
    printline(0, 1);
    OS_putchar(NL);
  }
#endif    
  if (error_num == ERROR_OOM)
  {
    printmsg("rebooting...");
//    for (long t0 = OS_get_millis(); OS_get_millis() - t0 < 1000; osal_run_system());
    for (uint8 cnt = 0; --cnt ; osal_run_system() )
       OS_delaymicroseconds(2);
    clean_memory(); 
    OS_reboot(0);
//    return IX_OUTOFMEMORY;
  }
  goto prompt;  

// --- Commands --------------------------------------------------------------

  VAR_TYPE val;

cmd_elif:
    val = expression(EXPR_NORMAL);
    if (error_num || *txtpos != NL)
    {
      GOTO_QWHAT;
    }
    if (val)
    {
      goto run_next_statement;
    }
    else
    {
      unsigned char nest = 0;
      for (;;)
      {
        txtpos = *++lineptr;
        if (lineptr >= program_end)
        {
          printmsg(error_msgs[ERROR_OK]);
          goto prompt;
        }
        switch (txtpos[sizeof(LINENUM) + sizeof(char)])
        {
          case KW_IF:
            nest++;
            break;
          case KW_ELSE:
            if (!nest)
            {
              txtpos += sizeof(LINENUM) + sizeof(char) + 1;
              goto run_next_statement;
            }
            break;
          case KW_ELIF:
            if (!nest)
            {
              txtpos += sizeof(LINENUM) + sizeof(char);
              goto interperate;
            }
            break;
          case KW_END:
            if (!nest)
            {
              txtpos += sizeof(LINENUM) + sizeof(char) + 1;
              goto run_next_statement;
            }
            nest--;
            break;
          default:
            break;
        }
      }
    }
 
cmd_else:
  {
    unsigned char nest = 0;
    ignore_blanks();
    if (*txtpos != NL)
    {
      GOTO_QWHAT;
    }
    for (;;)
    {
      txtpos = *++lineptr;
      if (lineptr >= program_end)
      {
        printmsg(error_msgs[ERROR_OK]);
        goto prompt;
      }
      if (txtpos[sizeof(LINENUM) + sizeof(char)] == KW_IF)
      {
        nest++;
      }
      else if (txtpos[sizeof(LINENUM) + sizeof(char)] == KW_END)
      {
        if (!nest)
        {
          txtpos += sizeof(LINENUM) + sizeof(char) + 1;
          goto run_next_statement;
        }
        nest--;
      }
    }
  }

forloop:
  {
    unsigned char vname;
    unsigned char var;
    VAR_TYPE initial;
    VAR_TYPE step;
    VAR_TYPE terminal;
    for_frame *f;

    if (*txtpos < 'A' || *txtpos > 'Z')
    {
      GOTO_QWHAT;
    }
    var = *txtpos++;
    ignore_blanks();
    if (*txtpos != OP_EQ)
    {
      GOTO_QWHAT;
    }
    txtpos++;

    initial = expression(EXPR_NORMAL);
    if (error_num)
    {
      GOTO_QWHAT;
    }

    if (*txtpos++ != ST_TO)
    {
      GOTO_QWHAT;
    }

    terminal = expression(EXPR_NORMAL);
    if (error_num)
    {
      GOTO_QWHAT;
    }

    if (*txtpos++ == ST_STEP)
    {
      step = expression(EXPR_NORMAL);
      if (error_num)
      {
        GOTO_QWHAT;
      }
    }
    else
    {
      txtpos--;
      step = 1;
    }
    ignore_blanks();
    if (*txtpos != NL)
    {
      GOTO_QWHAT;
    }

    CHECK_SP_OOM(sizeof(for_frame), qoom);
    f = (for_frame *)sp;
    if (VARIABLE_IS_EXTENDED(var))
    {
      GOTO_QWHAT;
    }
    VARIABLE_INT_SET(var, initial);
    f->header.frame_type = FRAME_FOR_FLAG;
    f->header.frame_size = sizeof(for_frame);
    f->for_var = var;
    f->terminal = terminal;
    f->step = step;
    f->line = lineptr;
    goto run_next_statement;
  }

cmd_gosub:
  {
    gosub_frame *f;

    linenum = expression(EXPR_NORMAL);
    if (error_num || *txtpos != NL)
    {
      GOTO_QWHAT;
    }
    CHECK_SP_OOM(sizeof(gosub_frame), qoom);
    f = (gosub_frame *)sp;
    f->header.frame_type = FRAME_GOSUB_FLAG;
    f->header.frame_size = sizeof(gosub_frame);
    f->line = lineptr;
    lineptr = findlineptr();
    if (lineptr >= program_end)
    {
      goto print_error_or_ok;
    }
    txtpos = *lineptr + sizeof(LINENUM) + sizeof(char);
    goto interperate;
  }
  
next:
  // Find the variable name
  if (*txtpos < 'A' || *txtpos > 'Z' || txtpos[1] != NL)
  {
    GOTO_QWHAT;
  }

gosub_return:  
  switch (cleanup_stack())
  {
  case RETURN_GOSUB:
    goto run_next_statement;
  case RETURN_EVENT:
    goto prompt;
  case RETURN_FOR:
    goto run_next_statement;
  case RETURN_ERROR_OOM:
    goto qoom;
  case RETURN_ERROR_OR_OK:
    goto print_error_or_ok;
//  case RETURN_QWHAT:
  default:
    GOTO_QWHAT;
  }
  
//
// PX(Y) = Z
// Assign a value to a pin.
//
assignpin:
  {
    unsigned char pin[1];
    
    pin[0] = pin_parse();
    if (error_num)
    {
      GOTO_QWHAT;
    }
    if (*txtpos != OP_EQ)
    {
      GOTO_QWHAT;
    }
    txtpos++;
    val = expression(EXPR_NORMAL);
    if (error_num)
    {
      GOTO_QWHAT;
    }
    pin[0] |= (val ? WIRE_PIN_HIGH : WIRE_PIN_LOW);
    pin_wire(pin, pin + 1);
    
    goto run_next_statement;
  }

//
// X = Y
// Assign a value to a variable.
//
assignment:
  {
    variable_frame* frame;
    unsigned char* ptr;

    ptr = parse_variable_address(&frame);
    if (*txtpos != OP_EQ || (ptr == NULL && (frame == NULL || frame->type != VAR_DIM_BYTE)))
    {
      GOTO_QWHAT;
    }
    error_num = ERROR_OK;
    txtpos++;
    if (ptr)
    {
      val = expression(EXPR_NORMAL);
      if (error_num)
      {
        GOTO_QWHAT;
      }
      if (frame->type == VAR_DIM_BYTE)
      {
        *ptr = val;
      }
      else
      {
        *(VAR_TYPE*)ptr = val;
      }
    }
    else
    {
      // Array assignment
      ptr = ((unsigned char*)frame) + sizeof(variable_frame);
      unsigned short len = frame->header.frame_size - sizeof(variable_frame);
      while (len--)
      {
        val = expression(EXPR_COMMA);
        if (error_num)
        {
          GOTO_QWHAT;
        }
        *ptr++ = val;
      }
    }
    
    if (frame->ble)
    {
      ble_notify_assign(frame->ble);
    }
    goto run_next_statement;
  }

#if ENABLE_BLE_CONSOLE  
//
// LIST [<linenr>]
// List the current program starting at the optional line number.
//
list:
  {
    unsigned char indent = 1;

    testlinenum();

    // Should be EOL
    if (*txtpos != NL)
    {
      GOTO_QWHAT;
    }
    
    for(lineptr = findlineptr(); lineptr < program_end; lineptr++)
    {
      list_line = *lineptr;
      switch (list_line[sizeof(LINENUM) + sizeof(char)])
      {
        case KW_ELSE:
        case KW_ELIF:
          if (indent > 1)
          {
            indent--;
          }
          // Fall through
        case KW_IF:
        case KW_FOR:
          printline(0, indent);
          indent++;
          break;
        case KW_NEXT:
        case KW_END:
          if (indent > 1)
          {
            indent--;
          }
          // Fall through
        default:
          printline(0, indent);
          break;
      }
    }
  }
  goto run_next_statement;
#endif  

print:
#if ENABLE_BLE_CONSOLE
  for (;;)
  {
    if (*txtpos == NL)
    {
      break;
    }
    else if (!print_quoted_string())
    {
      if (*txtpos == '"' || *txtpos == '\'')
      {
        GOTO_QWHAT;
      }
      else if (*txtpos == ',' || *txtpos == WS_SPACE)
      {
        txtpos++;
      }
      else
      {
        VAR_TYPE e;
        e = expression(EXPR_COMMA);
        if (error_num)
        {
          GOTO_QWHAT;
        }
        printnum(0, e);
      }
    }
  }
  OS_putchar(NL);
#endif  
  goto run_next_statement;

//
// MEM
// Print the current free memory.
// and free heap
mem:
  printnum(0, flashstore_freemem());
  printmsg(memorymsg);
  printnum(0, sp - heap);
  printmsg(" bytes on heap free.");
#if CHECK_MIN_MEMORY
  CHECK_MIN_MEMORY();
  printnum(0, minMemory);
  printmsg(" bytes lowest free heap.");
#endif  
#if OSALMEM_METRICS
  extern uint16 blkMax;  // Max cnt of all blocks ever seen at once.
  extern uint16 blkCnt;  // Current cnt of all blocks.
  extern uint16 blkFree; // Current cnt of free blocks.
  extern uint16 memAlo;  // Current total memory allocated.
  extern uint16 memMax;  // Max total memory ever allocated at once.
  printmsg("OSAL Memory Metrics:");
  printnum(0, blkMax);
  printmsg(" max cnt of all blocks ever seen at once.");
  printnum(0, blkCnt);
  printmsg(" current cnt of all blocks.");
  printnum(0, blkFree);
  printmsg(" current cnt of free blocks.");
  printnum(0, memAlo);
  printmsg(" current total memory allocated.");
  printnum(0, memMax);
  printmsg(" max total memory ever allocated at once.");           
#endif  
  goto run_next_statement;

//
// REBOOT [UP]
//  Reboot the device. If the UP option is present, reboot into upgrade mode.
//
cmd_reboot:
  {
    unsigned char up = 0;
#ifdef OAD_IMAGE_VERSION
    if (txtpos[0] == 'U' && txtpos[1] == 'P' && txtpos[2] == NL)
    {
      up = 1;
    }
#endif
    // stop pending timer, etc.
    clean_memory(); 
    OS_reboot(up);
  }
  GOTO_QWHAT; // Not reached

//
// DIM <var>(<size>)
// Converts a variable into an array of bytes of the given size.
//
cmd_dim:
  {
    VAR_TYPE size;
    unsigned char name;

    if (*txtpos < 'A' || *txtpos > 'Z')
    {
      GOTO_QWHAT;
    }
    name = *txtpos++;
    size = expression(EXPR_BRACES);
    if (error_num || !size)
    {
      GOTO_QWHAT;
    }
    create_dim(name, size, NULL);
    if (error_num)
    {
      GOTO_QWHAT;
    }
  }
  goto run_next_statement;

//
// TIMER <timer number>, <timeout ms> [REPEAT] GOSUB <linenum>
// Creates an optionally repeating timer which will call a specific subroutine everytime it fires.
//
cmd_timer:
  {
    unsigned char timer;
    VAR_TYPE timeout;
    unsigned char repeat = 0;
    LINENUM subline;
    
    timer = (unsigned char)expression(EXPR_COMMA);
    if (error_num)
    {
      GOTO_QWHAT;
    }
    ignore_blanks();
    if (*txtpos == TI_STOP)
    {
      txtpos++;
      OS_timer_stop(timer);
    }
    else
    {
      timeout = expression(EXPR_NORMAL);
      if (error_num)
      {
        GOTO_QWHAT;
      }
      ignore_blanks();
      if (*txtpos == TI_REPEAT)
      {
        txtpos++;
        repeat = 1;
      }
      if (*txtpos == KW_GOSUB)
      {
        txtpos++;
        subline = (LINENUM)expression(EXPR_NORMAL);
        if (error_num)
        {
          GOTO_QWHAT;
        }
      }
      else
      {
#ifdef FEATURE_SAMPLING 
        // allow TIMER without GOSUB
        if (*txtpos == NL)
        {
          subline = 0;
        }
        else
#endif
          GOTO_QWHAT;
      }
      if (!OS_timer_start(timer, timeout, repeat, subline))
      {
        GOTO_QWHAT;
      }
    }
  }
  goto run_next_statement;

//
// DELAY <timeout>
// Pauses execution for timeout-ms. -1 means forever
//
cmd_delay:
  val = expression(EXPR_NORMAL);
  if (error_num)
  {
    GOTO_QWHAT;
  }
  if (val >= 0)
  {
    OS_timer_start(DELAY_TIMER, val, 0, *(LINENUM*)lineptr[1]);
  }
  else if (val < -1)
  {
    GOTO_QWHAT;
  }
  goto prompt;

//
// AUTORUN ON|OFF
// When on, will load the save program at boot time and execute it.
//
cmd_autorun:
#if ENABLE_BLE_CONSOLE  
  {
    VAR_TYPE v = expression(EXPR_NORMAL);
    if (error_num)
    {
      GOTO_QWHAT;
    }
    if (v)
    {
      unsigned char autorun[7];
      autorun[2] = 7;
      *(unsigned long*)&autorun[3] = FLASHSPECIAL_AUTORUN;
      addspecial_with_compact(autorun);
    }
    else
    {
      flashstore_deletespecial(FLASHSPECIAL_AUTORUN);
    }
  }
#endif  
  goto run_next_statement;

cmd_pinmode:
  {
    unsigned char pinMode;
    unsigned char cmds[4];
    unsigned char* cmd = cmds;
    
    *cmd++ = WIRE_PIN;
    *cmd++ = pin_parse();
    if (error_num)
    {
      GOTO_QWHAT;
    }

    ignore_blanks();
    switch (*txtpos++)
    {
      case PM_INPUT:
        *cmd++ = WIRE_INPUT;
        pinMode = *txtpos++;
        if (pinMode == NL)
        {
          *cmd++ = WIRE_INPUT_NORMAL;
          txtpos--;
        }
        else if (pinMode == PM_PULLUP)
        {
          *cmd++ = WIRE_INPUT_PULLUP;
        }
        else if (pinMode == PM_PULLDOWN)
        {
          *cmd++ = WIRE_INPUT_PULLDOWN;
        }
        else if (pinMode == PM_ADC)
        {
          if (PIN_MAJOR(cmd[-1]) != 0)
          {
            GOTO_QWHAT;
          }
          *cmd++ = WIRE_INPUT_ADC;
//--- aquistion mode -----         
          
//--- end aquisition mode ----
        }
        else
        {
          txtpos--;
          GOTO_QWHAT;
        }
        break;
      case PM_OUTPUT:
        *cmd++ = WIRE_OUTPUT;
        *cmd++ = WIRE_INPUT_NORMAL; // Despite this saying 'input', it sets up the rest of the pin
        break;
      default:
        txtpos--;
        GOTO_QWHAT;
    }
    
    pin_wire(cmds, cmd);
    if (error_num)
    {
      GOTO_QWHAT;
    }
#ifdef FEATURE_CALIBRATION
    PMUX = 0; // disable 32KHz clock
#endif      
  }
  goto run_next_statement;

//
// INTERRUPT ATTACH <pin> RISING|FALLING GOSUB <linenr>
// Attach an interrupt handler to the given pin. When the pin either falls or rises, the specified
// subroutine will be called.
//
// INTERRUPT DETACH <pin>
//
cmd_interrupt:
#if !defined(ENABLE_INTERRUPT) || (ENABLE_INTERRUPT)
  {
    unsigned short pin;
    unsigned char i;

    if (*txtpos == IN_ATTACH)
    {
      LINENUM line;
      unsigned char falling = 0;

      txtpos++;
      pin = pin_parse();
      if (error_num)
      {
        GOTO_QWHAT;
      }
      if (PIN_MAJOR(pin) == 2 && PIN_MINOR(pin) >= 4)
      {
        goto qbadpin;
      }
      ignore_blanks();
      switch (*txtpos++)
      {
        case PM_RISING:
          falling = 0;
          break;
        case PM_FALLING:
          falling = 1;
          break;
        default:
          txtpos--;
          GOTO_QWHAT;
      }
      if (*txtpos++ != KW_GOSUB)
      {
        txtpos--;
        GOTO_QWHAT;
      }
      line = expression(EXPR_NORMAL);
      if (error_num)
      {
        GOTO_QWHAT;
      }
      
      if (!OS_interrupt_attach(pin, line))
      {
        goto qbadpin;
      }

      i = 1 << PIN_MINOR(pin);
      if (PIN_MAJOR(pin) == 0)
      {
        PICTL = (PICTL & ~0x01) | falling;
        P0IEN |= i;
        IEN1 |= 1 << 5;
      }
      else if (PIN_MAJOR(pin) == 1)
      {
        if (PIN_MINOR(pin) < 4)
        {
          PICTL = (PICTL & ~0x02) | (falling << 1);
        }
        else
        {
          PICTL = (PICTL & ~0x04) | (falling << 2);
        }
        P1IEN |= i;
        IEN2 |= 1 << 4;
      }
      else
      {
        PICTL = (PICTL & ~0x08) | (falling << 3);
        P2IEN |= i;
        IEN2 |= 1 << 1;
      }
    }
    else if (*txtpos == IN_DETACH)
    {
      txtpos++;
      pin = pin_parse();
      if (error_num)
      {
        GOTO_QWHAT;
      }
      
      i = ~(1 << PIN_MINOR(pin));
      if (PIN_MAJOR(pin) == 0)
      {
        P0IEN &= i;
        if (P0IEN == 0)
        {
          IEN1 &= ~(1 << 5);
        }
      }
      else if (PIN_MAJOR(pin) == 1)
      {
        P1IEN &= i;
        if (P1IEN == 0)
        {
          IEN2 &= ~(1 << 4);
        }
      }
      else
      {
        P2IEN &= i;
        if (P2IEN == 0)
        {
          IEN2 &= ~(1 << 1);
        }
      }
      
      if (!OS_interrupt_detach(pin))
      {
        goto qbadpin;
      }
    }
    else
    {
      GOTO_QWHAT;
    }
    goto run_next_statement; 
  }
#else
  GOTO_QWHAT;
#endif
  
//
// SERIAL <baud>,<parity:N|P>,<bits>,<stop>,<flow> [ONREAD GOSUB <linenum>] [ONWRITE GOSUB <linenum>]
// or  
// SERIAL #<port> <baud>,<parity:N|P>,<bits>,<stop>,<flow> [ONREAD GOSUB <linenum>] [ONWRITE GOSUB <linenum>]
//
cmd_serial:
#if HAL_UART
  {
    unsigned char port = 0;
    if (*txtpos == '#')
    {
      txtpos++;
      ignore_blanks();
      port = expression(EXPR_COMMA);
      if (port > (OS_MAX_SERIAL-1))
      {
        GOTO_QWHAT;
      }
    }
    ignore_blanks();
    unsigned long baud = expression(EXPR_COMMA);
    ignore_blanks();
    unsigned char parity = *txtpos++;
    ignore_blanks();
    if (*txtpos++ != ',')
    {
      GOTO_QWHAT;
    }
    unsigned char bits = expression(EXPR_COMMA);
    unsigned char stop = expression(EXPR_COMMA);
    unsigned char flow = *txtpos++;
    if (error_num)
    {
      GOTO_QWHAT;
    }
    LINENUM onread = 0;
    if (*txtpos == BLE_ONREAD)
    {
      txtpos++;
      if (*txtpos++ != KW_GOSUB)
      {
        GOTO_QWHAT;
      }
      onread = expression(EXPR_NORMAL);
    }
    LINENUM onwrite = 0;
    if (*txtpos == BLE_ONWRITE)
    {
      txtpos++;
      if (*txtpos++ != KW_GOSUB)
      {
        GOTO_QWHAT;
      }
      onwrite = expression(EXPR_NORMAL);
    }
    if (OS_serial_open(port, baud, parity, bits, stop, flow, onread, onwrite))
    {
      GOTO_QWHAT;
    }
  }
  goto run_next_statement;
#else // HAL_UART
  GOTO_QWHAT;
#endif // HAL_UART
  
#if !defined(ENABLE_WIRE) || ENABLE_WIRE  
//
// WIRE ...
//
cmd_wire:  
  if (lineptr >= program_end)
  {
    goto qdirect;
  }
  pin_wire_parse();
  if (error_num)
  {
    GOTO_QWHAT;
  }
  goto run_next_statement;
#endif
  
ble_gatt:
  if (lineptr >= program_end)
  {
    goto qdirect;
  }
  switch(*txtpos++)
  {
    case BLE_SERVICE:
      servicestart = *(LINENUM*)(txtpos - 5); // <lineno:2> <len:1> GATT:1 SERVICE:1
      servicecount = 1;
      break;
    case KW_READ:
    case KW_WRITE:
    case BLE_WRITENORSP:
    case BLE_NOTIFY:
    case BLE_INDICATE:
    case BLE_AUTH:
      {
        servicecount++;
        for (;;)
        {
          switch (*txtpos++)
          {
            case BLE_NOTIFY:
            case BLE_INDICATE:
              servicecount++;
              break;
            case KW_READ:
            case KW_WRITE:
            case BLE_WRITENORSP:
            case BLE_AUTH:
              break;
            default:
              goto value_done;
          }
        }
      value_done:;
        const unsigned char ch = *--txtpos;
        if (ch >= 'A' && ch <= 'Z')
        {
          variable_frame* vframe;
          unsigned char* ptr = get_variable_frame(ch, &vframe);
          if (vframe == &normal_variable)
          {
            vframe = (variable_frame*)heap;
            CHECK_HEAP_OOM(sizeof(variable_frame) + VAR_SIZE, qoom);
            vframe->header.frame_type = FRAME_VARIABLE_FLAG;
            vframe->header.frame_size = sizeof(variable_frame) + VAR_SIZE;
            vframe->type = VAR_INT;
            vframe->name = ch;
            vframe->ble = NULL;
            *(VAR_TYPE*)(vframe + 1) = *(VAR_TYPE*)ptr;
            VARIABLE_SAVE(vframe);
          }
        }
        else
        {
          GOTO_QWHAT;
        }
      }
      break;
    case BLE_CHARACTERISTIC:
      servicecount += 2; // NB. We include an extra for the description. Optimize later!
      break;
    case KW_END:
      switch (ble_build_service())
      {
        default:
          break;
        case 1:
          GOTO_QWHAT;
        case 2:
          SET_ERR_LINE;
          goto qoom;
      }
      break;
    case KW_CLOSE:
      // Close any active connections
      GAPRole_TerminateConnection();
      break;
    default:
      GOTO_QWHAT;
  }
  goto run_next_statement;
  
//
// SCAN <time> LIMITED|GENERAL [ACTIVE] [DUPLICATES] ONDISCOVER GOSUB <linenum>
//  or
// SCAN LIMITED|GENERAL|NAME "..."|CUSTOM "..."|END
//
ble_scan:
  if (*txtpos < 0x80)
  {
#if ( HOST_CONFIG & OBSERVER_CFG )        
    unsigned char active = 0;
    unsigned char dups = 0;
#endif
    unsigned char mode = 0;

    val = expression(EXPR_NORMAL);
    if (error_num)
    {
      GOTO_QWHAT;
    }

    ignore_blanks();
    switch (*txtpos)
    {
      case BLE_GENERAL:
        mode |= 1;
        txtpos++;
        if (*txtpos == BLE_LIMITED)
        {
          mode |= 2;
          txtpos++;
        }
        break;
      case BLE_LIMITED:
        mode |= 2;
        if (*txtpos == BLE_GENERAL)
        {
          mode |= 1;
          txtpos++;
        }

        break;
      default:
        GOTO_QWHAT;
    }

#if ( HOST_CONFIG & OBSERVER_CFG )    
    if (*txtpos == BLE_ACTIVE)
    {
      txtpos++;
      active = 1;
    }
    if (*txtpos == BLE_DUPLICATES)
    {
      txtpos++;
      dups = 1;
    }

    if (txtpos[0] != BLE_ONDISCOVER || txtpos[1] != KW_GOSUB)
    {
      GOTO_QWHAT;
    }
    txtpos += 2;
#endif
    
    linenum = expression(EXPR_NORMAL);
    if (error_num)
    {
      GOTO_QWHAT;
    }
    uint16 param = val;
    GAPRole_SetParameter(TGAP_GEN_DISC_SCAN, sizeof(param), &param);
    GAPRole_SetParameter(TGAP_LIM_DISC_SCAN, sizeof(param), &param);
#if ( HOST_CONFIG & OBSERVER_CFG )        
    blueBasic_discover.linenum = linenum;
    uint16 param = !dups;
    GAPRole_SetParameter(TGAP_FILTER_ADV_REPORTS, sizeof(param), &param);
    GAPObserverRole_CancelDiscovery();
    mode = 3;
    GAPObserverRole_StartDiscovery(mode, !!active, 0);
#endif    
    goto run_next_statement;
  }
  // Fall through ...
  
ble_advert:
  {
    if (!ble_adptr)
    {
      ble_adptr = ble_adbuf;
    }
    unsigned char ch = *txtpos;
    switch (ch)
    {
      case BLE_LIMITED:
        if (ble_adptr + 3 > ble_adbuf + sizeof(ble_adbuf))
        {
          SET_ERR_LINE;
          goto qtoobig;
        }
        *ble_adptr++ = 2;
        *ble_adptr++ = GAP_ADTYPE_FLAGS;
        *ble_adptr++ = GAP_ADTYPE_FLAGS_LIMITED | GAP_ADTYPE_FLAGS_BREDR_NOT_SUPPORTED;
        txtpos++;
        break;
      case BLE_GENERAL:
        if (ble_adptr + 3 > ble_adbuf + sizeof(ble_adbuf))
        {
          SET_ERR_LINE;
          goto qtoobig;
        }
        *ble_adptr++ = 2;
        *ble_adptr++ = GAP_ADTYPE_FLAGS;
        *ble_adptr++ = GAP_ADTYPE_FLAGS_GENERAL | GAP_ADTYPE_FLAGS_BREDR_NOT_SUPPORTED;
        txtpos++;
        break;
      case BLE_NAME:
        {
          ch = *++txtpos;
          if (ch != SQUOTE && ch != DQUOTE)
          {
            GOTO_QWHAT;
          }
          else
          {
            short len;
            short alen;

            len = find_quoted_string();
            if (len == -1)
            {
              GOTO_QWHAT;
            }
            if (ble_adptr + 3 > ble_adbuf + sizeof(ble_adbuf))
            {
              SET_ERR_LINE;
              goto qtoobig;
            }
            else
            {
              alen = ble_adbuf + sizeof(ble_adbuf) - ble_adptr - 2;
              if (alen > len)
              {
                *ble_adptr++ = len + 1;
                *ble_adptr++ = GAP_ADTYPE_LOCAL_NAME_COMPLETE;
                alen = len;
              }
              else
              {
                *ble_adptr++ = alen + 1;
                *ble_adptr++ = GAP_ADTYPE_LOCAL_NAME_SHORT;
              }
              OS_memcpy(ble_adptr, txtpos, alen);
              ble_adptr += alen;
              GGS_SetParameter(GGS_DEVICE_NAME_ATT, len < GAP_DEVICE_NAME_LEN ? len : GAP_DEVICE_NAME_LEN, txtpos);
              txtpos += len + 1;
            }
          }
        }
        break;
      case BLE_CUSTOM:
        {
          unsigned char* lenptr;
          if (ble_adptr == NULL)
          {
            GOTO_QWHAT;
          }
          if (ble_adptr + 1 > ble_adbuf + sizeof(ble_adbuf))
          {
            SET_ERR_LINE;
            goto qtoobig;
          }
          lenptr = ble_adptr++;

          for (;;)
          {
            ch = *++txtpos;
            if (ch == NL)
            {
              *lenptr = (unsigned char)(ble_adptr - lenptr - 1);
              break;
            }
            else if (ch == SQUOTE || ch == DQUOTE)
            {
              unsigned short v;
              txtpos++;
              for (;;)
              {
                ignore_blanks();
                v = parse_int(2, 16);
                if (error_num)
                {
                  if (*txtpos == ch)
                  {
                    error_num = ERROR_OK;
                    break;
                  }
                  GOTO_QWHAT;
                }
                if (ble_adptr + 1 > ble_adbuf + sizeof(ble_adbuf))
                {
                  SET_ERR_LINE;
                  goto qtoobig;
                }
                *ble_adptr++ = (unsigned char)v;
              }
            }
            else if (ch >= 'A' && ch <= 'Z')
            {
              variable_frame* frame;
              unsigned char* ptr;
              
              if (txtpos[1] != NL && txtpos[1] != WS_SPACE)
              {
                GOTO_QWHAT;
              }

              ptr = get_variable_frame(ch, &frame);
              if (frame->type != VAR_DIM_BYTE)
              {
                GOTO_QWHAT;
              }
              if (ble_adptr + frame->header.frame_size - sizeof(variable_frame) > ble_adbuf + sizeof(ble_adbuf))
              {
                SET_ERR_LINE;
                goto qtoobig;
              }
              for (ch = sizeof(variable_frame); ch < frame->header.frame_size; ch++)
              {
                *ble_adptr++ = *ptr++;
              }
            }
            else if (ch != WS_SPACE)
            {
              GOTO_QWHAT;
            }
          }
        }
        break;
      case KW_END:
        if (ble_adptr == ble_adbuf)
        {
          ble_adptr = NULL;
          GOTO_QWHAT;
        }
        if (ble_isadvert)
        {
          uint8 advert = TRUE;
          GAPRole_SetParameter(_GAPROLE(BLE_ADVERT_DATA), ble_adptr - ble_adbuf, ble_adbuf);
          GAPRole_SetParameter(_GAPROLE(BLE_ADVERT_ENABLED), sizeof(uint8), &advert);
        }
        else
        {
          GAPRole_SetParameter(_GAPROLE(BLE_SCAN_RSP_DATA), ble_adptr - ble_adbuf, ble_adbuf);
        }
        ble_adptr = NULL;
        txtpos++;
        break;
      default:
        if (ch != SQUOTE && ch != DQUOTE)
        {
          GOTO_QWHAT;
        }
        else
        {
          ch = ble_get_uuid();
          if (!ch)
          {
            GOTO_QWHAT;
          }
          if (ble_adptr + 2 + ble_uuid_len > ble_adbuf + sizeof(ble_adbuf))
          {
            SET_ERR_LINE;
            goto qtoobig;
          }
          *ble_adptr++ = ble_uuid_len + 1;
          switch (ble_uuid_len)
          {
            case 2:
              *ble_adptr++ = GAP_ADTYPE_16BIT_COMPLETE;
              break;
            case 4:
              *ble_adptr++ = GAP_ADTYPE_32BIT_COMPLETE;
              break;
            case 16:
              *ble_adptr++ = GAP_ADTYPE_128BIT_COMPLETE;
              break;
            default:
              GOTO_QWHAT;
          }
          OS_memcpy(ble_adptr, ble_uuid, ble_uuid_len);
          ignore_blanks();
          if (*txtpos == BLE_MORE)
          {
            ble_adptr[-1] &= 0xFE;
            txtpos++;
          }
          ble_adptr += ble_uuid_len;
        }
        break;
    }
    goto run_next_statement;
  }

//
// BTPOKE <paramater>, <value>|<array>
//
cmd_btpoke:
  {
    unsigned short param;
    unsigned char* ptr;

    param = (unsigned short)expression(EXPR_COMMA);
    if (error_num)
    {
      GOTO_QWHAT;
    }
    // Expects an array
    if (param & 0x8000)
    {
      param = _GAPROLE(param);
      variable_frame* vframe;
      ptr = get_variable_frame(*txtpos, &vframe);
      if (vframe->type != VAR_DIM_BYTE)
      {
        GOTO_QWHAT;
      }
      if (param >= _GAPROLE(BLE_PAIRING_MODE) && param <= _GAPROLE(BLE_ERASE_SINGLEBOND))
      {
#if GAP_BOND_MGR       
        if (GAPBondMgr_SetParameter(param, vframe->header.frame_size - sizeof(variable_frame), ptr) != SUCCESS)
        {
          GOTO_QWHAT;
        }
#else
        GOTO_QWHAT;
#endif
      }
      else if (GAPRole_SetParameter(param, vframe->header.frame_size - sizeof(variable_frame), ptr) != SUCCESS)
      {
        GOTO_QWHAT;
      }
    }
    // Expects an int
    else
    {
      val = expression(EXPR_NORMAL);
      unsigned char len = _GAPLEN(param);
      param = _GAPROLE(param);
      if (error_num)
      {
        GOTO_QWHAT;
      }
      if (param >= _GAPROLE(BLE_PAIRING_MODE) && param <= _GAPROLE(BLE_ERASE_SINGLEBOND))
      {
#if GAP_BOND_MGR
        if (GAPBondMgr_SetParameter(param, len, &val) != SUCCESS)
        {
          GOTO_QWHAT;
        }
#else
        GOTO_QWHAT;
#endif 
      }
      else if (_GAPROLE(param) >= _GAPROLE(BLE_PROFILEROLE) && _GAPROLE(param) <= _GAPROLE(BLE_ADV_NONCONN_ENABLED))
      {
        if (GAPRole_SetParameter(param, len, &val) != SUCCESS) 
        {
          GOTO_QWHAT;
        }
      }
      else
      {
        // for all lower addresses we call GAP directly
        // always uint16, masking any size with _GAPROLE()
        if (GAP_SetParamValue( _GAPROLE(param), val) != SUCCESS)
        {
          GOTO_QWHAT;
        }
      }
    }
  }
  goto run_next_statement;

//
// OPEN <0-3>, READ|TRUNCATE|APPEND "<A-Z> | <0-9>"[, modulo[, record]]
//  Open a numbered file for read, write or append access.
//  optional modulo paramter wraps read, write record number around
//  in case the file name is a number between 0-9 access SNV
cmd_open:
  {
    unsigned char hasOffset = FALSE;
    unsigned char id = expression(EXPR_COMMA);
    if (error_num || id >= FS_NR_FILE_HANDLES)
    {
      GOTO_QWHAT;
    }
    os_file_t* file = &files[id];
#if ENABLE_SNV    
    bool bSnv = txtpos[2] >= '0' && txtpos[2] <= '9';
#else
#define bSnv 0
#endif
    if (txtpos[1] == '"' && txtpos[3] == '"' &&
        ((txtpos[2] >= 'A' && txtpos[2] <= 'Z')  || bSnv ))
    {
      file->filename = txtpos[2];
      file->record = 0;
      file->poffset = FLASHSPECIAL_DATA_OFFSET;
      file->modulo = FLASHSPECIAL_NR_FILE_RECORDS;
    }
    else
    {
      GOTO_QWHAT;
    }
    unsigned char kw = *txtpos;
    if (txtpos[4] == ',')
    {
      txtpos += 5;
      VAR_TYPE value = expression(EXPR_COMMA);
      if (error_num || value > FLASHSPECIAL_NR_FILE_RECORDS || value <= 0)
      {
        GOTO_QWHAT;
      }
      file->modulo = (unsigned short) value;
      if (*txtpos != NL)
      {
        value = expression(EXPR_NORMAL);
        if (error_num || value < 0)
        {
          GOTO_QWHAT;
        }
        file->record = value % file->modulo; // keep record in bounds with modulo
        hasOffset = TRUE;
      }
    }  
    switch (kw)
    {
      case KW_READ: // Read
        file->action = 'R';
        break;
      case FS_TRUNCATE: // Truncate
      {
        file->action = 'W';
        if (bSnv)
          break;
//        DEBUG_P20_CLR;
        for (unsigned long special = FS_MAKE_FILE_SPECIAL(file->filename, file->record); flashstore_deletespecial(special); special++)
        {
          // keep OSAL spinning
          if (special % 16 == 0) osal_run_system();
        }
//        DEBUG_P20_SET;
        break;
      }
      case FS_APPEND: // Append
      {
        if (bSnv)
          GOTO_QWHAT;
        file->action = 'W';
        unsigned short record = file->record;
        file->record = 0;
        for (unsigned long special = FS_MAKE_FILE_SPECIAL(file->filename, 0); flashstore_findspecial(special); special++, file->record++)
        {
          if (hasOffset && (unsigned short) (special & 0xffff) >= record)
          {
            break;
          }
          // keep OSAL spinning
          if (special % 16 == 0) osal_run_system();          
        }
        break;
      }
      default:
        GOTO_QWHAT;
    }
  }
  goto run_next_statement;

//
// CLOSE <0-3>
//  Close the numbered file.
// CLOSE SERIAL
//  Close the serial port
// CLOSE I2C
//  Close the i2c port
cmd_close:
  {
    if (*txtpos == KW_SERIAL)
#if HAL_UART      
    {
      unsigned char port = 0;
      txtpos++;
      ignore_blanks();
      if (*txtpos != NL)
      {
        port = expression(EXPR_COMMA);
        if (error_num || port > (OS_MAX_SERIAL-1))
        {
          GOTO_QWHAT;
        }
      }
      OS_serial_close(port);
    }
#else  // HAL_UART
    {
      GOTO_QWHAT;
    }
#endif // HAL_UART
#if HAL_I2C_SLAVE
    else if (*txtpos == KW_I2C)
    {
      txtpos++;
      OS_i2c_close(0);
    }
#endif
    else
    {
      unsigned char id = expression(EXPR_COMMA);
      if (error_num || id >= FS_NR_FILE_HANDLES)
      {
        GOTO_QWHAT;
      }
      files[id].action = 0;
    }
  }
  goto run_next_statement;

//
// READ #<0-3>, <variable>[, ...]
//  Read from the currrent place in the numbered file into the variable
// READ #SERIAL, <variable>[, ...]  
// READ #I2C, <variable>[, ...]
cmd_read:
  {
    if (*txtpos++ != '#')
    {
      GOTO_QWHAT;
    }
    else if (*txtpos == KW_SERIAL)
#if HAL_UART
    {
      unsigned char port = 0;
      txtpos++;
      ignore_blanks();
      if (*txtpos != ',')
      {
        port = *txtpos++ - '0';
        if (port > (OS_MAX_SERIAL-1))
        {
          GOTO_QWHAT;
        }
      }
      for (;;)
      {
        ignore_blanks();
        if (*txtpos == NL)
        {
          break;
        }
        else if (*txtpos++ != ',')
        {
          GOTO_QWHAT;
        }
        variable_frame* vframe = NULL;
        unsigned char* ptr = parse_variable_address(&vframe);
        if (ptr)
        {
          if (vframe->type == VAR_INT)
          {
            *(VAR_TYPE*)ptr = OS_serial_read(port);
          }
          else if (vframe->type == VAR_DIM_BYTE)
          {
            *ptr = OS_serial_read(port);
          }
        }
        else if (vframe)
        {
          // No address, but we have a vframe - this is a full array
          if (error_num == ERROR_EXPRESSION)
            error_num = ERROR_OK; // clear parsing error due to missing index braces
          unsigned char alen = vframe->header.frame_size - sizeof(variable_frame);
          for (ptr = (unsigned char*)vframe + sizeof(variable_frame); alen; alen--)
          {
            *ptr++ = OS_serial_read(port);
          }
        }
        else
        {
          GOTO_QWHAT;
        }
      }
    }
#else // HAL_UART
    {
      GOTO_QWHAT;
    }
#endif // HAL_UART
#if HAL_I2C_SLAVE
    else if (*txtpos == KW_I2C)
    {
      txtpos++;
      for (;;)
      {
        ignore_blanks();
        if (*txtpos == NL)
        {
          break;
        }
        else if (*txtpos++ != ',')
        {
          GOTO_QWHAT;
        }
        variable_frame* vframe = NULL;
        unsigned char* ptr = parse_variable_address(&vframe);
        if (ptr)
        {
          if (vframe->type == VAR_INT)
          {
            *(VAR_TYPE*)ptr = OS_i2c_read(0);
          }
          else
          {
            *ptr = OS_i2c_read(0);
          }
        }
        else if (vframe)
        {
          // No address, but we have a vframe - this is a full array
          if (error_num == ERROR_EXPRESSION)
            error_num = ERROR_OK; // clear parsing error due to missing index braces
          unsigned char alen = vframe->header.frame_size - sizeof(variable_frame);
          for (ptr = (unsigned char*)vframe + sizeof(variable_frame); alen; alen--)
          {
            *ptr++ = OS_i2c_read(0);
          }
        }
        else
        {
          GOTO_QWHAT;
        }
      }
    }
#endif
    else
    {
      unsigned char id = expression(EXPR_COMMA);
      if (error_num || id >= FS_NR_FILE_HANDLES || files[id].action != 'R')
      {
        GOTO_QWHAT;
      }
      os_file_t* file = &files[id];

#if ENABLE_SNV      
      if (file->filename >= 'A')
#endif        
      {
        unsigned char* special = flashstore_findspecial(FS_MAKE_FILE_SPECIAL(file->filename, file->record));
        if (!special)
        {
          SET_ERR_LINE;
          goto qeof;
        }
        unsigned char len = special[FLASHSPECIAL_DATA_LEN];

        txtpos--;
        for (;;)
        {
          ignore_blanks();
          if (*txtpos == NL)
          {
            break;
          }
          else if (*txtpos++ != ',')
          {
            GOTO_QWHAT;
          }
          variable_frame* vframe = NULL;
          unsigned char* ptr = parse_variable_address(&vframe);
          if (ptr)
          {
            if (file->poffset == len)
            {
              file->record = (file->record + 1) % file->modulo;
              special = flashstore_findspecial(FS_MAKE_FILE_SPECIAL(file->filename, file->record));
              if (!special)
              {
                SET_ERR_LINE;
                goto qeof;
              }
              file->poffset = FLASHSPECIAL_DATA_OFFSET;
              len = special[FLASHSPECIAL_DATA_LEN];
            }
            if (vframe->type == VAR_DIM_BYTE)
            {
              *ptr = special[file->poffset++];
            }
            else if (vframe->type == VAR_INT)
            {
              *(VAR_TYPE*)ptr = *(VAR_TYPE*)(special+file->poffset);
              file->poffset += sizeof(VAR_TYPE);
            }
          }
          else if (vframe)
          {
            // No address, but we have a vframe - this is a full array
            if (error_num == ERROR_EXPRESSION)
              error_num = ERROR_OK; // clear parsing error due to missing index braces
            unsigned char alen = vframe->header.frame_size - sizeof(variable_frame);
            ptr = (unsigned char*)vframe + sizeof(variable_frame);
            while (alen)
            {
              if (file->poffset == len)
              {
                file->record = (file->record + 1) % file->modulo;
                special = flashstore_findspecial(FS_MAKE_FILE_SPECIAL(file->filename, file->record));
                if (!special)
                {
                  SET_ERR_LINE;
                  goto qeof;
                }
                file->poffset = FLASHSPECIAL_DATA_OFFSET;
                len = special[FLASHSPECIAL_DATA_LEN];
              }
              unsigned char blen = (alen < len - file->poffset ? alen : len - file->poffset);
              OS_memcpy(ptr, special + file->poffset, blen);
              ptr += blen;
              alen -= blen;
              file->poffset += blen;
            }
          }
          else
          {
            GOTO_QWHAT;
          }
        }
      }
#if ENABLE_SNV
      else
      {
        // we read from SNV
        unsigned char* item = heap;
        osalSnvId_t id = SNV_MAKE_ID(file->filename);
        unsigned char len = get_snv_length(id);
        CHECK_HEAP_OOM(len, qhoom3);
        if (osal_snv_read( id, len, item) != SUCCESS)
        {
           heap = item;
           SET_ERR_LINE;
           goto qeof;
        }
        file->poffset = 1;
        txtpos--;
        for (;;)
        {
          ignore_blanks();
          if (*txtpos == NL)
          {
            break;
          }
          else if (*txtpos++ != ',')
          {
            GOTO_QWHAT;
          }
          if (file->poffset >= len)
          {
            heap = item;
            SET_ERR_LINE;
            goto qeof;
          }
          variable_frame* vframe = NULL;
          unsigned char* ptr = parse_variable_address(&vframe);
          if (ptr)
          {
            unsigned char v = item[file->poffset++];
            if (vframe->type == VAR_DIM_BYTE)
            {
              *ptr = v;
            }
            else if (vframe->type == VAR_INT)
            {
              *(VAR_TYPE*)ptr = v;
            }
          }
          else if (vframe)
          {
            // No address, but we have a vframe - this is a full array
            if (error_num == ERROR_EXPRESSION)
              error_num = ERROR_OK; // clear parsing error due to missing index braces
            unsigned char alen = vframe->header.frame_size - sizeof(variable_frame);
            ptr = (unsigned char*)vframe + sizeof(variable_frame);
            while (alen)
            {
              if (file->poffset >= len)
              {
                heap = item;
                SET_ERR_LINE;
                goto qeof;
              }
              unsigned char blen = (alen < len - file->poffset ? alen : len - file->poffset);
              OS_memcpy(ptr, item + file->poffset, blen);
              ptr += blen;
              alen -= blen;
              file->poffset += blen;
            }
          }
          else
          {
            GOTO_QWHAT;
          }
        }  // for(;;)
      heap = item;
      goto run_next_statement;
qhoom3:
      heap = item;
      goto qoom;
    //------------------------------    
      }
#endif  //ENABLE_SNV
    }
  }
  goto run_next_statement;

//
// WRITE #<0-3>, <variable>|<byte>[, ...]
//  Write from the variable into the currrent place in the numbered file
// WRITE #SERIAL, <variable>|<byte>[, ...]
//  Write from the variable to the serial port
//
cmd_write:
  {
    if (*txtpos++ != '#')
    {
      GOTO_QWHAT;
    }
    else if (*txtpos == KW_SERIAL)
#if HAL_UART
    {
      unsigned char port = 0;
      txtpos++;
      ignore_blanks();
      if (*txtpos != ',')
      {
        port = *txtpos++ - '0';
        if (port > (OS_MAX_SERIAL - 1))
        {
          GOTO_QWHAT;
        }
      }
      for (;;)
      {
        ignore_blanks();
        if (*txtpos == NL)
        {
          break;
        }
        if (*txtpos == ',')
        {
          txtpos++;
        }
        variable_frame* vframe = NULL;
        unsigned char* ptr = parse_variable_address(&vframe);
        if (ptr)
        {
          unsigned char send_byte;
          if (vframe->type == VAR_DIM_BYTE)
          {
            send_byte = *ptr;
//            OS_serial_write(port, *ptr) == 0);
          }
          else
          {
            send_byte = *(VAR_TYPE*)ptr;
            //OS_serial_write(port, *(VAR_TYPE*)ptr);
          }
          while (!OS_serial_write(port, send_byte))
          {
            // keep OSAL spinning
            osal_run_system();
          }
        }
        else if (vframe)
        {
          // No address, but we have a vframe - this is a full array
          if (error_num == ERROR_EXPRESSION)
            error_num = ERROR_OK; // clear parsing error due to missing index braces
          unsigned char alen;
          ptr = (unsigned char*)vframe + sizeof(variable_frame);
          for (alen = vframe->header.frame_size - sizeof(variable_frame); alen; alen--)
          {
            for ( ; OS_serial_write(port, *ptr) == 0 ; )
            {
              // keep OSAL spinning
              osal_run_system();
            }
            ptr++;
          }
        }
        else if (*txtpos == NL)
        {
          break;
        }
        else
        {
          VAR_TYPE val = expression(EXPR_COMMA);
          if (error_num)
          {
            GOTO_QWHAT;
          }
          while (!OS_serial_write(port, val))
          {
            // keep OSAL spinning
            osal_run_system();
          }   
        }
      }
      goto run_next_statement;
    }
#else // HAL_UART
    {
      GOTO_QWHAT;
    }
#endif // HAL_UART
    else
    {
      if (lineptr >= program_end)
      {
        goto qdirect;
      }
      unsigned char id = expression(EXPR_COMMA);
      if (error_num || id >= FS_NR_FILE_HANDLES || files[id].action != 'W')
      {
        GOTO_QWHAT;
      }
#if ENABLE_SNV      
      if (files[id].filename >= 'A')
#endif
      {
        if (files[id].record == FLASHSPECIAL_NR_FILE_RECORDS)
        {
          SET_ERR_LINE;
          goto qtoobig;
        }
        unsigned long special = FS_MAKE_FILE_SPECIAL(files[id].filename, files[id].record++);
        if (files[id].modulo < FLASHSPECIAL_NR_FILE_RECORDS)
        {
          files[id].record %= files[id].modulo;
          flashstore_deletespecial(special);
        }
        unsigned char* item = heap;
        unsigned char* iptr = item + FLASHSPECIAL_DATA_OFFSET;
        unsigned char ilen = FLASHSPECIAL_DATA_OFFSET;
        CHECK_HEAP_OOM(ilen, qhoom);
        *(unsigned long*)&item[FLASHSPECIAL_ITEM_ID] = special;

        txtpos--;
        for (;;)
        {
          ignore_blanks();
          if (*txtpos == NL)
          {
            break;
          }
          else if (*txtpos++ != ',')
          {
            heap = item;
            GOTO_QWHAT;
          }
          variable_frame* vframe = NULL;
          unsigned char* ptr = parse_variable_address(&vframe);
          if (ptr)
          {
            if (vframe->type == VAR_DIM_BYTE)
            {
              CHECK_HEAP_OOM(1, qhoom);
              *iptr = *ptr;
            }
            else
            {
              CHECK_HEAP_OOM(sizeof(VAR_TYPE), qhoom);
              *(VAR_TYPE*)iptr = *(VAR_TYPE*)ptr;
            }
          }
          else if (vframe)
          {
            // No address, but we have a vframe - this is a full array
            if (error_num == ERROR_EXPRESSION)
              error_num = ERROR_OK; // clear parsing error due to missing index braces
            unsigned char alen = vframe->header.frame_size - sizeof(variable_frame);
            CHECK_HEAP_OOM(alen, qhoom);
            OS_memcpy(iptr, ((unsigned char*)vframe) + sizeof(variable_frame), alen);
          }
          else
          {
            VAR_TYPE val = expression(EXPR_COMMA);
            if (error_num)
            {
              heap = item;
              GOTO_QWHAT;
            }
            CHECK_HEAP_OOM(1, qhoom);
            *iptr = val;
            if (*txtpos != NL)
            {
              txtpos--;
            }
          }
          if (iptr - item > 0xFC)
          {
            heap = item;
            SET_ERR_LINE;
            goto qtoobig;
          }
          iptr = heap;
        }
        item[FLASHSPECIAL_DATA_LEN] = iptr - item;
        if (!addspecial_with_compact(item))
        {
          SET_ERR_LINE;
          goto qhoom;
        }
//        DEBUG_P20_SET;
        heap = item;
        goto run_next_statement;
qhoom:
        heap = item;
        goto qoom;
      }
#if ENABLE_SNV
      else 
      {
        // we write to SNV
        unsigned char* item = heap;
        unsigned char* iptr = item + 1;
        CHECK_HEAP_OOM(1, qhoom2);  // reserve a byte for the length
        txtpos--;
        for (;;)
        {
          ignore_blanks();
          if (*txtpos == NL)
          {
            break;
          }
          else if (*txtpos++ != ',')
          {
            heap = item;
            GOTO_QWHAT;
          }
          variable_frame* vframe = NULL;
          unsigned char* ptr = parse_variable_address(&vframe);
          if (ptr)
          {
            CHECK_HEAP_OOM(1, qhoom2);
            if (vframe->type == VAR_DIM_BYTE)
            {
              *iptr = *ptr;
            }
            else
            {
              *iptr = *(VAR_TYPE*)ptr;
            }
          }
          else if (vframe)
          {
            // No address, but we have a vframe - this is a full array
            if (error_num == ERROR_EXPRESSION)
              error_num = ERROR_OK; // clear parsing error due to missing index braces
            unsigned char alen = vframe->header.frame_size - sizeof(variable_frame);
            CHECK_HEAP_OOM(alen, qhoom2);
            OS_memcpy(iptr, ((unsigned char*)vframe) + sizeof(variable_frame), alen);
          }
          else
          {
            VAR_TYPE val = expression(EXPR_COMMA);
            if (error_num)
            {
              heap = item;
              GOTO_QWHAT;
            }
            CHECK_HEAP_OOM(1, qhoom2);
            *iptr = val;
            if (*txtpos != NL)
            {
              txtpos--;
            }
          }
          if (iptr - item > 0xFE)
          {
            heap = item;
            SET_ERR_LINE;
            goto qtoobig;
          }
          iptr = heap;
        }
        item[0] = iptr - item;  // save length 
        if (osal_snv_write(SNV_MAKE_ID(files[id].filename), item[0], item) != SUCCESS)
        {
          SET_ERR_LINE;
          goto qhoom2;
        }
//        DEBUG_P20_SET;
        heap = item;
        goto run_next_statement;
qhoom2:
        heap = item;
        goto qoom;
      }
#endif //ENABLE_SNV
    }
  }

#if !defined(ENABLE_SPI) || ENABLE_SPI  
//
// SPI MASTER <port 0|1|2|3>, <mode 0|1|2|3>, LSB|MSB <speed> [, wordsize]
//  or
// SPI TRANSFER Px(y) <array>
//
cmd_spi:
  {
    if (*txtpos == SPI_MASTER)
    {
      // Setup
      unsigned char port;
      unsigned char mode;
      unsigned char msblsb;
      unsigned char speed;

      txtpos++;
      port = expression(EXPR_COMMA);
      mode = expression(EXPR_COMMA);
      if (error_num || port > 3 || mode > 3)
      {
        GOTO_QWHAT;
      }
      msblsb = *txtpos;
      if (msblsb != SPI_MSB && msblsb != SPI_LSB)
      {
        GOTO_QWHAT;
      }
      txtpos++;
      speed = expression(EXPR_COMMA);
      if (*txtpos != NL)
      {
        switch (expression(EXPR_NORMAL))
        {
          case 8:
            spiWordsize = 0;
            break;
          case 16:
            spiWordsize = 1;
            break;
          case 32:
            spiWordsize = 3;
            break;
          default:
            GOTO_QWHAT;
        }
      }
      else
      {
        spiWordsize = 255;
      }
      if (error_num)
      {
        GOTO_QWHAT;
      }

      mode = (mode << 6) | (msblsb == SPI_MSB ? 0x20 : 0x00);
      switch (speed)
      {
        case 1: // E == 15, M = 0
          mode |= 15;
          break;
        case 2: // E = 16
          mode |= 16;
          break;
        case 4: // E = 17
          mode |= 17;
          break;
        default:
          GOTO_QWHAT;
      }
      
      if ((port & 2) == 0)
      {
        spiChannel = 0;
        if ((port & 1) == 0)
        {
          P0SEL |= 0x2C;
        }
        else
        {
          P1SEL |= 0x2C;
        }
        PERCFG = (PERCFG & 0xFE) | (port & 1);
        U0BAUD = 0;
        U0GCR = mode;
        U0CSR = 0x00; // SPI mode, recv enable, SPI master
      }
      else
      {
        spiChannel = 1;
        if ((port & 1) == 0)
        {
          P0SEL |= 0x38;
        }
        else
        {
          P1SEL |= 0xE0;
        }
        PERCFG = (PERCFG & 0xFD) | ((port & 1) << 1);
        U1BAUD = 0;
        U1GCR = mode;
        U1CSR = 0x00;
      }
    }
    else if (*txtpos == SPI_TRANSFER)
    {
      // Transfer
      unsigned char pin[2];
      variable_frame* vframe;
      unsigned char* ptr;
      
      txtpos++;
      pin[0] = pin_parse();
      pin[1] = pin[0] | WIRE_PIN_LOW;
      pin[0] |= WIRE_PIN_HIGH;
      if (error_num)
      {
        GOTO_QWHAT;
      }
      ignore_blanks();
      const unsigned char ch = *txtpos;
      if (ch < 'A' || ch > 'Z')
      {
        GOTO_QWHAT;
      }
      ptr = get_variable_frame(ch, &vframe);
      if (vframe->type != VAR_DIM_BYTE)
      {
        GOTO_QWHAT;
      }
      txtpos++;

      // .. transfer ..
      pin_wire(pin + 1, pin + 2);
      unsigned short len = vframe->header.frame_size - sizeof(variable_frame);
      unsigned short pos = 0;
      if (spiChannel == 0)
      {
        for (;;)
        {
          U0CSR &= 0xF9; // Clear flags
          U0DBUF = *ptr;
#ifdef SIMULATE_PINS
          U0CSR |= 0x02;
#endif
          while ((U0CSR & 0x02) != 0x02)
            ;
          *ptr++ = U0DBUF;
          pos++;
          if (pos == len)
          {
            break;
          }
          else if ((pos & spiWordsize) == 0)
          {
            pin_wire(pin, pin + 2);
          }
        }
      }
      else
      {
        for (;;)
        {
          U1CSR &= 0xF9;
          U1DBUF = *ptr;
#ifdef SIMULATE_PINS
          U1CSR |= 0x02;
#endif
          while ((U1CSR & 0x02) != 0x02)
            ;
          *ptr++ = U1DBUF;
          pos++;
          if (pos == len)
          {
            break;
          }
          else if ((pos & spiWordsize) == 0)
          {
            pin_wire(pin, pin + 2);
          }
        }
      }
      pin_wire(pin, pin + 1);
    }
    else
    {
      GOTO_QWHAT;
    }
  }
  goto run_next_statement;
#endif
  
#if HAL_I2C  
//
// I2C MASTER <scl pin> <sda pin> [PULLUP]
//  or
// I2C WRITE <addr>, <data, ...> [, READ <variable>|<array>]
//  or
// I2C READ <addr>, <variable>|<array>, ...
//  note: The CC2541 has i2c hardware which we are not yet using.
// or
// I2C SLAVE <addr> [ONREAD GOSUB <linenum>] [ONWRITE GOSUB <linenum>]
//  note: this uses the CC2541 i2c hardware with fixed pins
cmd_i2c:
  switch (*txtpos++)
  {
    case SPI_MASTER:
#if HAL_I2C_SLAVE
    {
      unsigned char pullup = 0;
      unsigned char* ptr = heap;

      i2cScl = pin_parse();
      i2cSda = pin_parse();
      ignore_blanks();
      if (*txtpos == PM_PULLUP)
      {
        txtpos++;
        pullup = 1;
      }
      // Setup the i2c port with optional INPUT_PULLUP.
      // We the outputs to LOW so when the pin is set to OUTPUT, it
      // will be driven low by default.
      *ptr++ = WIRE_PIN_INPUT | i2cScl;
      if (pullup)
      {
        *ptr++ = WIRE_INPUT_PULLUP;
      }
      *ptr++ = WIRE_LOW;
      *ptr++ = WIRE_PIN_INPUT | i2cSda;
      if (pullup)
      {
        *ptr++ = WIRE_INPUT_PULLUP;
      }
      *ptr++ = WIRE_LOW;
      
      pin_wire(heap, ptr);
      break;
    }
#else  // HAL_I2C_SLAVE
    {
        OS_I2C_INIT;
        break;
    }
#endif  // HAL_I2C_SLAVE
    
#if HAL_I2C_SLAVE    
  case SPI_SLAVE:
    {
      unsigned char addr = expression(EXPR_NORMAL);
      //if (addr > 127 || error_num) {
      //  goto qwhat;
      //}
      LINENUM onread = 0;
      if (*txtpos == BLE_ONREAD)
      {
        txtpos++;
        if (*txtpos++ != KW_GOSUB)
        {
          GOTO_QWHAT;
        }
        onread = expression(EXPR_NORMAL);
      }
      LINENUM onwrite = 0;
      if (*txtpos == BLE_ONWRITE)
      {
        txtpos++;
        if (*txtpos++ != KW_GOSUB)
        {
          GOTO_QWHAT;
        }
        onwrite = expression(EXPR_NORMAL);
      }
      if (OS_i2c_open(addr, onread, onwrite))
      {
        GOTO_QWHAT;
      }
      break;
    }
#endif
    
#define WIRE_SDA_LOW()    *ptr++ = WIRE_PIN_OUTPUT | i2cSda;
#define WIRE_SCL_LOW()    *ptr++ = WIRE_PIN_OUTPUT | i2cScl;
#define WIRE_SDA_HIGH()   *ptr++ = WIRE_PIN_INPUT | i2cSda;
#define WIRE_SCL_HIGH()   *ptr++ = WIRE_PIN_INPUT | i2cScl;
#define WIRE_SCL_WAIT()   *ptr++ = WIRE_WAIT_HIGH;
#define WIRE_SDA_READ()   *ptr++ = WIRE_PIN_READ | i2cSda;

    case KW_WRITE:
    case KW_READ:
#if HAL_I2C_SLAVE      
    {
      unsigned char* rdata = NULL;
      unsigned char* data;
      unsigned char len = 0;
      unsigned char i = 0;
      unsigned char* ptr = heap;
      unsigned char rnw = (txtpos[-1] == KW_READ ? 1 : 0);

      *ptr++ = WIRE_INPUT_SET;
      *(unsigned char**)ptr = NULL;
      ptr += sizeof(unsigned char*);
      *ptr++ = 0;

      // Start
      WIRE_SDA_LOW();
      WIRE_SCL_LOW();
      
      // Encode data we want to write
      for (;; len++)
      {
        unsigned char b;
        unsigned char d;
        if (*txtpos == NL)
        {
          goto i2c_end;
        }
        else if (*txtpos == KW_READ)
        {
          txtpos++;
          if (rnw || !len)
          {
            GOTO_QWHAT;
          }
          // Switch from WRITE to READ
          // Re-start
          WIRE_SCL_HIGH();
          WIRE_SDA_LOW();
          WIRE_SCL_LOW();
          rnw = 1;
          d = i;
        }
        else
        {
          d = expression(EXPR_COMMA);
          if (error_num)
          {
            GOTO_QWHAT;
          }
          // Remember the address
          if (len == 0)
          {
            i = d;
          }
        }
        d |= rnw;
        for (b = 128; b; b >>= 1)
        {
          if (d & b)
          {
            WIRE_SDA_HIGH();
          }
          else
          {
            WIRE_SDA_LOW();
          }
          WIRE_SCL_HIGH();
          WIRE_SCL_WAIT();
          WIRE_SCL_LOW();
        }

        // Ack
        WIRE_SDA_HIGH();
        WIRE_SCL_HIGH();
        WIRE_SCL_WAIT();
        WIRE_SCL_LOW();
        
        // If this is a read we have no more to write, so we go and read instead.
        if (rnw)
        {
          break;
        }
      }

      // Encode data we want to read
      variable_frame* vframe;
      ignore_blanks();
      i = *txtpos;
      if (i < 'A' || i > 'Z')
      {
        GOTO_QWHAT;
      }
      txtpos++;
      rdata = get_variable_frame(i, &vframe);
      if (vframe->type == VAR_DIM_BYTE)
      {
        len = vframe->header.frame_size - sizeof(variable_frame);
        OS_memset(rdata, 0, len);
      }
      else
      {
        *(VAR_TYPE*)rdata = 0;
        len = 1;
      }

      *ptr++ = WIRE_INPUT_SET;
      data = ptr;
      ptr += sizeof(unsigned char*);
      *ptr++ = sizeof(unsigned char);
      
      // Read bits
      WIRE_SCL_LOW();
      WIRE_SDA_HIGH();
      for (i = len; i--; )
      {
        unsigned char b;
        for (b = 8; b; b--)
        {
          WIRE_SCL_HIGH();
          WIRE_SCL_WAIT();
          WIRE_SDA_READ();
          WIRE_SCL_LOW();
        }
        // Ack
        if (i)
        {
          WIRE_SDA_LOW();
        }
        // Nack
        else
        {
          WIRE_SDA_HIGH();
        }
        WIRE_SCL_HIGH();
        WIRE_SCL_WAIT();
        WIRE_SCL_LOW();
        WIRE_SDA_HIGH();
      }
i2c_end:
      // End
      WIRE_SDA_LOW();
      WIRE_SCL_HIGH();
      WIRE_SDA_HIGH();

      if (data)
      {
        *(unsigned char**)data = ptr;
        OS_memset(ptr, 0, len * 8 * 2);
      }
      
      pin_wire(heap, ptr);
      if (error_num)
      {
        break;
      }
      // If we read data, reassemble it
      if (data)
      {
        unsigned char v = 0;
        unsigned char idx = 0;
        len *= 8;
        for (idx = 1, ptr++; idx <= len; idx++, ptr += 2)
        {
          v = (v << 1) | (*ptr ? 1 : 0);
          if (!(idx & 7))
          {
            *rdata++ = v;
            ptr++;
            v = 0;
          }
        }
      }
      break;
    }
#else // HAL_I2C_SLAVE
    {
    // hardware master read/write
    // ... tbd
      
      // INA 226 Slave address 
      // A1 / A0
      // GND / GND : 1000000
      unsigned char* rdata = NULL;
      unsigned char len = 0;
      unsigned char i = 0;
      unsigned char* ptr = heap;
      unsigned char* saved_heap = heap;
      unsigned char rnw = (txtpos[-1] == KW_READ ? 1 : 0);
      // Encode data we want to write
      for (;;)
      {
        unsigned char d;
        ignore_blanks();
        if (*txtpos == NL)
        {
          break;
        }
        d = copy_dim(ptr);
        if (error_num)
        {
           heap = saved_heap;
           GOTO_QWHAT;
        }
        if (d)
        {
          len += d;
          ptr += d;
          ignore_blanks();
          if (*txtpos == ',')
            txtpos++;
        }
        else 
        {
          d = expression(EXPR_COMMA);
          if (error_num)
          {
            heap = saved_heap;
            GOTO_QWHAT;
          }
          *ptr++ = d;
          len++;
        }        
        // If this is a read we have no more to write, so we go and read instead.
        if (rnw)
        {
          break;
        }
      }
      // if write we call the write command
      if (!rnw)
      {
        if (len > 0)
        {
          OS_I2C_WRITE(saved_heap[0], len - 1, saved_heap + 1);
        }
        heap = saved_heap;
        break;
      }
      variable_frame* vframe;
      ignore_blanks();
      i = *txtpos;
      if (i < 'A' || i > 'Z')
      {
        heap = saved_heap;
        GOTO_QWHAT;
      }
      txtpos++;
      rdata = get_variable_frame(i, &vframe);
      if (vframe->type == VAR_DIM_BYTE)
      {
        len = vframe->header.frame_size - sizeof(variable_frame);
      }
      else
      {
        len = 1;
        *(VAR_TYPE*)rdata = 0;
      }
      OS_I2C_READ(heap[0], len, rdata);
      heap = saved_heap;
      break;    
    }
#endif // HAL_I2C_SLAVE    
    default:
      txtpos--;
      GOTO_QWHAT;
  }
  if (error_num)
  {
    GOTO_QWHAT;
  }
  goto run_next_statement;  
#endif
  
//
// ANALOG RESOLUTION, 8|10|12|14
//  Set the number of bits returned from an ADC operation.
// ANALOG REFERENCE, INTERNAL|EXTERNAL
// ANALOG STOP
//  Stop timer based sampling
// ANALOG TRUE, 0|1
//  in timer based sampling select processing
//  0: averaging
//  1: True RMS
// ANALOG TIMER id, timeout, port map ,strobe port, strobe polarity LOW|HIGH, duty
//  Background adc sampling via timer with strobe and delay
//  id: timer id to use as time base (TIMER id, periode)
//  timeout: timer period in ms
//  port map: P0 bit map of pins to sample
//    special mode when bit 6,7 is set: calculate true rms of p0(7)-p0(6)
//  strobe port: use P0(x) as strobe signal for sampling
//  strobe polarity: LOW, HIGH, 0, or 1
//  strobe duty: duty active in ms (1..timeout)
cmd_analog:
  switch (*txtpos)
  {
#ifdef FEATURE_SAMPLING
    case CO_LOW:
      txtpos++;
      
      break;
    case TI_STOP:
      txtpos++;
      sampling.map = 0;
      break;
    case KW_TIMER:
      {
        txtpos++;
        sampling.timer = (unsigned short) expression(EXPR_COMMA);
        sampling.timeout = (unsigned short) expression(EXPR_COMMA);
        unsigned char map = (unsigned char) expression(EXPR_COMMA);
        uint8 pin = pin_parse();
        ignore_blanks();
        if (*txtpos != ',' | error_num != ERROR_OK
            | PIN_MAJOR(pin))
          GOTO_QWHAT;
        sampling.pin = 1 << PIN_MINOR(pin);
        txtpos++;
        switch (expression(EXPR_COMMA))
        {
        case 0:
        case CO_LOW:
          sampling.polarity = 0;
          break;
        case 1:
        case CO_HIGH:
          sampling.polarity = 1;
          break;
        default:
          GOTO_QWHAT;
        }
        sampling.duty = (unsigned short) expression(EXPR_NORMAL);
        if (error_num || sampling.duty < 1 || sampling.duty >= sampling.timeout)
          GOTO_QWHAT;
        sampling.map = map;
        P0DIR |= sampling.pin; // set output mode   
        OS_timer_start(sampling.timer, sampling.timeout, 1, 0);
      }
      break;
#endif     
  default:    
    switch (expression(EXPR_COMMA))
    {
      case CO_REFERENCE:
        switch (expression(EXPR_NORMAL))
        {
          case CO_INTERNAL:
            analogReference = 0x00;
            break;
          case CO_EXTERNAL:
            analogReference = 0x40;
            break;
          case CO_AVDD:
            analogReference = 0x80;
            break;
          default:
            GOTO_QWHAT;
        }
        break;
      case CO_RESOLUTION:
        switch (expression(EXPR_NORMAL))
        {
          case 8:
            analogResolution = 0x00;
            break;
          case 10:
            analogResolution = 0x10;
            break;
          case 12:
            analogResolution = 0x20;
            break;
          case 14:
            analogResolution = 0x30;
            break;
          default:
            GOTO_QWHAT;
        }
        break; 
#if defined(FEATURE_TRUE_RMS)
      case CO_TRUE:
        {
          sampling.mode = expression(EXPR_NORMAL);
          if (error_num)
            GOTO_QWHAT;
          OS_memset(sampling.adc, 0, sizeof(sampling.adc));
        }
        break;
#endif
      default:
        GOTO_QWHAT;
    }
  }
  goto run_next_statement;

cmd_config:
  switch (*txtpos)
  {
    case FUNC_MILLIS:
    {
      if (txtpos[1] != ',')
      {
        GOTO_QWHAT;
      }
      txtpos += 2;
      VAR_TYPE time = expression(EXPR_NORMAL);
      if (error_num)
      {
        GOTO_QWHAT;
      }
      OS_set_millis(time);
      break;
    }
    default:
      switch (expression(EXPR_COMMA))
      {
        case CO_POWER:
        {
          unsigned char option = expression(EXPR_NORMAL);
          if (error_num)
          {
            GOTO_QWHAT;
          }
          switch (option)
          {
#ifdef FEATURE_BOOST_CONVERTER
            case 0:
              // Mode 0: Boost converter is always off
              BlueBasic_powerMode = 0;
              FEATURE_BOOST_CONVERTER = 0;
              break;
            case 1:
              // Mode 1: Boost converter if always on
              BlueBasic_powerMode = 1;
              FEATURE_BOOST_CONVERTER = 1;
              break;
            case 2:
              // Mode 2: Boost convert is on when awake, off when asleep
              BlueBasic_powerMode = 2;
              FEATURE_BOOST_CONVERTER = 1;
              break;
#else
            case 0:
              break;
#endif // FEATURE_BOOST_CONVERTER
            // enable or disable sleep mode
            // CONFIG POWER, 3  disable sleep
            // CONFIG POWER, 4  enable sleep
            case 3:
              OS_enable_sleep(0);
              break;
            case 4:
              OS_enable_sleep(1);
              break;
            default:
              GOTO_QWHAT;
          }
          break;
        }
        default:
          GOTO_QWHAT;
      }
  }
  goto run_next_statement;
}

//
// clean up the stack e.g. when return is encountered
//
static unsigned char cleanup_stack(void)
{  
  // Now walk up the stack frames and find the frame we want, if present
  while (sp < variables_begin)
  {
    switch (((frame_header*)sp)->frame_type)
    {
      case FRAME_GOSUB_FLAG:
        if (txtpos[-1] == KW_RETURN)
        {
          gosub_frame *f = (gosub_frame *)sp;
          lineptr = f->line;
          sp += f->header.frame_size;
          //goto run_next_statement;
          return RETURN_GOSUB;
        }
        break;
      case FRAME_EVENT_FLAG:
        if (txtpos[-1] == KW_RETURN)
        {
          sp += ((frame_header*)sp)->frame_size;
          //goto prompt;
          return RETURN_EVENT;
        }
        break;
      case FRAME_FOR_FLAG:
        // Flag, Var, Final, Step
        if (txtpos[-1] == KW_NEXT)
        {
          for_frame *f = (for_frame *)sp;
          // Is the the variable we are looking for?
          if (*txtpos == f->for_var)
          {
            VAR_TYPE v = VARIABLE_INT_GET(f->for_var) + f->step;
            VARIABLE_INT_SET(f->for_var, v);
            // Use a different test depending on the sign of the step increment
            if ((f->step > 0 && v <= f->terminal) || (f->step < 0 && v >= f->terminal))
            {
              // We have to loop so don't pop the stack
              lineptr = f->line;
            }
            else
            {
              // We've run to the end of the loop. drop out of the loop, popping the stack
              sp += f->header.frame_size;
              txtpos++;
            }
            //goto run_next_statement;
            return RETURN_FOR;
          }
        }
        break;
      case FRAME_VARIABLE_FLAG:
        {
          VARIABLE_RESTORE((variable_frame*)sp);
        }
        break;
      default:
        SET_ERR_LINE;
        //goto qoom;
        return RETURN_ERROR_OOM;
    }
    sp += ((frame_header*)sp)->frame_size;
  }
  // Didn't find the variable we've been looking for
  // If we're returning from the main entry point, then we're done
  if (txtpos[-1] == KW_RETURN)
  {
    //goto print_error_or_ok;
    return RETURN_ERROR_OR_OK;
  }
  //GOTO_QWHAT;  
  return RETURN_QWHAT;
}

//
// Parse the immediate text into a PIN value.
// Pins are of the form PX(Y) and are returned as 0xXY
//
static unsigned char pin_parse(void)
{
  unsigned char major;
  unsigned char minor;

  if (error_num)
  {
    return 0;
  }
  
  ignore_blanks();
  
  major = *txtpos++;
  switch (major)
  {
#ifdef ENABLE_PORT0
    case KW_PIN_P0:
#endif
#ifdef ENABLE_PORT1
    case KW_PIN_P1:
#endif
#ifdef ENABLE_PORT2
    case KW_PIN_P2:
#endif
      break;
    default:
      txtpos--;
      goto badpin;
  }
  minor = expression(EXPR_BRACES);
  if (error_num || minor > 7)
  {
    goto badpin;
  }
#ifdef ENABLE_PORT2
#ifdef ENABLE_LOWPOWER_CLOCK
  // Exclude pins used for lowpower clock
  if (major == KW_PIN_P2 && (minor == 3 || minor == 4))
  {
    goto badpin;
  }
#endif
#ifdef ENABLE_DEBUG_INTERFACE
  if (major == KW_PIN_P2 && (minor == 1 || minor == 2))
  {
    goto badpin;
  }
#endif
#endif // ENABLE_PORT2
#ifdef ENABLE_PORT1
#ifdef OS_UART_PORT
#if OS_UART_PORT == HAL_UART_PORT_1
  if (major == KW_PIN_P1 && minor >= 4)
  {
    goto badpin;
  }
#endif
#endif
#endif // ENABLE_PORT1
  return PIN_MAKE(major - KW_PIN_P0, minor);
badpin:
  error_num = ERROR_BADPIN;
  return 0;
}

static int16 adc_read(unsigned char minor)
{
  int16 val;
  halIntState_t intState;
  HAL_ENTER_CRITICAL_SECTION(intState);
  ADCCON3 = minor | analogResolution | analogReference;
#ifdef SIMULATE_PINS
  ADCCON1 = 0x80;
#endif
  while ((ADCCON1 & 0x80) == 0)
    ;
  val = ADCL;
  val |= ADCH << 8;
  HAL_EXIT_CRITICAL_SECTION(intState);        
  return val;  
}


//
// Read pin
//
static VAR_TYPE pin_read(unsigned char major, unsigned char minor)
{
  switch (major)
  {
    case 0:
      if (APCFG & (1 << minor))
      {
#ifdef FEATURE_SAMPLING  
        if (sampling.map & (1 << minor))
        {
          if (sampling.map & 0xc0)
          {
            int32 val;
            val = sampling.adc[0];   
            val += sampling.adc[1];  
            val += sampling.adc[2];  
            val += sampling.adc[3];
            val += sampling.adc[4];
            val += sampling.adc[5];
            val += sampling.adc[6];
            val += sampling.adc[7];
#ifdef FEATURE_TRUE_RMS
            if (sampling.mode == 1)
              val = (int32)sqrt((val > 0 ? 8.0 : -8.0) * val) * (val > 0 ? 1 : -1);
            else
#endif
              // sampling_mode == 2 for averaging
              val /= 4;
            return val;
          }
          return sampling.adc[minor];
        }
#endif  
        return adc_read(minor) >> (8 - (analogResolution >> 3));
      }
      return (P0 >> minor) & 1;
    case 1:
      return (P1 >> minor) & 1;
    default:
      return (P2 >> minor) & 1;
  }
}

#ifdef FEATURE_SAMPLING
void interpreter_sampling(void)
{
  static uint8 samplingCnt;
  uint16 next;
    
  if ( ((P0 & sampling.pin) > 0) != sampling.polarity ) 
  {
    next = sampling.duty;
  }
  else
  {
    next = sampling.timeout - sampling.duty;
    // check if bit 6&7 are set, make P0(6)-P0(7) differential measurement
    // for BlueBattery current measurement
    // no other ports can be used in this mode
    if (sampling.map & 0xc0)
    {
      int16 a;
      switch (sampling.map)
      {
      case 0xc0:
        a = adc_read(11); // read channel 11 (AIN6-AIN7)
        break;
      case 0x40:
        a = adc_read(6);
        break;
      default:  // 0x80
        a = adc_read(7);
      }
  #ifdef FEATURE_TRUE_RMS    
      if (sampling.mode == 1)
      {
        a += 3;
        a /= 4;  // make it 14 bit
        sampling.adc[7 & samplingCnt++] = (long)a * (a < 0 ? -a : a); // result 28-bit signed 
      } else
  #endif
      {
        sampling.adc[7 & samplingCnt++] = a;      
      }
    }
    else 
    {  
      unsigned char map = sampling.map;
      unsigned char pin = 7;
      while (map)
      {
        if (map & 0x80)
        {
          int16 a = adc_read(pin);
          sampling.adc[pin] = ((int32)sampling.adc[pin] * 7 + a) >> 3;
        }
        map <<= 1;
        pin--;
      }
    }  
  }
  P0 ^= sampling.pin;
  // reschedule timer with repeat in case timer interrupt gets lost
  OS_timer_start(sampling.timer, next, 1, 0);
}

#endif  // FEATURE_SAMPLING

#if !defined(ENABLE_WIRE) || ENABLE_WIRE
//
// Execute a wire command.
// Essentially it compiles the BASIC wire representation into a set of instructions which
// can be executed quickly to read and write pins.
//
static void pin_wire_parse(void)
{
  // Starting a new set of WIRE operations?
  if (pinParsePtr == NULL)
  {
    pinParsePtr = heap;
    pinParseCurrent = 0;
    pinParseReadAddr = NULL;
  }

  for (;;)
  {
    const unsigned char op = *txtpos++;
    switch (op)
    {
      case NL:
        txtpos--;
        return;
      case WS_SPACE:
      case ',':
        break;
      case KW_END:
        pin_wire(heap, pinParsePtr);
        pinParsePtr = NULL;
        return;
#ifdef ENABLE_PORT0
      case KW_PIN_P0:
#endif
#ifdef ENABLE_PORT1
      case KW_PIN_P1:
#endif
#ifdef ENABLE_PORT2
      case KW_PIN_P2:
#endif
        txtpos--;
        *pinParsePtr++ = WIRE_PIN;
        pinParseCurrent = pin_parse();
        *pinParsePtr++ = pinParseCurrent;
        if (error_num)
        {
          goto wire_error;
        }
        break;
      case PM_OUTPUT:
        *pinParsePtr++ = WIRE_OUTPUT;
        break;
      case PM_INPUT:
        *pinParsePtr++ = WIRE_INPUT;
        break;
      case PM_TIMEOUT:
        *pinParsePtr++ = WIRE_TIMEOUT;
        *(unsigned short*)pinParsePtr = expression(EXPR_COMMA);
        pinParsePtr += sizeof(unsigned short);
        if (error_num)
        {
          goto wire_error;
        }
        break;
      case PM_WAIT:
      {
        const unsigned char op = txtpos[1];
        if (*txtpos == KW_CONSTANT && (op == CO_HIGH || op == CO_LOW))
        {
          txtpos += 2;

          unsigned char size;
          variable_frame* vframe;
          unsigned char* vptr = parse_variable_address(&vframe);
          if (!vptr)
          {
            goto wire_error;
          }
          if (vframe->type == VAR_DIM_BYTE)
          {
            *vptr = 0;
            size = sizeof(unsigned char);
          }
          else
          {
            *(VAR_TYPE*)vptr = 0;
            size = sizeof(VAR_TYPE);
          }
          if (pinParseReadAddr != vptr)
          {
            *pinParsePtr++ = WIRE_INPUT_SET;
            *(unsigned char**)pinParsePtr = vptr;
            pinParsePtr += sizeof(unsigned char*);
            *pinParsePtr++ = size;
            pinParseReadAddr = vptr;
          }
          pinParseReadAddr += size;

          *pinParsePtr++ = (op == CO_HIGH ? WIRE_WAIT_HIGH : WIRE_WAIT_LOW);
          break;
        }
        *pinParsePtr++ = WIRE_WAIT_TIME;
        *(unsigned short*)pinParsePtr = expression(EXPR_COMMA);
        pinParsePtr += sizeof(unsigned short);
        if (error_num)
        {
          goto wire_error;
        }
        break;
      }
      case KW_READ:
      {
        unsigned char adc = 0;
        unsigned char size = 0;
        if (*txtpos == PM_ADC)
        {
          adc = 1;
          txtpos++;
        }
        variable_frame* vframe;
        unsigned char* vptr = parse_variable_address(&vframe);
        if (!vptr)
        {
          goto wire_error;
        }
        if (vframe->type == VAR_DIM_BYTE)
        {
          *vptr = 0;
          size = sizeof(unsigned char);
        }
        else
        {
          *(VAR_TYPE*)vptr = 0;
          size = sizeof(VAR_TYPE);
        }
        if (pinParseReadAddr != vptr)
        {
          *pinParsePtr++ = WIRE_INPUT_SET;
          *(unsigned char**)pinParsePtr = vptr;
          pinParsePtr += sizeof(unsigned char*);
          *pinParsePtr++ = size;
          pinParseReadAddr = vptr;
        }
        pinParseReadAddr += size;
        if (adc)
        {
          *pinParsePtr++ = WIRE_INPUT_READ_ADC;
        }
        else
        {
          *pinParsePtr++ = WIRE_INPUT_READ;
        }
        break;
      }
      case PM_PULSE:
      {
        unsigned char v = *txtpos++;
        if (v >= 'A' && v <= 'Z')
        {
          variable_frame* vframe;
          unsigned char* vptr = get_variable_frame(v, &vframe);
          
          const unsigned char size = (vframe->type == VAR_DIM_BYTE ? sizeof(unsigned char) : sizeof(VAR_TYPE));
          if (pinParseReadAddr != vptr)
          {
            *pinParsePtr++ = WIRE_INPUT_SET;
            *(unsigned char**)pinParsePtr = vptr;
            pinParsePtr += sizeof(unsigned char*);
            *pinParsePtr++ = size;
            pinParseReadAddr = vptr;
          }
          pinParseReadAddr += size;

          *pinParsePtr++ = WIRE_INPUT_PULSE;
          if (size == sizeof(unsigned char))
          {
            *pinParsePtr++ = vframe->header.frame_size - sizeof(variable_frame);
          }
          else
          {
            *pinParsePtr++ = 1;
          }
          break;
        }
        goto wire_error;
      }
      default:
        txtpos--;
        VAR_TYPE val = expression(EXPR_COMMA);
        if (!error_num)
        {
          *pinParsePtr++ = (val ? WIRE_HIGH : WIRE_LOW);
          break;
        }
        goto wire_error;
    }
  }
wire_error:
  if (!error_num)
  {
    error_num = ERROR_GENERAL;
  }
}
#endif

static void pin_wire(unsigned char* ptr, unsigned char* end)
{
  // We now have an fast expression to manipulate the pins. Do it.
  // We inline all the pin operations to keep the time down.
#if !defined(ENABLE_WIRE) || ENABLE_WIRE
  unsigned short wtimeout = 256;
  unsigned short ptimeout = 256;
#endif
  unsigned char major = 0;
  unsigned char dbit = 1;
  unsigned char len;
  unsigned short count;
  unsigned char* dptr = NULL;
  unsigned char dstep = 0;

  while (ptr < end)
  {
    const unsigned char cmd = *ptr++;

    if (cmd & WIRE_CMD_MASK)
    {
      major = PIN_MAJOR(cmd);
      dbit = 1 << PIN_MINOR(cmd);
      switch (cmd & WIRE_CMD_MASK)
      {
        case WIRE_PIN_OUTPUT:
          goto wire_output;
        case WIRE_PIN_INPUT:
          goto wire_input;
        case WIRE_PIN_HIGH:
          goto wire_high;
        case WIRE_PIN_LOW:
          goto wire_low;
        case WIRE_PIN_READ:
          goto wire_read;
        default:
          goto wire_error;
      }
    }
    else
    {
      switch (WIRE_CASE(cmd))
      {
        case WIRE_CASE(WIRE_PIN):
        {
          len = *ptr++;
          major = PIN_MAJOR(len);
          dbit = 1 << PIN_MINOR(len);
          break;
        }
        case WIRE_CASE(WIRE_OUTPUT):
        wire_output:
          switch (major)
          {
            case 0:
              P0DIR |= dbit;
              break;
            case 1:
              P1DIR |= dbit;
              break;
            case 2:
              P2DIR |= dbit;
              break;
          }
          break;
        case WIRE_CASE(WIRE_INPUT):
        wire_input:
          switch (major)
          {
            case 0:
              P0DIR &= ~dbit;
              break;
            case 1:
              P1DIR &= ~dbit;
              break;
            case 2:
              P2DIR &= ~dbit;
              break;
          }
          break;
        case WIRE_CASE(WIRE_INPUT_NORMAL):
          switch (major)
          {
            case 0:
              P0SEL &= ~dbit;
              P0INP |= dbit;
              APCFG &= ~dbit;
              break;
            case 1:
              P1SEL &= ~dbit;
              P1INP |= dbit;
              break;
            case 2:
              P2SEL &= ~dbit;
              P2INP |= dbit;
              break;
          }
          break;
        case WIRE_CASE(WIRE_INPUT_PULLUP):
          switch (major)
          {
            case 0:
              P0SEL &= ~dbit;
              P0INP &= ~dbit;
              APCFG &= ~dbit;
              P2INP &= ~(1 << 5);
              break;
            case 1:
              P1SEL &= ~dbit;
              P1INP &= ~dbit;
              P2INP &= ~(1 << 6);
              break;
            case 2:
              P2SEL &= ~dbit;
              P2INP &= ~dbit;
              P2INP &= ~(1 << 7);
              break;
          }
          break;
        case WIRE_CASE(WIRE_INPUT_PULLDOWN):
          switch (major)
          {
            case 0:
              P0SEL &= ~dbit;
              P0INP &= ~dbit;
              APCFG &= ~dbit;
              P2INP |= 1 << 5;
              break;
            case 1:
              P1SEL &= ~dbit;
              P1INP &= ~dbit;
              P2INP |= 1 << 6;
              break;
            case 2:
              P2SEL &= ~dbit;
              P2INP &= ~dbit;
              P2INP |= 1 << 7;
              break;
          }
          break;
        case WIRE_CASE(WIRE_INPUT_ADC):
          if (major == 0)
          {
            P0SEL &= ~dbit;
            P0INP |= dbit;
            APCFG |= dbit;
          }
          break;
        case WIRE_CASE(WIRE_HIGH):
        wire_high:
          switch (major)
          {
            case 0:
              P0 |= dbit;
              break;
            case 1:
              P1 |= dbit;
              break;
            case 2:
              P2 |= dbit;
              break;
          }
          break;
        case WIRE_CASE(WIRE_LOW):
        wire_low:
          switch (major)
          {
            case 0:
              P0 &= ~dbit;
              break;
            case 1:
              P1 &= ~dbit;
              break;
            case 2:
              P2 &= ~dbit;
              break;
          }
          break;
#if !defined(ENABLE_WIRE) || ENABLE_WIRE    
        case WIRE_CASE(WIRE_TIMEOUT):
#define WIRE_USEC_TO_PULSE_COUNT(U) ((((U) - 24) * 82) >> 8) // Calibrated Aug 17, 2014
#define WIRE_USEC_TO_WAIT_COUNT(U)  ((((U) - 21) * 179) >> 8) // Calibrated Aug 17, 2014
#define WIRE_COUNT_TO_USEC(C)       ((((C) * 393) >> 8) + 1) // Calbirated Aug 17, 2014
          wtimeout = *(unsigned short*)ptr;
          ptimeout = WIRE_USEC_TO_PULSE_COUNT(wtimeout);
          wtimeout = WIRE_USEC_TO_WAIT_COUNT(wtimeout);
          ptr += sizeof(unsigned short);
          break;
        case WIRE_CASE(WIRE_WAIT_TIME):
          OS_delaymicroseconds(*(short*)ptr - 12); // WIRE_WAIT_TIME execution takes 12us minimum - Calibrated Aug 16, 2014
          ptr += sizeof(unsigned short);
          break;
        case WIRE_CASE(WIRE_INPUT_PULSE):
        {
          len = *ptr++;
          while (len--)
          {
            count = 1;
            switch (major)
            {
              case 0:
              {
                const unsigned char v = P0 & dbit;
                for (; v == (P0 & dbit) && count < ptimeout; count++)
                  ;
                break;
              }
              case 1:
              {
                const unsigned char v = P1 & dbit;
                for (; v == (P1 & dbit) && count < ptimeout; count++)
                  ;
                break;
              }
              case 2:
              {
                const unsigned char v = P2 & dbit;
                for (; v == (P2 & dbit) && count < ptimeout; count++)
                  ;
                break;
              }
            }
            if (dstep)
            {
              if (dstep == 1)
              {
                *dptr = count;
              }
              else
              {
                *(unsigned short*)dptr = count;
              }
              dptr += dstep;
            }
          }
          // Calibrate
          if (dstep)
          {
            len = ptr[-1];
            if (dstep == 1)
            {
              unsigned char* data = dptr;
              while (len--)
              {
                data--;
                *data = WIRE_COUNT_TO_USEC(*data);
              }
            }
            else
            {
              *(VAR_TYPE*)(dptr - dstep) = WIRE_COUNT_TO_USEC(*(unsigned short*)(dptr - dstep));
            }
          }
          break;
        }
        case WIRE_CASE(WIRE_WAIT_HIGH):
        {
          count = 1;
          switch (major)
          {
            case 0:
              for (; !(P0 & dbit) && count < wtimeout; count++)
                ;
              break;
            case 1:
              for (; !(P1 & dbit) && count < wtimeout; count++)
                ;
              break;
            case 2:
              for (; !(P2 & dbit) && count < wtimeout; count++)
                ;
              break;
          }
          goto wire_store;
        }
        case WIRE_CASE(WIRE_WAIT_LOW):
        {
          count = 1;
          switch (major)
          {
            case 0:
              for (; (P0 & dbit) && count < wtimeout; count++)
                ;
              break;
            case 1:
              for (; (P1 & dbit) && count < wtimeout; count++)
                ;
              break;
            case 2:
              for (; (P2 & dbit) && count < wtimeout; count++)
                ;
              break;
          }
        wire_store:
          if (dstep)
          {
            count = WIRE_COUNT_TO_USEC(count);
            if (dstep == 1)
            {
              *(unsigned char*)dptr = count;
            }
            else
            {
              *(VAR_TYPE*)dptr = count;
            }
            dptr += dstep;
          }
          break;
        }
#endif  // ENABLE_WIRE
        case WIRE_CASE(WIRE_INPUT_READ):
        wire_read:
        {
          switch (major)
          {
            case 0:
              *dptr = P0 & dbit;
#ifdef SIMULATE_PINS
              *dptr = dbit;
#endif
              break;
            case 1:
              *dptr = P1 & dbit;
              break;
            case 2:
              *dptr = P2 & dbit;
              break;
          }
          dptr += dstep;
          break;
        }
        case WIRE_CASE(WIRE_INPUT_READ_ADC):
        {
          // Quickly calculate minor number from bitmask
          unsigned char minor = !!(dbit & 0xAA);
          minor |= !!(dbit & 0xCC) << 1;
          minor |= !!(dbit & 0xF0) << 2;
          ADCCON3 = minor | analogResolution | analogReference;
#ifdef SIMULATE_PINS
          ADCCON1 = 0x80;
#endif
          while ((ADCCON1 & 0x80) == 0)
            ;
          count = ADCL;
          count |= ADCH << 8;
          count = ((short)count) >> (8 - (analogResolution >> 3));
          if (dstep == sizeof(unsigned char))
          {
            *dptr = count;
          }
          else
          {
            *(short*)dptr = count;
          }
          dptr += dstep;
          break;
        }
        case WIRE_CASE(WIRE_INPUT_SET):
          dptr = *(unsigned char**)ptr;
          ptr += sizeof(unsigned char*);
          dstep = *ptr++;
          break;
        default:
          goto wire_error;
      }
    }
  }
  return;
wire_error:
  if (!error_num)
  {
    error_num = ERROR_GENERAL;
  }
  return;
}

static unsigned char addspecial_with_compact(unsigned char* item)
{
  unsigned char ret;
  SEMAPHORE_FLASH_WAIT();
  ret = flashstore_addspecial(item);
  if (!ret )
  {
    flashstore_compact(item[sizeof(unsigned short)], heap, sp);
    ret = flashstore_addspecial(item);
  }
  SEMAPHORE_FLASH_SIGNAL();
  return ret;
}

//
// Build a new BLE service and register it with the system
//
static char ble_build_service(void)
{
  unsigned char** line;
  gattAttribute_t* attributes;
  unsigned char ch;
  unsigned char cmd;
  short val;
  short count = 0;
  unsigned char* chardesc = NULL;
  unsigned char chardesclen = 0;
  LINENUM onconnect = 0;

  linenum = servicestart;
  line = findlineptr();
  txtpos = *line + sizeof(LINENUM) + sizeof(char);

  unsigned char* origheap = heap;

  // Allocate the service frame info (to be filled in later)
  service_frame* frame = (service_frame*)heap;
  CHECK_HEAP_OOM(sizeof(service_frame), qoom);
  
  // Allocate space on the stack for the service attributes
  attributes = (gattAttribute_t*)(heap + sizeof(unsigned short));
  CHECK_HEAP_OOM(sizeof(gattAttribute_t) * servicecount + sizeof(unsigned short), qoom);
  
  for (count = 0; count < servicecount; count++)
  {
    // Ignore all GATT commands (we did them already)
    if (*txtpos++ != KW_GATT)
    {
      continue;
    }
    cmd = *txtpos++;

    if (cmd == KW_END)
    {
      break;
    }
    
    // Some defaults
    attributes[count].type.uuid = NULL;
    attributes[count].type.len = 2;
    attributes[count].permissions = GATT_PERMIT_READ;
    attributes[count].handle = 0;
    *(unsigned char**)&attributes[count].pValue = NULL;

    if (cmd == BLE_SERVICE || cmd == BLE_CHARACTERISTIC)
    {
      ch = *txtpos;
      if (ch == '"' || ch == '\'')
      {
        ch = ble_get_uuid();
        if (!ch)
        {
          goto error;
        }
        if (cmd == BLE_SERVICE)
        {
          attributes[count].type.uuid = ble_primary_service_uuid;
          unsigned char* ptr = heap;
          CHECK_HEAP_OOM(ble_uuid_len + sizeof(gattAttrType_t), qoom);
          OS_memcpy(ptr + sizeof(gattAttrType_t), ble_uuid, ble_uuid_len);
          ((gattAttrType_t*)ptr)->len = ble_uuid_len;
          ((gattAttrType_t*)ptr)->uuid = ptr + sizeof(gattAttrType_t);
          *(unsigned char**)&attributes[count].pValue = ptr;
          
          ignore_blanks();
          if (*txtpos == BLE_ONCONNECT)
          {
            if (txtpos[1] == KW_GOSUB)
            {
              txtpos += 2;
              onconnect = expression(EXPR_NORMAL);
              if (error_num)
              {
                goto error;
              }
            }
            else
            {
              goto error;
            }
          }
        }

        if (cmd == BLE_CHARACTERISTIC)
        {
          attributes[count].type.uuid = ble_characteristic_uuid;

          ignore_blanks();
          
          // Description
          val = find_quoted_string();
          if (val != -1)
          {
            chardesc = txtpos;
            chardesclen = (unsigned char)val;
            txtpos += val + 1;
          }
          else
          {
            chardesc = NULL;
            chardesclen = 0;
          }
        }
      }
      else
      {
        goto error;
      }
    }
    else if (cmd == KW_READ || cmd == KW_WRITE || cmd == BLE_NOTIFY || cmd == BLE_INDICATE || cmd == BLE_WRITENORSP || cmd == BLE_AUTH)
    {
      txtpos--;
      ch = 0;
      for (;;)
      {
        switch (*txtpos++)
        {
          case KW_READ:
            ch |= GATT_PROP_READ;
            break;
          case KW_WRITE:
            ch |= GATT_PROP_WRITE;
            break;
          case BLE_NOTIFY:
            ch |= GATT_PROP_NOTIFY;
            break;
          case BLE_INDICATE:
            ch |= GATT_PROP_INDICATE;
            break;
          case BLE_WRITENORSP:
            ch |= GATT_PROP_WRITE_NO_RSP;
            break;
          case BLE_AUTH:
            ch |= GATT_PROP_AUTHEN;
            switch (*(txtpos - 2))
            {
              case KW_READ:
                attributes[count].permissions |= GATT_PERMIT_AUTHEN_READ;
                break;
              case KW_WRITE:
                attributes[count].permissions |= GATT_PERMIT_AUTHEN_WRITE;
                break;
              default:
                break;
            }
            break;
          default:
            txtpos--;
            goto done;
        }
      }
done:
      CHECK_HEAP_OOM(1, qoom);
      heap[-1] = ch;
      *(unsigned char**)&attributes[count - 1].pValue = heap - 1;
      
      ch = *txtpos;
      if (ch < 'A' || ch > 'Z')
      {
        goto error;
      }

      txtpos++;
      if (*txtpos != WS_SPACE && *txtpos != NL && *txtpos < 0x80)
      {
        goto error;
      }
      
      unsigned char* uuid = heap;
      gatt_variable_ref* vref = (gatt_variable_ref*)(uuid + ble_uuid_len);
      CHECK_HEAP_OOM(ble_uuid_len + sizeof(gatt_variable_ref), qoom);
      vref->read = 0;
      vref->write = 0;
      vref->var = ch;
      vref->attrs = attributes;
      vref->cfg = NULL;
      
      OS_memcpy(uuid, ble_uuid, ble_uuid_len);
      *(unsigned char**)&attributes[count].pValue = (unsigned char*)vref;
      attributes[count].permissions = GATT_PERMIT_READ | GATT_PERMIT_WRITE;
      attributes[count].type.uuid = uuid;
      attributes[count].type.len = ble_uuid_len;

      for (;;)
      {
        ignore_blanks();
        ch = *txtpos;
        if (ch == BLE_ONREAD || ch == BLE_ONWRITE)
        {
          txtpos++;
          if (*txtpos != KW_GOSUB)
          {
            goto error;
          }
          txtpos++;
          linenum = expression(EXPR_NORMAL);
          if (error_num)
          {
            goto error;
          }
          if (ch == BLE_ONREAD)
          {
            vref->read = linenum;
          }
          else
          {
            vref->write = linenum;
          }
        }
        else if (ch == NL)
        {
          break;
        }
        else
        {
          goto error;
        }
      }

      if ((attributes[count - 1].pValue[0] & (GATT_PROP_INDICATE|GATT_PROP_NOTIFY)) != 0)
      {
        gattCharCfg_t *runtimeProfileCharCfg = (gattCharCfg_t*)heap;
        CHECK_HEAP_OOM(sizeof(gattCharCfg_t) * linkDBNumConns, qoom);
        GATTServApp_InitCharCfg(INVALID_CONNHANDLE, runtimeProfileCharCfg);
        count++;
        attributes[count].type.uuid = ble_client_characteristic_config_uuid;
        attributes[count].type.len = 2;
        attributes[count].permissions = GATT_PERMIT_READ | GATT_PERMIT_WRITE;
        attributes[count].handle = 0;
        *(unsigned char**)&attributes[count].pValue = (unsigned char*)&vref->cfg;
        vref->cfg = runtimeProfileCharCfg;
        variable_frame* vframe;
        get_variable_frame(vref->var, &vframe);
        vframe->ble = vref;
      }
 
      if (chardesclen > 0)
      {
        count++;
        attributes[count].type.uuid = ble_characteristic_description_uuid;
        attributes[count].type.len = 2;
        attributes[count].permissions = GATT_PERMIT_READ;
        attributes[count].handle = 0;

        unsigned char* desc = heap;
        CHECK_HEAP_OOM(chardesclen + 1, qoom);
        OS_memcpy(desc, chardesc, chardesclen);
        desc[chardesclen] = 0;
        *(unsigned char**)&attributes[count].pValue = desc;
        chardesclen = 0;
      }
    }
    else
    {
      goto error;
    }

    txtpos = *++line + sizeof(LINENUM) + sizeof(char);
  }

  // Build a stack header for this service
  frame->header.frame_type = FRAME_SERVICE_FLAG;
  frame->header.frame_size = heap - origheap;
  frame->attrs = attributes;
  frame->connect = onconnect;
  ((unsigned short*)attributes)[-1] = count;
  
  // Register the service 
  ch = GATTServApp_RegisterService((gattAttribute_t*)attributes, count,
                                   GATT_MAX_ENCRYPT_KEY_SIZE, &ble_service_callbacks);
  if (ch != SUCCESS)
  {
    goto error;
  }

  return 0;

error:
  heap = origheap;
  txtpos = *line + sizeof(LINENUM) + sizeof(char);
  return 1;
qoom:
  heap = origheap;
  txtpos = *line + sizeof(LINENUM) + sizeof(char);
  return 2;
}

//
// Parse a string into a UUID
//  This is fairly lax in what it considers valid.
//
static char ble_get_uuid(void)
{
  VAR_TYPE v = 0;
  unsigned char* end;
  
  end = txtpos + find_quoted_string();
  if (end < txtpos)
  {
    return 0;
  }

  ble_uuid_len = 0;
  while (txtpos < end)
  {
    if (ble_uuid_len >= sizeof(ble_uuid))
    {
      return 0;
    }
    else if (*txtpos == '-')
    {
      txtpos++;
    }
    else
    {
      v = parse_int(2, 16);
      if (error_num)
      {
        return 0;
      }
      ble_uuid[ble_uuid_len++] = (unsigned char)v;
    }
  }
#ifdef TARGET_CC254X
  // Reverse
  for (v = 0; v < ble_uuid_len / 2; v++)
  {
    unsigned char c = ble_uuid[v];
    ble_uuid[v] = ble_uuid[ble_uuid_len - v - 1];
    ble_uuid[ble_uuid_len - v - 1] = c;
  }
#endif
  txtpos++;
  return ble_uuid_len > 0;
}

//
// Calculate the maximum read/write offset for the specified variable
//
static unsigned short ble_max_offset(gatt_variable_ref* vref, unsigned short offset, unsigned char maxlen)
{
  variable_frame* frame;
  unsigned char moffset = offset + maxlen;

  get_variable_frame(vref->var, &frame);

  if (frame->type == VAR_DIM_BYTE)
  {
    if (moffset > frame->header.frame_size - sizeof(variable_frame))
    {
      moffset = frame->header.frame_size - sizeof(variable_frame);
    }
  }
  else
  {
    if (moffset > VAR_SIZE)
    {
      moffset = VAR_SIZE;
    }
  }

  return moffset;
}

#ifdef DEBUG_SERIAL
#define DEBUG_OUT(x) OS_serial_write(1,x)
#else
#define DEBUG_OUT(x)
#endif

//
// When a BLE characteristic is read, we process the incoming request from
// the appropriate BASIC variable. If an ONREAD event is specified we notify the user
// of the read request *before* we do the actual read.
//
static unsigned char ble_read_callback(unsigned short handle, gattAttribute_t* attr, unsigned char* value, unsigned char* len, unsigned short offset, unsigned char maxlen, uint8 method)
{
  gatt_variable_ref* vref;
  unsigned char moffset;
  unsigned char* v;
  variable_frame* frame;
  
  DEBUG_OUT('<');
  
  if (attr->type.uuid == ble_client_characteristic_config_uuid)
  {
    return FAILURE;
  }
  
  vref = (gatt_variable_ref*)attr->pValue;
  moffset = ble_max_offset(vref, offset, maxlen);
  if (!moffset)
  {
    return FAILURE;
  }

  // run interpreter only if its the first paket
  if (vref->read && offset == 0)
  {
    SEMAPHORE_READ_WAIT();
    interpreter_run(vref->read, INTERPRETER_CAN_RETURN);
  }

  v = get_variable_frame(vref->var, &frame);
#ifdef TARGET_CC254X
  if (frame->type == VAR_DIM_BYTE)
  {
    OS_memcpy(value, v + offset, moffset - offset);
    unsigned char var_len = frame->header.frame_size - sizeof(variable_frame);
    // when the DIM array is larger than 20 bytes (max payload)
    // the read CB will be called with increasing offset until all data is transported
    // block the interpreter until all data has been transported
    //bluebasic_block_execution = (moffset == var_len) ? 0 : 1;
    if (moffset == var_len) {
      SEMAPHORE_READ_SIGNAL();
    } //else {
      //SEMAPHORE_READ_WAIT();
    //}
  }
  else
  {
    for (unsigned char i = offset; i < moffset; i++)
    {
      value[i - offset] = v[moffset - i - 1];
    }
  }
#else
  OS_memcpy(value, v + offset, moffset - offset);
#endif

  *len = moffset - offset;
  return SUCCESS;
}

//
// When a BLE characteristic is written, we process the incomign data and update
// the appropriate BASIC variable. If an ONWRITE event is specified we notify the user
// of the change *after* the write.
//
static unsigned char ble_write_callback(unsigned short handle, gattAttribute_t* attr, unsigned char* value, unsigned char len, unsigned short offset, uint8 method)
{
  gatt_variable_ref* vref;
  unsigned char moffset;
  unsigned char* v;
  variable_frame* frame;

  DEBUG_OUT('>');
  
  if (attr->type.uuid == ble_client_characteristic_config_uuid)
  {
    return GATTServApp_ProcessCCCWriteReq(handle, attr, value, len, offset, GATT_CLIENT_CFG_NOTIFY);
  }
 
  vref = (gatt_variable_ref*)attr->pValue;
  moffset = ble_max_offset(vref, offset, len);
  if (moffset != offset + len)
  {
    return FAILURE;
  }
  
  v = get_variable_frame(vref->var, &frame);

#ifdef TARGET_CC254X
  if (frame->type == VAR_DIM_BYTE)
  {
    OS_memcpy(v + offset, value, moffset - offset);
  }
  else
  {
    for (unsigned char i = offset; i < moffset; i++)
    {
      v[moffset - i - 1] = value[i - offset];
    }
  }
#else
  OS_memcpy(v + offset, value, moffset - offset);
#endif
  
  if (vref->write)
  {
    interpreter_run(vref->write, INTERPRETER_CAN_RETURN);
  }

  if (attr[1].type.uuid == ble_client_characteristic_config_uuid)
  {
    ble_notify_assign(vref);
  }
  
  return SUCCESS;
}

//
// Send a BLE NOTIFY event
//
static void ble_notify_assign(gatt_variable_ref* vref)
{
  DEBUG_OUT('!');
  GATTServApp_ProcessCharCfg(vref->cfg, &(vref->var),
                             FALSE, vref->attrs,
                             ((unsigned short*)vref->attrs)[-1], INVALID_TASK_ID,
                             ble_read_callback);
}

//
// init all BLE client characteristic configurations values
//
void ble_init_ccc( void )
{
  unsigned char* ptr;
  service_frame* vframe;
  short i;
  
  for (ptr = (unsigned char*)program_end; ptr < heap; ptr += ((frame_header*)ptr)->frame_size)
  {
    vframe = (service_frame*)ptr;
    if (vframe->header.frame_type == FRAME_SERVICE_FLAG)
    {
        for (i = ((short*)vframe->attrs)[-1]; i; i--)
        {
          if (vframe->attrs[i].type.uuid == ble_client_characteristic_config_uuid)
          {
            gattCharCfg_t *conn = *(gattCharCfg_t **)vframe->attrs[i].pValue;
            GATTServApp_InitCharCfg(INVALID_CONNHANDLE, conn);
          }
        }
    }
  }
} 

//
// BLE connection management. If the ONCONNECT event was specified when a service
// was created, we forward any connection changes up to the user code.
//
void ble_connection_status(unsigned short connHandle, unsigned char changeType, signed char rssi)
{
  unsigned char* ptr;
  service_frame* vframe;

  for (ptr = (unsigned char*)program_end; ptr < heap; ptr += ((frame_header*)ptr)->frame_size)
  {
    vframe = (service_frame*)ptr;
    if (vframe->header.frame_type == FRAME_SERVICE_FLAG)
    {
#ifndef BLUEBATTERY
      if (vframe->connect)
      {
       unsigned char vname;
       if (VARIABLE_IS_EXTENDED('H') || VARIABLE_IS_EXTENDED('S') || VARIABLE_IS_EXTENDED('V'))
        {
          continue; // Silently fail
        }
        VARIABLE_INT_SET('H', connHandle);
        VARIABLE_INT_SET('S', changeType);
        if (changeType == LINKDB_STATUS_UPDATE_STATEFLAGS)
        {
          unsigned char f = 0;
          for (unsigned char j = 0x01; j < 0x20; j <<= 1) // No direct way to read flag bits!
          {
            if (linkDB_State(connHandle, j))
            {
              f |= j;
            }
          }
          VARIABLE_INT_SET('V', f);
        }
        else if (changeType == LINKDB_STATUS_UPDATE_RSSI)
        {
          VARIABLE_INT_SET('V', rssi);
        }
        interpreter_run(vframe->connect, INTERPRETER_CAN_RETURN);
      }
#endif
      if (changeType == LINKDB_STATUS_UPDATE_REMOVED || (changeType == LINKDB_STATUS_UPDATE_STATEFLAGS && !linkDB_Up(connHandle)))
      {
        ble_init_ccc();
      }
    }
  }  
}

#ifdef TARGET_CC254X
#if ( HOST_CONFIG & OBSERVER_CFG )    
extern void interpreter_devicefound(unsigned char addtype, unsigned char* address, signed char rssi, unsigned char eventtype, unsigned char len, unsigned char* data)
{
  unsigned char vname;

  if (blueBasic_discover.linenum && !VARIABLE_IS_EXTENDED('A') && !VARIABLE_IS_EXTENDED('R') && !VARIABLE_IS_EXTENDED('E'))
  {
    unsigned char* osp = sp;
    error_num = ERROR_OK;
    VARIABLE_INT_SET('A', addtype);
    VARIABLE_INT_SET('R', rssi);
    VARIABLE_INT_SET('E', eventtype);
    create_dim('B', 8, address);
    create_dim('V', len, data);
    if (!error_num)
    {
      interpreter_run(blueBasic_discover.linenum, INTERPRETER_CAN_RETURN);
    }
    sp = osp;
  }
}
#endif
#endif // TARGET_CC254X
