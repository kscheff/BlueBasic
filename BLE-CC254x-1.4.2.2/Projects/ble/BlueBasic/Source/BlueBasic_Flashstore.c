//
//  BluseBasic_Flashstore.c
//  BlueBasic
//
//  Created by tim on 9/7/14.
//  Copyright (c) 2014 tim. All rights reserved.
//

#include "os.h"

extern __data unsigned char* heap;
extern __data unsigned char* sp;
#define FLASHSTORE_WORDS(LEN)     ((LEN) >> 2)

#ifdef SIMULATE_FLASH
//unsigned char __store[FLASHSTORE_LEN];
unsigned char __store[124l * FLASHSTORE_PAGESIZE];
#define FLASHSTORE_CPU_BASEADDR (__store)
#define FLASHSTORE_DMA_BASEADDR (0)
struct
{
    unsigned short free;
    unsigned short waste;
    unsigned short *special;
} orderedpages[124];
#else
__code const unsigned char _flashstore[4] @ "FLASHSTORE" = {0xff, 0xff, 0xff, 0xff} ;
static struct
{
  unsigned short free;
  unsigned short waste;
  unsigned short *special;
} orderedpages[FLASHSTORE_NRPAGES];
#endif

static const unsigned char* flashstore = (unsigned char*)FLASHSTORE_CPU_BASEADDR;
#define FLASHSTORE_FADDR(ADDR)  ((((unsigned char*)(ADDR) - FLASHSTORE_CPU_BASEADDR) + FLASHSTORE_DMA_BASEADDR) >> 2)
#define FLASHSTORE_FPAGE(ADDR)  (FLASHSTORE_FADDR(ADDR) >> 9)

static unsigned short** lineindexstart;
static unsigned short** lineindexend;

#define FLASHSTORE_PAGEBASE(IDX)  &flashstore[FLASHSTORE_PAGESIZE * (IDX)]
#define FLASHSTORE_PADDEDSIZE(SZ) (((SZ) + 3) & -4)


//
// Flash page structure:
//  <age:4><data:FLASHSTORE_PAGESIZE-4>
//  Each page is given an age, starting at 1, as they are used. An age of 0xFFFFFFFF means the page is empty.
//  The ages are used to reconstruct the program data by keeping the pages use ordered correctly.
//
#ifdef TARGET_CC254X
typedef unsigned long flashpage_age;
#else
typedef unsigned int flashpage_age;
#endif
static flashpage_age lastage = 1;

#define VAR_TYPE    int32_t
extern void printmsg(const char *msg);
extern void printnum(signed char fieldsize, VAR_TYPE num);

//
// Flash item structure:
//  <id:2><len:1><data:len>
//  Id's between 1 and 0xFFFD are valid program lines. Id==0 is an invalidated item, Id=0xFFFE is special (undefined)
//  and Id=0xFFFF is free space to the end of the page.
//

static void flashstore_invalidate(unsigned short* mem);

//
// Heapsort
//  Modified from: http://www.algorithmist.com/index.php/Heap_sort.c
//
static void siftdown(short root, short bottom)
{
  unsigned short** lines = lineindexstart;
  for (;;)
  {
    short max = root * 2 + 1;
    
    if (max > bottom)
    {
      return;
    }
    if (max < bottom)
    {
      short other = max + 1;
      if (*lines[other] > *lines[max])
      {
        max = other;
      }
    }
    if (*lines[root] >= *lines[max])
    {
      return;
    }
    
    unsigned short* temp = lines[root];
    lines[root] = lines[max];
    lines[max] = temp;

    root = max;
  }
}

static void flashpage_heapsort(void)
{
  short i;
  unsigned short** lines = lineindexstart;
  unsigned short count = lineindexend - lineindexstart;

  for (i = count / 2; i >= 0; i--)
  {
    osal_run_system();
    siftdown(i, count - 1);
  }
  for (i = count - 1; i >= 0; i--)
  {
    unsigned short* temp = lines[0];
    lines[0] = lines[i];
    lines[i] = temp;
    osal_run_system();
    siftdown(0, i - 1);
  }
}


