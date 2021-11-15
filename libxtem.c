/* SPDX-License-Identifier: GPL-3.0-or-later */

#include "libxtem.h"

#include <inttypes.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <malloc.h>
#include <unistd.h>

#define NOTIMP(...)                                                            \
  do {                                                                         \
    printf("NOTIMP :%d : ", __LINE__);                                         \
    printf(__VA_ARGS__);                                                       \
  } while (0)
#define AX x->r[0].w
#define CX x->r[1].w
#define DX x->r[2].w
#define BX x->r[3].w
#define SP x->r[4].w
#define BP x->r[5].w
#define SI x->r[6].w
#define DI x->r[7].w

#define IP x->ip
#define FL x->fl

#define CS x->s.cs
#define DS x->s.ds
#define ES x->s.es
#define SS x->s.ss

typedef union
{
  uint16_t w;
  struct
  {
    uint8_t l, h;
  } b;
} reg_t;

typedef reg_t regs_t[8];

typedef struct
{
  uint16_t cs;
  uint16_t ss;
  uint16_t ds;
  uint16_t es;
} segs_t;

typedef struct
{
  regs_t r;
  segs_t s;
  uint16_t ip;
  uint16_t fl;
  unsigned char* bios;
  unsigned char* ram;
  unsigned char* membuf;
  int membuflen;
  enum
  {
    SEG_DS,
    SEG_ES
  } def_seg;
  enum
  {
    REP_NOT,
    REP_REPNZ,
    REP_REPZ
  } def_rep;
} xtem_t;

static void
xtem_reset(xtem_t* x)
{
  AX = 0x0000;
  CX = 0x0000;
  DX = 0x0480;
  BX = 0x0000;
  SP = 0x0000;
  BP = 0x0000;
  SI = 0x0000;
  DI = 0x0000;
  IP = 0xFFF0;
  FL = 0x0002;
  CS = 0xF000;
  SS = 0x0000;
  DS = 0x0000;
  ES = 0x0000;
}

#define RAM_FIRST 0x00000
#define RAM_LAST 0x80000
//#define BIOS_FIRST 0xf8000
#define BIOS_FIRST 0xf0000
#define BIOS_LAST 0xfffff
#define MEM_LAST 0xfffff

static int
xtem_load_bios(xtem_t* x, char* bios_file)
{
  int bioslen = 0;
  FILE* f = fopen(bios_file, "rb");
  if (!f) {
    perror("open bios file");
    exit(1);
  }
  fseek(f, 0, SEEK_END);
  bioslen = ftell(f);
  printf("reading bios file %" PRIu32 "\n", bioslen);
  rewind(f);
  x->bios = calloc(1, bioslen);
  fread(x->bios, bioslen, 1, f);
  fclose(f);
  return 42;
}

static xtem_t*
xtem_init()
{
  xtem_t* x = calloc(1, sizeof(xtem_t));
  xtem_reset(x);
  //	xtem_load_bios(x, "bios");
  xtem_load_bios(x, "bios64");
  x->ram = calloc(1, RAM_LAST - RAM_FIRST + 1);
  return x;
}

static int
xtem_cleanup(xtem_t* x)
{
  if (x) {
    if (x->bios) {
      free(x->bios);
    }
    if (x->ram) {
      free(x->ram);
    }
    if (x->membuf) {
      free(x->membuf);
    }
    free(x);
  }
  return 0;
}

/*	read memory without side effects
        caller expects a backdoor memory pointer in return
*/
static void
memr(xtem_t* x, void** dest, int* len, int addr)
{
  if ((addr >= RAM_FIRST) && (addr <= RAM_LAST)) {
    *dest = x->ram + addr - RAM_FIRST;
    if (addr + *len > RAM_LAST) {
      *len = RAM_LAST - addr + 1;
    }
  } else if ((addr >= BIOS_FIRST) && (addr <= BIOS_LAST)) {
    *dest = x->bios + addr - BIOS_FIRST;
    // 4 10 7 =>
    if (addr + *len > BIOS_LAST) {
      *len = BIOS_LAST - addr + 1;
    }
  } else {
    if (!x->membuf) {
      x->membuf = calloc(1, x->membuflen = *len);
    }
    if (*len > x->membuflen) {
      x->membuf = realloc(x->membuf, x->membuflen = *len);
    }
    for (int i = 0; i < *len; i++) {
      if ((addr + i) <= MEM_LAST) {
        x->membuf[i] = 0xcc;
      } else {
      }
    }
    *dest = x->membuf;
  }
}

