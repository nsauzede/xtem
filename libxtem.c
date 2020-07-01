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
} xtem_t;

static void xtem_reset(xtem_t *x) {
	x->r.ax = 0x0000;
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
	return x;
}

#define BIOS_FIRST 0xf8000
#define BIOS_LAST 0xfffff
#define MEM_LAST 0xfffff
static unsigned char *membuf = 0;
static int membuflen = 0;
/*	read memory without side effects
	caller expects a backdoor memory pointer in return
*/
static void memr(xtem_t *x, void **dest, int *len, int addr) {
	x = x;
	if ((addr >= BIOS_FIRST) && (addr <= BIOS_LAST)) {
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

typedef struct {
	xtem_t *x;
} rsp_t;

rsp_t *xtem_rsp_init() {
	rsp_t *r = calloc(1, sizeof(rsp_t));
	r->x = xtem_init();
	return r;
}

int xtem_rsp_s(rsp_t *r) {
	r = r;
	//printf("Hello step -- press enter\n");
	//getchar();
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
