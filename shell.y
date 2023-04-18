%code requires
{
#include <string>

#if __cplusplus > 199711L
#define register
#endif
}


%union{
  char *string_val;
  std::string *cpp_string;
}


%token <cpp_string> WORD
%token NOTOKEN NEWLINE LESS GREAT GREATGREAT GREATAMPERSAND GREATGREATAMPERSAND 
TWOGREAT PIPE AMPERSAND INSIDE_QUOTES


%{
// Imports
#include "shell.hh"

#include <iostream>
#include <cstring>
#include <string>
#include <dirent.h>
#include <algorithm>
#include <unistd.h>
#include <regex.h>


// Namespaces
using namespace std;


// Global variables
vector<string *> files;


// Function prototypes
void yyerror(const char * s);
int yylex();




// Reformats the regex string to something regex library library 
char *reformatRegexStr(char *str) {
  // Beginning the string
  char fixed_str[1025];
  fixed_str[0] = '^';
  int place_index = 1;

  for (int index = 0; index < strlen(str); index++) {
    while ((str[index] == '*') && (str[index + 1] == '*')) {
      // Removing redundancies
      index++;
    }

    // Reformatting
    if (str[index] == '*') {
      fixed_str[place_index++] = '.';
      fixed_str[place_index++] = '*';
    }
    else if (str[index] == '.') {
      fixed_str[place_index++] = '\\';
      fixed_str[place_index++] = '.';
    }
    else if (str[index] == '?') {
      fixed_str[place_index++] = '.';
    }
    else {
      fixed_str[place_index++] = str[index];
    }
  }
  
  // Ending the string
  fixed_str[place_index++] = '$';
  fixed_str[place_index] = 0;
  
  return strdup((const char *) fixed_str);
}


// Helper function to recursively find potential file
void expandWildcard(char *prefix, char *suffix) {
  // Done with wildcard expansion
  if (suffix[0] == 0) {
    if (strchr(&prefix[1], '/') == NULL) {
      files.push_back(new string(&prefix[1]));
    }
    else {
      files.push_back(new string(prefix));
    }
    return;
  }

  // Getting the current component
  char *s = strchr(suffix, '/');
  char component[1025];
  if (s != NULL) {
    strncpy(component, suffix, s - suffix);
    component[s - suffix] = 0;
    suffix = s + 1;
  }
  else {
    strcpy(component, suffix);
    suffix = suffix + strlen(suffix);
  }

  // Checking if regular expression matching is necessary
  char newPrefix[1026];
  if ((strchr(component, '*') == NULL) && (strchr(component, '?') == NULL)) {
    // Unnecessary so simply adding component to path
    if (prefix[0] == 0) {
      sprintf(newPrefix, "%s", component);
    }
    else if (prefix[strlen(prefix) - 1] == '/') {
      sprintf(newPrefix, "%s%s", prefix, component);
    }
    else {
      sprintf(newPrefix, "%s/%s", prefix, component);
    }
    expandWildcard(newPrefix, suffix);
    return;
  }

  // Getting the correct working directory
  char *dir;
  if (prefix[0] == 0) {
    dir = (char *) ".";
  }
  else {
    dir = prefix;
  }

  DIR *d = opendir(dir);
  if (d == NULL) {
    return;
  }

  // Setting up regular expression
  regex_t re; regmatch_t match;
  char *fixed_str = reformatRegexStr(component);
  regcomp(&re, fixed_str, REG_EXTENDED|REG_NOSUB);
  free(fixed_str);

  struct dirent *ent;
  while ((ent = readdir(d)) != NULL) {
    // Going through each file in directory
    if (regexec(&re, ent->d_name, 1, &match, 0) == 0) {
      bool add = false;

      // Match occurred
      if (ent->d_name[0] == '.') {
        // Hidden file
        if (component[0] == '.') {
          // Hidden files must be specified, otherwise ignored
          add = true;
        }
      }
      else {
        add = true;
      }

      // Adding file to path and expanding on that
      if (add) {
        if (prefix[strlen(prefix) - 1] == '/') {
          sprintf(newPrefix, "%s%s", prefix, ent->d_name);
        }
        else {
          sprintf(newPrefix, "%s/%s", prefix, ent->d_name);
        }
        expandWildcard(newPrefix, suffix);
      }
    }
  }

  // Cleaning up
  regfree(&re);
  closedir(d);
}


// Helper to sort files
bool str_compare(const string * c1, const string * c2) {
  return (strcmp(c1->c_str(), c2->c_str()) < 0);
}


// API call for wildcard expansion
void expandStr(string *string_str) {
  char *str = (char *) string_str->c_str();
  if ((strchr(str, '*') != NULL) || (strchr(str, '?') != NULL)) {
    // Needs expanding
    files.clear();
    if (str[0] == '/') {
      expandWildcard((char *) "/", &str[1]);
    }
    else {
      expandWildcard((char *) "", str);
    }

    if (files.size() == 0) {
      Command::_currentSimpleCommand->insertArgument(string_str);
    }
    else {
      // Sorting per linux specifications
      sort(files.begin(), files.end(), str_compare);

      // Generating to print string
      for (string *file: files) {
        Command::_currentSimpleCommand->insertArgument(file);
      }
    }
  }
  else {
    Command::_currentSimpleCommand->insertArgument(string_str);
  }
}
%}


