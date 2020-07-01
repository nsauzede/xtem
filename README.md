# XTEM

An XT emulator with RSP binding (for remote gdb connection).

Copyright (C) 2020 Nicolas Sauzede <nsauzede@laposte.net>.

# How to build/test

First, in a terminal, execute :
```
$ make clean all && python3 rspd.py
```

Then, in a second terminal, execute :
```
./run_gdb.sh
```

You should get :
```
...
---------------------------[ STACK ]---
CCCC CCCC CCCC CCCC CCCC CCCC CCCC CCCC 
CCCC CCCC CCCC CCCC CCCC CCCC CCCC CCCC 
---------------------------[ DS:SI ]---
00000000: CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC  ................
00000010: CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC  ................
00000020: CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC  ................
00000030: CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC  ................
---------------------------[ ES:DI ]---
00000000: CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC  ................
00000010: CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC  ................
00000020: CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC  ................
00000030: CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC  ................
----------------------------[ CPU ]----
AX: 0000 BX: 0000 CX: 0000 DX: 0000
SI: 0000 DI: 0000 SP: 0000 BP: 0000
CS: FFFF DS: 0000 ES: 0000 SS: 0000

IP: 0000 EIP:00000000
CS:IP: FFFF:0000 (0xFFFF0)
SS:SP: 0000:0000 (0x00000)
SS:BP: 0000:0000 (0x00000)
OF <0>  DF <0>  IF <0>  TF <0>  SF <0>  ZF <0>  AF <0>  PF <0>  CF <0>
ID <0>  VIP <0> VIF <0> AC <0>  VM <0>  RF <0>  NT <0>  IOPL <0>
---------------------------[ CODE ]----
   0xffff0:	jmp    0xf000:0xe05b
   0xffff5:	xor    WORD PTR [bx+si],si
   0xffff7:	das    
   0xffff8:	xor    bh,BYTE PTR [bx+si]
   0xffffa:	das    
   0xffffb:	xor    WORD PTR [bx],si
   0xffffd:	add    dh,bh
   0xfffff:	lods   ax,WORD PTR ds:[si]
   0x100000:	int3   
   0x100001:	int3   
0x00000000 in ?? ()
real-mode-gdb$ 
```

# Credits

generic XT bios by Plasma (Jon Ρetrosky and Ya'akov Miles).

See here : http://www.phatcode.net/downloads.php?id=101
