#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <syslog.h>
#include <string.h>
#include <signal.h>

#define BACKLOG   (20)
#define BUFFER_SIZE  (512)

int socket_fd = 0;
int open_fd = 0;
int accept_fd;

char *receive_buffer;
char *chec_ptr=NULL;

//Check for Signal Handlers
void signal_handler(int signal)
{
  if (signal == SIGINT)
    {
      syslog(LOG_DEBUG, "Caught signal SIGINT, exiting!!");
    }
  else if( signal == SIGTERM)
    {
      syslog(LOG_DEBUG, "Caught signal SIGTERM, exiting!!");
    }
  else if(signal==SIGKILL)
    {
      syslog(LOG_DEBUG,"Caught Signal SIGKILL,exiting!!");
    }
  else if(signal==SIGSEGV)
    {
      syslog(LOG_DEBUG,"Caught Signal SIGSEGV,exiting!!");
    }
  close(socket_fd);
  close(open_fd);
  close(accept_fd);
  unlink("/var/tmp/aesdsocketdata");
  closelog();
  exit(0);
}
//Check for daemon creation
int deamon_create()
{
  int sock_status = 0;
  pid_t pid;
  pid = fork ();
  if (pid == -1){
      syslog(LOG_ERR , "fork\n");
      return -1;
  }
  else if (pid != 0){
      exit (EXIT_SUCCESS);
  }

  sock_status = setsid();
  if (sock_status == -1){
      syslog(LOG_ERR , "setsid\n");
      return -1;
  }

  sock_status = chdir("/");
  if (sock_status == -1){
      syslog(LOG_ERR , "chdir\n");
      return -1;
  }

  // Redirect the stdin , stdout and stderror to /dev/null
  open ("/dev/null", O_RDWR);
  dup (0);
  dup (0);

  return 0;
}

