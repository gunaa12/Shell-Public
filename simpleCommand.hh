#ifndef simplcommand_hh
#define simplecommand_hh

// Imports
#include <string>
#include <vector>


struct SimpleCommand {
  // A vector of strings representing the arguments
  std::vector<std::string *> _arguments;

  SimpleCommand();
  ~SimpleCommand();
  void insertArgument(std::string *argument);
  void print();
};

#endif