//
// Initialize the flashstore.
//  Rebuild the program store from the flash store.
//  Called when the interpreter powers up.
//
unsigned char** flashstore_init(unsigned char** startmem)
{
  lineindexstart = (unsigned short**)startmem;
  lineindexend = lineindexstart;

  OS_flashstore_init();

  unsigned char ordered = 0;
  const unsigned char* page;
  unsigned char pnr = FLASHSTORE_NRPAGES;
  for (page = flashstore; pnr--; page += FLASHSTORE_PAGESIZE)
  {
    orderedpages[ordered].waste = 0;
    orderedpages[ordered].special = (unsigned short*)0;
    if (*(flashpage_age*)page > lastage)
    {
      lastage = *(flashpage_age*)page;
    }

    // Analyse page
    const unsigned char* ptr;
    // the loop need to be kept inside a page, even at the end of the memory bank
    for (ptr = page + sizeof(flashpage_age); (ptr <= page + (FLASHSTORE_PAGESIZE-1)) && (ptr > page) ; )
    {
      unsigned short id = *(unsigned short*)ptr;
      if (id == FLASHID_FREE)
      {
        break;
      }
      else if (id == FLASHID_INVALID)
      {
        orderedpages[ordered].waste += FLASHSTORE_PADDEDSIZE(ptr[sizeof(unsigned short)]);
      }
      else if (id != FLASHID_SPECIAL)
      {
        // Valid program line - record entry (sort later)
        *lineindexend++ = (unsigned short*)ptr;
      }
      else if (orderedpages[ordered].special == (unsigned short*)0)
      {
        // mark the first secial entry in the page
        // for faster access via find_special
        orderedpages[ordered].special = (unsigned short*) ptr;
      }
      ptr += FLASHSTORE_PADDEDSIZE(ptr[sizeof(unsigned short)]);
      osal_run_system();
    } 
    orderedpages[ordered].free = FLASHSTORE_PAGESIZE - (ptr - page);
    ordered++;
  }

  // We now have a set of program lines, indexed from "startmem" to "mem" which we need to sort
  flashpage_heapsort();
  
  return (unsigned char**)lineindexend;
}

//
// Find the closest, lower or equal, ID
//
unsigned short** flashstore_findclosest(unsigned short id)
{
  unsigned short** lines = lineindexstart;
  unsigned short min = 0;
  unsigned short max = lineindexend - lines;
  if (max > 0)
  {
    max -= 1;
    while (min < max)
    {
      unsigned short mid = min + (max - min) / 2;
      if (*lineindexstart[mid] < id)
      {
        min = mid + 1;
      }
      else
      {
        max = mid;
      }
    }
  }
  return lines + min;
}

//
// Find space for the new line in the youngest page
//
static signed char flashstore_findspace(unsigned char len)
{
  unsigned char pg;
  unsigned char spg = 0;
  flashpage_age age = 0xFFFFFFFF;
  for (pg = 0; pg < FLASHSTORE_NRPAGES; pg++)
  {
    flashpage_age cage = *(flashpage_age*)FLASHSTORE_PAGEBASE(pg);
    if (cage < age && orderedpages[pg].free >= len)
    {
      spg = pg;
      age = cage;
    }
  }
  return age == 0xFFFFFFFF ? -1 : spg;
}

//
// Add a new line to the flash store, returning the total number of lines.
//
unsigned char** flashstore_addline(unsigned char* line)
{
  unsigned short id = *(unsigned short*)line;
  unsigned short** oldlineptr = flashstore_findclosest(id);
  unsigned char found = 0;
  if (oldlineptr < lineindexend && **oldlineptr == id)
  {
    found = 1;
  }

  // Find space for the new line in the youngest page
  unsigned char len = FLASHSTORE_PADDEDSIZE(line[sizeof(unsigned short)]);
  signed char pg = flashstore_findspace(len);
  if (pg != -1)
  {
    unsigned short* mem = (unsigned short*)(FLASHSTORE_PAGEBASE(pg) + FLASHSTORE_PAGESIZE - orderedpages[pg].free);
    OS_flashstore_write(FLASHSTORE_FADDR(mem), line, FLASHSTORE_WORDS(len));
    orderedpages[pg].free -= len;
    // If there was an old version, invalidate it
    if (found)
    {
      // If there was an old version, invalidate it
      flashstore_invalidate(*oldlineptr);

      // Insert new line into index
      *oldlineptr = mem;
    }
    else
    {
      // Insert new line
      unsigned short len = sizeof(unsigned short*) * (lineindexend - oldlineptr);
      if (len == 0)
      {
        oldlineptr[0] = mem;
      }
      else
      {
        OS_rmemcpy(oldlineptr + 1, oldlineptr, len);
        if (**oldlineptr > id)
        {
          oldlineptr[0] = mem;
        }
        else
        {
          oldlineptr[1] = mem;
        }
      }
      lineindexend++;
    }
    return (unsigned char**)lineindexend;
  }
  else
  {
    // No space
    return NULL;
  }
}

