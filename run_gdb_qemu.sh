#!/bin/sh

gdb -ix gdbinit_real_mode.txt -ex 'set tdesc filename target.xml' -ex 'target remote :1234'
