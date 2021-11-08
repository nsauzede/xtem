#ifndef LIBRSPD_H_
#define LIBRSPD_H_

/* Public API */

#include <stddef.h>

typedef struct
{
  int port;
  int (*question)(void* rsp);
  int (*get_regs)(void* rsp);
  int (*read_mem)(void* rsp, size_t addr, size_t len);
  int (*cont)(void* rsp);
} rsp_init_t;

void*
rsp_init(rsp_init_t* init, int init_size);
int
rsp_port(void* rsp);
int
rsp_execute(void* rsp);
int
rsp_send(void* rsp, const char* src, int size);
int
rsp_cleanup(void* rsp);

#ifndef LIBRSP_HEADER_ONLY

/* Private APIs / Implementation - Don't use, might change */
#define RSP_DEBUG

#include <inttypes.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
// #ifdef RSP_DEBUG
#include <stdio.h>
// #endif
#include <pthread.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>

#ifdef RSP_DEBUG
#define dbg_printf(...)                                                        \
  do {                                                                         \
    printf("%s:%d:%s: ", __FILE__, __LINE__, __func__);                        \
    printf(__VA_ARGS__);                                                       \
  } while (0)
#else
#define dbg_printf(...)                                                        \
  do {                                                                         \
  } while (0)
#endif

/*

RSP User | RSP API |pipes| RSP THR/server |sock| RSP client

*/

typedef enum
{
  rsp_state_invalid,
  rsp_state_listening,
  rsp_state_accepted,
} rsp_state_t;

typedef enum
{
  rsp_cmd_invalid,
} rsp_cmd_t;

typedef struct
{
  union
  {
    rsp_init_t init;
    struct
    {
      int port;
    };
  };

  int from_thr[2];
  int to_thr[2];

  pthread_t thr;
  // thread private
  rsp_state_t state;
  int ss, cs;

} rsp_private_t;

static rsp_state_t
rsp_read_from_thr(rsp_private_t* rsp)
{
  rsp_state_t state;
  read(rsp->from_thr[0], &state, sizeof(state));
  return state;
}

static void
rsp_write_from_thr(rsp_private_t* rsp, rsp_state_t state)
{
  write(rsp->from_thr[1], &state, sizeof(state));
}

static ssize_t
rsp_write_cs(rsp_private_t* rsp, const void* buf, size_t count)
{
  ssize_t n = write(rsp->cs, buf, count);
  if (!n) {
    dbg_printf("CS hangup\n");
    close(rsp->cs);
    rsp->cs = -1;
    dbg_printf("Listening on port %d..\n", rsp->port);
    rsp->state = rsp_state_listening;
  }
  if (n != (ssize_t)count) {
    dbg_printf(
      "CS incomplete write (needed %d succeeded %d)\n", (int)count, (int)n);
    close(rsp->cs);
    rsp->cs = -1;
    dbg_printf("Listening on port %d..\n", rsp->port);
    rsp->state = rsp_state_listening;
  }
  return n;
}

int
rsp_send(void* rsp_, const char* src, int size)
{
  rsp_private_t* rsp = (rsp_private_t*)rsp_;
  int csum = 0;
  for (int i = 0; i < size; i++) {
    csum += src[i];
  }
  csum &= 0xff;
  char trailer3[] = "#XX";
  snprintf(trailer3, sizeof(trailer3), "#%02x", csum);
  rsp_write_cs(rsp, "$", 1);
  rsp_write_cs(rsp, src, size);
  rsp_write_cs(rsp, trailer3, strlen(trailer3));
  return 0;
}

static ssize_t
rsp_read_cs(rsp_private_t* rsp, void* buf, size_t count)
{
  ssize_t n = read(rsp->cs, buf, count);
  if (!n) {
    dbg_printf("CS hangup\n");
    close(rsp->cs);
    rsp->cs = -1;
    dbg_printf("Listening on port %d..\n", rsp->port);
    rsp->state = rsp_state_listening;
  }
  if (n != (ssize_t)count) {
    dbg_printf(
      "CS incomplete read (needed %d succeeded %d)\n", (int)count, (int)n);
    close(rsp->cs);
    rsp->cs = -1;
    dbg_printf("Listening on port %d..\n", rsp->port);
    rsp->state = rsp_state_listening;
  }
  return n;
}

