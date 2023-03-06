/*******************************
Author Rajesh Srirangam
Description : Socket Server Implementation
Reference   : https://beej.us/guide/bgnet/html/
              https://man7.org/linux/man-pages/man3/daemon.3.html
************************************/

/*****HEADERS*******/
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <syslog.h>
#include <signal.h>
#include <sys/stat.h>
#include <linux/fs.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <sys/queue.h>
#include "queue.h"
#include <pthread.h>
#include <time.h>
#include <sys/time.h>
/*********************/

/****MACROS******/
#define BACKLOGS    10
#define BUFFER_SIZE 512
/********************/

int socket_fd = 0, accept_fd = 0,open_fd=0;
char *receive_buffer = NULL;
char *chec_ptr=NULL;
int total_packet_size=0;
SLIST_HEAD(slisthead, slist_data_s) head;
typedef struct thread_data
{
	pthread_mutex_t *mutex;
	pthread_t thread_id;
	bool thread_complete_success;
	int accept_socket_fd;
}thread_data_t;

struct slist_data_s
{
	thread_data_t thread_data_args;
	SLIST_ENTRY(slist_data_s) entries;
};
typedef struct slist_data_s slist_data_t;
pthread_mutex_t mutex1;

static void signal_handler (int signal)
{
	if(signal== SIGINT)
	{
		syslog(LOG_DEBUG,"Caught Signal SIGINT, exiting");						
	}
	
	else if(signal== SIGTERM)
	{
		syslog(LOG_DEBUG,"Caught Signal SIGTERM, exiting");
		
	}
	/*else if(signal==SIGALRM)
	{
		syslog(LOG_DEBUG,"Caught Signal SIGALRM, exiting");
	}*/
	

	shutdown(socket_fd, SHUT_RDWR);
 	shutdown(accept_fd, SHUT_RDWR);
 	close(socket_fd);
 	close(accept_fd);
 	close(open_fd);
 	unlink("/var/tmp/aesdsocketdata");
 	slist_data_t *add_thread = NULL;
 	while (SLIST_FIRST(&head) != NULL)
	{
 	SLIST_FOREACH(add_thread, &head, entries)
        {
		pthread_join(add_thread->thread_data_args.thread_id, NULL);
        	SLIST_REMOVE(&head, add_thread, slist_data_s, entries);
        	free(add_thread);
        	break;
      	}
      	}
	exit(0);				
}


void daemon_create()
{
  int sock_status = 0;
  pid_t pid;
  pid = fork ();
  if (pid == -1){
      syslog(LOG_ERR , "fork\n");
      exit(-1);
  }
  else if (pid != 0){
      exit (EXIT_SUCCESS);
  }

  sock_status = setsid();
  if (sock_status == -1){
      syslog(LOG_ERR , "setsid\n");
      exit(-1);
  }

  sock_status = chdir("/");
  if (sock_status == -1){
      syslog(LOG_ERR , "chdir\n");
      exit(-1);
  }
  open ("/dev/null", O_RDWR);
  dup (0);
  dup (0);				
}


