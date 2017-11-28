#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <sys/wait.h>
#include <sys/types.h>
#include "mysh.h"

// Helper function that returns input redirection FD
int openInput(char *fileName) {
	int fd = open(fileName, O_RDONLY);
	if (fd == -1) {
		perror("Cannot open input redirect file");
		exit(EXIT_FAILURE);
	}
	else {
		return fd;
	}
}

// Helper function that returns output redirection FD
int openOutput(char *fileName) {
	int fd = open(fileName, O_WRONLY | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR);
	if (fd == -1) {
		perror("Cannot open output redirect file");
		exit(EXIT_FAILURE);
	}
	else {
		return fd;
	}
}

// Signal handler to help remove zombie processes
void onchild() {
	int *status;
	waitpid(-1, status, WNOHANG);
}


int main() {
	// Used to store a line of user input from command line
	char userInput[MAX_LINE];
	// Copies userInput and is used to parse line because strtok is destructive
	char tempInput[MAX_LINE];
	// Used to hold tokens created by strtok / holds command to execute
	char *token, *command;
	// Argument vector to hold command line arguments that are passed to execvp()
	char **argv; 
	// input/output FD / counter for number of args / pid from fork() / bool for & / bool for |
	int inputFD, outputFD, argvIndex, pid, background, pipeCheck;
	// Needed for waitpid, not used though
	int *status;
	// FD's that correspond to read/write end of the pipe in case of '|'
	int pipeFD[2];

	// This loop is the shell program
	while (TRUE) {
		printf("(mysh) ");
		/* Checks to see if any child processes have terminated,
		   if so handle them so they are no longer zombies.
		*/ 
		signal(SIGCHLD, onchild);

		// If EOF is entered exit the shell
		if (fgets(userInput, MAX_LINE, stdin) == NULL) {
			exit(EXIT_SUCCESS);
		}

		// If user does not input anything, continue to next iteration of loop
		if (strcmp(userInput, "\n") == 0) {
			continue;
		}

		// Allocate space for agrv for child to use in execvp
		argv = malloc(sizeof(char *) * MAX_ARGS);
		// Reset argvIndex to 1 for building up Argv for child
		argvIndex = 1;
		// Reset input/output FD's
		inputFD = -1;
		outputFD = -1;
		// Reset background boolean
		background = 0;
		// Reset pipe boolean
		pipeCheck = 0;

		// Parse for '&' symbol
		strncpy(tempInput, userInput, MAX_LINE);
		if (strpbrk(tempInput, "&") != NULL) {
			background = 1;
		}

		// Parse for '|' symbol
		if (strpbrk(tempInput, "|") != NULL) {
			pipeCheck = 1;
			// Open pipe
			if (pipe(pipeFD) == -1) {
				perror("Error opening pipe");
				exit(EXIT_FAILURE);
			}
		}

		// Parse for output redirection character and obtain outputFD for later use by child
		strncpy(tempInput, userInput, MAX_LINE);
		if (strpbrk(tempInput, ">") != NULL) {
			// Discards eveything in front of '>' 
			token = strtok(tempInput, ">");
			if ((token = strtok(NULL, "<>&\n ")) != NULL) {
				outputFD = openOutput(token);
			}
		}

		// Parse for input redirection character and obtain inputFD for later use by child
		strncpy(tempInput, userInput, MAX_LINE);
		if (strpbrk(tempInput, "<") != NULL) {
			// Discards everything in front of '<'
			token = strtok(tempInput, "<");
			if ((token = strtok(NULL, "<>&\n ")) != NULL) {
				inputFD = openInput(token);
			}
		}

		// Parse for command and arguments to build argv
		strncpy(tempInput, userInput, MAX_LINE);
		// Command and arguments before redirection metacharacters
		token = strtok(tempInput, "<>|&\n");
		// Parses everything before any metacharacters, first token is the command to execute
		token = strtok(token, " \n");
		// Create space for the command to execute plus one byte to null terminate
		command = malloc(sizeof(char) * strlen(token) + 1);
		strncpy(command, token, strlen(token));
		command[strlen(token)] = '\0';
		// Get any remaining arguments to build argv
		argv[0] = strdup(command);
		while ((token = strtok(NULL, "\n ")) != NULL) {
			argv[argvIndex] = strdup(token);
			argvIndex++;
		}
		// Null terminate argument vector
		argv[argvIndex] = NULL;

		// If command is 'cd' no need to fork a new process, just call chdir()
		if (strcmp(command, "cd") == 0) {
			if (chdir(argv[1]) == -1) {
				perror("Directory does not exist");
			}
		}

		// Need to create child process in order to call command
		else {
			pid = fork();
			// Error has occured
			if (pid == -1) {
				perror("fork failed");
				exit(EXIT_FAILURE);
			}
			// *** Child Process *** 
			else if (pid == 0) {
				// If '|' if present, redirect stdout to write end of pipe
				if (pipeCheck) {
					if (dup2(pipeFD[1], 1) < 0) {
						perror("Error duping output to pipe");
						exit(EXIT_FAILURE);
					}
				}
				// If '>' is present, redirect stdout to output file FD
				else if (outputFD != -1) {
					if (dup2(outputFD, 1) < 0) {
						perror("Error duping output file to stdout");
						exit(EXIT_FAILURE);
					}
				}
				// If '<' is present, redirect input file FD to stdin 
				if (inputFD != -1) {
					if (dup2(inputFD, 0) < 0) {
						perror("Error duping input file to stdin");
						exit(EXIT_FAILURE);
					}
				}
				// Exec the given program and supply the remaining command line args
				if (execvp(command, argv) == -1){
					perror("Failed to exec command");
					exit(EXIT_FAILURE);
				}
			}
			// *** Parent Process ***
			else {
				
				// If '&' present check if child is done, if not too bad carry on
				if (background) {
					waitpid(pid, status, WNOHANG);
				}
				// Wait until child process is done executing
				else {
					waitpid(pid, status, 0);
				}
				/*  If '|' is present repeat process of getting command, creating argv, 
				    forking a new child process, copying read end of pipe to stdin, 
				    and executing the given command
				*/
				if (pipeCheck) {
					close(pipeFD[1]);
					// Free allocated space for command
					free(command);
					// Free the previous arguments in argv
					for (int i = 0; i < argvIndex; i++) {
						free(argv[i]);
					}	
					// Reset argvIndex to create new argument vector
					argvIndex = 1;
					// Get a copy of user input, put in tempInput
					strncpy(tempInput, userInput, MAX_LINE);
					// Get rid of everything before '|' symbol
					token = strtok(tempInput, "|");
					// Get the 2nd command to be executed
					token = strtok(NULL, "\n ");
					// Create space for the command to execute plus one byte to null terminate
					command = malloc(sizeof(char) * strlen(token) + 1);	
					strncpy(command, token, strlen(token));
					command[strlen(token)] = '\0';
					// Get any remaining arguments to build argv
					argv[0] = strdup(command);
					while ((token = strtok(NULL, "\n ")) != NULL) {
						// Only grab arguments until a meta character is hit
						if (strpbrk(token, "<>&") == NULL) {
							argv[argvIndex] = strdup(token);
							argvIndex++;							
						}
						else {
							break;
						}

					}
					// Null terminate argument vector
					argv[argvIndex] = NULL;

					// Create a new child process and exec the second command
					pid = fork();
					if (pid == -1) {
						perror("fork failed");
						exit(EXIT_FAILURE);
					}
					// *** Child Process ***
					else if (pid == 0) {
						// Copy read end of pipe to stdin
						if (dup2(pipeFD[0], 0) < 0) {
							perror("Error duping read end of pipe to stdin");
							exit(EXIT_FAILURE);
						}
						// If '>' is present, redirect stdout to output file FD
						if (outputFD != -1) {
							if (dup2(outputFD, 1) < 0) {
								perror("Error duping output file to stdout");
								exit(EXIT_FAILURE);
							}	
						}	
						// Exec the 2nd command
						if (execvp(command, argv) == -1){
							perror("Failed to exec command");
							exit(EXIT_FAILURE);
						}
					}
					// *** Parent Process ***
					else {

						// If '&' present check if child is done, if not too bad carry on
						if (background) {
							waitpid(pid, status, WNOHANG);
						}
						// Wait for child process to finish processesing
						else {
							waitpid(pid, status, 0);
						}
					}
				}	
			}		
		}
		// Freeing any allocated memory
		for (int i = 0; i < argvIndex; i++) {
			free(argv[i]);
		}
		free(argv);
		free(command);
		if (pipeCheck) {
			close(pipeFD[0]);
		}
	}
}