int main(int argc , char *argv[])
{
  int sock_port_check = 1;
  openlog(NULL , 0 , LOG_USER);
  signal(SIGINT , signal_handler);
  signal(SIGTERM , signal_handler);

  uint8_t daemon_check_flag = 0;                   //Check Daemon created or not
  if(argc == 2)
    {
      if (strcmp(argv[1], "-d") == 0)
        {
          daemon_check_flag = 1;
        }
      else
        {
          syslog(LOG_ERR, "Invalid paramter");
          return -1;
        }
    }

  int sock_status = 0;
  struct addrinfo *servinfo;
  struct addrinfo hints;

  //Check for file open
  open_fd = open("/var/tmp/aesdsocketdata" , O_RDWR | O_CREAT, S_IWUSR | S_IRUSR | S_IWGRP | S_IRGRP | S_IROTH);
  // Error check
  if (open_fd == -1){
      syslog(LOG_ERR , "open\n");
      return -1;
  }



  // The structure has to empty initially
  memset(&hints, 0, sizeof(hints));

  hints.ai_family = AF_INET;     // IPv4 type of connection
  hints.ai_socktype = SOCK_STREAM; // TCP stream sockets
  hints.ai_flags = AI_PASSIVE;     // Fill in the IP

  //Check for address which need to sent to socket
  if ((sock_status = getaddrinfo(NULL, "9000", &hints, &servinfo)) != 0) {
      syslog(LOG_ERR , "getaddrinfo\n");
      return -1;
  }
  //Check for socket establishment
  socket_fd = socket(servinfo->ai_family , servinfo->ai_socktype , servinfo->ai_protocol);
  if(socket_fd == -1){
      syslog(LOG_ERR , "socket\n");
      return -1;
  }

  //Check for binding issues
  sock_status = setsockopt(socket_fd , SOL_SOCKET , SO_REUSEADDR , &sock_port_check , sizeof(sock_port_check));
  if (sock_status == -1){
      syslog(LOG_ERR , "setsocketopt\n");
      return -1;
  }

  //Check for bind establishment
  sock_status = bind(socket_fd , servinfo->ai_addr , servinfo->ai_addrlen);
  if (sock_status == -1){
      syslog(LOG_ERR , "bind\n");
      return -1;
  }

  //Check for daemon flag status
  if (daemon_check_flag){
      sock_status = deamon_create();

      if (sock_status == -1){
          syslog(LOG_ERR , "deamon_create\n");
      }
      else{
          syslog(LOG_DEBUG , "Daemon created suceesfully\n");
      }
  }

  //Check listen connection is established
  sock_status = listen(socket_fd , BACKLOG);
  if (sock_status == -1){
      perror("listen");
      return -1;
  }
  freeaddrinfo(servinfo);
  struct sockaddr client_addr;
  socklen_t addr_size = sizeof(client_addr);
  char recv_buffer[BUFFER_SIZE];
  receive_buffer = (char *)malloc(sizeof(char) * BUFFER_SIZE);
  if (receive_buffer == NULL){
      syslog(LOG_ERR , "malloc\n");
      return -1;
  }

  uint8_t packet_check_flag = 0;
  int byte_count = 0;
  int datapacket = 0;
  int final_size = 1;
  int realloc_int = 0;
  static int file_size = 0;
  while(1){
      //Check for accept connection
      accept_fd = accept(socket_fd , &client_addr, &addr_size);
      if (accept_fd == -1){
          syslog(LOG_ERR , "accept\n");
          return -1;
      }

      // Print IP address
      // The inet_toa function requires a sockaddr_in structure
      // Reference: https://stackoverflow.com/questions/3060950/how-to-get-ip-address-from-sock-structure-in-c
      struct sockaddr_in * ip_client = (struct sockaddr_in *)&client_addr;
      syslog(LOG_INFO , "Accepted connection from %s\n", inet_ntoa(ip_client->sin_addr));

      while(!packet_check_flag){
          byte_count = recv(accept_fd , recv_buffer , BUFFER_SIZE , 0);
          // printf("Byte count:%d\n", byte_count);
          if ( byte_count == -1){
              syslog(LOG_ERR , "recv\n");
              return -1;
          }
          else{
              final_size++;
              for(datapacket = 0; datapacket < BUFFER_SIZE; datapacket++){      //Check for newline character
                  if(recv_buffer[datapacket] == '\n'){
                      packet_check_flag = 1;
                      break;
                  }
              }
              memcpy((receive_buffer + (BUFFER_SIZE * realloc_int)) , recv_buffer , BUFFER_SIZE);
              if(final_size >=BUFFER_SIZE) //Check for buffer is able to reallocate or not
                {
                  if(!(chec_ptr = realloc(receive_buffer, BUFFER_SIZE + final_size + 1)))
                    {
                      syslog(LOG_ERR , "realloc\n");
                      return -1;
                    }
                  else
                    {
                      receive_buffer = chec_ptr;
                      strncat(receive_buffer, recv_buffer, final_size);
                    }

                }
              if(!packet_check_flag)    //Check for packet check flag status
                {
                  receive_buffer = (char *)realloc(receive_buffer , (sizeof(char) * (BUFFER_SIZE * final_size)));

                  if (receive_buffer == NULL){

                      syslog(LOG_ERR , "realloc\n");
                  }
                  else{
                      realloc_int++;
                  }
                }

          }
      }

      packet_check_flag = 0;
      //Write the buffer data to the file
      int buffer_len = ((datapacket + 1) + (BUFFER_SIZE * realloc_int));
      int write_len = write(open_fd , receive_buffer , buffer_len);
      if(write_len == -1){
          syslog(LOG_ERR , "write\n");
          return -1;
      }
      file_size += write_len;
      realloc_int = 0, final_size = 1;
      char *send_buffer = (char *)malloc(sizeof(char) * file_size);
      if(send_buffer == NULL){
          syslog(LOG_ERR , "malloc\n");
          return -1;
      }

      //Check for beginning of file
      if((lseek(open_fd, 0, SEEK_SET))== -1) {
          perror("lseek error: ");
          return -1;

      }

      //Read buffer data from file
      sock_status = read(open_fd , send_buffer , file_size);
      if(sock_status == -1){
          syslog(LOG_ERR , "read\n");
          return -1;
      }

      //Send buffer data via socket
      sock_status = send(accept_fd , send_buffer , file_size , 0);
      if(sock_status == -1){
          syslog(LOG_ERR , "send\n");
          return -1;
      }
      free(send_buffer);
      receive_buffer = realloc(receive_buffer , BUFFER_SIZE);
      close(accept_fd);
      syslog(LOG_INFO , "Closed connection from %s\n", inet_ntoa(ip_client->sin_addr));

  }
  return 0;
}

