#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <netdb.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>

int main(int argc, char* argv[])
{
  struct hostent* h;
  //
  if(argc != 2) {
    printf("Error in number of input arguments!\n");
    return EXIT_FAILURE;
  }
  //
  if((h = gethostbyname(argv[1])) == NULL) {
    printf("Error in calling gethostbyname function!\n");
    return EXIT_FAILURE;
  }
  else {
    printf("Host name: %s\n", h->h_name);
    struct in_addr temp;
    temp.s_addr = *((unsigned long*)(h->h_addr));
    char* p =inet_ntoa(temp);
    printf("IP: %s\n", p);
  }
  //
  printf("End of this test!\n");
  //
  return EXIT_SUCCESS;
}
