#include <arpa/inet.h>
#include <errno.h>
#include <netdb.h>
#include <netinet/in.h>
#include <pthread.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/_pthread/_pthread_t.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <time.h> // For timestamping logs
#include <unistd.h>

#define PORT "3490"
#define BACKLOG 10
#define LOG_FILE "server.log" // Define the log file name

// Since pthread_create() only allows a single void* argument , we wrap
// arguments into a struct
struct thread_args {
  int client_fd;
  struct sockaddr_storage client_addr;
};

// here we are returning a generic pointer - to any data type
// it’s returning pointers to internal fields inside the appropriate address
// structure why use void* cause inet_ntop() expects const void *src
void *get_in_addr(struct sockaddr *sa) {
  if (sa->sa_family == AF_INET)
    return &((struct sockaddr_in *)sa)->sin_addr;

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

// define thread function . this will handle the request and send the response
void *handle_client(void *arg) {
  struct thread_args *args = (struct thread_args *)arg;
  int client_fd = args->client_fd;

  char s[INET6_ADDRSTRLEN];
  inet_ntop(args->client_addr.ss_family,
            get_in_addr((struct sockaddr *)&args->client_addr), s, sizeof s);

  char log_buffer[256];
  snprintf(log_buffer, sizeof(log_buffer), "Handling client in thread from %s", s);
  log_message(log_buffer);

  // Receive request
  char buf[2048];
  int bytes_received = recv(client_fd, buf, sizeof(buf) - 1, 0);

  if (bytes_received <= 0) {
    perror("recv");
    log_message("recv failed in thread");
    close(client_fd);
    free(arg);
    pthread_exit(NULL);
  }

  buf[bytes_received] = '\0';
  printf("Thread received from client:\n%s\n", buf);

  // Check the request path
  char method[8], path[1024];
  sscanf(buf, "%s %s", method, path);

  if (strcmp(path, "/luffy") == 0) {
    // Serve the image
    FILE *image = fopen("responses/media/luffy.webp", "rb");
    if (image == NULL) {
      perror("image open failed");
      close(client_fd);
      free(arg);
      pthread_exit(NULL);
    }

    fseek(image, 0, SEEK_END);
    long image_size = ftell(image);
    rewind(image);

    char *image_data = malloc(image_size);
    if (!image_data) {
      perror("malloc failed");
      fclose(image);
      close(client_fd);
      free(arg);
      pthread_exit(NULL);
    }

    fread(image_data, 1, image_size, image);
    fclose(image);

    char image_header[256];
    snprintf(image_header, sizeof(image_header),
             "HTTP/1.1 200 OK\r\n"
             "Content-Type: image/webp\r\n"
             "Content-Length: %ld\r\n"
             "Connection: close\r\n"
             "\r\n", image_size);

    send(client_fd, image_header, strlen(image_header), 0);
    send(client_fd, image_data, image_size, 0);
    free(image_data);

  } else {
    // Serve HTML response
    const char *features = "<h1>Simple Server</h1>"
                          "<ul>"
                           "<li>Multi-threaded request handling using pthreads</li>"
                           "<li>Logging with timestamps</li>"
                           "<li>Serves plain text and images</li>"
                           "<li>Error handling with logs</li>"
                           "</ul>";

    const char *body_template = "<html><head><title>Simple Server</title></head><body>%s</body></html>";

    size_t body_len = strlen(body_template) + strlen(features) + 1;
    char *body = malloc(body_len);
    if (!body) {
      perror("malloc failed");
      close(client_fd);
      free(arg);
      pthread_exit(NULL);
    }

    sprintf(body, body_template, features);
    log_message(body);

    char header[2048];
    snprintf(header, sizeof(header),
             "HTTP/1.1 200 OK\r\n"
             "Content-Type: text/html\r\n"
             "Content-Length: %ld\r\n"
             "Connection: close\r\n"
             "\r\n", strlen(body));
    log_message(header);

    send(client_fd, header, strlen(header), 0);
    send(client_fd, body, strlen(body), 0);

    free(body);
  }

  close(client_fd);
  free(arg);
  pthread_exit(NULL);
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
    snprintf(log_buffer, sizeof(log_buffer), "getaddrinfo: %s",
             gai_strerror(rv));
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

    struct thread_args *args = malloc(sizeof(struct thread_args));
    args->client_fd = new_fd;
    args->client_addr = their_addr;

    pthread_t tid;
    if (pthread_create(&tid, NULL, handle_client, args) != 0) {
      perror("pthread_create");
      close(new_fd);
      free(args);
    } else {
      pthread_detach(tid); // automatically clean up thread when its done
    }
  }

  return 0;
}