%{
// Imports
#include "y.tab.hh"
#include "shell.hh"
#include <cstring>
#include <string>
#include <sys/wait.h>
#include <unistd.h>
#include <stdlib.h>


// Useful macros
#define STR_SIZE (1024)


// Declaring global variables
extern char *abs_path;
extern int backgroundPID;


// Function prototypes
extern char *read_line();
static void yyunput (int c,char *buf_ptr);


// Putting a character back into the buffer
void myunputc(int c) {
  unput(c);
}


// Gets the next character from the file/buffer
int mygetc(FILE * f) {
  static char *p;
  char ch;

  // stdin is not a tty. Call real getc
  if (!isatty(0)) {
    return getc(f);
  }

  // stdin is a tty. Call our read_line
  if (p==NULL || *p == 0) {
    char * s = read_line();
    p = s;
  }

  ch = *p;
  p++;
  return ch;
}


// Changing which getc is used when global call occurs
#undef getc
#define getc(f) mygetc(f)


// Given a string, removes all the escape characters
void remove_escape_char(char *str) {
  int len = strlen(str), place_index = 0;
  for (int index = 0; index < len; index++) {
    char c = str[index];
    if (c != '\\') {
      if ((str[index] == '&') && (index == len - 1)) {
        // Needs to be executed in background
        Shell::_currentCommand._background = true;
        str[place_index] = 0;
        return;
      }
      str[place_index++] = c;
    }
    else {
      char c = str[index + 1];
      if ((c == '\\') || (c == '"') || (c == '&')) {
        index++;
        str[place_index++] = c;
      }
    }
  }
  str[place_index] = 0;
}


// Sources a file by executing all contents
void runSourceFile(char *filename) {
  // Opening source file
  FILE *file = fopen(filename, "r");
  if (file == NULL) {
    perror("Could not open source file!\n");
  }

  // Executing the source file by creating a second buffer to handle file input
  Shell::_fromSource = true;
  yypush_buffer_state(yy_create_buffer(file, YY_BUF_SIZE));
  BEGIN(0);
  yyparse();
  yypop_buffer_state();
  Shell::_fromSource = false;

  // Closing source file
  fclose(file);
  file = NULL;
}


// Parsing the inputted string
%}
%option noyywrap
%%


\n {
  /* Newline */
  return NEWLINE;
}


[ \t] {
  /* Tabs and spaces */
}


"source "[^ \t\n]* {
  /* Special command to run source file */
  char *filename = (char *) (yytext + 7);
  runSourceFile(filename);
}


"~"[^ \n\t]* {
  /* Expanding tilde into home directory */
  char home_dir[STR_SIZE];
  char *home_dir_str = home_dir;
  strcpy(home_dir_str, getenv("HOME"));
  if (yytext[1] == '/') {
    home_dir_str = strcat(home_dir_str, &yytext[1]);
  }
  else if (yytext[1] != 0) {
    home_dir_str = strcpy(home_dir_str, "/homes/");
    home_dir_str = strcat(home_dir_str, &yytext[1]);
  }

  // putting tilde expansion back into buffer
  for (int index = strlen(home_dir_str) - 1; index >= 0; index--) {
    myunputc(home_dir_str[index]);
  }
}


"<" {
  /* Input overwrite redirection */
  return LESS;
}


">" {
  /* Output overwrite redirection */
  return GREAT;
}


">>" {
  /* Input append redirection */
  return GREATGREAT;
}


">&" {
  /* Output and error overwrite redirection */
  return GREATAMPERSAND;
}


">>&" {
  /* Output and error append redirection */
  return GREATGREATAMPERSAND;
}


"2>" {
  /* Error overwrite redirection */
  return TWOGREAT;
}


"|" {
  /* Piping redirection */
  return PIPE;
}


"&" {
  /* Background processing */
  return AMPERSAND;
}


["]([^"]|\\.)*["] {
  // Deals with quoted words
  char *word = (char *) (yytext + 1);
  word[strlen(word) - 1] = 0;
  remove_escape_char(word);
  yylval.cpp_string = new std::string(word);
  return WORD;
}


(\$\{\$\}) {
  // Special command to print the pid of shell process
  char str_arr[1025];
  char *str = str_arr;
  sprintf(str_arr, "%d", getpid());
  yylval.cpp_string = new std::string(str);
  return WORD;
}