static void
memw(xtem_t* x, void** dest, int* len, int addr)
{
  memr(x, dest, len, addr);
}

static int
parity_odd16(uint16_t val)
{
  int count = 0;
  for (int i = 0; i < 16; i++) {
    if (val & (1 << i)) {
      count++;
    }
  }
  if (count % 2) {
    return 0;
  }
  return 1;
}

#define CMPRANGE(p, a, b) ((port >= a) && (port <= b))
#define CMPRANGE0(p, a) (port == a)
char*
hint_out0(int port, int data)
{
  data = data;
  return CMPRANGE(port, 0x0000, 0x001F)
           ? "The first legacy DMA controller, often used for transfers to "
             "floppies."
         : CMPRANGE(port, 0x0020, 0x0021)
           ? "The first Programmable Interrupt Controller"
         : CMPRANGE(port, 0x0022, 0x0023)
           ? "Access to the Model-Specific Registers of Cyrix processors."
         : CMPRANGE(port, 0x0040, 0x0047)
           ? "The PIT (Programmable Interval Timer)"
         : CMPRANGE(port, 0x0060, 0x0064)
           ? "The '8042' PS/2 Controller or its predecessors, dealing with "
             "keyboards and mice."
         : CMPRANGE(port, 0x0070, 0x0071) ? "The CMOS and RTC registers"
         : CMPRANGE(port, 0x0080, 0x008F) ? "The DMA (Page registers)"
         : CMPRANGE0(port, 0x0092)
           ? "The location of the fast A20 gate register"
         : CMPRANGE(port, 0x00A0, 0x00A1) ? "The second PIC"
         : CMPRANGE(port, 0x00C0, 0x00DF)
           ? "The second DMA controller, often used for soundblasters"
         : CMPRANGE0(port, 0x00E9)
           ? "Home of the Port E9 Hack. Used on some emulators to directly "
             "send text to the hosts' console."
         : CMPRANGE(port, 0x0170, 0x0177)
           ? "The secondary ATA harddisk controller."
         : CMPRANGE(port, 0x01F0, 0x01F7)
           ? "The primary ATA harddisk controller."
         : CMPRANGE(port, 0x0278, 0x027A) ? "Parallel port"
         : CMPRANGE(port, 0x02F8, 0x02FF) ? "Second serial port"
         : CMPRANGE(port, 0x03B0, 0x03DF)
           ? "The range used for the IBM VGA, its direct predecessors, as well "
             "as any modern video card in legacy mode."
         : CMPRANGE(port, 0x03F0, 0x03F7) ? "Floppy disk controller"
         : CMPRANGE(port, 0x03F8, 0x03FF) ? "First serial port"
                                          : "???";
}
char*
hint_out(int port, int data)
{
  data = data;
  return CMPRANGE(port, 0x0000, 0x001F)
           ? "first legacy DMA controller, floppies"
         : CMPRANGE(port, 0x0020, 0x0021)
           ? "first Programmable Interrupt Controller"
         : CMPRANGE(port, 0x0022, 0x0023)
           ? "Model-Specific Registers of Cyrix processors"
         : CMPRANGE(port, 0x0040, 0x0047) ? "PIT (Programmable Interval Timer)"
         : CMPRANGE(port, 0x0060, 0x0064)
           ? "'8042' PS/2 Controller, keyboards and mice"
         : CMPRANGE(port, 0x0070, 0x0071) ? "CMOS and RTC registers"
         : CMPRANGE(port, 0x0080, 0x008F) ? "DMA (Page registers)"
         : CMPRANGE0(port, 0x0092)        ? "fast A20 gate register"
         : CMPRANGE(port, 0x00A0, 0x00A1) ? "second PIC"
         : CMPRANGE(port, 0x00C0, 0x00DF)
           ? "second DMA controller, soundblasters"
         : CMPRANGE0(port, 0x00E9)        ? "Port E9 Hack"
         : CMPRANGE(port, 0x0170, 0x0177) ? "secondary ATA harddisk controller"
         : CMPRANGE(port, 0x01F0, 0x01F7) ? "primary ATA harddisk controller"
         : CMPRANGE(port, 0x0278, 0x027A) ? "Parallel port"
         : CMPRANGE(port, 0x02F8, 0x02FF) ? "Second serial port"
         : CMPRANGE(port, 0x03B0, 0x03DF) ? "VGA"
         : CMPRANGE(port, 0x03F0, 0x03F7) ? "Floppy disk controller"
         : CMPRANGE(port, 0x03F8, 0x03FF) ? "First serial port"
                                          : "???";
}
// return : 0 => executed 1 insn succesfully
// return : 1 => executed 1 prefix succesfully (eg: not atomic for IRQ handling)
// return : <0 => error
static int
step(xtem_t* x)
{
  int ret = 0;
  uint8_t *_opc = 0, *opc;
  int len = 8;
  int pc = CS * 16 + IP;
  printf("%05x ", pc);
  memr(x, (void**)&_opc, &len, pc);
  if (!_opc) {
    return 1;
  }
  opc = _opc;
  uint8_t Ib, Eb, Ev;
  uint16_t Iw;
  uint16_t seg;
  int pfx_seg = 0;
  int pfx_rep = 0;
  uint8_t mod, reg, rm;
#if 1
  uint16_t* mem;
  int addr;
  uint16_t Gv;
#endif
#define OF 0x800
#define DF 0x400
#define IF 0x200
#define TF 0x100
#define SF 0x080
#define ZF 0x040
#define AF 0x010
#define PF 0x004
#define CF 0x001
  //   printf("RIGHT NOW DS=%04" PRIx16 "\n", DS);
  while (1) {
    switch (opc[0]) {
      case 0x26: //	ES:
        IP++;
        printf("ES:\n");
        x->def_seg = SEG_ES;
        pfx_seg = 1;
        break;
      case 0xF3: //	REPZ:
        IP++;
        printf("REPZ:\n");
        x->def_rep = REP_REPZ;
        pfx_rep = 1;
        break;
      case 0x33: //	XOR		Gv	Ev
        IP++;
        Ev = *(uint8_t*)(opc + 1);
        IP++;
        printf("XOR		Gv	Ev\n");
        switch (Ev) {
          case 0xC0: // ax,ax
            Iw = AX = AX ^ AX;
            break;
          case 0xFF: // di,di
            Iw = DI = DI ^ DI;
            break;
          default:
            ret = -6;
            NOTIMP("Ev=%02" PRIx8 "\n", Ev);
            break;
        }
        if (Iw == 0) {
          FL |= ZF;
        } else {
          FL &= ~ZF;
        }
        if (parity_odd16(Iw)) {
          FL |= PF;
        } else {
          FL &= ~PF;
        }
        break;
      // PC=fe0cd OPC=3B15
      // 001110 1 1 00 010 101
      // mod=0 reg=2 rm=5
      // 000FE0CD  3B15              cmp    dx,WORD PTR [di],
      case 0x3B: //	CMP		REG16,REG16/MEM16
        mod = (opc[1] & 0xc0) >> 6;
        reg = (opc[1] & 0x38) >> 3;
        rm = opc[1] & 0x07;
        mem = 0;
        len = 2;
        printf("CMP		REG16/MEM16,REG16\n");
        switch (mod) {
          case 0x0: // memory mode, no displacement follows except rm==6
            switch (rm) {
              case 0x5: //(di)
                printf("USING SEG %s\n", x->def_seg == SEG_ES ? "ES" : "DS");
                addr = (x->def_seg == SEG_ES ? ES : DS) * 16 + DI;
                memr(x, (void**)&mem, &len, addr);
                if (!mem) {
                  NOTIMP("Failed to acquire mem\n");
                  return -1;
                }
                break;
              default:
                ret = -5;
                NOTIMP("???? mod=%01" PRIx8 " reg=%01" PRIx8 " rm=%01" PRIx8
                       "\n",
                       mod,
                       reg,
                       rm);
                break;
            }
            break;
          default:
            ret = -5;
            NOTIMP("mod=%02" PRIx8 "\n", mod);
            break;
        }
        if (mem) {
          uint16_t regv;
          switch (reg) {
            case 0x2:
              regv = DX;
              break;
            default:
              ret = -5;
              NOTIMP("???? mod=%01" PRIx8 " reg=%01" PRIx8 " rm=%01" PRIx8 "\n",
                     mod,
                     reg,
                     rm);
              break;
          }
          if (!ret) {
            IP += 2;
            if (*((uint16_t*)mem) == regv) {
              FL |= ZF;
            } else {
              FL &= ~ZF;
            }
          }
        } else {
          ret = -1;
          NOTIMP("!mem\n");
          break;
        }
        break;
      case 0x40: //	INC		eAX
        IP++;
        printf("INC		AX\n");
        if (AX == 0xff) {
          FL |= AF;
        } else {
          FL &= ~AF;
        }
        if (parity_odd16(AX)) {
          FL |= PF;
        } else {
          FL &= ~PF;
        }
        AX++;
        break;
#if 0
		case 0x4e://	DEC		eSI
			IP++;
			printf("DEC		eSI\n");
			SI--;
			break;
#endif
      case 0x75: //	JNZ		Ib
        IP++;
        Ib = *(uint8_t*)(opc + 1);
        IP++;
        printf("JNZ		Ib\n");
        if (!(FL & ZF)) {
          IP += Ib;
        }
        break;
#if 1
      // PC=fe0ca OPC=89 15
      // 100010 0 1 00 010 101
      // mod=0 reg=2 rm=5
      // 000FE0CA  8915              mov    WORD PTR [di],dx
      case 0x89: //	MOV		REG16/MEM16,REG16
        mod = (opc[1] & 0xc0) >> 6;
        reg = (opc[1] & 0x38) >> 3;
        rm = opc[1] & 0x07;
        mem = 0;
        len = 2;
        printf("MOV		REG16/MEM16,REG16\n");
        switch (mod) {
          case 0x0: // memory mode, no displacement follows except rm==6
            switch (rm) {
              case 0x5: //(di)
                printf("USING SEG %s\n", x->def_seg == SEG_ES ? "ES" : "DS");
                addr = (x->def_seg == SEG_ES ? ES : DS) * 16 + DI;
                memw(x, (void**)&mem, &len, addr);
                if (!mem) {
                  NOTIMP("Failed to acquire mem\n");
                  return -1;
                }
                break;
              default:
                ret = -5;
                NOTIMP("???? mod=%01" PRIx8 " reg=%01" PRIx8 " rm=%01" PRIx8
                       "\n",
                       mod,
                       reg,
                       rm);
                break;
            }
            break;
#if 0
				case 0x1://memory mode, disp8 follows
					break;
				case 0x2://memory mode, disp16 follows
					break;
				case 0x3://register mode, no displacement follows
					break;
#endif
          default:
            ret = -5;
            NOTIMP("mod=%02" PRIx8 "\n", mod);
            break;
        }
        if (mem) {
          uint16_t regv;
          switch (reg) {
            case 0x2:
              regv = DX;
              break;
            default:
              ret = -5;
              NOTIMP("???? mod=%01" PRIx8 " reg=%01" PRIx8 " rm=%01" PRIx8 "\n",
                     mod,
                     reg,
                     rm);
              break;
          }
          if (!ret) {
            IP += 2;
            *((uint16_t*)mem) = regv;
          }
        } else {
          ret = -1;
          NOTIMP("!mem\n");
          break;
        }
        break;
#endif
#if 1
      //               d w mod reg r/m
      // 8B36 : 100010 1 1  00 110 110
      // 8BE8 : 100010 1 1  11 011 000
      // 8B367200          mov si,[0x72]
      // 8BE8              mov bp,ax
      case 0x8B: //	MOV		REG16,REG16
        mod = (opc[1] & 0xc0) >> 6;
        reg = (opc[1] & 0x38) >> 3;
        rm = opc[1] & 0x07;
        printf("MOV		REG16/MEM16,REG16\n");
        if (mod == 0x00) {
          Ev = *(uint8_t*)(opc + 1);
          Gv = *(uint16_t*)(opc + 2);
          mem = 0;
          len = 2;
          printf("USING SEG %s\n", x->def_seg == SEG_ES ? "ES" : "DS");
          addr = (x->def_seg == SEG_ES ? ES : DS) * 16 + Gv;
          memr(x, (void**)&mem, &len, addr);
          if (!mem) {
            NOTIMP("Failed to acquire mem\n");
            return -1;
          }
          switch (reg) {
            case 0x06:
              SI = *mem;
              break;
            default:
              ret = -5;
              NOTIMP("reg=%02" PRIx8 "\n", reg);
              break;
          }
          if (!ret) {
            IP += 4;
          }
        } else if (mod == 0x03) {
          switch (rm) {
            case 0x00:
              Iw = AX;
              break;
            default:
              ret = -5;
              NOTIMP("rm=%02" PRIx8 "\n", rm);
              break;
          }
          switch (reg) {
            case 0x00:
              AX = Iw;
              break;
            case 0x01:
              CX = Iw;
              break;
            case 0x02:
              DX = Iw;
              break;
            case 0x03:
              BX = Iw;
              break;
            case 0x04:
              SP = Iw;
              break;
            case 0x05:
              BP = Iw;
              break;
            case 0x06:
              SI = Iw;
              break;
            case 0x07:
              DI = Iw;
              break;
            default:
              ret = -5;
              NOTIMP("reg=%02" PRIx8 "\n", reg);
              break;
          }
          if (!ret) {
            IP += 2;
          }
        } else {
          ret = -5;
          NOTIMP("mod=%02" PRIx8 "\n", mod);
        }
        break;
#endif
#if 1
      case 0x8E: //	MOV		Sw	Ew
        IP++;
        uint16_t Sw = (*(uint16_t*)(opc + 1) & 0xF0) >> 4;
        uint16_t Ew = *(uint16_t*)(opc + 1) & 0x0F;
        IP++;
        printf("MOV		Sw	Ew\n");
        uint16_t regv;
        switch (Ew) {
          case 0x3: // bx
            regv = BX;
            break;
          case 0x8: // ax
            regv = AX;
            break;
          default:
            ret = -5;
            NOTIMP("Ew=%01" PRIx8 "\n", Ew);
            break;
        }
        switch (Sw) {
          case 0xC: // es
            printf("SETTING ES=%04" PRIx16 "\n", regv);
            ES = regv;
            break;
          case 0xD: // ds
            printf("SETTING DS=%04" PRIx16 "\n", regv);
            DS = regv;
            break;
          default:
            ret = -4;
            NOTIMP("Sw=%01" PRIx8 "\n", Sw);
            break;
        }
        break;
#endif
      case 0x90: //	NOP
        IP++;
        printf("NOP\n");
        break;
      case 0xAB: //	STOSW
        IP++;
        printf("STOSW\n");
        printf("USING REP %s\n",
               x->def_rep == REP_REPNZ  ? "REPNZ"
               : x->def_rep == REP_REPZ ? "REPZ"
                                        : "REP");
        mem = 0;
        len = 2;
        addr = ES * 16 + DI;
        memw(x, (void**)&mem, &len, addr);
        if (!mem) {
          NOTIMP("Failed to acquire mem\n");
          return -1;
        }
        while (1) {
          *((uint16_t*)mem) = AX;
          DI += 2;
          if (x->def_rep == REP_NOT) {
            break;
          }
          CX--;
          if (((x->def_rep == REP_REPNZ) && (!CX)) ||
              ((x->def_rep == REP_REPZ) && (!CX))) {
            break;
          }
        }
        //			ret = -1;
        break;
      case 0xB0 ... 0xB7: //	MOV		Reg8	Ib
        IP++;
        reg = *(uint8_t*)(opc + 0) & 0x7;
        Ib = *(uint8_t*)(opc + 1);
        IP++;
        printf(
          "MOV		Reg8=%01" PRIx8 "	Ib=%02" PRIx8 "\n", reg, Ib);
        switch (reg) {
          case 0x0:
            AX &= 0xff00;
            AX |= Ib;
            break;
          case 0x1:
            CX &= 0xff00;
            CX |= Ib;
            break;
          case 0x2:
            DX &= 0xff00;
            DX |= Ib;
            break;
          case 0x3:
            BX &= 0xff00;
            BX |= Ib;
            break;
          case 0x4:
            AX &= 0xff;
            AX |= Ib << 8;
            break;
          case 0x5:
            CX &= 0xff;
            CX |= Ib << 8;
            break;
          case 0x6:
            DX &= 0xff;
            DX |= Ib << 8;
            break;
          case 0x7:
            BX &= 0xff;
            BX |= Ib << 8;
            break;
        }
        break;
      case 0xB8 ... 0xBF: //	MOV		Reg16	Iw
        IP++;
        reg = *(uint8_t*)(opc + 0) & 0x7;
        Iw = *(uint16_t*)(opc + 1);
        IP += 2;
        printf(
          "MOV		Reg16=%01" PRIx8 "	Ib=%04" PRIx16 "\n", reg, Iw);
        switch (reg) {
          case 0x0:
            AX = Iw;
            break;
          case 0x1:
            CX = Iw;
            break;
          case 0x2:
            DX = Iw;
            break;
          case 0x3:
            BX = Iw;
            break;
          case 0x4:
            SP = Iw;
            break;
          case 0x5:
            BP = Iw;
            break;
          case 0x6:
            SI = Iw;
            break;
          case 0x7:
            DI = Iw;
            break;
        }
        break;
      case 0xE6: //	OUT		Ib	AL
        IP++;
        Ib = *(uint8_t*)(opc + 1);
        IP++;
        printf("OUT Ib=%02" PRIx8 " AL=%02" PRIx8 "\t\t[%s]\n",
               Ib,
               AX & 0xff,
               hint_out(Ib, AX & 0xff));
        break;
      case 0xea: //	JMP		Ap
        IP++;
        uint16_t ofs = *(uint16_t*)(opc + 1);
        seg = *(uint16_t*)(opc + 3);
        printf("JMP		Ap=%04" PRIx16 ":%04" PRIx16 "\n", seg, ofs);
        CS = seg;
        IP = ofs;
        break;
      case 0xEE: //	OUT		DX	AL
        IP++;
        printf("OUT DX=%04" PRIx16 " AL=%02" PRIx8 "\t[%s]\n",
               DX,
               AX & 0xff,
               hint_out(DX, AX & 0xff));
        break;
      case 0xfa: //	CLI
        IP++;
        printf("CLI\n");
        FL &= ~(1 << 9);
        break;
      case 0xfc: //	CLD
        IP++;
        printf("CLD\n");
        FL &= ~(1 << 10);
        break;
      case 0xFE: //	GRP4	Eb
        IP++;
        Eb = *(uint8_t*)(opc + 1);
        IP++;
        printf("GRP4	Eb=%02" PRIx8 " : ", Eb);
        switch (Eb) {
          case 0xC0: // GRP4/0	INC
            printf("INC		Al\n");
            AX = (AX & 0xff00) + (((AX & 0xff) + 1));
            break;
          default:
            ret = -3;
            NOTIMP("GRP4/%01" PRIx8 "\n", Eb & 0xf);
            break;
        }
        break;
      default:
        ret = -2;
        NOTIMP("PC=%05" PRIx32 " OPC=%02" PRIx8 " %02" PRIx8 " %02" PRIx8
               " %02" PRIx8 "\n",
               pc,
               opc[0],
               opc[1],
               opc[2],
               opc[3]);
        break;
    }
    break;
  }
  if (!pfx_seg) {
    x->def_seg = SEG_DS;
  } else {
    if (!ret) {
      ret = 1;
    }
  }
  if (!pfx_rep) {
    x->def_rep = REP_NOT;
  } else {
    if (!ret) {
      ret = 1;
    }
  }
  return ret;
}

