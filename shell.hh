#ifndef shell_hh
#define shell_hh

// Imports
#include "command.hh"

#include <string>
#include <vector>


// Namespaces
using namespace std;


// Declaring global variables
extern int backgroundPID;
extern int shellPID;
extern int returnVal;
extern char lastArgArr[];
extern char *lastArgument;
extern vector<string *> files;


// Helper functions
void expandWildcard(char *, char *);


// Shell class
struct Shell {
  static void prompt();

  static Command _currentCommand;
  static bool _fromSource;
};

#endif
