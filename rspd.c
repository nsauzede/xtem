#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "librspd.h"

int xlen = 64;
int
rsp_question(void* rsp)
{
  char* buf = "S05";
  rsp_send(rsp, buf, strlen(buf));
  return 0;
}

#define LEN64 (560 * 2)
#define LEN32 (312 * 2)
int
rsp_get_regs(void* rsp)
{
  char buf[LEN64 + 1];
  int len = xlen == 64 ? LEN64 : LEN32;
  memset(buf, '0', len);
  rsp_send(rsp, buf, len);
  return 0;
}

int
rsp_read_mem(void* rsp, size_t addr, size_t len)
{
  char* buf = malloc(len * 2);
  memset(buf, '0' + (addr % 10), len * 2);
  rsp_send(rsp, buf, len * 2);
  return 0;
}

int
main(int argc, char* argv[])
{
  int arg = 1;
  if (arg < argc) {
    sscanf(argv[arg++], "%d", &xlen);
  }
  int port = 1235;
  void* rsp = rsp_init(&(rsp_init_t){ .port = port,
                                      .question = rsp_question,
                                      .get_regs = rsp_get_regs,
                                      .read_mem = rsp_read_mem },
                       sizeof(rsp_init_t));
  if (!rsp) {
    return 1;
  }
  while (1) {
    static int count = 0;
    // printf("rsp_execute: %d\n", count);
    count++;
    if (count >= 2) {
      // break;
    }
    if (rsp_execute(rsp)) {
      break;
    }
    sleep(1);
  }
  rsp_cleanup(rsp);
  return 0;
}
