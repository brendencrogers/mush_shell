#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/wait.h>
#include <signal.h>
#include "mush2.h"
#include "mush.h"

#define READ 0
#define WRITE 1
#define MAX_FD 1028

int main(int argc, char *argv[]) {
    
    /* mush shell */
    FILE* fp;
    pipeline pipeln;
    char* prompt = "8-P ";
    struct clstage curr_stage;
    int fd;
    int olen, plen;
    int i;
    int* all_pipes;
    struct sigaction sigact;
    sigact.sa_flags = 0;
    sigact.sa_handler = handler;
    sigemptyset(&sigact.sa_mask);
    if (sigaction(SIGINT, &sigact, NULL) == -1) {
        perror("sigaction");
        exit(1);
    }

    /* Check args and open appropriate inputs/outputs */
    if (argc < 2) {
        fp = fdopen(STDIN_FILENO, "r");
    }

    else {
        /* If more than 1 arg */
        fp = fopen(argv[1], "r");
        if (fp == NULL) {
            perror("open");
            exit(1);
        }
    }

    /* Prompt and read command */
    char *line = NULL;

    while (!feof(fp)) {
        if ((isatty(STDIN_FILENO) == 1) && (isatty(STDOUT_FILENO) == 1) && (argc == 1)) {
            printf("%s", prompt);
        }
        line = readLongString(fp);
        if (!line) {
            /* Reprompt */
        }
        else {
            pipeln = crack_pipeline(line);
            if (!pipeln) {
                perror("Invalid pipeline");
            }
            curr_stage = pipeln->stage[0];
            olen = pipeln->length;
            plen = olen - 1; /* may not be necessary */
            all_pipes = malloc(sizeof(int) * (2 * olen));
            /*
            print_pipeline(fdopen(STDOUT_FILENO, "w"), pipeln);
            */

            /* Set up pipes for Length - 1 */
            int pipe_cnt = 0;

            for (i = 0; i < olen; i++) {
                if (i < plen) {
                    /* Create pipes for pipe len */
                    if (pipe((all_pipes + (i * 2))) == -1) {
                        perror("pipe");
                        exit(1);
                    }
                    pipe_cnt++;
                }
            }
            /*
            printf("Pipe Count: %d\n", pipe_cnt);
            printf("Length: %d\n", pipeln->length);
            */

            /* Fork processes */

            curr_stage = pipeln->stage[0];
            int k = 0;
            int status[olen];
            int cd_arg = 0;

            for (i = 0; i < olen; i++) {
                
                if (strcmp(curr_stage.argv[0], "cd") == 0) {
                    /* Check for cd */
                    cd_arg = 1;
                    if (curr_stage.argv[1] == NULL) {
                        /* cd without args */
                        if (chdir(getenv("HOME")) == -1) {
                            perror("cd");
                            exit(1);
                        }
                    }
                    else {
                        /* cd with args */
                        if (chdir(curr_stage.argv[1]) == -1) {
                            perror("cd");
                            exit(1);
                        }
                    }
                }
                
                else {
                    /* Execute command */
                    int pid = fork();
                    curr_stage = pipeln->stage[i];
                    /*printf("Current_stage: %s\n", curr_stage.argv[0]);*/
                    
                    if (pid == 0) {
                        /* Child process */
                        
                        if (i == 0) {
                            /* First process - Move write to stdout */
                            if (curr_stage.outname == NULL) {
                                if (dup2(all_pipes[1], STDOUT_FILENO) == -1) {
                                    perror("dup2 1");
                                    exit(1);
                                }
                            }
                            else {
                                fd = open(curr_stage.outname, O_RDWR | O_CREAT | O_TRUNC, 0666);
                                if (fd == -1) {
                                    perror("open");
                                    exit(1);
                                }
                                if (dup2(fd, STDOUT_FILENO) == -1) {
                                    perror("dup2 2");
                                    exit(1);
                                }
                            }
                            
                            for (k = 0; k < (plen * 2); k++) { /* Plen or olen?
                                printf("Closing fd (First process): %d\n", all_pipes[k]); */
                                if (close(all_pipes[k]) == -1) {
                                    perror("close 1");
                                    exit(1);
                                }
                            }
                            execvp(curr_stage.argv[0], curr_stage.argv);
                            perror(curr_stage.argv[0]);
                            exit(1);
                        }
                        else if (i == plen) {
                            /* Last process - Move read to stdin */
                            if (curr_stage.inname == NULL) {
                                if (dup2(all_pipes[(i * 2) - 2], STDIN_FILENO) == -1) {
                                    perror("dup2 3");
                                    exit(1);
                                }
                            }
                            else {
                                fd = open(curr_stage.inname, O_RDONLY);
                                if (fd == -1) {
                                    perror("open");
                                    exit(1);
                                }
                                /*
                                if (dup2(all_pipes[(i * 2) - 2], fd) == -1) {
                                    perror("dup2 4");
                                    exit(1);
                                }
                                */
                                if (dup2(fd, STDIN_FILENO) == -1) {
                                    perror("dup2 4");
                                    exit(1);
                                }
                            }
                            
                            for (k = 0; k < (plen * 2); k++) { /* Plen or olen?
                                printf("Closing fd (Last process): %d\n", all_pipes[k]); */
                                if (close(all_pipes[k]) == -1) {
                                    perror("close 2");
                                    exit(1);
                                }
                            }
                            execvp(curr_stage.argv[0], curr_stage.argv);
                            perror(curr_stage.argv[0]);
                            exit(1);
                        }
                        else {
                            /* Middle processes - Both ends need to be reconnected */
                            if (curr_stage.inname == NULL) {
                                if (dup2(all_pipes[(i * 2) - 2], STDIN_FILENO) == -1) {
                                    perror("dup2 5");
                                    exit(1);
                                }
                            }
                            else {
                                fd = open(curr_stage.inname, O_RDONLY);
                                if (fd == -1) {
                                    perror("open");
                                    exit(1);
                                }
                                if (dup2(fd, STDIN_FILENO) == -1) {
                                    perror("dup2 6");
                                    exit(1);
                                }
                            }
                            if (curr_stage.outname == NULL) {
                                if (dup2(all_pipes[(i * 2) + 1], STDOUT_FILENO) == -1) {
                                    perror("dup2 7");
                                    exit(1);
                                }
                            }
                            else {
                                fd = open(curr_stage.outname, O_RDWR | O_CREAT | O_TRUNC, 0666);
                                if (fd == -1) {
                                    perror("open");
                                    exit(1);
                                }
                                if (dup2(fd, STDOUT_FILENO) == -1) {
                                    perror("dup2 3");
                                    exit(1);
                                }
                            }
                            
                            for (k = 0; k < (plen * 2); k++) { /* Plen or olen?
                                printf("Closing fd (Middle process): %d\n", all_pipes[k]); */
                                if (close(all_pipes[k]) == -1) {
                                    perror("close 3");
                                    exit(1);
                                }
                            }
                            execvp(curr_stage.argv[0], curr_stage.argv);
                            perror(curr_stage.argv[0]);
                            exit(1);
                        }
                    }
                    else {
                        /* Parent process */
                        if (pid == -1) {
                            perror("fork");
                            exit(1);
                        }
                    }
                }
            }

            for (k = 0; k < (plen * 2); k++) {
                /* 
                printf("Closing fd (Parent process): %d\n", all_pipes[k]);
                printf("On iteration (k): %d\n", k); */
                if (close(all_pipes[k]) == -1) {
                    perror("close 4");
                    exit(1);
                }
            }
            free(all_pipes);
            free_pipeline(pipeln);

            if (cd_arg == 0) {
                int j = 0;
                while (j < olen) {
                    if (wait(&status[j]) == -1) {
                        perror("wait");
                        exit(1);
                    }
                    /*printf("process %d waited\n", j);*/
                    j++;
                }
            }
        }
        
    }

    printf("\n");

    return 0;
}

void handler(int signum) {

    write(STDOUT_FILENO, "\n", 1);
}