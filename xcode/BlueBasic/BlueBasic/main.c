//
//  main.c
//  BlueBasic
//
//  Created by tim on 7/13/14.
//  Copyright (c) 2014 tim. All rights reserved.
//

#include <stdio.h>
#include "os.h"

extern void interpreter_setup(void);
extern void interpreter_loop(void);

char *flash_file;

int main(int argc, const char * argv[])
{
  
  // take first parameter as file name for the flashstore
  if (argc > 1) {
    flash_file = (char*) argv[1];
  } else {
    flash_file = "/tmp/flashstore";
  }
  
  printf("%s (C) 2018 Kai Scheffer and Tim\n"
         "Build: "  __DATE__  " "   __TIME__  "\n"
         "Flashstore: %s\n", argv[0], flash_file);
  
  interpreter_setup();
  interpreter_loop();
  
  return 0;
}

