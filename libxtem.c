/* SPDX-License-Identifier: GPL-3.0-or-later */

#include <inttypes.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <malloc.h>

#define NOTIMP(...) do{printf("NOTIMP :%d : ", __LINE__);printf(__VA_ARGS__);}while(0)

typedef struct {
	uint16_t ax;
	uint16_t cx;
	uint16_t dx;
	uint16_t bx;
	uint16_t sp;
	uint16_t bp;
	uint16_t si;
	uint16_t di;
	uint16_t ip;
	uint16_t fl;
	uint16_t cs;
	uint16_t ss;
	uint16_t ds;
	uint16_t es;
} regs_t;

typedef struct {
	regs_t r;
	unsigned char *bios;
	unsigned char *ram;
} xtem_t;

static void xtem_reset(xtem_t *x) {
	x->r.ax = 0x0000;
	x->r.cx = 0x0000;
	x->r.dx = 0x0480;
	x->r.bx = 0x0000;
	x->r.sp = 0x0000;
	x->r.bp = 0x0000;
	x->r.si = 0x0000;
	x->r.di = 0x0000;
	x->r.ip = 0xFFF0;
	x->r.fl = 0x0002;
	x->r.cs = 0xF000;
	x->r.ss = 0x0000;
	x->r.ds = 0x0000;
	x->r.es = 0x0000;
}

#define RAM_FIRST 0x00000
#define RAM_LAST 0x80000
//#define BIOS_FIRST 0xf8000
#define BIOS_FIRST 0xf0000
#define BIOS_LAST 0xfffff
#define MEM_LAST 0xfffff

