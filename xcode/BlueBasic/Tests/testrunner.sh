#!/bin/sh

#  testrunner.sh
#  BlueBasic
#
#  Created by tim on 7/15/14.
#  Copyright (c) 2014 tim. All rights reserved.

BLUEBASIC="$HOME/Library/Developer/Xcode/DerivedData/BlueBasic-d*/Build/Products/Release/BlueBasic"

for test in $(cat tests)
do
if [[ "$test" != !* ]]; then
    exec < $test.test
    input=""
    expected=""
    while read line
    do
      if [ "$line" = '.' ]
      then
        input=$expected
        expected=""
      else
        expected="$expected
$line"
      fi
    done
    expected=${expected:1} # remove first newline
    rm -f /tmp/flashstore
    result=$(echo "${input:1}" | $BLUEBASIC | sed '1,4d') # remove startup header
    if [ "$result" = "$expected" ]
    then
      echo "** $test: SUCCESS"
    else
      echo "** $test: FAILURE"
      echo "Result:
'$result'
  Expected:
'$expected'"
      exit 1
    fi
  fi
done
