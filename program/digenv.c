/*
 * NAME:
 *   digenv - a program for browsing environment variables
 * 
 * SYNTAX:
 *   digenv [PARAMETERS]
 *
 * DESCRIPTION:
 *   The digenv program is the equivalent of executing:
 *      printenv | sort | less 
 *   in your everyday system shell. If parameters are given, it will run:
 *      printenv | grep PARAMETERS | sort | less
 *   With the exception of that instead of less it will use whatever you've set 
 *   as your PAGER environment variable. 
 *   If PAGER isn't set, less is used. 
 *   If less doesn't exist, more is used.
 *   
 * EXAMPLES:
 *   digenv PATH
 *
 * AUTHOR:
 *   Writted by Carl-Oscar Erneholm (coer@kth.se) 
 *          and Martin Nycander     (mnyc@kth.se)
 *
 * SEE ALSO:
 *   printenv(1), grep(1), sort(1), less(1)
 */

#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/wait.h>

#define PIPEREAD  0
#define PIPEWRITE 1

/* If the name of your pager is more than 50 chars, I hate you. */
char PAGER[50];

/*
 * Performs a fork, aborts exeuction if it fails.
 */
int checked_fork()
{
    int cpid = fork();
    if (cpid == -1)
    {
        fprintf(stderr, "Could not fork, aborting execution.\n");
        exit(errno);
    }
    return cpid;
}

/*
 * Closes a file, aborts execution if it fails.
 */
void checked_close(int p)
{
    if(close(p) == -1 )
    {
        fprintf(stderr, "Could not close pipe.");
        exit(EXIT_FAILURE);
    }
}

/*
 * Opens a pipe, aborts execution if it fails.
 */
void checked_pipe(int pipes[2])
{
    if(pipe(pipes) == -1)
    {
        fprintf(stderr, "Could not create a pipe, aborting execution.\n");
        exit(EXIT_FAILURE);
    }
}

/* 
 * Attempts to find the environment variable PAGER
 * and sets the global variable accordingly.
 *
 * If no such variable exists, 'less' is used.
 *
 * If 'less' does not exists, 'more' is used.
 * 
 */
int find_and_set_pager()
{
    int cid, less_existance, devnull;

    char * pager = getenv("PAGER");
    if (pager != 0)
    {
        strcpy(PAGER, pager);
        return 0;
    }

    /* Use less instead? (check with 'which'-command) */
    cid = checked_fork();
    if (cid == 0)
    {
        /* Ignoring output */
        devnull = open("/dev/null", O_WRONLY); 
        dup2(devnull, STDOUT_FILENO);
        dup2(devnull, STDERR_FILENO);
        execlp("which", "which", "less", NULL);
        exit(1);
    }
    waitpid(cid, &less_existance, 0);
    less_existance = WEXITSTATUS(less_existance);

    if (less_existance == 0)
    {
        /* Less exists. Use it. */
        strcpy(PAGER, "less");
        return 0;
    }
    /* Use more instead. */
    strcpy(PAGER, "more");
    return 1;
}

/*
 * Waits for a process to exit and checks the return value.
 */
void wait_and_error_check_process(int pid)
{
    int status;
    
    /* Wait for process to die */
    waitpid(pid, &status, 0);

    /* Check return value */
    status = WEXITSTATUS(status);
    if (status != 0)
    {
        exit(EXIT_FAILURE);
    }
}

/*
 * Main program
 */
int main(int argc, char* argv[])
{
    int i = 0;
    int pipes[4][2];
    int cpid;

    /* Figure out what pager to use */
    find_and_set_pager();

    /* Open a pipe to use for printenv */
    checked_pipe(pipes[i]);

    /* Open printenv in a child process */
    cpid = checked_fork();
    if(cpid == 0)
    {
        /* Close unneeded pipe.*/
        checked_close(pipes[i][PIPEREAD]);
        dup2(pipes[i][PIPEWRITE], STDOUT_FILENO);
        checked_close(pipes[i][PIPEWRITE]);
        execlp("printenv", "printenv", NULL);
        exit(EXIT_FAILURE);
    }
    wait_and_error_check_process(cpid);

    /* Grep */
    checked_pipe(pipes[++i]);

    /* Only run grep if there are arguments */
    if(argc > 1)
    {
        cpid = checked_fork();
        if(cpid == 0)
        {
            /* Close unused pipes.*/
            checked_close(pipes[i-1][PIPEWRITE]);
            checked_close(pipes[i][PIPEREAD]);

            /* Put the pipes into place.*/
            dup2(pipes[i-1][PIPEREAD], STDIN_FILENO);
            dup2(pipes[i][PIPEWRITE], STDOUT_FILENO);
            checked_close(pipes[i-1][PIPEREAD]);
            checked_close(pipes[i][PIPEWRITE]);
            argv[0] = "grep";
            execvp("grep", argv);
            exit(EXIT_FAILURE);
        }
        /* We wont be using this pipe anymore so lets close it.*/
        checked_close(pipes[i-1][PIPEREAD]);
        checked_close(pipes[i-1][PIPEWRITE]);

        wait_and_error_check_process(cpid);

        /* Open a pipe to the next process */
        checked_pipe(pipes[++i]);
    }

    /* Sort */
    cpid = checked_fork();
    if(cpid == 0)
    {
        /* Close unused pipes.*/
        checked_close(pipes[i-1][PIPEWRITE]);
        checked_close(pipes[i][PIPEREAD]);

        /* Put the pipes into place.*/
        dup2(pipes[i-1][PIPEREAD], STDIN_FILENO);
        dup2(pipes[i][PIPEWRITE], STDOUT_FILENO);
        checked_close(pipes[i-1][PIPEREAD]);
        checked_close(pipes[i][PIPEWRITE]);
        execlp("sort", "sort", NULL);
        exit(EXIT_FAILURE);
    }
    checked_close(pipes[i-1][PIPEREAD]);
    checked_close(pipes[i-1][PIPEWRITE]);

    wait_and_error_check_process(cpid);

    /* Pager */
    cpid = checked_fork();
    if(cpid == 0)
    {
        /* Close unused pipe.*/
        checked_close(pipes[i][PIPEWRITE]);
        dup2(pipes[i][PIPEREAD], STDIN_FILENO);
        checked_close(pipes[i][PIPEREAD]);
        execlp(PAGER, PAGER, NULL);
        exit(EXIT_FAILURE);
    }
    checked_close(pipes[i][PIPEREAD]);
    checked_close(pipes[i][PIPEWRITE]);
    wait_and_error_check_process(cpid);
    return 0;
}