typedef struct
{
  xtem_t* x;
} rsp_t;

void*
xtem_rsp_init()
{
  rsp_t* r = calloc(1, sizeof(rsp_t));
  r->x = xtem_init();
  return r;
}

int
xtem_rsp_cleanup(void* r_)
{
  rsp_t* r = (rsp_t*)r_;
  if (r) {
    xtem_cleanup(r->x);
    free(r);
  }
  return 0;
}

int
xtem_rsp_s(void* r_)
{
  rsp_t* r = (rsp_t*)r_;
  while (1) {
    int n = step(r->x);
    if (n == 1) {
      continue; // was a prefix
    } else if (n == 0) {
      break; // was an atomic insn
    } else {
      printf("%s: an error ? n=%d\n", __func__, n);
      // exit(1); // was an error
      return n;
    }
  }
  return 42;
}

int
xtem_rsp_c(void* r_)
{
  rsp_t* r = (rsp_t*)r_;
  int ret = 0;
  while (1) {
    ret = step(r->x);
    if (ret < 0) {
      break;
    }
  }
  return ret;
}

int
xtem_rsp_g(void* r_, char* data)
{
  rsp_t* r = (rsp_t*)r_;
  int len = strlen(data);
  //	printf("len=%d\n", len);
  int pos = 0;
  do {
#define WR_REG16(reg16)                                                        \
  do {                                                                         \
    if (pos + 2 > len)                                                         \
      break;                                                                   \
    pos += snprintf(data + pos, 2 + 1, "%02" PRIx8, reg16 & 0xff);             \
    if (pos + 2 > len)                                                         \
      break;                                                                   \
    pos += snprintf(data + pos, 2 + 1, "%02" PRIx8, reg16 >> 8);               \
    if (pos + 4 > len)                                                         \
      break;                                                                   \
    pos += snprintf(data + pos, 4 + 1, "%04" PRIx16, 0);                       \
  } while (0)

    WR_REG16(r->AX);
    WR_REG16(r->CX);
    WR_REG16(r->DX);
    WR_REG16(r->BX);
    WR_REG16(r->SP);
    WR_REG16(r->BP);
    WR_REG16(r->SI);
    WR_REG16(r->DI);
    WR_REG16(r->IP);
    WR_REG16(r->FL);
    WR_REG16(r->CS);
    WR_REG16(r->SS);
    WR_REG16(r->DS);
    WR_REG16(r->ES);
    WR_REG16(0); // fs
    WR_REG16(0); // gs
  } while (0);
  for (; pos < len; pos++) {
    data[pos] = '0';
  }
  // printf("Hello data=%s\n", data);
  //	printf("Hello pos=%d data=%s\n", pos, data);
  // getchar();
  return 42;
}

