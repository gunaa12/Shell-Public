// Imports
#include "shell.hh"
#include "y.tab.hh"

#include <cstdio>
#include <unistd.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/wait.h>


// Namespaces
using namespace std;


// Declaring/defining global variables
char abs_path_arr[1025];
char *abs_path = abs_path_arr;
bool Shell::_fromSource;
Command Shell::_currentCommand;


// Function Prototypes
void yyrestart(FILE *file);
int yyparse(void);
void myunputc(int c);


// Prompts the user to input a command
void Shell::prompt() {
  // An actual user is interacting through a terminal
  if ((isatty(0) == 1) && (_fromSource == false)) {
    // Previous command failed, so printing registerd error message
    if (returnVal != 0) {
      char *err_str = getenv("ON_ERROR");
      if (err_str != NULL) {
        printf("%s\n", err_str);
      }
    }

    // Printing the registered or default prompt message
    char *prompt_str = getenv("PROMPT");
    if (prompt_str == NULL) {
      printf("myshell>");
    }
    else {
      printf("%s", prompt_str);
    }

    // Ensuring user is immediately prompted
    fflush(stdout);
  }
}


// Handles any signals generated for this process
extern "C" void sigIntHandler(int err) {
  if (err == SIGINT) {
    // Ctrl-C was inputted meaning user needs to be reprompted
    if (Shell::_currentCommand._simpleCommands.size() == 0) {
      printf("\n");
      Shell::prompt();
    }
  }
  else if (err == SIGCHLD) {
    // Dealing with finished child to prevent zombie processes
    int pid = waitpid(-1, NULL, WNOHANG);
    while (pid > 0) {
      printf("[%d] exit.\n", pid);
      pid = waitpid(-1, NULL, WNOHANG);
    }
  }
  else {
    // Other signals
    exit(1);
  }
}


//Sets the signal handler callback function
void handleSignals() {
  struct sigaction sa;
  sa.sa_handler = sigIntHandler;
  sigemptyset(&sa.sa_mask);
  sa.sa_flags = SA_RESTART;
  sigaction(SIGINT, &sa, NULL);
  sigaction(SIGCHLD, &sa, NULL);
}


// Runs the shellrc file
void shellrc() {
  char *defaultSourceFileName = (char *) ".shellrc";
  FILE *in_file = fopen(defaultSourceFileName, "r");
  if (in_file == NULL) {
    // No shellrc file to run
    return;
  }

  // Running the shellrc
  Shell::_fromSource = true;
  yyrestart(in_file);
  yyparse();
  yyrestart(stdin);
  Shell::_fromSource = false;

  // Cleaning up resources
  fclose(in_file);
}


int main(int argc, char **argv) {
  // Running .shellrc source file
  shellrc();

  // Setting up absolute path of the program
  abs_path = realpath(argv[0], abs_path);

  // Setting up signal handlers
  handleSignals();

  // Starting user input and functionality
  Shell::prompt();
  yyrestart(stdin);
  yyparse();
}