unsigned char flashstore_addspecial(unsigned char* item)
{
  *(unsigned short*)item = FLASHID_SPECIAL;
  unsigned char len = FLASHSTORE_PADDEDSIZE(item[sizeof(unsigned short)]);
  // Find space for the new line in the youngest page
  signed char pg = flashstore_findspace(len);
  if (pg != -1)
  {
    unsigned short* mem = (unsigned short*)(FLASHSTORE_PAGEBASE(pg) + FLASHSTORE_PAGESIZE - orderedpages[pg].free);
    OS_flashstore_write(FLASHSTORE_FADDR(mem), item, FLASHSTORE_WORDS(len));
    orderedpages[pg].free -= len;
    if ( ((orderedpages[pg].special != 0) && (mem < orderedpages[pg].special)) || orderedpages[pg].special == 0 )
    {
      // the new entry is located before the saved one
      // or its the first entry in this page
      orderedpages[pg].special = mem;
    }
    return 1;
  }
  else
  {
    return 0;
  }
}

unsigned char flashstore_deletespecial(unsigned long specialid)
{
  unsigned char* ptr = flashstore_findspecial(specialid);
  if (ptr)
  {
    flashstore_invalidate((unsigned short*)ptr);
    unsigned char page = ((unsigned char*)ptr - flashstore) / FLASHSTORE_PAGESIZE;
    if (orderedpages[page].special == (unsigned short*)ptr)
      // we put 1 (impossible page pointer) as an indicator, to search
      orderedpages[page].special = (unsigned short*)1; 
    return 1;
  }
  return 0;
}

unsigned char* flashstore_findspecial(unsigned long specialid)
{
  const unsigned char* page;
  unsigned char pnr = 0;
  for (page = flashstore; pnr < FLASHSTORE_NRPAGES; page += FLASHSTORE_PAGESIZE, pnr++)
  {
    const unsigned char* ptr;
    // when no special is marked then we don't need to search the page
    if (orderedpages[pnr].special == 0)
    {
      continue;
    }
    if (orderedpages[pnr].special != (unsigned short*)1)
    {
      ptr = (unsigned char*)orderedpages[pnr].special;
    }
    else
    {
      ptr = page + sizeof(flashpage_age);
    }
    for ( ;
         (ptr <= page + (FLASHSTORE_PAGESIZE-1)) && (ptr > page);
         ptr += FLASHSTORE_PADDEDSIZE(ptr[sizeof(unsigned short)]))
    {
      unsigned short id = *(unsigned short*)ptr;
      if (id == FLASHID_FREE)
      {
        if (orderedpages[pnr].special == (unsigned short*)1)
        {
          orderedpages[pnr].special = 0;
        }
        break;
      }
      else if (id == FLASHID_SPECIAL) {
        if (orderedpages[pnr].special == (unsigned short*)1)
          orderedpages[pnr].special = (unsigned short*)ptr;
        if (*(unsigned long*)(ptr + FLASHSPECIAL_ITEM_ID) == specialid)
        {
          return (unsigned char*)ptr;
        }
      }
    }
  }
  return NULL;
}

//
// Remove the line from the flash store.
//
unsigned char** flashstore_deleteline(unsigned short id)
{
  unsigned short** oldlineptr = flashstore_findclosest(id);
  if (lineindexstart != lineindexend) {
    if (*oldlineptr != NULL && **oldlineptr == id)
    {
      lineindexend--;
      flashstore_invalidate(*oldlineptr);
      if (lineindexend == lineindexstart) {
        // when this was the only line present we delete it
        *oldlineptr = 0;
      } else {
        OS_memcpy(oldlineptr, oldlineptr + 1, sizeof(unsigned short*) * (lineindexend - oldlineptr));
      }
    }
  }
  return (unsigned char**)lineindexend;
}

