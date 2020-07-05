/* SPDX-License-Identifier: GPL-3.0-or-later */

#include <inttypes.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <malloc.h>

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
	x->r.ax = 0x1234;
	x->r.cx = 0x0000;
	x->r.dx = 0x0000;
	x->r.bx = 0x0000;
	x->r.sp = 0x0000;
	x->r.bp = 0x0000;
	x->r.si = 0x0000;
	x->r.di = 0x0000;
	x->r.ip = 0x0000;
	x->r.fl = 0x0002;
	x->r.cs = 0xffff;
	x->r.ss = 0x0000;
	x->r.ds = 0x0000;
	x->r.es = 0x0000;
}

#define RAM_FIRST 0x00000
#define RAM_LAST 0x80000
#define BIOS_FIRST 0xf8000
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
	xtem_load_bios(x, "bios");
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

static int step(xtem_t *x) {
	int ret = 0;
	uint8_t *opc = 0;
	int len = 8;
	int pc = x->r.cs * 16 + x->r.ip;
	memr(x, (void **)&opc, &len, pc);
	if (!opc) {
		return 1;
	}
	uint8_t Ib, Eb, Ev;
	uint16_t Iv;
	uint16_t seg;
	switch (opc[0]) {
		case 0x33://	XOR		Gv	Ev
			x->r.ip++;
			Ev = *(uint8_t *)(opc + 1);
			x->r.ip++;
			printf("XOR		Gv	Ev\n");
			switch (Ev) {
				case 0xC0://ax,ax
					x->r.ax = x->r.ax ^ x->r.ax;
					break;
				default:
					ret = 6;
					printf("Ev=%02" PRIx8 " notimp !!\n", Ev);
					break;
			}
			break;
		case 0x40://	INC		eAX
			x->r.ip++;
			printf("INC		AX\n");
			x->r.ax++;
			break;
		case 0x4e://dec si
			x->r.ip++;
			x->r.si--;
			break;
		case 0x8B://	MOV		Gv	Ev
			x->r.ip++;
			uint16_t Gv = *(uint16_t *)(opc + 2);
			Ev = *(uint8_t *)(opc + 1);
			x->r.ip += 3;
			uint16_t *mem = 0;
			int len = 2;
			int addr = x->r.ds * 16 + Gv;
			memr(x, (void **)&mem, &len, addr);
			if (!mem) {
				return 1;
			}
			printf("MOV		Gv=%04" PRIx16 " Ev=%01" PRIx8 "\n", Gv, Ev);
			switch (Ev) {
				case 0x36:
					x->r.si = *mem;
					break;
			}
			break;
		case 0x8E://	MOV		Sw	Ew
			x->r.ip++;
			uint16_t Sw = (*(uint16_t *)(opc + 1) & 0xF0) >> 4;
			uint16_t Ew = *(uint16_t *)(opc + 1) & 0x0F;
			x->r.ip++;
			switch (Sw) {
				case 0xD://ds
					seg = x->r.ds;
					switch (Ew) {
						case 0x8:
							x->r.ax = seg;
							break;
						default:
							ret = 5;
							printf("Ew=%01" PRIx8 " notimp !!\n", Ew);
							break;
					}
					break;
				default:
					ret = 4;
					printf("Sw=%01" PRIx8 " notimp !!\n", Sw);
					break;
			}
			break;
		case 0x90://	NOP
			x->r.ip++;
			printf("NOP\n");
			break;
		case 0xb0://	MOV		Al	Ib
			x->r.ip++;
			Ib = *(uint8_t *)(opc + 1);
			x->r.ip++;
			printf("MOV		Al	Ib=%02" PRIx8 "\n", Ib);
			x->r.ax &= 0xff00;
			x->r.ax |= Ib;
			break;
		case 0xB2://	MOV		DL	Ib
			x->r.ip++;
			Ib = *(uint8_t *)(opc + 1);
			x->r.ip++;
			printf("MOV		Dl	Ib=%02" PRIx8 "\n", Ib);
			x->r.dx &= 0xff00;
			x->r.dx |= Ib;
			break;
		case 0xB8://	MOV		eAX	Iv
			x->r.ip++;
			Iv = *(uint16_t *)(opc + 1);
			x->r.ip += 2;
			printf("MOV		eAX	Iv=%04" PRIx16 "\n", Iv);
			x->r.ax = Iv;
			break;
		case 0xBA://	MOV		eDX	Iv
			x->r.ip++;
			Iv = *(uint16_t *)(opc + 1);
			x->r.ip += 2;
			printf("MOV		eDX	Iv=%04" PRIx16 "\n", Iv);
			x->r.dx = Iv;
			break;
		case 0xE6://	OUT		Ib	AL
			x->r.ip++;
			Ib = *(uint8_t *)(opc + 1);
			x->r.ip++;
			printf("OUT Ib=%02" PRIx8 " AL ???\n", Ib);
			break;
		case 0xea:// jmp ofs seg
		{
			uint16_t ofs = *(uint16_t *)(opc + 1);
			seg = *(uint16_t *)(opc + 3);
			printf("JMP ofs=%04" PRIx16 " seg=%04" PRIx16 "\n", ofs, seg);
			x->r.cs = seg;
			x->r.ip = ofs;
			break;
		}
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
			printf("GRP4	Eb=%02" PRIx8 "\n", Eb);
			switch (Eb) {
				case 0xC0: //GRP4/0	INC
					printf("INC		Al\n");
					x->r.ax = (x->r.ax & 0xff00) + (((x->r.ax & 0xff) + 1));
					break;
				default:
					ret = 3;
					printf("GRP4/%01" PRIx8 " notimp !!\n", Eb & 0xf);
					break;
			}
			break;
		default:
			ret = 2;
			printf("PC=%05" PRIx32 " OPC=%02" PRIx8 " %02" PRIx8 " %02" PRIx8 " %02" PRIx8 " notimp !!\n", pc, opc[0], opc[1], opc[2], opc[3]);
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
	return step(r->x);
}

int xtem_rsp_c(rsp_t *r) {
	while (1) {
		if (step(r->x)) {
			break;
		}
	}
	return 42;
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
