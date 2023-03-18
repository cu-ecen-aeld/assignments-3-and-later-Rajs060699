
#include <stdio.h>
#include <netdb.h>
#include <netinet/in.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <signal.h>
#include <syslog.h>
#include <errno.h>
#include <stdbool.h>
#include <pthread.h>
#include <stdarg.h>
#include <sys/queue.h>
#include <time.h>
#include <sys/time.h>
#include <poll.h>

/*macros*/
#define USE_AESD_CHAR_DEVICE 1

#ifdef USE_AESD_CHAR_DEVICE
#define FILE "/dev/aesdchar"
#else
#define FILE "/var/tmp/aesdsocketdata"
#endif

#define PORT "9000"
#define BUF_SIZE 1000
#define MAX_CONNECTIONS 10

/*global variables*/
static int servfd = -1;
static struct addrinfo  *updated_addr;
static pthread_mutex_t mutex_buffer = PTHREAD_MUTEX_INITIALIZER;
bool signal_recv = false;

typedef struct 
{
   int clientfd;
   pthread_t thread_id; 
   pthread_mutex_t* mutex;
   bool isComplete;
} thread_data; 

struct slist_data_s
{
   thread_data   params;
   SLIST_ENTRY(slist_data_s) entries;
};

typedef struct slist_data_s slist_data_t;

void cleanup()
{

    if(servfd > -1)
    {
        shutdown(servfd, SHUT_RDWR); 

        close(servfd);
    }

    pthread_mutex_destroy(&mutex_buffer);

    closelog();
}

static void signal_handler(int sig)
{
    syslog(LOG_INFO, "Signal Caught %d\n\r", sig);
    signal_recv = true;
    
    if(sig == SIGINT)
    {
       cleanup();
    }
    else if(sig == SIGTERM)
    {
       cleanup();
    }
}
 #ifndef USE_AESD_CHAR_DEVICE
