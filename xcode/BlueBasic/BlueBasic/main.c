//
//  main.c
//  BlueBasic
//
//  Created by tim on 7/13/14.
//  Copyright (c) 2014 tim. All rights reserved.
//

#include <stdio.h>
#include "os.h"

extern bool interpreter_setup(void);
extern void interpreter_loop(void);

char *flash_file;
unsigned char flashstore_nrpages = 8; // default 8 pages aka 16K BASIC program

int main(int argc, const char * argv[])
{
  
  // take first parameter as file name for the flashstore
  if (argc > 1) {
    flash_file = (char*) argv[1];
  } else {
    flash_file = "/tmp/flashstore";
  }
  
  // take 2nd parameter as number of flash pages (2K per page)
  if (argc > 2) {
    int i = atoi(argv[2]);
    if (i < 1 || i > 124) {
      printf("error: flash page count out of range (4-124)\n"
             "Usage:\n"
             "%s flashstore size\n"
             "  flashstore: file"
             "  size: number of flash pages to use (1 page equals 2KB)\n",
             argv[0]);
      exit(1);
    }
    flashstore_nrpages = i;
  }
    
  printf("%s (C) 2018-2020 Kai Scheffer and Tim\n"
         "  Build: "  __DATE__  " "   __TIME__  "\n"
         "  Flashstore: %s\n"
         "  Size: %d\n"
         , argv[0], flash_file, flashstore_nrpages);
  
  interpreter_setup();
  interpreter_loop();
  
  return 0;
}

