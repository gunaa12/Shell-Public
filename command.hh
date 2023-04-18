#ifndef command_hh
#define command_hh

// Imports
#include "simpleCommand.hh"
#include <signal.h>


// Namespaces
using namespace std;


// Command class prototype
struct Command {
  // Variables
  vector<SimpleCommand *> _simpleCommands;
  string * _outFile;
  string * _inFile;
  string * _errFile;
  bool _outFileAppend;
  bool _errFileAppend;
  bool _background;
  bool _ambiguousRedirect;
  static SimpleCommand *_currentSimpleCommand;


  // Functions
  Command();
  void insertSimpleCommand( SimpleCommand * simpleCommand );
  void handleSetEnv();
  void handlePrintEnv();
  void handleRemoveEnv();
  void handleChangeDir();
  void setErrFile();
  void execute();
  void cleanUp(int, int, int);
  void clear();
};

#endif