//
// Delete everything from the store.
//
unsigned char** flashstore_deleteall(void)
{
  static unsigned char pg;
  for (pg = 0; pg < FLASHSTORE_NRPAGES; pg++)
  {
    if (orderedpages[pg].free != FLASHSTORE_PAGESIZE - sizeof(flashpage_age))
    {
      const unsigned char* base = FLASHSTORE_PAGEBASE(pg);
      OS_flashstore_erase(FLASHSTORE_FPAGE(base));
      lastage++;
      OS_flashstore_write(FLASHSTORE_FADDR(base), (unsigned char*)&lastage, FLASHSTORE_WORDS(sizeof(lastage)));
      orderedpages[pg].waste = 0;
      orderedpages[pg].free = FLASHSTORE_PAGESIZE - sizeof(flashpage_age);
      orderedpages[pg].special = 0;
    }
    // keep OSAL spinning
    osal_run_system();
  }

  lineindexend = lineindexstart;
  return (unsigned char**)lineindexend;
}

//
// How much space is free?
//
unsigned int flashstore_freemem(void)
{
  unsigned int free = 0;
  unsigned char pg;
  for (pg = 0; pg < FLASHSTORE_NRPAGES; pg++)
  {
    unsigned int pgFree = orderedpages[pg].free + orderedpages[pg].waste;
    free += pgFree;
    printnum(2, pg);
    printnum(5, FLASHSTORE_PAGESIZE - pgFree);
    printmsg(" page bytes occupied");
  }
  return free;
}

void flashstore_compact(unsigned char len, unsigned char* tempmemstart, unsigned char* tempmemend)
{
  const unsigned short available = tempmemend - tempmemstart + (unsigned char*)lineindexend - (unsigned char*)lineindexstart;  
  // Find the lowest age page which this will fit in.
  static unsigned char pg;
  unsigned char selected = 0;
  static unsigned short occupied;
  flashpage_age age = 0xFFFFFFFF;
  len = FLASHSTORE_PADDEDSIZE(len);
  for (pg = 0; pg < FLASHSTORE_NRPAGES; pg++)
  {
    flashpage_age cage = *(flashpage_age*)FLASHSTORE_PAGEBASE(pg);
    occupied = FLASHSTORE_PAGESIZE - orderedpages[pg].free - orderedpages[pg].waste;
    if ((cage < age && FLASHSTORE_PAGESIZE - occupied >= len)
        && occupied <= available)
    {
      selected = pg;
      age = cage;
    }
  }
  occupied = FLASHSTORE_PAGESIZE - orderedpages[selected].free - orderedpages[selected].waste;
  
  unsigned short heap_len = 0;
  char corrupted = 0;
  // Need at least FLASHSTORE_PAGESIZE
  if (occupied > tempmemend - tempmemstart)  
  {
    if (occupied > available)
      return;
    // use additionally index area for compacting
    // by moving heap to to beginning
    // Attention: this assumes that tempmemstart is the current heap end!
    heap_len = tempmemstart - (unsigned char*)lineindexend;
    if (heap_len)
    {
      OS_memcpy((void*)lineindexstart, (void*)lineindexend, heap_len);
      tempmemstart = (unsigned char*) lineindexstart + heap_len;
      corrupted = 1; // since we use the index area it is definitly corrupted
    }
  }
  
  if (age != 0xFFFFFFFF)
  {
    // Found enough space for the line, compact the page
    
    // Copy required page data into RAM
    unsigned char* ram = tempmemstart;
    unsigned char* flash = (unsigned char*)FLASHSTORE_PAGEBASE(selected);
    static unsigned char* ptr;
    unsigned short mem_length = 0;
    char deleted = 0;
    unsigned short *special = 0;
    for (ptr = flash + sizeof(flashpage_age); (ptr <= flash + (FLASHSTORE_PAGESIZE-1)) && (ptr > flash); )
    {
      unsigned short id = *(unsigned short*)ptr;
      unsigned char len = FLASHSTORE_PADDEDSIZE(ptr[sizeof(unsigned short)]);
      if (id == FLASHID_FREE)
      {
        // the rest of the page is empty
        break;
      }
      else if (id != FLASHID_INVALID)
      {
        corrupted |= (id != FLASHID_SPECIAL) && deleted;
        if (special == 0 && id == FLASHID_SPECIAL)
        {
          special = (unsigned short *)(flash + mem_length + sizeof(flashpage_age));
        }
        if (mem_length + len <= available)
        {
          OS_memcpy(ram, ptr, len);
          ram += len;
          mem_length += len;
        }
        else
          return;  // mem_length doesn't fit  
      }
      else
      {
        deleted = 1;
      }
      ptr += len;
    }
//    osal_run_system();        
    // Erase the page
    OS_flashstore_erase(FLASHSTORE_FPAGE(flash));
//    osal_run_system();
    lastage++;
    OS_flashstore_write(FLASHSTORE_FADDR(flash), (unsigned char*)&lastage, FLASHSTORE_WORDS(sizeof(lastage)));
    orderedpages[selected].waste = 0;
    orderedpages[selected].free = FLASHSTORE_PAGESIZE - mem_length - sizeof(flashpage_age);
    orderedpages[selected].special = special;

    // Copy the old lines back in.
    flash += sizeof(flashpage_age);
    OS_flashstore_write(FLASHSTORE_FADDR(flash), tempmemstart, FLASHSTORE_WORDS(mem_length));
//    osal_run_system();
    if (corrupted)
    {
      if (heap_len)
      {
        // copy old heap data back into position before rebuilding the index
        OS_memcpy((void*)lineindexend, (void*)lineindexstart, heap_len);
      }
      // We corrupted memory, so we need to reinitialize
      flashstore_init((unsigned char**)lineindexstart);
    }
  }
}