int
xtem_rsp_m(void* r_, char* data, int addr, int len)
{
  rsp_t* r = (rsp_t*)r_;
  unsigned char* buf = 0;
  memr(r->x, (void**)&buf, &len, addr);
  if (buf) {
    for (int i = 0; i < len; i++) {
      sprintf(data + 2 * i, "%02x", buf[i]);
    }
  }
  return 42;
}

#include "librspd.h"
typedef struct
{
  void* x;
  void* r;
  int intr;
  int kill;
} lx_t;

static int xlen = 32;
static int
rsp_question(void* lx_)
{
  lx_t* lx = (lx_t*)lx_;
  char* buf = "S05";
  rsp_send(lx->r, buf, strlen(buf));
  return 0;
}

#define LEN64 (560 * 2)
#define LEN32 (312 * 2)

static int
rsp_get_regs(void* lx_)
{
  lx_t* lx = (lx_t*)lx_;
  char buf[LEN64 + 1];
  int len = xlen == 64 ? LEN64 : LEN32;
  memset(buf, '0', len);
  xtem_rsp_g(&lx->x, buf);
  rsp_send(lx->r, buf, len);
  return 0;
}

static int
rsp_read_mem(void* lx_, size_t addr, size_t len_)
{
  lx_t* lx = (lx_t*)lx_;
  int len = len_;
  char* data = malloc(len * 2 + 1);
  memset(data, '0', len * 2);
  xtem_rsp_m(&lx->x, data, addr, len);
  rsp_send(lx->r, data, len * 2);
  free(data);
  return 0;
}

