#include <arpa/inet.h>
#include <errno.h>
#include <netdb.h>
#include <netinet/in.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <time.h> // For timestamping logs

#define PORT "3490"
#define BACKLOG 10
#define LOG_FILE "server.log" // Define the log file name

//here we are returning a generic pointer - to any data type
//it’s returning pointers to internal fields inside the appropriate address structure
//why use void* cause inet_ntop() expects const void *src
void *get_in_addr(struct sockaddr *sa) {
  if (sa->sa_family == AF_INET) return &((struct sockaddr_in *)sa)->sin_addr;

  return &(((struct sockaddr_in6 *)sa)->sin6_addr);
}

// Function to write log messages to a file
void log_message(const char *message) {
  FILE *log_file = fopen(LOG_FILE, "a"); // Open in append mode
  if (log_file == NULL) {
    perror("Error opening log file");
    return;
  }

  time_t now = time(NULL);
  struct tm *t = localtime(&now);
  char timestamp[64];
  strftime(timestamp, sizeof(timestamp), "%Y-%m-%d %H:%M:%S", t);

  fprintf(log_file, "[%s] %s\n", timestamp, message);
  fclose(log_file);
}

int main(void) {
  log_message("Server started.");

  // listen on sockfd , new connection on new_fd;
  int sockfd, new_fd;
  struct addrinfo hints, *servinfo, *p;
  struct sockaddr_storage their_addr; // connector's address information
  // why using sockaddr_storage cause its general and we dont restrict user to
  // IPv4 or IPv6
  int rv, yes = 1;
  socklen_t sin_size;
  char s[INET6_ADDRSTRLEN];

  memset(&hints, 0, sizeof hints);
  hints.ai_family = AF_UNSPEC;     // accept both ipv4 and ipv6
  hints.ai_socktype = SOCK_STREAM; // tcp stream socket
  hints.ai_flags = AI_PASSIVE;     // use my IP for binding

  // By calling getaddrinfo we asking system to give us a list of address
  // structures so we can use them to bind a socket for incoming connections on
  // this port we are not resolving someone ele's address - all we are doing is
  // im going to be a server so please tell me which addresses(ipv4 , ipv6 , etc
  // .. ) I should bind to so clients can connect to me
  if ((rv = getaddrinfo(NULL, PORT, &hints, &servinfo)) != 0) {
    fprintf(stderr, "get addrinfo : %s \n ", gai_strerror(rv));
    char log_buffer[256];
    snprintf(log_buffer, sizeof(log_buffer), "getaddrinfo: %s", gai_strerror(rv));
    log_message(log_buffer);
    return 1;
  }
  log_message("getaddrinfo successful.");

  // loop through all the results and bind to the first we can
  // protocol specifies which transport layer protocol to use tcp , udp , or
  // both socktype is communication style and protocol is actual engine that
  // runs communication style
  for (p = servinfo; p != NULL; p = p->ai_next) {
    if ((sockfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) == -1) {
      perror("server : socket");
      log_message("socket: failed");
      continue;
    }
    log_message("Socket created successfully.");

    // set behaviour or limits on socket
    // SO_REUSEADDR option allows our program to reuse local address (IP and
    // PORT) , even if its in the TIME_WAIT state And why is this important when
    // a server closes a socket , the port goes into TIME_WAIT state and cannot
    // be resused immediately - this could cause this error when restarting your
    // server here yes is for enable this socket option
    if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int)) == -1) {
      perror("setsocket");
      log_message("setsockopt: failed");
      exit(1);
    }
    log_message("Setsockopt SO_REUSEADDR successful.");

    if (bind(sockfd, p->ai_addr, p->ai_addrlen) == -1) {
      close(sockfd);
      perror("server : bind");
      log_message("bind: failed");
      continue;
    }
    log_message("Bind successful.");
    break;
  }

  freeaddrinfo(servinfo); // all done with this structure

  if (p == NULL) {
    fprintf(stderr, "Server : failed to bind\n");
    log_message("Server failed to bind.");
    exit(1);
  }

  if (listen(sockfd, BACKLOG) == -1) {
    perror("listen");
    log_message("listen: failed");
    exit(1);
  }
  log_message("Listening on socket.");

  printf("server : waiting for connections... \n");
  log_message("Waiting for connections...");

  while (1) { // main accept loop
    sin_size = sizeof their_addr;
    new_fd = accept(sockfd, (struct sockaddr *)&their_addr, &sin_size);
    if (new_fd == -1) {
      perror("accept");
      log_message("accept: failed");
      continue;
    }
    log_message("Accepted a new connection.");

    inet_ntop(their_addr.ss_family, get_in_addr((struct sockaddr *)&their_addr),
              s, sizeof s);

    /*
     ---- Print connector's details on my server -----
    */
    printf("Client connected \n");
    char log_buffer[256];
    snprintf(log_buffer, sizeof(log_buffer), "Client connected from %s", s);
    log_message(log_buffer);

    char buf[2048];
    int bytes_received = recv(new_fd, buf, sizeof(buf) - 1, 0);

    if (bytes_received == -1) {
      perror("recv");
      log_message("recv: failed");
    } else {
      buf[bytes_received] = '\0'; // null terminate
      printf("Request from client : \n %s \n", buf);
      snprintf(log_buffer, sizeof(log_buffer), "Received %d bytes from client: %s", bytes_received, buf);
      log_message(log_buffer);
    }

    /*
     ---- Print connector's details logic ends here ----
    */

    char response[] = "HTTP/1.1 200 OK\r\n"
                      "Content-Type: text/plain\r\n"
                      "Content-Length:  13\r\n"
                      "\r\n"
                      "Hello, World!";

    if (send(new_fd, response, strlen(response), 0) == -1) {
      perror("send");
      log_message("send: failed");
    } else {
      log_message("Sent response to client: Hello, World!");
    }

    close(new_fd); // parent doesnt need this
    log_message("Closed connection with client.");
    // if(!fork()){ //this is the child process
    // close(sockfd); // child doesn't need the listener
    // if(send(new_fd, "Hello, World!", 13, 0) == -1) perror("send");
    // close(new_fd);
    // exit(0);
    // }
    // close(new_fd); // parent doesnt need this
  }

  return 0;
}