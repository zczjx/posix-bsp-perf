#include "fcntl.h"
#include "inttypes.h"
#include "memory.h"
#include "signal.h"
#include "stdarg.h"
#include "stddef.h"
#include "stdint.h"
#include "stdlib.h"
#include "string.h"
#include "errno.h"
#include "unistd.h"
#include "time.h"
#include "sys/types.h"
#include "sys/stat.h"
#include "sys/time.h"
#include "sys/param.h"
#include "netdb.h"
#include "dlfcn.h"
#include "arpa/inet.h"
#include "poll.h"
#include "sys/socket.h"
#include "sys/random.h"
#include "sys/un.h"
#include "sys/epoll.h"
#include "sys/eventfd.h"
#include "sys/ioctl.h"
#include "sys/mman.h"
#include "sys/queue.h"
#include "sys/select.h"
#include "sys/sendfile.h"
#include "sys/uio.h"
#include "sys/wait.h"
#include "sys/resource.h"
#include "sys/timerfd.h"
#include "netinet/in.h"
#include "netinet/tcp.h"
#include "ifaddrs.h"
#include "stdio.h"


#undef KEY
#if defined(__i386)
# define KEY '_','_','i','3','8','6'
#elif defined(__x86_64)
# define KEY '_','_','x','8','6','_','6','4'
#elif defined(__PPC64__)
# define KEY '_','_','P','P','C','6','4','_','_'
#elif defined(__ppc64__)
# define KEY '_','_','p','p','c','6','4','_','_'
#elif defined(__PPC__)
# define KEY '_','_','P','P','C','_','_'
#elif defined(__ppc__)
# define KEY '_','_','p','p','c','_','_'
#elif defined(__aarch64__)
# define KEY '_','_','a','a','r','c','h','6','4','_','_'
#elif defined(__ARM_ARCH_7A__)
# define KEY '_','_','A','R','M','_','A','R','C','H','_','7','A','_','_'
#elif defined(__ARM_ARCH_7S__)
# define KEY '_','_','A','R','M','_','A','R','C','H','_','7','S','_','_'
#endif

#define SIZE (sizeof(short))
static char info_size[] =  {'I', 'N', 'F', 'O', ':', 's','i','z','e','[',
  ('0' + ((SIZE / 10000)%10)),
  ('0' + ((SIZE / 1000)%10)),
  ('0' + ((SIZE / 100)%10)),
  ('0' + ((SIZE / 10)%10)),
  ('0' +  (SIZE    % 10)),
  ']',
#ifdef KEY
  ' ','k','e','y','[', KEY, ']',
#endif
  '\0'};

#ifdef __CLASSIC_C__
int main(argc, argv) int argc; char *argv[];
#else
int main(int argc, char *argv[])
#endif
{
  int require = 0;
  require += info_size[argc];
  (void)argv;
  return require;
}