static int
rsp_stepi(void* lx_)
{
  lx_t* lx = (lx_t*)lx_;
  int ret = 0;
  while (1) {
    if (lx->intr) {
      lx->intr = 0;
      break;
    }
    printf("%s: doing step..\n", __func__);
    int n = step(lx->x);
    printf("%s: step returned %d\n", __func__, n);
    if (n == 1) {
      continue; // was a prefix
    } else if (n == 0) {
      break; // was an atomic insn
    } else {
      printf("%s: an error ? n=%d\n", __func__, n);
      // exit(1); // was an error
      ret = n;
      break;
    }
  }
  rsp_question(lx_);
  return ret;
}

static int
rsp_cont(void* lx_)
{
  lx_t* lx = (lx_t*)lx_;
  int ret = 0;
  while (1) {
    if (lx->intr) {
      lx->intr = 0;
      break;
    }
    ret = step(lx->x);
    if (ret < 0) {
      break;
    }
  }
  rsp_question(lx_);
  printf("%s: returning %d\n", __func__, ret);
  return ret;
}

static int
rsp_kill(void* lx_)
{
  lx_t* lx = (lx_t*)lx_;

  printf("%s: KILL!!!!!!!!!!!!!\n", __func__);
  lx->kill = 1;
  return 0;
}

