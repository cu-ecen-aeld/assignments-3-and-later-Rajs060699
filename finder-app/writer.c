/***************************************
@File Name:writer.c
@Author: Rajesh Srirangam
@Date :29 Jan,2023
@File Description:Used C file for writing a text string in a file
*******************************************/


/*********HEADERS************/
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>
#include <unistd.h>
#include <libgen.h>
#include <dirent.h>
/*********************************/

/*****************************
@Function:main.c
@Param :argc,argv
@Return:NULL
@Description:Used for writing a text string in a file
********************************/

int main (int argc,char*argv[])
{
//If argument count is 3 then parameters are valid or it is invalid
if(argc==3)                                   
{
syslog(LOG_DEBUG,"Parameters are valid");
}
else
{
syslog(LOG_ERR,"Parameters are not valid is %d",argc-1);
return 1;
}
int fp;
//Used for file opening
//argv[1]->writefile,O_RDWR (Read and Write Permission) | O_CREAT(Used for file creation) | O_TRUNC(Used for truncate the file),0777(Permission for owner,group and everyone is granted)
fp=open(argv[1],O_RDWR | O_CREAT | O_TRUNC,0777);     
if(fp==-1)
{
//File Open is failed
syslog(LOG_ERR,"Failed to open the file is %d",fp);    
return 1;                  
} 
//Used for writing string (argv[2]->writestr) to fp
int write_return_val=write(fp,argv[2],strlen(argv[2]));    
//Check for string length is equal to write string return value and also writer return value is negative
if(write_return_val!=strlen(argv[2]) || write_return_val ==-1)        
{
//printf("Failed to write the string to the file ");
syslog(LOG_ERR,"Failed to write the string to the file");  
return 1;
}
else
{
syslog(LOG_DEBUG,"Writing %s to %s",argv[2],argv[1]);
}
return 0;
}
