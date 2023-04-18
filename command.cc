// Imports
#include "command.hh"
#include "shell.hh"

#include <cstdio>
#include <cstdlib>
#include <iostream>
#include <cstring>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <fcntl.h>


// Useful defines
#define STDIN_VAL (0)
#define STDOUT_VAL (1)
#define STDERR_VAL (2)

#define DEFAULT_FILE_PERMISSIONS (S_IRUSR | S_IWUSR)
#define FILE_REWRITE (O_CREAT | O_WRONLY | O_TRUNC)
#define FILE_APPEND (O_CREAT | O_WRONLY | O_APPEND)


// Namespaces
using namespace std;


// Defining/declaring/initializing global variables
extern char **environ;
int backgroundPID = 0, shellPID = getpid(), returnVal = 0;
char lastArgArr[1025];
char *lastArgument = lastArgArr;
SimpleCommand * Command::_currentSimpleCommand;


// Constructor
Command::Command() {
  // Initializes a new vector of simple commands
  _simpleCommands = std::vector<SimpleCommand *>();

  // Variables to help with command execution
  _inFile = NULL;

  _outFile = NULL;
  _outFileAppend = false;

  _errFile = NULL;
  _errFileAppend = false;

  _background = false;
  _ambiguousRedirect = false;
}


// Inserts a simple command into total command
void Command::insertSimpleCommand( SimpleCommand * simpleCommand ) {
  _simpleCommands.push_back(simpleCommand);
}


// Adds new item to the list of environment variables
void Command::handleSetEnv() {
  // Generating the put env command
  string equal = "=";
  string putenvCommand = *(_simpleCommands[0]->_arguments[1]) + equal + *(_simpleCommands[0]->_arguments[2]);

  // Heap allocating the cpp string into c string
  /*short size = putenvCommand.size();
  char *cPutenvCommand = (char *) malloc(size + 1);
  for (int index = 0; index < size; index++) {
    cPutenvCommand[index] = putenvCommand[index];
  }
  cPutenvCommand[size] = 0;
  */
  char *cPutenvCommand = strdup(putenvCommand.c_str());

  // Adding the environment variable
  putenv(cPutenvCommand);

  // Freeing malloc memory
  free(cPutenvCommand);
  cPutenvCommand = NULL;
}


// Prints out the environment variables
void Command::handlePrintEnv() {
  int var = 0;
  while (environ[var] != NULL) {
    printf("%s\n", environ[var++]);
  }
  fflush(stdout);
}


// Handles the environment variable removal process
void Command::handleRemoveEnv() {
  char *name = (char *) _simpleCommands[0]->_arguments[1]->c_str();
  unsetenv(name);
}


// Changes the pwd
void Command::handleChangeDir() {
  int result, num_of_args = _simpleCommands[0]->_arguments.size();
  if (num_of_args == 1) {
    // Input is "cd"
    result = chdir(getenv("HOME"));
  }
  else if (num_of_args == 2) {
    // There is a path following "cd"
    result = chdir(_simpleCommands[0]->_arguments[1]->c_str());
  }

  // If the directory changing process fails
  if (result == -1) {
    fprintf(stderr, "cd: can't cd to %s\n", _simpleCommands[0]->_arguments[1]->c_str());
  }
}


// Changes IO stream 2 to where error is being redirected to
void Command::setErrFile() {
  if (_errFile != NULL) {
    int ferr;
    if (_errFileAppend == true) {
      // Opening the error file in append mode
      ferr = open(_errFile->c_str(), FILE_APPEND, DEFAULT_FILE_PERMISSIONS);
    }
    else {
      // Opening the error file in overwrite mode
      ferr = open(_errFile->c_str(), FILE_REWRITE, DEFAULT_FILE_PERMISSIONS);
    }

    // Changing the stdout file stream to new error output file
    dup2(ferr, 2);
    close(ferr);
  }
}


