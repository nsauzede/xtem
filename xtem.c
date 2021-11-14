#include "libxtem.h"

int
main()
{
  // int port = 0;
  int port = 1235;
  void* x = libxtem_init(port);
  libxtem_execute(x);
  libxtem_cleanup(x);
  return 0;
}
