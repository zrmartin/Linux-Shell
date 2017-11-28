mysh is a linux shell program that can execute commands, handle input/output redirection, run commands in the background, and pipe the output of one command to the input of another.

In order to compile the program just call make

To execute the program run the program with ./mysh

You can now enter commands like you are using a normal linux terminal
< for file input redirection
> for file output redirection
& to run the command in the background to allow user to enter commands without waiting for previous command to finish
| to pipe the output of one command to the input of another command
*mysh can only handle 1 | symbol for each command given.

To exit enter the command ctrl + c