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
  libxtem_execute(x);
  libxtem_cleanup(x);
  return 0;
}
