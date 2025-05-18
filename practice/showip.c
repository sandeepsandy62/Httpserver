#include<stdio.h>
#include<string.h>
#include<string.h>
#include<sys/types.h>
#include<sys/socket.h>
#include<arpa/inet.h>
#include<netinet/in.h>
#include<netdb.h>
#include <errno.h>
#include<stdlib.h>

#define MYPORT "3490" // the port users will be connecting to
#define BACKLOG 10 // how many pending connections queue will hold



int main(int argc , char* argv[]){
    struct addrinfo hints , *res , *p ;
    struct sockaddr_storage their_addr;
    socklen_t addr_size;
    int status , sockfd , new_fd ;
    char ipstr[INET6_ADDRSTRLEN];
   char *body = "<h1>Sandeep is here !!!</h1>";
// char response[2048];
// snprintf(response, sizeof(response),
//          "HTTP/1.1 200 OK\r\n"
//          "Content-Type: text/html\r\n"
//          "Content-Length: %zu\r\n"
//          "Connection: close\r\n"
//          "\r\n"
//          "%s", strlen(body), body);

FILE *image = fopen("luffy.webp", "rb");
if (image == NULL) {
    perror("Image open failed");
    return 1;
}

// Get the image size
fseek(image, 0, SEEK_END);
long image_size = ftell(image);
rewind(image);

// Allocate memory and read image into buffer
char *image_data = malloc(image_size);
if (image_data == NULL) {
    perror("Malloc failed");
    return 1;
}
fread(image_data, 1, image_size, image);
fclose(image);

char header[2048];
snprintf(header, sizeof(header),
         "HTTP/1.1 200 OK\r\n"
         "Content-Type: image/webp\r\n"
         "Content-Length: %ld\r\n"
         "Connection: close\r\n"
         "\r\n", image_size);

         int bytes_sent ;

    // if(argc != 2){
    //     fprintf(stderr,"usage : showip hostname\n");
    //     return 1;
    // }

    memset(&hints,0,sizeof(hints));
    hints.ai_family = AF_UNSPEC ; //AF_INET or AF_INET6 to force version
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE; // fill in my ip for me 

    if((status = getaddrinfo(NULL,MYPORT, &hints, &res))!=0){
        fprintf(stderr,"getaddrinfo: %s\n",gai_strerror(status));
        return 2;
    }

    printf("IP Addresses for %s : \n\n",argv[1]);

    for(p = res ; p != NULL ; p = p->ai_next){
        void *addr;
        char *ipver;

        //get the pointer to the address itself ,
        //different fields in IPv4 and IPv6:
        if(p->ai_family == AF_INET){ //IPv4
        //downcast 
        struct sockaddr_in *ipv4 = (struct sockaddr_in *) p -> ai_addr ;
        addr = &(ipv4->sin_addr);
        ipver = "IPv4";
        }else{ //IPv6
        struct sockaddr_in6 *ipv6 = (struct sockaddr_in6 *) p ->ai_addr ;
        addr = &(ipv6->sin6_addr);
        ipver = "IPv6";
        }

        //convert the ip to a string and print it 
        inet_ntop(p->ai_family,addr,ipstr,sizeof ipstr);
        printf("%s : %s \n" , ipver , ipstr) ;
        printf("Listening on %s : %s \n " , ipver , ipstr);
    }

    sockfd = socket(res->ai_family,res->ai_socktype,res->ai_protocol);

    if(sockfd == -1){
        fprintf(stderr,"create socket : %s \n " , gai_strerror(sockfd));
        return 1;
    }

    // bind it to the port we passed 
    // bind(sockfd,res->ai_addr,res->ai_addrlen);

    //connect without setting any port , we only care about destionation
    //connect(sockfd,res->ai_addr,res->ai_addrlen);

    bind(sockfd,res->ai_addr,res->ai_addrlen);
    listen(sockfd,BACKLOG);

    //now accept an incoming connection

    addr_size = sizeof their_addr;
    new_fd = accept(sockfd,(struct sockaddr*)&their_addr,&addr_size);

    

    if(new_fd == -1){
        perror("accept");
        return 1;
    }

    printf("Client connected \n");

    char buf[2048];
    int bytes_received = recv(new_fd,buf,sizeof(buf)-1,0);

    //buffer to receive client data 
    if(bytes_received == -1){
        perror("recv");
    }else{
        buf[bytes_received] = '\0' ; // null terminate
        printf("Request from client : \n %s \n",buf);
    }

    bytes_sent = send(new_fd, header, strlen(header), 0);
    send(new_fd, image_data, image_size, 0);

    



    freeaddrinfo(res); //free the linke list

    return 0;
}