void *timer_thread(void *args)
{
   size_t bufLen;
   time_t rawtime;
   struct tm *localTime;
   struct timespec request = {0, 0};
   int timeInSecs = 10; //Timer Interval

   while (!signal_recv)
   {
      /*obtain the current monotonic time*/    
      if (clock_gettime(CLOCK_MONOTONIC, &request))
      {
         syslog(LOG_ERR, "Error: failed to get monotonic time, [%s]\n", strerror(errno));
         continue;
      }
      
      /*update requested time to +1 second from previous obtained monotonic time*/
      request.tv_sec += 1; 
      request.tv_nsec += 1000000;
    
      /*precisely sleep for 1 second*/
      if (clock_nanosleep(CLOCK_MONOTONIC, TIMER_ABSTIME, &request, NULL) != 0)
      {
         if (errno == EINTR)
         {
            break; 
         }
      }

      /*ensure we sleep 10 seconds before adding timestamp to FILE*/
      if ((--timeInSecs) <= 0)
      {
         char buffer[100] = {0};
         
         time(&rawtime);        
         localTime = localtime(&rawtime); 
         
         /*timestamp in accordance to RFC2822 format*/
         bufLen = strftime(buffer, 100, "timestamp:%a, %d %b %Y %T %z\n", localTime);

         /*to open the file and write to it according to RFC 2822 spec*/
         int fd = open(FILE, O_RDWR | O_APPEND, 0644);
        
         if (fd < 0)
         {
            syslog(LOG_ERR, "failed to open a file:%d\n!!!", errno);
         }

         /*lock using mutex, since the file is getting updated from multiple threads*/
         int retVal = pthread_mutex_lock(&mutex_buffer);
        
         if(retVal)
         {
            syslog(LOG_ERR, "Error in locking the mutex");
            close(fd);
         }
        
         /*seek to the end of file, since we want to append the timestamp at the end*/
         lseek(fd, 0, SEEK_END); 

         int noOfBytesWritten = write(fd, buffer, bufLen);
        
         syslog(LOG_INFO, "Timestamp %s written to file\n", buffer);
   
         if (noOfBytesWritten < 0)
         {
            syslog(LOG_ERR, "Write of timestamp failed errno %d",errno);
         }
        
         retVal = pthread_mutex_unlock(&mutex_buffer);
        
         if(retVal)
         {
            syslog(LOG_ERR, "Error in unlocking the mutex\n\r");
            close(fd);
         }
        
         close(fd);
         
         /*update the timeInSecs variable for next cycle*/
         timeInSecs = 10;
      } 
  }

  pthread_exit(NULL);
}
#endif
void* handlePacketsOfEachClient(void* thread_params)
{  

    thread_data* params = (thread_data*)thread_params;

    char* client_read_buf = (char*)malloc(sizeof(char) * BUF_SIZE);
    
    if(client_read_buf == NULL)
    {
       syslog(LOG_ERR,"malloc failed %d\n\r", (int)(params->thread_id));
       params->isComplete = true;
    }
    else
    {
       /*zero contents of client_read_buf*/
       memset(client_read_buf, 0, BUF_SIZE);
    }

    uint32_t ctr = 1; 
    int curr_pos = 0;

    while(!(params->isComplete))
    {
       /*to read packets from client*/
       int noOfBytesRead = read(params->clientfd, client_read_buf + curr_pos, BUF_SIZE);
        
       if (noOfBytesRead < 0) 
       {
          syslog(LOG_ERR, "reading from socket errno=%d\n", errno);
          free(client_read_buf);
          params->isComplete = true;
          pthread_exit(NULL);
       }

       if (noOfBytesRead == 0)
           continue;

       curr_pos += noOfBytesRead;
       
       /*
        if a newline is found, it implies we have recived the complete packet
        thus we break and proceed to next steps
        */
       if (strchr(client_read_buf, '\n')) 
       {
           break;
       } 
       
       /*else we need to expand the size of our buffer, therefore we realloc*/
       ctr++;
       client_read_buf = (char*)realloc(client_read_buf, (ctr * BUF_SIZE));
       
       if(client_read_buf == NULL)
       {
          syslog(LOG_ERR,"realloc error %d\n\r", (int)params->thread_id);
          free(client_read_buf);
          params->isComplete = true;
          pthread_exit(NULL);
       }
    }

    /* open the file/device */
    int fd = open(FILE, O_RDWR | O_APPEND, 0766);
    
    if (fd < 0)
    {
       syslog(LOG_ERR, "failed to open a file: %d\n\r", errno);
    }

    /*to seek to end of the file in order to write to the device*/
    lseek(fd, 0, SEEK_END);
    
    int retVal = pthread_mutex_lock(params->mutex);
    if(retVal)
    {
        free(client_read_buf);
        params->isComplete = true;
        pthread_exit(NULL);
    }

    /*write the packet content read to the device*/
    int writeByteCount = write(fd, client_read_buf, curr_pos);
    if(writeByteCount < 0)
    {
        syslog(LOG_ERR, "write to device failed: errno %d\n\r", errno);
        free(client_read_buf);
        params->isComplete = true;
        close(fd);
        pthread_exit(NULL);
    }

    /*to reset file offset back to beginning of file*/
    lseek(fd, 0, SEEK_SET);

    retVal = pthread_mutex_unlock(params->mutex);
    if(retVal)
    {
        free(client_read_buf);
        params->isComplete = true;
        pthread_exit(NULL);
    }

    close(fd);
    printf("wrote %d bytes\n\r", writeByteCount);
    
    int read_offset = 0;

    /*to open the device for reading*/
    int fd_dev = open(FILE, O_RDWR | O_APPEND, 0766);
    if(fd_dev < 0)
    {
        syslog(LOG_ERR, "FILE open failed, errno=%d\n", errno);
        free(client_read_buf);
        params->isComplete = true;
        pthread_exit(NULL);    
    }

    /*move offset back to start since we have used write() previously*/
    lseek(fd_dev, read_offset, SEEK_SET);

    char* client_write_buf = (char*)malloc(sizeof(char) * BUF_SIZE);

    curr_pos = 0;
    
    /*to clear the buffer contents*/
    memset(client_write_buf,0, BUF_SIZE);
    
    /*intially set the reallocing buffer counter to 1*/
    ctr = 1;
    
    while(1) 
    {
       retVal = pthread_mutex_lock(params->mutex);
        
       if(retVal)
       {
          free(client_read_buf);
          free(client_write_buf);
          params->isComplete = true;
          pthread_exit(NULL);
       }

       /*read the device one byte at a time and proceed*/
       int noOfBytesRead = read(fd_dev, &client_write_buf[curr_pos], 1);

       retVal = pthread_mutex_unlock(params->mutex);   
       
       if(retVal)
       {
          free(client_read_buf);
          free(client_write_buf);
          params->isComplete = true;
          pthread_exit(NULL);
       }

       if(noOfBytesRead < 0)
       {
          syslog(LOG_ERR, "Error reading from file errno %d\n", errno);
          break;
       }

       if(noOfBytesRead == 0)
           break;

        if(client_write_buf[curr_pos] == '\n')
        {
            /*write the buf contents read from FILE above to the client*/
           int noOfBytesWritten = write(params->clientfd, client_write_buf, curr_pos + 1 );
           
           printf("wrote %d bytes to client\n", noOfBytesWritten);

           if(noOfBytesWritten < 0)
           {
              printf("errno is %d", errno);
              syslog(LOG_ERR, "Error writing to client fd %d\n", errno);
              break;
           }
           memset(client_write_buf, 0, (curr_pos + 1));

           curr_pos = 0;
        } 
        else 
        {
           curr_pos++;
           
           /*if we are unable to find '\n; i.e end of a packet, it implies that we need
             a larger buffer to read contents of the whole packet from the device. Thus
             we need a realloc*/
           if(curr_pos > sizeof(client_write_buf))
           {
              ctr++;
              
              /*reallocate BUF_SIZE amount of bytes every time*/
              client_write_buf = realloc(client_write_buf, ctr * BUF_SIZE);
              
              if(client_write_buf == NULL)
              {
                 syslog(LOG_ERR,"realloc error %d\n\r", (int)params->thread_id);
                 free(client_write_buf);
                 free(client_read_buf);
                 params->isComplete = true;
                 pthread_exit(NULL);
              }
           }   
        }
    }

    /*to clean up all the resources*/
    close(fd_dev);
    
    free(client_write_buf);
    free(client_read_buf);
    
    params->isComplete = true;
    
    pthread_exit(NULL);
}

