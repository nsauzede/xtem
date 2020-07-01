#!/bin/sh

#gdb -ix gdbinit_real_mode.txt -ex 'set arch i8' -ex 'target remote :1234'
#gdb -ix gdbinit_real_mode.txt -ex 'target remote :1234' -ex 'set tdesc filename target.xml'
gdb -ix gdbinit_real_mode.txt -ex 'set tdesc filename target.xml' -ex 'target remote :1234'
