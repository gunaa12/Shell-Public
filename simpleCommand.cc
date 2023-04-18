// Imports
#include "simpleCommand.hh"

#include <cstdio>
#include <cstdlib>
#include <iostream>


// Namespaces
using namespace std;


// Constructor
SimpleCommand::SimpleCommand() {
  _arguments = vector<string *>();
}


// Destructor
SimpleCommand::~SimpleCommand() {
  for (auto & arg : _arguments) {
    delete arg;
  }
}


// Inserts an argument into the simple command
void SimpleCommand::insertArgument(string *argument) {
  _arguments.push_back(argument);
}


// Print out the simple command in a readable format
void SimpleCommand::print() {
  for (auto& arg : _arguments) {
    cout << *arg << "\t";
  }
  printf("\n");
}
