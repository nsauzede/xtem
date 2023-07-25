#ifndef libxtem_h
#define libxtem_h

#include <stddef.h>

/* SPDX-License-Identifier: GPL-3.0-or-later */

/* Flaky RSP API */

void*
xtem_rsp_init();
int
xtem_rsp_s(void* r);
int
xtem_rsp_g(void* r, char* data);
int
xtem_rsp_m(void* r, char* data, size_t len, size_t addr);
int
xtem_rsp_cleanup(void* r);

/* XTEM API */
void*
libxtem_init(int rsp_port);
int
libxtem_execute(void* x);
int
libxtem_cleanup(void* x);

#endif /*libxtem_h*/
