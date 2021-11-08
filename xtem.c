#include "libxtem.h"

int
main()
{
  void* x = libxtem_init();
  while (1) {
    if (libxtem_execute(x)) {
      break;
    }
  }
  return 0;
}
