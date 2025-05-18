#include <arpa/inet.h>
#include <netdb.h> /*Provide network database operations , primarily used fo name resolution*/
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h> /*Includes socket related system calls and data structures*/
#include <sys/types.h> /*Includes definitions for various data types used in system calls*/

#define PORT "3940" // The port users will be connecting to

#define BACKLOG 10 // How many pending connections queue will hold

/*
This function extracts the actual IP address (not the port, not the family) from
a generic struct sockaddr * pointer. It's commonly used when we don't know if
the address is IPv4 or IPv6.
-> sa is a pointer to a generic struct sockaddr (which is just a generic
wrapper).
-> we check the family (sa->sa_family) to see if it's AF_INET (IPv4) or AF_INET6
(IPv6).
-> Then we downcast to the correct type:
-> struct sockaddr_in for IPv4
-> struct sockaddr_in6 for IPv6

sockaddr_in is a full IPv4 socket address, including port, family, and 4-byte IP
address. same goes for sockaddr_in6 we're downcasting and accessing just the
address, not the whole structure.

*/
void *get_in_addr(struct sockaddr *sa) {
  // if address is IPV4 adress allocate
  if (sa->sa_family == AF_INET) {
    // then downcast socaddr which is general address to sockaddr_in [ipv4]
    /*
    Sockaddr_in is internet style meaning ipv4
    [sockaddr]it stores another struct in_addr
    [in_addr]it store another struct in_addr_t
    [in_addr_t] it is a alias of __uint32_t [32 bit int]->4 bytes
    */
    return &(((struct sockaddr_in *)sa)->sin_addr);
  }

  // other wise get ipv6 address
  return &(((struct sockaddr_in6 *)sa)->sin6_addr);
}

int getaddrinfo(
    const char *node,             // eg : www.sandeepgogarla.com
    const char *service,          // eg : "http" or portnumber
    const struct addrinfo *hints, /*
    It's called "hints" because it can be used to provide, well, hints (in the
    sense of a tip; a suggestion that might come in useful but could be
    ignored). This indicates things like what protocol family (IPv4 vs. IPv6,
    for example) the caller wants, what socket type (datagram vs. streaming),
    what protocol (TCP vs. UDP), etc.
    we can pass NULL in for hints and thus indicate that we don't care what
    protocol family, socket type, or protocol you get back.
    */
    struct addrinfo **res);       /*
          Pointer to a linked list of results
          */

int main(int argc, char *argv[]) {
  int status;
  struct addrinfo hints;
  struct addrinfo *servinfo; // will point to results
  // it may contain garbage values in fields we dont specifically set .
  // it may confuse the getaddrinfo() function
  memset(&hints, 0, sizeof hints); // make sure struct is empty
  hints.ai_family = AF_UNSPEC;     // dont care IPV4 or IPV6
  hints.ai_socktype = SOCK_STREAM; // TCP Stream Sockets
  hints.ai_flags = AI_PASSIVE;     /* This tells getaddrinfo() to assign
      the address of my local host to the socket structure
      */

  // get ready to connect
  status = getaddrinfo(NULL, "3490", &hints, &servinfo);

  if (status != 0) {
    /*
    we are printing the error to standard error stream (stderr) , not standard
    output . gai_strerror(int errcode) is a function that converts the error
    code returned by getaddrinfo() into a human-readable error message string.
    */
    fprintf(stderr, "getaddrinfo error : %s\n", gai_strerror(status));
    exit(1);
  }

  // if everything works properly till here we will have a linked list of struct
  // addrinfo , each of which contains struct sockaddr

  // free the result linkedlist
  freeaddrinfo(servinfo);
}
