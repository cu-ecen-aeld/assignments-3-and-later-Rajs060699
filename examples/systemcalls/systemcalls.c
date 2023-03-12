/***********************************
//File systemcalls.c
//Author Rajesh Srirangam
//Date Feb 4,2023
//File Description Used for system calls initialisation
***************************************/

/************HEADERS***************/
#include "systemcalls.h"
#include <sys/types.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <fcntl.h>
/*********************************/

/**
 * @param cmd the command to execute with system()
 * @return true if the command in @param cmd was executed
 *   successfully using the system() call, false if an error occurred,
 *   either in invocation of the system() call, or if a non-zero return
 *   value was returned by the command issued in @param cmd.
*/
bool do_system(const char *cmd)
{

/*
 * TODO  add your code here
 *  Call the system() function with the command set in the cmd
 *   and return a boolean true if the system() call completed with success
 *   or false() if it returned a failure
*/

    if(system(cmd)==-1)                          //Command Processor is verified here
    {
    return false;
    }
    else
    {
    return true;
    }
}

/**
* @param count -The numbers of variables passed to the function. The variables are command to execute.
*   followed by arguments to pass to the command
*   Since exec() does not perform path expansion, the command to execute needs
*   to be an absolute path.
* @param ... - A list of 1 or more arguments after the @param count argument.
*   The first is always the full path to the command to execute with execv()
*   The remaining arguments are a list of arguments to pass to the command in execv()
* @return true if the command @param ... with arguments @param arguments were executed successfully
*   using the execv() call, false if an error occurred, either in invocation of the
*   fork, waitpid, or execv() command, or if a non-zero return value was returned
*   by the command issued in @param arguments with the specified arguments.
*/

bool do_exec(int count, ...)
{
    va_list args;
    va_start(args, count);
    char * command[count+1];
    int i;
    for(i=0; i<count; i++)
    {
        command[i] = va_arg(args, char *);
    }
    command[count] = NULL;
    // this line is to avoid a compile warning before your implementation is complete
    // and may be removed
    command[count] = command[count];

/*
 * TODO:
 *   Execute a system command by calling fork, execv(),
 *   and wait instead of system (see LSP page 161).
 *   Use the command[0] as the full path to the command to execute
 *   (first argument to execv), and use the remaining arguments
 *   as second argument to the execv() command.
 *
*/
pid_t ret_value=fork();          //Child process is created                                 
int val;
if(ret_value==-1)                //Child process is unsuccessful
{
	exit (-1);
}
else if(ret_value==0)           //Child process is successful
{
	if(execv(command[0],command)==-1) //Child process execution is failed
	{
		exit (-1);
	}
}		
else{
	if(waitpid(ret_value,&val,0)==-1)  //Wait for child process
	{
		return false;
		}
	if(WIFEXITED(val))      //Here child process is terminated normally
		{
			if(WEXITSTATUS(val)!=0) //Here process exit failed
			{
				return false;
			}
		}
	}

    va_end(args);

    return true;
}

/**
* @param outputfile - The full path to the file to write with command output.
*   This file will be closed at completion of the function call.
* All other parameters, see do_exec above
*/
bool do_exec_redirect(const char *outputfile, int count, ...)
{
    va_list args;
    va_start(args, count);
    char * command[count+1];
    int i;
    for(i=0; i<count; i++)
    {
        command[i] = va_arg(args, char *);
    }
    command[count] = NULL;
    // this line is to avoid a compile warning before your implementation is complete
    // and may be removed
    command[count] = command[count];


/*
 * TODO
 *   Call execv, but first using https://stackoverflow.com/a/13784315/1446624 as a refernce,
 *   redirect standard out to a file specified by outputfile.
 *   The rest of the behaviour is same as do_exec()
 *
*/

pid_t ret_value=fork();
int val;
if(ret_value==-1)            //Child process is unsuccessful
{
	exit (-1);
}
else if(ret_value==0)        //Child process is successful
{
	int fd = open(outputfile, O_WRONLY|O_TRUNC|O_CREAT, 0644);  //open() system call opens the file specified by pathname
	if (fd < 0) { perror("open"); abort(); } //file open failed
	if (dup2(fd, 1) < 0) { perror("dup2"); abort(); } //dup() system call allocates a new file
	close(fd);
	if(execv(command[0],command)==-1) //child execution fail
	{
		exit (-1);
	}
}		
else{
	if(waitpid(ret_value,&val,0)==-1) //Check child process pid argument has changed the state or not
	{
		return false;
		}
	if(WIFEXITED(val))               //Here child process is terminated normally
		{
			if(WEXITSTATUS(val)!=0) //Here process exit failed
			{
				return false;
			}
		}
	}


    va_end(args);

    return true;
}