/*point of entry*/
int main(int argc, char **argv) 
{

   openlog(NULL, 0, LOG_USER);

   /*register signal handlers*/
   if (signal (SIGINT, signal_handler) == SIG_ERR) 
   {
      syslog(LOG_ERR, "Cannot handle SIGINT!\n");
      cleanup();
   }

   if (signal (SIGTERM, signal_handler) == SIG_ERR) 
   {
      syslog(LOG_ERR, "Cannot handle SIGTERM!\n");
      cleanup();
   }

   bool createDeamon = false;
   
   /*check if need to createDeamon*/
   if (argc == 2) 
   {
      if (!strcmp(argv[1], "-d")) 
      {
         createDeamon = true;
      } 

      else 
      {
         syslog(LOG_ERR, "incorrect argument:%s", argv[1]);
         return (-1);
      }
   }
   
   /*create a FILE*/
   int write_fd = creat(FILE, 0766);

    if(write_fd < 0)
    {
       syslog(LOG_ERR, "aesdsocketdata file could not be created, error number %d", errno);
       cleanup();
       exit(1);
    }
    close(write_fd);
    
    /*Initialize the linked list*/
    slist_data_t *listPtr = NULL;
    
    SLIST_HEAD(slisthead, slist_data_s) head;
    SLIST_INIT(&head);

    struct addrinfo addr_hints;
    memset(&addr_hints, 0, sizeof(addr_hints));

    addr_hints.ai_family = AF_INET;
    addr_hints.ai_socktype = SOCK_STREAM;
    addr_hints.ai_flags = AI_PASSIVE;

    /*fill the updated_addr with necessary socket configurations*/
    int res = getaddrinfo(NULL, (PORT), &addr_hints, &updated_addr);
     
    if (res != 0) 
    {
        syslog(LOG_ERR, "getaddrinfo() error %s\n", gai_strerror(res));
        cleanup();
        return -1;
    }

    /* create socket for communication */
    servfd = socket(updated_addr->ai_family, SOCK_STREAM, 0);

    if (servfd < 0) 
    {
        syslog(LOG_ERR, "Socket creation failed:%d\n", errno);
        cleanup();
        return -1;
    }
    
    /*setsockopt with SO_REUSEADDR to avoid address already in use error*/
    if (setsockopt(servfd, SOL_SOCKET, SO_REUSEADDR, &(int){1}, sizeof(int)) < 0) 
    {
        syslog(LOG_ERR, "setsockopt failed with errono:%d\n", errno);
        cleanup();
        return -1;
    }

    /*bind the socket*/
    if (bind(servfd, updated_addr->ai_addr, updated_addr->ai_addrlen) < 0)
    {
        syslog(LOG_ERR, "failed to bind:%d\n", errno);;
        cleanup();
        return -1;
    }

    freeaddrinfo(updated_addr);

    /*listen for connections*/
    if (listen(servfd, MAX_CONNECTIONS)) 
    {
       syslog(LOG_ERR, "ERROR:failed to listen %d\n", errno);
       cleanup();
        return -1;
    }

    printf("waiting for client connections\n\r");

    /*create a daemon from this process*/
    if (createDeamon == true) 
    {  
        int retVal = daemon(0,0);

        if(retVal < 0)
        {
           syslog(LOG_ERR,"Failed to create daemon");
           cleanup();
           return -1;
        } 
    }

    /*in order to handle timestamp requirement, spawn a timer thread*/
#ifndef USE_AESD_CHAR_DEVICE
    pthread_t timer_thread_id; 
    pthread_create(&timer_thread_id, NULL, timer_thread, NULL);
#endif

    while(!(signal_recv)) 
    {
        struct sockaddr_in clientAddr;
        socklen_t clientAddrSize = sizeof(clientAddr);
        int clientfd = accept(servfd, (struct sockaddr*)&clientAddr, &clientAddrSize);

        if(signal_recv)
        {
            break;
        }

        if(clientfd < 0)
        {
            syslog(LOG_ERR, "failed to accept the connection: %s", strerror(errno));
            cleanup();
            return -1;
        }
        char client_info[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &(clientAddr.sin_addr), client_info, INET_ADDRSTRLEN);
        syslog(LOG_INFO, "Connection Accepted %s \n\r",client_info);
        listPtr = (slist_data_t *) malloc(sizeof(slist_data_t));
        SLIST_INSERT_HEAD(&head, listPtr, entries);
        listPtr->params.clientfd     = clientfd;
        listPtr->params.mutex        = &mutex_buffer;
        listPtr->params.isComplete   = false;
        pthread_create(&(listPtr->params.thread_id), NULL, handlePacketsOfEachClient, (void*)&listPtr->params);
        SLIST_FOREACH(listPtr, &head, entries)
        {     
            if(listPtr->params.isComplete == true)
            {
                pthread_join(listPtr->params.thread_id,NULL);
                shutdown(listPtr->params.clientfd, SHUT_RDWR);
                
                close(listPtr->params.clientfd);
                
                syslog(LOG_INFO, "Join spawned thread:%d\n\r",(int)listPtr->params.thread_id); 
            }
        }
    }

#ifndef USE_AESD_CHAR_DEVICE
    /*join the timer thread*/
    pthread_join(timer_thread_id, NULL);
#endif

    /*clean up pending nodes if any*/
    while (!SLIST_EMPTY(&head))
    {
        listPtr = SLIST_FIRST(&head);
        
        pthread_cancel(listPtr->params.thread_id);
        
        syslog(LOG_INFO, "Thread is killed:%d\n\r", (int) listPtr->params.thread_id);
        
        SLIST_REMOVE_HEAD(&head, entries);
        
        free(listPtr); 
        
        listPtr = NULL;
    }

    /*finally delete the FILE*/
    if (access(FILE, F_OK) == 0) 
    {
       remove(FILE);
    }
    
    /*and also the socket file descriptor*/
    cleanup();

    exit(0);
}