// Carries out the execution of a complex command
void Command::execute() {
  // No simple commands to execute
  if ( _simpleCommands.size() == 0 ) {
    Shell::prompt();
    return;
  }

  // Handling exitting the shell program
  string exitStr = "exit";
  if (_simpleCommands[0]->_arguments[0]->compare(exitStr) == 0) {
    printf("\n Good bye!!\n\n");
    exit(0);
  }

  // Not executing command if improper io streams are specified
  if (_ambiguousRedirect == true) {
    printf("Ambiguous output redirect.\n");
    Shell::prompt();
  }

  // Storing actual stdin, stdout, and stderr
  int stdin_copy = dup(STDIN_VAL);
  int stdout_copy = dup(STDOUT_VAL);
  int stderr_copy = dup(STDERR_VAL);

  // Setting to the correct error file
  setErrFile();

  // Handling special command executions that cannot be piped with other commands
  if (_simpleCommands.size() == 1) {
    string cmd = *(_simpleCommands[0]->_arguments[0]);
    string set_env_str = "setenv", unset_env_str = "unsetenv", change_dir = "cd";
    if (cmd == set_env_str) {
      handleSetEnv();
    }
    else if (cmd == unset_env_str) {
      handleRemoveEnv();
    }
    else if (cmd == change_dir) {
      handleChangeDir();
    }
  }

  // Creating new file stream for input
  int fdin;
  if (_inFile != NULL) {
    fdin = open(_inFile->c_str(), O_RDONLY);
    if (fdin < 0) {
      // IO redirection failed
      fprintf(stderr, "IO redirection failed!\n");
      cleanUp(stdin_copy, stdout_copy, stderr_copy);
      Shell::prompt();
    }
  }
  else {
    fdin = dup(STDIN_VAL);
  }

  // Iterating through and executing commands
  int fdout, ret;
  for (long unsigned int i = 0; i < _simpleCommands.size(); i++) {
    // Setting up input
    dup2(fdin, 0);
    close(fdin);

    // Getting correct output file stream
    if (i == _simpleCommands.size() - 1) {
      // Final simple command so output needs to redirect to specified out
      if (_outFile != NULL) {
        if (_outFileAppend == true) {
          // Appending to the output file
          fdout = open(_outFile->c_str(), FILE_APPEND, DEFAULT_FILE_PERMISSIONS);
        }
        else {
          // Overwriting to the output file
          fdout = open(_outFile->c_str(), FILE_REWRITE, DEFAULT_FILE_PERMISSIONS);
        }
      }
      else {
        fdout = dup(stdout_copy);
      }
    }
    else {
      // Intermediate simple command so output needs to be piped
      int fdpipe[2];
      pipe(fdpipe);
      fdout = fdpipe[1];
      fdin = fdpipe[0];
    }

    // Setting the correct output to stdout
    dup2(fdout, 1);
    close(fdout);

    // Executing command
    ret = fork();
    if (ret == 0) {
      // Child process
      auto arguments = _simpleCommands[i]->_arguments;

      // Special execution case
      string print_env_str = "printenv";
      if (*arguments[0] == print_env_str) {
        handlePrintEnv();
        _exit(0);
      }

      // Actual command to execute
      char **arg_list = new char* [arguments.size() + 1];
      arg_list[arguments.size()] = NULL;
      for (int index = 0; index < (int) arguments.size(); index++) {
        arg_list[index] = (char *) arguments[index]->c_str();
      }
      execvp(arg_list[0], arg_list);

      // Execvp should self terminates, otherwise something went wrong
      _exit(1);
    }
    else if (ret < 0) {
      // Child process creation failed
      exit(2);
    }

    // Waiting if necessary
    if(_background == false) {
      waitpid(ret, &returnVal, 0);

      // Saving execution result of executed command
      returnVal = WEXITSTATUS(returnVal);
    }
    else {
      // Getting the background PID of child executing current simple command
      backgroundPID = ret;
    }
  }

  // Saving the last simple argument executed
  auto final_simple_com = _simpleCommands[_simpleCommands.size() - 1];
  strcpy(lastArgument, final_simple_com->_arguments[final_simple_com->_arguments.size() - 1]->c_str());

  // Resetting and prompting the user for next command
  cleanUp(stdin_copy, stdout_copy, stderr_copy);
  Shell::prompt();
}


// Restores the IO streams and clears the arguments
void Command::cleanUp(int stdin_copy, int stdout_copy, int stderr_copy) {
  // Cleaning up standard in/out/err
  dup2(stdin_copy, STDIN_VAL);
  close(stdin_copy);
  dup2(stdout_copy, STDOUT_VAL);
  close(stdout_copy);
  dup2(stderr_copy, STDERR_VAL);
  close(stderr_copy);

  /*
   * Clears commands and resets all the flags for correct execution of future
   * commands.
   */
  clear();
}


// Clears all the simple areguments and resets the flags
void Command::clear() {
  // Deleting all the simple commands making up the current command
  for (auto simpleCommand : _simpleCommands) {
    for (auto arg: simpleCommand->_arguments) {
      arg->erase();
    }
    delete simpleCommand;
  }

  // Remove all references to the simple commands
  _simpleCommands.clear();

  // Clearing the input file
  if ( _inFile ) {
    delete _inFile;
  }
  _inFile = NULL;

  // Clearing the output/error file
  if (_outFile == _errFile) {
    delete _outFile;
    _outFile = NULL;
    _errFile = NULL;
  }
  else if ( _outFile ) {
    delete _outFile;
    _outFile = NULL;
  }
  if ( _errFile ) {
    delete _errFile;
    _errFile = NULL;
  }

  // Resetting other flags
  _background = false;
  _errFileAppend = false;
  _outFileAppend = false;
}