void alarm_handler ()
{

	
	int status =0;
	int open_fd=0;
	char time_string[100];
	time_t rawtime;
	struct tm* currenttime;
	time (&rawtime);
	currenttime = localtime (&rawtime);
	strftime(time_string, sizeof(time_string), "timestamp:%T %d %b %a %Y %z\n", currenttime); 
	
	open_fd = open("/var/tmp/aesdsocketdata", O_RDWR | O_APPEND , 0644);
	if (open_fd == -1)
	{
		syslog(LOG_ERR,"File cannot be opened in WRITE mode");
		perror("WRITE");
		exit(-1);
	}
	syslog(LOG_DEBUG, "OPEN status %d", open_fd);
	status = pthread_mutex_lock(&mutex1);
	if(status)
	{
		syslog(LOG_ERR, "Mutex_lock error");
		
	}
	
	lseek(open_fd, 0, SEEK_END);
	status = write(open_fd, time_string, strlen (time_string));
	syslog(LOG_DEBUG, "Number of bytes written %d", status);
	if (status == -1)
	{
		syslog(LOG_ERR,"The received data is not written");
		exit(-1);
	}
	status = pthread_mutex_unlock(&mutex1);
	if(status) 
	{
		syslog(LOG_ERR, "ERROR: mutex_lock() fail");
		
	}
	close(open_fd);
	
	
}
void signal_init()
{
	if (signal(SIGINT, signal_handler) == SIG_ERR)
	{
		syslog(LOG_ERR,"Error in handling SIGINT");
		exit(-1);
	}
	if (signal(SIGTERM, signal_handler) == SIG_ERR)
	{
		syslog(LOG_ERR,"Error in handling SIGINT");
		exit(-1);
	}


}
void *thread_func(void *thread_param)
{

	char *send_buffer = NULL;
	int bytes_read=0;
	int byte_count=0;
	int receive_bytes = 0;
	int status=0;
  	bool packet_complete = false;
  	int final_size=1;
  	char temp_buffer[BUFFER_SIZE];
	receive_buffer = (char *) malloc (BUFFER_SIZE * sizeof(char));
	thread_data_t *thread_data = (thread_data_t *)thread_param;
	if (receive_buffer == NULL)
	{
		syslog(LOG_ERR,"malloc() error"); 
		printf("Malloc Error \n");
      		exit(-1);
	
	}
	memset(receive_buffer,0,BUFFER_SIZE);
 
        memset(temp_buffer, 0, BUFFER_SIZE);
        
        while (!packet_complete)
        	{
        		receive_bytes = recv(thread_data->accept_socket_fd, temp_buffer, BUFFER_SIZE, 0);
        		if (receive_bytes == -1)
        		{
        			syslog(LOG_ERR,"Accept command error");
        			printf("Receive comamnd error\n");
        			exit(-1);
        		}
			else{
			final_size++;
			for (byte_count = 0; byte_count < BUFFER_SIZE; byte_count++)
			{
				if(temp_buffer[byte_count] == '\n')			
				{
					packet_complete = true;
					syslog(LOG_DEBUG,"String received is %s",temp_buffer);
					break;
				}
			
			}
			}
		        
		        total_packet_size = total_packet_size + receive_bytes;
		        receive_buffer = (char *) realloc (receive_buffer, total_packet_size + 1);
		        if(final_size >=BUFFER_SIZE) 
                {
                  if(!(chec_ptr = realloc(receive_buffer, BUFFER_SIZE + final_size + 1)))
                    {
                      syslog(LOG_ERR , "realloc\n");
                      exit(-1);
                    }
                  else
                    {
                      receive_buffer = chec_ptr;
                      strncat(receive_buffer, temp_buffer, final_size);
                    }

                }
		        if (receive_buffer == NULL)
		        {
		        	syslog(LOG_ERR,"Realloc() Error");
        			printf("Realloc() Error \n");
        			break;
		        } 
		        strncat(receive_buffer, temp_buffer, receive_bytes);
			syslog(LOG_DEBUG,"Appended String is %s",receive_buffer);
			}
			syslog(LOG_DEBUG,"Appended String is %s",receive_buffer);
		        open_fd = open("/var/tmp/aesdsocketdata", O_RDWR | O_APPEND , 0644);
			if (open_fd == -1)
			{
				syslog(LOG_ERR,"File cannot be opened in WRITE mode");
				perror("WRITE");
				exit(-1);
			}
			syslog(LOG_DEBUG, "OPEN status %d", open_fd);
			status = pthread_mutex_lock(thread_data->mutex);
			if(status)
			{
				syslog(LOG_ERR, "ERROR: mutex_lock() fail");
		
			}
	
			lseek(open_fd, 0, SEEK_END);
			status = write(open_fd, receive_buffer, strlen (receive_buffer));
			if (status == -1)
			{
				syslog(LOG_ERR,"The received data is not written ");
				exit(-1);
			}
			status = pthread_mutex_unlock(thread_data->mutex);
			if(status) 
			{
			syslog(LOG_ERR, "ERROR: mutex_lock() fail");
			}
			int file_size = lseek(open_fd, 0, SEEK_CUR);
			lseek(open_fd, 0, SEEK_SET);
			send_buffer = (char *) malloc(file_size * sizeof(char));
			status = pthread_mutex_lock(thread_data->mutex);
		        bytes_read = read(open_fd, send_buffer, file_size );
			if(bytes_read == -1)
			{
			
				syslog(LOG_ERR,"Error in Reading");
				exit(-1);
			}
			status = send(thread_data->accept_socket_fd, send_buffer, bytes_read, 0);
			if(status == -1)
			{
				syslog(LOG_ERR,"Sending to host failed");
				exit(-1);
			}
			status = pthread_mutex_unlock(thread_data->mutex);
			thread_data->thread_complete_success=true;
			close(open_fd);
			close(thread_data->accept_socket_fd);
			free(send_buffer);					
			free(receive_buffer);	     
			receive_buffer=NULL;
			send_buffer=NULL;   
			return (thread_data);
		  
 }

