/***********************************************/
/*          Jakub Mackovič - xmacko00          */
/*          Jakub Pastuszek - xpastu00         */
/*               VUT FIT Brno                  */
/*      Export informací o překladu adres      */
/*               Květen 2017                   */
/*               daemonize.c                   */
/***********************************************/

#include <pthread.h>
#include <unistd.h>

#include "daemonize.h"

int daemonize(void)
{
    /* Our process ID and Session ID */
    pid_t pid, sid;
    
    /* Fork off the parent process */
    pid = fork();
    if (pid < 0) {
            return 0;
    }
    /* If we got a good PID, then
       we can exit the parent process. */
    if (pid > 0) { // Child can continue to run even after the parent has finished executing
            exit(0);
    }

    /* Change the file mode mask */
    umask(0);
            
    /* Open any logs here */        
            
    /* Create a new SID for the child process */
    sid = setsid();
    if (sid < 0) {
            /* Log the failure */
            return 0;
    }
           
    /* Change the current working directory */
//        if ((chdir("/")) < 0) {
            /* Log the failure */
//               return 0;
//      }
    
    /* Close out the standard file descriptors */
    //Because daemons generally dont interact directly with user so there is no need of keeping these open
    close(STDIN_FILENO);
    close(STDOUT_FILENO);
    close(STDERR_FILENO);
    
    /* Daemon-specific initialization goes here */
   
    return 1; 
    /* An infinite loop */
}