//
// Invalidate the line entry at the given address.
//
static void flashstore_invalidate(unsigned short* mem)
{
  static struct
  {
    unsigned short invalid;
    unsigned char len;
    unsigned char padding;
  } invalid;
  OS_memcpy(&invalid, mem, sizeof(invalid));
  invalid.invalid = FLASHID_INVALID;

  OS_flashstore_write(FLASHSTORE_FADDR(mem), (unsigned char*)&invalid, FLASHSTORE_WORDS(sizeof(invalid)));

  orderedpages[((unsigned char*)mem - flashstore) / FLASHSTORE_PAGESIZE].waste += invalid.len;
}

//
// Support the OSAL flash API
//
#if (!defined(ENABLE_SNV) && !OAD_KEEP_NV_PAGES) || (ENABLE_SNV && !OAD_KEEP_NV_PAGES)
unsigned char osal_snv_read(unsigned char id, unsigned char len, void *pBuf)
{
#if !GAP_BOND_MGR
  return SUCCESS;  
#else
  unsigned char* mem = flashstore_findspecial(FLASHSPECIAL_SNV + id);
  if (mem && mem[FLASHSPECIAL_DATA_LEN] == len)
  {
    OS_memcpy(pBuf, mem + FLASHSPECIAL_DATA_OFFSET, len);
    return SUCCESS;
  }
  else
  {
    return NV_OPER_FAILED;
  }
#endif  
}
#endif

#if (!defined(ENABLE_SNV) && !OAD_KEEP_NV_PAGES) || !OAD_KEEP_NV_PAGES && !ENABLE_SNV
unsigned char osal_snv_write(unsigned char id, unsigned char len, void *pBuf)
{  
#if !GAP_BOND_MGR
  return SUCCESS;
#else  
  if (heap + len + FLASHSPECIAL_DATA_OFFSET > sp)
  {
    return NV_OPER_FAILED;
  }
  else
  {
    unsigned char* item = heap;
    heap += len + FLASHSPECIAL_DATA_OFFSET;

    *(unsigned long*)&item[FLASHSPECIAL_ITEM_ID] = FLASHSPECIAL_SNV + id;
    item[FLASHSPECIAL_DATA_LEN] = len + FLASHSPECIAL_DATA_OFFSET;
    OS_memcpy(item + FLASHSPECIAL_DATA_OFFSET, pBuf, len);
    unsigned char r = flashstore_addspecial(item);
    
    heap = item;
    return r != 0 ? SUCCESS : NV_OPER_FAILED;
  }
#endif  
}
#endif

#if (!defined(ENABLE_SNV) || ENABLE_SNV ) && !OAD_KEEP_NV_PAGES

unsigned char osal_snv_compact(unsigned char threshold)
{
  return SUCCESS;
}

unsigned char osal_snv_init()
{
  return SUCCESS;
}

#endif

#if ENABLE_SNV
unsigned char get_snv_length(unsigned char id)
{
        unsigned char len = 0;
        return osal_snv_read( id, 1, &len) == SUCCESS ? len : 0;
}
#endif