static int
rsp_valid_csum(rsp_private_t* rsp, int csum)
{
  char trailer3[] = "#XX";
  ssize_t n = rsp_read_cs(rsp, &trailer3, 3);
  if (n != 3)
    return 0;
  int sum = 0;
  if (1 != sscanf(&trailer3[1], "%02x", &sum))
    return 0;
  dbg_printf("read csum %x, input csum %x\n", sum, csum);
  if (csum != -1 && csum != sum) {
    // return 0;
  }
  return 1;
}

static int
rsp_valid_csum2(rsp_private_t* rsp, int csum)
{
  char trailer2[] = "XX";
  ssize_t n = rsp_read_cs(rsp, &trailer2, 2);
  if (n != 2)
    return 0;
  int sum = 0;
  if (1 != sscanf(trailer2, "%02x", &sum))
    return 0;
  dbg_printf("read csum %x, input csum %x\n", sum, csum);
  if (csum != -1 && csum != sum) {
    // return 0;
  }
  return 1;
}

// This guy throws remaining bytes until csum ("#XX") and return 1
// If any error returns 0
static int
rsp_throw_packet(rsp_private_t* rsp)
{
  ssize_t n;
  char c;
  while (1) {
    n = rsp_read_cs(rsp, &c, 1);
    if (n != 1) {
      return 0;
    }
    if (c == '#') {
      break;
    }
  }
  n = rsp_read_cs(rsp, &c, 1);
  if (n != 1) {
    return 0;
  }
  n = rsp_read_cs(rsp, &c, 1);
  if (n != 1) {
    return 0;
  }
  return 1;
}

static void
rsp_handle(rsp_private_t* rsp)
{
  while (rsp->cs != -1) {
    char c;
    ssize_t n = rsp_read_cs(rsp, &c, sizeof(c));
    if (n != sizeof(c))
      break;
    dbg_printf("Got CS0 '%c' (%x)\n", c, (int)c);
    if (c == '+')
      continue;
    if (c != '$') {
      dbg_printf("Received unknown async cmd '%c' (%x)\n", c, (int)c);
      break;
    }

    int csum = 0;
    n = rsp_read_cs(rsp, &c, sizeof(c));
    if (n != sizeof(c))
      break;
    csum += c;
    dbg_printf("Got CS1 '%c' (%x)\n", c, (int)c);
    if (c == '?') {
      if (!rsp_valid_csum(rsp, csum)) {
        break;
      }
      rsp_write_cs(rsp, "+", 1);
      if (rsp->init.question) {
        rsp->init.question(rsp);
        break;
      } else {
        dbg_printf("Unsupported question cb ?\n");
      }
    } else if (c == 'g') {
      if (!rsp_valid_csum(rsp, csum)) {
        break;
      }
      rsp_write_cs(rsp, "+", 1);
      if (rsp->init.get_regs) {
        rsp->init.get_regs(rsp);
        break;
      } else {
        dbg_printf("Unsupported get_regs cb ?\n");
      }
    } else if (c == 'm') {
      size_t addr = 0, len = 0;
      char saddr[] = "XXXXXXXXXXXXXXXX";
      char slen[] = "XXXXXXXXXXXXXXXX";
      char* ptr = saddr;
      int fail = 0;
      while (1) {
        n = rsp_read_cs(rsp, ptr, 1);
        if (n != 1) {
          fail = 1;
          break;
        }
        if ((*ptr >= '0' && *ptr <= '9') || (*ptr >= 'a' && *ptr <= 'f') ||
            (*ptr >= 'A' && *ptr <= 'F')) {
          csum += *ptr;
          ptr++;
          continue;
        }
        if (*ptr == ',')
          break;
        fail = 1;
        break;
      }
      if (fail)
        break;
      sscanf(saddr, "%zx", &addr);
      ptr = slen;
      fail = 0;
      while (1) {
        n = rsp_read_cs(rsp, ptr, 1);
        if (n != 1) {
          fail = 1;
          break;
        }
        if ((*ptr >= '0' && *ptr <= '9') || (*ptr >= 'a' && *ptr <= 'f') ||
            (*ptr >= 'A' && *ptr <= 'F')) {
          csum += *ptr;
          ptr++;
          continue;
        }
        if (*ptr == '#')
          break;
        fail = 1;
        break;
      }
      if (fail)
        break;
      sscanf(slen, "%zx", &len);
      csum &= 0xff;
      if (!rsp_valid_csum2(rsp, csum)) {
        break;
      }
      rsp_write_cs(rsp, "+", 1);
      if (rsp->init.read_mem) {
        rsp->init.read_mem(rsp, addr, len);
        break;
      } else {
        dbg_printf("Unsupported read_mem cb ?\n");
      }
    } else {
      dbg_printf("Received unknown sync cmd '%c' (%x)\n", c, (int)c);
      if (!rsp_throw_packet(rsp)) {
        break;
      }
    }
    rsp_write_cs(rsp, "+", 1);
    rsp_write_cs(rsp, "$#00", 4);
    break;
  }
}