static int
rsp_intr(void* lx_)
{
  lx_t* lx = (lx_t*)lx_;

  printf("%s: INTR!!!!!!!!!!!!!\n", __func__);
  lx->intr = 1;
  return 0;
}

static int
lx_dumpregs(xtem_t* x)
{
  //   uint32_t PC = (CS << 4) + IP;
  //   printf("PC=%08" PRIX32 "\n", PC);
  printf("AX: %04" PRIX16 " ", AX);
  printf("BX: %04" PRIX16 " ", BX);
  printf("CX: %04" PRIX16 " ", CX);
  printf("DX: %04" PRIX16 " ", DX);
  printf("\n");
  printf("SI: %04" PRIX16 " ", SI);
  printf("DI: %04" PRIX16 " ", DI);
  printf("SP: %04" PRIX16 " ", SP);
  printf("BP: %04" PRIX16 " ", BP);
  printf("\n");
  printf("CS: %04" PRIX16 " ", CS);
  printf("DS: %04" PRIX16 " ", DS);
  printf("ES: %04" PRIX16 " ", ES);
  printf("SS: %04" PRIX16 " ", SS);
  printf("\n");
  return 0;
}

void*
libxtem_init(int rsp_port)
{
  lx_t* res = calloc(1, sizeof(lx_t));
  printf("%s: lx=%p\n", __func__, res);
  res->x = xtem_init();
  if (rsp_port) {
    res->r = rsp_init(&(rsp_init_t){
      .user = res,
      .port = rsp_port,
      .question = rsp_question,
      .get_regs = rsp_get_regs,
      .read_mem = rsp_read_mem,
      .stepi = rsp_stepi,
      .cont = rsp_cont,
      .kill = rsp_kill,
      .intr = rsp_intr,
    });
  }
  return res;
}

int
libxtem_cleanup(void* lx_)
{
  lx_t* lx = (lx_t*)lx_;
  if (lx) {
    rsp_cleanup(lx->r);
    xtem_cleanup(lx->x);
  }
  return 0;
}

int
libxtem_execute(void* lx_)
{
  lx_t* lx = (lx_t*)lx_;
  xtem_t* x = lx->x;
  if (lx->r)
    return rsp_execute(lx->r);
  else {
    while (1) {
      //   uint32_t PC = (CS << 4) + IP;
      //   printf("PC=%08" PRIX32 "\n", PC);
      lx_dumpregs(x);
      int n = step(x);
      if (n != 1 && n != 0)
        return 1;
    }
    //  return PC == 0xfe05d;
    //   sleep(1);
    return 0;
  }
}