(\$\{\?\}) {
  // Special command to print the execution result of previous command
  char str_arr[11];
  char *str = str_arr;
  sprintf(str, "%d", returnVal);
  yylval.cpp_string = new std::string(str);
  return WORD;
}


(\$\{!\}) {
  // Special command to print the background PID of the last executed commands
  char str_arr[11];
  char *str = str_arr;
  sprintf(str, "%d", backgroundPID);
  yylval.cpp_string = new std::string(str);
  return WORD;
}


(\$\{_\}) {
  // Special command to print the last argument of the last executed command
  yylval.cpp_string = new std::string(lastArgument);
  return WORD;
}


(\$\{SHELL\}) {
  // Prints the absolute path of the shell process
  yylval.cpp_string = new std::string(abs_path);
  return WORD;
}


(\$\{[^ \$\}\t\n]*\})|((([^ \t\n]*)(\$\{[^ \$\}\t\n]*\})+([^ \t\n]*)(\$\{[^ \$\}\t\n]*\})*([^ \t\n]*)))+|((([^ \t\n]*)(\$\{[^ \$\}\t\n]*\})*([^ \t\n]*)(\$\{[^ \$\}\t\n]*\})+([^ \t\n]*)))+ {
  // Expands variables
  std::string str = std::string(yytext);
  std::string prior_str = "";
  std::string exp_str = "";
  size_t pos = str.find("${");
  while (pos != std::string::npos) {
    if (pos > 0) {
      prior_str = str.substr(0, pos);
    }

    // Getting the variable to expand and expanding it
    exp_str = str.substr(pos + 2, str.find("}") - pos - 2);
    exp_str = *(new std::string(getenv(exp_str.c_str())));

    // Recombining the string to contain 1 more variable expansion than before
    str = prior_str + exp_str + str.substr(str.find("}") + 1);
    pos = str.find("${");
  }

  // Cleaning up
  prior_str.erase();
  exp_str.erase();
  
  yylval.cpp_string = new std::string(str.c_str());
  str.erase();
  return WORD;
}


($\([^\n]*\))|(`[^\n]*`)  {
  // Executing subshell commands

  // Getting the input to the subshell
  remove_escape_char(yytext);
  int len = strlen(yytext);
  if (yytext[0] == '$') {
    strncpy(yytext, yytext + 2, len - 2);
    yytext[len - 3] = 0;
  }
  else {
    strncpy(yytext, yytext + 1, len - 2);
    yytext[len - 2] = 0;
  }

  // generating subshell input
  char *input_string = strcat(yytext, "\nexit\n");

  // Saving current io streams
  int stdin_copy = dup(0);
  int stdout_copy = dup(1);

  // Setting up communication channels with subshell process
  int pin[2], pout[2];
  pipe(pin);
  pipe(pout);

  dup2(pin[0], 0);
  dup2(pout[1], 1);

  // Writing input to pipe
  write(pin[1], (const char *) input_string, strlen(input_string));
  close(pin[1]);

  int ret = fork();
  if (ret == 0) {
    // Child process to execute the subshell
    char *ptrs[2];
    ptrs[0] = (char *)"/proc/self/exe";
    ptrs[1] = NULL;
    execvp(ptrs[0], ptrs);
    _exit(1);
  }

  // Reading subshell output
  waitpid(ret, NULL, 0);
  char buf[STR_SIZE];
  size_t read_size = read(pout[0], buf, STR_SIZE);
  close(pout[0]);

  // Pushing subshell output back to buffer
  for (int index = read_size - 16; index >= 0; index--) {
    if (buf[index] == '\n') {
      buf[index] = '\t';
    }
    myunputc(buf[index]);
  }

  // Restoring the IO streams
  dup2(stdin_copy, 0);
  close(stdin_copy);
  dup2(stdout_copy, 1);
  close(stdout_copy);
}


\<\([^\n]*\) {
  // Process substitution
  yylval.cpp_string = new std::string(yytext);
  return WORD;
}


[^ >|\t\n][^ >|\t\n]*  {
  // Dealing with all other regular words
  remove_escape_char(yytext);
  yylval.cpp_string = new std::string(yytext);
  return WORD;
}