%%
goal:
  command_list
  ;


command_list:
  command_line
  | command_list command_line
  ;


command_line:
  pipe_list io_modifier_list background_optional NEWLINE {
    Shell::_currentCommand.execute();
  }
  | NEWLINE {
    Shell::_currentCommand.execute();
  }
  | error NEWLINE{ yyerrok; }
  ;


pipe_list:
  cmd_and_args
  | pipe_list PIPE cmd_and_args
  ;


cmd_and_args:
  command_word arg_list {
    Shell::_currentCommand.insertSimpleCommand( Command::_currentSimpleCommand );
  }
  ;


command_word:
  WORD {
    Command::_currentSimpleCommand = new SimpleCommand();
    Command::_currentSimpleCommand->insertArgument( $1 );
  }
  ;


arg_list:
  arg_list arg
  | /* empty list */
  ;

arg:
  WORD {
    expandStr( $1 );
  }
  ;

io_modifier_list:
  io_modifier_list io_modifier
  | /* empty string */
  ;


io_modifier:
  GREATGREAT WORD {
    if (Shell::_currentCommand._outFile != NULL) {
      Shell::_currentCommand._ambiguousRedirect = true;
    }
    else {
      Shell::_currentCommand._outFile = $2;
      Shell::_currentCommand._outFileAppend = true;
    }
  }
  | GREAT WORD {
    if (Shell::_currentCommand._outFile != NULL) {
      Shell::_currentCommand._ambiguousRedirect = true;
    }
    else {
      Shell::_currentCommand._outFile = $2;
    }
  }
  | GREATGREATAMPERSAND WORD {
    if ((Shell::_currentCommand._outFile != NULL) ||
        (Shell::_currentCommand._errFile != NULL)) {
      Shell::_currentCommand._ambiguousRedirect = true;
    }
    else {
      Shell::_currentCommand._outFile = $2;
      Shell::_currentCommand._errFile = $2;
      Shell::_currentCommand._outFileAppend = true;
      Shell::_currentCommand._errFileAppend = true;
    }
  }
  | GREATAMPERSAND WORD {
    if (Shell::_currentCommand._outFile != NULL) {
      Shell::_currentCommand._ambiguousRedirect = true;
    }
    else {
      Shell::_currentCommand._outFile = $2;
      Shell::_currentCommand._errFile = $2;
    }
  }
  | LESS WORD {
    if (Shell::_currentCommand._inFile != NULL) {
      Shell::_currentCommand._ambiguousRedirect = true;
    }
    else {
      Shell::_currentCommand._inFile = $2;
    }
  }
  | TWOGREAT WORD {
    if (Shell::_currentCommand._errFile != NULL) {
      Shell::_currentCommand._ambiguousRedirect = true;
    }
    else {
      Shell::_currentCommand._errFile = $2;
    }
  }
  ;


background_optional:
  AMPERSAND {
    Shell::_currentCommand._background = true;
  }
  | /* empty string */
  ;
%%

void
yyerror(const char * s)
{
  fprintf(stderr,"%s", s);
}

#if 0
main()
{
  yyparse();
}
#endif