static void*
rsp_thread(void* arg)
{
  rsp_private_t* rsp = (rsp_private_t*)arg;
  rsp->ss = rsp->cs = -1;

  rsp->ss = socket(PF_INET, SOCK_STREAM, 0);
  int on = 1;
  if (-1 == setsockopt(rsp->ss, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on))) {
    perror("setsockopt");
    exit(1);
  }
  struct sockaddr_in sa;
  sa.sin_family = AF_INET;
  sa.sin_port = htons(rsp->port);
  sa.sin_addr.s_addr = INADDR_ANY;
  if (-1 == bind(rsp->ss, (struct sockaddr*)&sa, sizeof(sa))) {
    perror("bind");
    exit(1);
  }
  if (-1 == listen(rsp->ss, 1)) {
    perror("listen");
    exit(1);
  }
  dbg_printf("Listening on port %d..\n", rsp->port);
  rsp->state = rsp_state_listening;
  // must notify the waiting API
  rsp_write_from_thr(rsp, rsp->state);

  while (1) {
    if (rsp->cs == -1) {
    }

    while (1) {
      int max = -1;
      fd_set rfds;
      FD_ZERO(&rfds);
      if (rsp->to_thr[0] != -1) {
        FD_SET(rsp->to_thr[0], &rfds);
        if (max < rsp->to_thr[0]) {
          max = rsp->to_thr[0];
        }
      }
      if (rsp->ss != -1) {
        FD_SET(rsp->ss, &rfds);
        if (max < rsp->ss) {
          max = rsp->ss;
        }
      }
      if (rsp->cs != -1) {
        FD_SET(rsp->cs, &rfds);
        if (max < rsp->cs) {
          max = rsp->cs;
        }
      }
      int n = select(max + 1, &rfds, 0, 0, 0);
      if (n == -1) {
        perror("select");
        exit(1);
      }
      if (n > 0) {
        dbg_printf("Something to read! %d\n", n);
        if (rsp->cs != -1 && FD_ISSET(rsp->cs, &rfds)) {
          dbg_printf("Reading CS\n");
          rsp_handle(rsp);
        }
        if (rsp->ss != -1 && FD_ISSET(rsp->ss, &rfds)) {
          dbg_printf("Reading SS\n");
          rsp->cs = accept(rsp->ss, 0, 0);
          if (rsp->cs == -1) {
            perror("accept");
            exit(1);
          }
          dbg_printf("Accepted %d on %d\n", rsp->cs, rsp->port);
          rsp->state = rsp_state_accepted;
        }
        if (FD_ISSET(rsp->to_thr[0], &rfds)) {
          dbg_printf("Reading CMD\n");
          rsp_cmd_t cmd;
          read(rsp->to_thr[0], &cmd, sizeof(cmd));
          dbg_printf("Got CMD %d\n", cmd);
        }
      }
    }
  }
  return 0;
}

void*
rsp_init(rsp_init_t* init, int size)
{
  if (!init || size != sizeof(rsp_init_t))
    return 0;
  rsp_private_t* rsp = calloc(1, sizeof(rsp_private_t));
  rsp->init = *init;
  pipe(rsp->to_thr);
  pipe(rsp->from_thr);
  if (pthread_create(&rsp->thr, 0, rsp_thread, rsp)) {
    dbg_printf("Failed to create thread\n");
    free(rsp);
    return 0;
  }
  rsp_read_from_thr(rsp);
  return rsp;
}

int
rsp_port(void* rsp_)
{
  rsp_private_t* rsp = (rsp_private_t*)rsp_;
  if (!rsp)
    return -1;
  else
    return rsp->port;
}

int
rsp_execute(void* rsp_)
{
  rsp_private_t* rsp = (rsp_private_t*)rsp_;
  if (!rsp)
    return 1;
  return 0;
}

int
rsp_cleanup(void* rsp_)
{
  rsp_private_t* rsp = (rsp_private_t*)rsp_;
  if (rsp) {
    free(rsp);
  }
  return 0;
}

#endif

#endif /*LIBRSPD_H_*/