static int xtem_load_bios(xtem_t *x, char *bios_file) {
	int bioslen = 0;
	FILE *f = fopen(bios_file, "rb");
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

static xtem_t *xtem_init() {
	xtem_t *x = calloc(1, sizeof(xtem_t));
	xtem_reset(x);
//	xtem_load_bios(x, "bios");
	xtem_load_bios(x, "bios64");
	x->ram = calloc(1, RAM_LAST - RAM_FIRST + 1);
	return x;
}

static unsigned char *membuf = 0;
static int membuflen = 0;
/*	read memory without side effects
	caller expects a backdoor memory pointer in return
*/
static void memr(xtem_t *x, void **dest, int *len, int addr) {
	x = x;
	if ((addr >= RAM_FIRST) && (addr <= RAM_LAST)) {
		*dest = x->ram + addr - RAM_FIRST;
		if (addr + *len > RAM_LAST) {
			*len = RAM_LAST - addr + 1;
		}
	} else if ((addr >= BIOS_FIRST) && (addr <= BIOS_LAST)) {
		*dest = x->bios + addr - BIOS_FIRST;
		//4 10 7 => 
		if (addr + *len > BIOS_LAST) {
			*len = BIOS_LAST - addr + 1;
		}
	} else {
		if (!membuf) {
			membuf = calloc(1, membuflen = *len);
		}
		if (*len > membuflen) {
			membuf = realloc(membuf, membuflen = *len);
		}
		for (int i = 0; i < *len; i++) {
			if ((addr + i) <= MEM_LAST) {
				membuf[i] = 0xcc;
			} else {
			}
		}
		*dest = membuf;
	}
}

static void memw(xtem_t *x, void **dest, int *len, int addr) {
	memr(x, dest, len, addr);
}

static int step(xtem_t *x) {
	int ret = 0;
	uint8_t *_opc = 0, *opc;
	int len = 8;
	int pc = x->r.cs * 16 + x->r.ip;
	printf("%05x ", pc);
	memr(x, (void **)&_opc, &len, pc);
	if (!_opc) {
		return 1;
	}
	opc = _opc;
	uint8_t Ib, Eb, Ev, reg;
	uint16_t Iw;
	uint16_t seg;
#if 1
	uint16_t *mem;
	int addr;
	uint16_t Gv;
#endif
	enum {SEG_DS, SEG_ES} def_seg = SEG_DS;
	while (1) {
	switch (opc[0]) {
		case 0x26://	ES:
			x->r.ip++;
			printf("ES:\n");
			def_seg = SEG_ES;
			opc++;
			continue;
		case 0x33://	XOR		Gv	Ev
			x->r.ip++;
			Ev = *(uint8_t *)(opc + 1);
			x->r.ip++;
			printf("XOR		Gv	Ev\n");
			switch (Ev) {
				case 0xC0://ax,ax
					x->r.ax = x->r.ax ^ x->r.ax;
					break;
				case 0xFF://di,di
					x->r.di = x->r.di ^ x->r.di;
					break;
				default:
					ret = 6;
					NOTIMP("Ev=%02" PRIx8 "\n", Ev);
					break;
			}
			break;
		case 0x40://	INC		eAX
			x->r.ip++;
			printf("INC		AX\n");
			x->r.ax++;
			break;
#if 0
		case 0x4e://	DEC		eSI
			x->r.ip++;
			printf("DEC		eSI\n");
			x->r.si--;
			break;
#endif
#if 1
		// PC=fe0ca OPC=89 15
		// 100010 0 1 00 010 101
		// mod=0 reg=2 rm=5
		// 000FE0CA  8915              mov    WORD PTR [di],dx
		case 0x89://	MOV		REG16/MEM16,REG16
		{
			x->r.ip++;
			uint8_t mod = (opc[1] & 0xc0) >> 6;
			uint8_t reg = (opc[1] & 0x38) >> 3;
			uint8_t rm = opc[1] & 0x07;
			x->r.ip++;
			mem = 0;
			len = 2;
			switch (mod) {
				case 0x0://memory mode, no displacement follows except rm==6
					switch (rm) {
						case 0x5://(di)
							addr = (def_seg == SEG_ES ? x->r.es : x->r.ds) * 16 + x->r.di;
							memw(x, (void **)&mem, &len, addr);
							if (!mem) {
								return 1;
							}
							break;
						default:
							ret = 5;
							NOTIMP("???? mod=%01" PRIx8 " reg=%01" PRIx8 " rm=%01" PRIx8 "\n", mod, reg, rm);
							break;
					}
					break;
				case 0x1://memory mode, disp8 follows
					break;
				case 0x2://memory mode, disp16 follows
					break;
				case 0x3://register mode, no displacement follows
					break;
			}
			if (mem) {
				uint16_t regv;
				switch (reg) {
					case 0x2:
						regv = x->r.dx;
						break;
					default:
						ret = 5;
						NOTIMP("???? mod=%01" PRIx8 " reg=%01" PRIx8 " rm=%01" PRIx8 "\n", mod, reg, rm);
						break;
				}
				*((uint16_t *)mem) = regv;
			}
			break;
		}
#endif
#if 1
		// 8B367200          mov si,[0x72]
		// 8BE8              mov bp,ax
		case 0x8B://	MOV		Gv	Ev
			x->r.ip++;
			Gv = *(uint16_t *)(opc + 2);
			Ev = *(uint8_t *)(opc + 1);
			x->r.ip += 3;
			mem = 0;
			len = 2;
			addr = x->r.ds * 16 + Gv;
			memr(x, (void **)&mem, &len, addr);
			if (!mem) {
				return 1;
			}
			printf("MOV		Gv=%04" PRIx16 " Ev=%01" PRIx8 "\n", Gv, Ev);
			switch (Ev) {
				case 0x36:
					x->r.si = *mem;
					break;
				case 0xe8:
					x->r.bp = x->r.ax;
					break;
				default:
					ret = 5;
					NOTIMP("Ev=%02" PRIx8 "\n", Ev);
					break;
			}
			break;
#endif
#if 1
		case 0x8E://	MOV		Sw	Ew
			x->r.ip++;
			uint16_t Sw = (*(uint16_t *)(opc + 1) & 0xF0) >> 4;
			uint16_t Ew = *(uint16_t *)(opc + 1) & 0x0F;
			x->r.ip++;
			printf("MOV		Sw	Ew\n");
			uint16_t regv;
			switch (Ew) {
				case 0x3://bx
					regv = x->r.bx;
					break;
				case 0x8://ax
					regv = x->r.ax;
					break;
				default:
					ret = 5;
					NOTIMP("Ew=%01" PRIx8 "\n", Ew);
					break;
			}
			switch (Sw) {
				case 0xC://es
					x->r.es = regv;
					break;
				case 0xD://ds
					x->r.ds = regv;
					break;
				default:
					ret = 4;
					NOTIMP("Sw=%01" PRIx8 "\n", Sw);
					break;
			}
			break;
#endif
		case 0x90://	NOP
			x->r.ip++;
			printf("NOP\n");
			break;
		case 0xB0 ... 0xB7://	MOV		Reg8	Ib
			x->r.ip++;
			reg = *(uint8_t *)(opc + 0) & 0x7;
			Ib = *(uint8_t *)(opc + 1);
			x->r.ip++;
			printf("MOV		Reg8=%01" PRIx8 "	Ib=%02" PRIx8 "\n", reg, Ib);
			switch (reg) {
				case 0x0:
					x->r.ax &= 0xff00;x->r.ax |= Ib;
					break;
				case 0x1:
					x->r.cx &= 0xff00;x->r.cx |= Ib;
					break;
				case 0x2:
					x->r.dx &= 0xff00;x->r.dx |= Ib;
					break;
				case 0x3:
					x->r.bx &= 0xff00;x->r.bx |= Ib;
					break;
				case 0x4:
					x->r.ax &= 0xff;x->r.ax |= Ib << 8;
					break;
				case 0x5:
					x->r.cx &= 0xff;x->r.cx |= Ib << 8;
					break;
				case 0x6:
					x->r.dx &= 0xff;x->r.dx |= Ib << 8;
					break;
				case 0x7:
					x->r.bx &= 0xff;x->r.bx |= Ib << 8;
					break;
			}
			break;
		case 0xB8 ... 0xBF://	MOV		Reg16	Iw
			x->r.ip++;
			reg = *(uint8_t *)(opc + 0) & 0x7;
			Iw = *(uint16_t *)(opc + 1);
			x->r.ip += 2;
			printf("MOV		Reg16=%01" PRIx8 "	Ib=%04" PRIx16 "\n", reg, Iw);
			switch (reg) {
				case 0x0:
					x->r.ax = Iw;
					break;
				case 0x1:
					x->r.cx = Iw;
					break;
				case 0x2:
					x->r.dx = Iw;
					break;
				case 0x3:
					x->r.bx = Iw;
					break;
				case 0x4:
					x->r.sp = Iw;
					break;
				case 0x5:
					x->r.bp = Iw;
					break;
				case 0x6:
					x->r.si = Iw;
					break;
				case 0x7:
					x->r.di = Iw;
					break;
			}
			break;
		case 0xE6://	OUT		Ib	AL
			x->r.ip++;
			Ib = *(uint8_t *)(opc + 1);
			x->r.ip++;
			printf("OUT Ib=%02" PRIx8 " AL ???\n", Ib);
			break;
		case 0xea://	JMP		Ap
			x->r.ip++;
			uint16_t ofs = *(uint16_t *)(opc + 1);
			seg = *(uint16_t *)(opc + 3);
			printf("JMP		Ap=%04" PRIx16 ":%04" PRIx16 "\n", seg, ofs);
			x->r.cs = seg;
			x->r.ip = ofs;
			break;
		case 0xEE://	OUT		DX	AL
			x->r.ip++;
			printf("OUT DX=%04" PRIx16 " AL ???\n", x->r.dx);
			break;
		case 0xfa://	CLI
			x->r.ip++;
			printf("CLI\n");
			x->r.fl &= ~(1 << 9);
			break;
		case 0xfc://	CLD
			x->r.ip++;
			printf("CLD\n");
			x->r.fl &= ~(1 << 10);
			break;
		case 0xFE://	GRP4	Eb
			x->r.ip++;
			Eb = *(uint8_t *)(opc + 1);
			x->r.ip++;
			printf("GRP4	Eb=%02" PRIx8 " : ", Eb);
			switch (Eb) {
				case 0xC0: //GRP4/0	INC
					printf("INC		Al\n");
					x->r.ax = (x->r.ax & 0xff00) + (((x->r.ax & 0xff) + 1));
					break;
				default:
					ret = 3;
					NOTIMP("GRP4/%01" PRIx8 "\n", Eb & 0xf);
					break;
			}
			break;
		default:
			ret = 2;
			NOTIMP("PC=%05" PRIx32 " OPC=%02" PRIx8 " %02" PRIx8 " %02" PRIx8 " %02" PRIx8 "\n", pc, opc[0], opc[1], opc[2], opc[3]);
			break;
	}
	break;
	}
	return ret;
}

typedef struct {
	xtem_t *x;
} rsp_t;

rsp_t *xtem_rsp_init() {
	rsp_t *r = calloc(1, sizeof(rsp_t));
	r->x = xtem_init();
	return r;
}

int xtem_rsp_s(rsp_t *r) {
	if (step(r->x)) {
		exit(1);
	}
	return 42;
}

int xtem_rsp_c(rsp_t *r) {
	int ret = 0;
	while (1) {
		ret = step(r->x);
		if (ret) {
			break;
		}
	}
	return ret;
}

int xtem_rsp_g(rsp_t *r, char *data) {
	int len = strlen(data);
	//printf("len=%d\n", len);
	int pos = 0;
	do {
#define WR_REG16(reg16) do{ \
	if (pos + 4 > len) break; \
	pos += snprintf(data + pos, 2 + 1, "%02" PRIx8, reg16 & 0xff); \
	if (pos + 4 > len) break; \
	pos += snprintf(data + pos, 2 + 1, "%02" PRIx8, reg16 >> 8); \
	if (pos + 4 > len) break; \
	pos += snprintf(data + pos, 4 + 1, "%04" PRIx16, 0); \
	} while (0)

		WR_REG16(r->x->r.ax);
		WR_REG16(r->x->r.cx);
		WR_REG16(r->x->r.dx);
		WR_REG16(r->x->r.bx);
		WR_REG16(r->x->r.sp);
		WR_REG16(r->x->r.bp);
		WR_REG16(r->x->r.si);
		WR_REG16(r->x->r.di);
		WR_REG16(r->x->r.ip);
		WR_REG16(r->x->r.fl);
		WR_REG16(r->x->r.cs);
		WR_REG16(r->x->r.ss);
		WR_REG16(0);//fs
		WR_REG16(0);//gs
	} while (0);
	//printf("Hello data=%s\n", data);
	//getchar();
	return 42;
}

int xtem_rsp_m(rsp_t *r, char *data, int addr, int len) {
	//int len = strlen(data);
	//printf("addr=%x len=%d\n", addr, len);
	unsigned char *buf = 0;
	memr(r->x, (void **)&buf, &len, addr);
	if (buf) {
		for (int i = 0; i < len; i++) {
			sprintf(data + 2 * i, "%02x", buf[i]);
		}
	}
	//printf("Hello data=%s -- press enter\n", data);
	//getchar();
	return 42;
}
