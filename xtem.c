#include "libxtem.h"
#include <stdio.h>

int
main(int argc, char* argv[])
{
  int arg = 1;
  int port = 1235;
  if (arg < argc) {
    sscanf(argv[arg++], "%d", &port);
  }
  void* x = libxtem_init(port);
  while (1) {
    int n = libxtem_execute(x);
    printf("%s: n=%d\n", __func__, n);
    if (n < 0)
      break;
  }
  libxtem_cleanup(x);
  return 0;
}