int main(int argc,  char *argv[])
{
	struct itimerval delay;
	signal (SIGALRM, alarm_handler);
	delay.it_value.tv_sec =10;
	delay.it_interval.tv_sec=10;
	delay.it_value.tv_usec =0;
	delay.it_interval.tv_usec=0;
	
	struct addrinfo hints;
	struct addrinfo *result, *temp_ptr;  	
	struct sockaddr_in host_address;
	socklen_t host_address_size;
	slist_data_t *add_thread=NULL;
	int status=0;
	
	SLIST_INIT(&head);
	signal_init();

       // signal(SIGINT , signal_handler);
        //signal(SIGTERM , signal_handler);
	openlog(NULL , 0 , LOG_USER);
	printf("Welcome to Socket \n");
	memset(&hints, 0, sizeof hints); 
	hints.ai_family = AF_UNSPEC;    
	hints.ai_socktype = SOCK_STREAM; 
	hints.ai_flags = AI_PASSIVE;    
	status =  getaddrinfo(NULL,  "9000" , &hints, &result);
        if (status != 0)
        {
      		syslog(LOG_ERR,"Getaddrinfo error");  	
      		exit(-1);
        }
        

        for(temp_ptr = result; temp_ptr != NULL; temp_ptr = temp_ptr->ai_next)
        { 
        	socket_fd= socket(result->ai_family, result->ai_socktype, 0);
        	if (socket_fd == -1)
        	{
        		syslog(LOG_ERR,"Socket Creation error");  	
      			exit(-1);
        
       	}
       	status = setsockopt(socket_fd, SOL_SOCKET, SO_REUSEADDR, &(int){1}, sizeof(int));
		status= bind(socket_fd, result->ai_addr, result->ai_addrlen);
		if (status == 0)
        	{
        		syslog(LOG_DEBUG,"Bind is successful");  	
      			break; 
       	}
       }
        
       freeaddrinfo(result);
       if(temp_ptr == NULL)
       {
       	syslog(LOG_DEBUG,"ERROR: bind() fail");
       	exit(-1);
       
       }
	if( argc > 1 && !strcmp("-d", argv[1]) ) 
	{
		syslog(LOG_INFO, "Run as daemon");
		daemon_create();
	}
	status= listen(socket_fd, BACKLOGS);
       if (status == -1)
       {
        	syslog(LOG_ERR,"Error in listen");  	
      		exit(-1);
        
        }
        
       	open_fd = creat("/var/tmp/aesdsocketdata", 0644);
        	if (open_fd == -1)
        	{
        		syslog(LOG_ERR,"File cannot be created");
        		exit(-1);        	
        	}
      
      	status = setitimer (ITIMER_REAL, &delay, NULL);
	if(status)
	{
		syslog(LOG_ERR, "Error in timer set");
		exit (-1);
	}
	
	status = pthread_mutex_init(&mutex1, NULL);
	if (status != 0)
	{
		syslog(LOG_ERR, "Mutex Initialization error");
		exit (-1);
	
	}
        while (1)
        {

        	host_address_size = sizeof(struct sockaddr);
        	accept_fd= accept(socket_fd, (struct sockaddr *)&host_address, &host_address_size);
        	if (accept_fd == -1)
        	{
        		syslog(LOG_ERR,"Accept comamnd error");
        		exit(-1);
        	}
        	syslog(LOG_DEBUG, "Accepted connection from %s\n", inet_ntoa(host_address.sin_addr)); 
        	printf("Accepted connection from %s\n", inet_ntoa(host_address.sin_addr));
        	add_thread = (slist_data_t *) malloc(sizeof(slist_data_t));  
        	SLIST_INSERT_HEAD(&head, add_thread, entries);     
        	add_thread->thread_data_args.mutex = &mutex1;
        	add_thread->thread_data_args.accept_socket_fd = accept_fd;
        	add_thread->thread_data_args.thread_complete_success = false;
        	pthread_create(&(add_thread->thread_data_args.thread_id), NULL, thread_func, &(add_thread->thread_data_args));
        	

        	SLIST_FOREACH(add_thread, &head, entries)
        	{
        		pthread_join(add_thread->thread_data_args.thread_id, NULL);
        		SLIST_REMOVE(&head, add_thread, slist_data_s, entries);
        		free(add_thread);
        		syslog(LOG_DEBUG, "Closed connection from %s\n",inet_ntoa(host_address.sin_addr)); 
        	      	break;
        	        	
        	}
        	
        	
	}
	//closelog();
}













