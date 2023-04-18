// Imports
#include "shell.hh"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <termios.h>
#include <dirent.h>


// Defines
#define MAX_BUFFER_LINE 2048


// Global Variables
extern "C" void tty_raw_mode(void);
int line_length, line_pos, history_length = 0, history_index = 0;
char line_buffer[MAX_BUFFER_LINE];
char *history[100];
bool prevCmdTab = false;


// Printing read line usage
void read_line_print_usage() {
  char * usage = (char *) "\n"
    " ctrl-?       Print usage\n"
    " Backspace    Deletes last character\n"
    " up arrow     See last command in the history\n";

  write(1, usage, strlen(usage));
}

char *commonPrefix(const char *str1, const char *str2, char *prefix) {
  for (; *str1 != 0 && *str1 == *str2; ++str1, ++str2) {
    *prefix = *str1;
    prefix++;
  }

  *prefix = 0;
  return prefix;
}


/*
 * Input a line with some basic editing.
 */
char * read_line() {
  // saving initial terminal settings
  struct termios init_terminal;
  tcgetattr(0, &init_terminal);

  // Set terminal in raw mode
  tty_raw_mode();
  line_length = 0;
  line_pos = 0;

  for (int index = 0; index < MAX_BUFFER_LINE; index++) {
    line_buffer[index] = 0;
  }

  // Read one line until enter is typed
  while (1) {
    // Read one character in raw mode.
    char ch;
    read(0, &ch, 1);

    if (ch == 9) {
      // Tab autocomplete
      if ((line_length == 0) || (line_pos != line_length)) {
        continue;
      }

      int place_index = 0, temp = line_length - 1;
      char recent_word[1024];
      while ((temp >= 0) && (line_buffer[temp] != ' ')) {
        temp--;
      }
      while (temp < line_length) {
        recent_word[place_index++] = line_buffer[++temp];
      }
      int recent_word_len = --place_index;
      recent_word[place_index++] = '?';
      recent_word[place_index++] = '*';
      recent_word[place_index] = 0;

      expandWildcard((char *) "", recent_word);

      char to_add[100];
      if (files.size() == 1) {
        strcpy(to_add, files[0]->c_str());
      }
      else if (files.size() > 1) {
        commonPrefix(files[0]->c_str(), files[1]->c_str(), to_add);
        for (int i = 2; i < (int) files.size(); i++) {
          commonPrefix(to_add, files[i]->c_str(), to_add);
        }
      }

      bool show_options;
      if ((prevCmdTab == true) && (to_add[recent_word_len] == 0)) {
        show_options = true;
      }
      else {
        show_options = false;
      }

      if (show_options == true) {
        if (files.size() != 0) {
          char nl = '\n';
          char space = ' ';
          write(1, &nl, 1);

          for (int i = 0; i < (int) files.size(); i++) {
            write(1, files[i]->c_str(), files[i]->size());
            write(1, &space, 1);
            write(1, &space, 1);
          }

          write(1, &nl, 1);
          char *to_print = getenv("PROMPT");
          if (to_print == NULL) {
            to_print = (char *) "myshell>";
          }
          write(1, to_print, strlen(to_print));
          write(1, line_buffer, line_length);
        }
      }
      else {
        int len = strlen(&to_add[recent_word_len]);
        if (len != 0) {
          strcpy(&line_buffer[line_length], &to_add[recent_word_len]);
          write(1, &to_add[recent_word_len], len);
          line_length += len;
          line_pos += len;
        }
      }

      prevCmdTab = true;
      files.clear();
    }
    else if (ch == 127) {
      if ((line_pos >= 0) && (line_pos < line_length)) {
        int pos = line_pos;
        while (pos <= (line_length + 2)) {
          line_buffer[pos] = line_buffer[pos + 1];
          pos++;
        }

        char backspace = 8, space = ' ';
        pos = line_pos;
        while (pos < line_length) {
          write(1, &space, 1);
          pos++;
        }

        while (pos > 0) {
          write(1, &backspace, 1);
          pos--;
        }

        line_length = strlen(line_buffer);
        write(1, line_buffer, line_length);

        pos = line_length;
        while (pos > line_pos) {
          write(1, &backspace, 1);
          pos--;
        }
      }
      prevCmdTab = false;
    }
    else if (ch == 8) {
      if (line_pos > 0) {
        if (line_pos == line_length) {
          // Backspace was typed
          // Go back one character
          ch = 8;
          write(1,&ch,1);

          // Write a space to erase the last character read
          ch = ' ';
          write(1,&ch,1);

          // Go back one character
          ch = 8;
          write(1,&ch,1);

          // Remove one character from buffer
          line_buffer[line_pos] = 0;
          line_length--;
          line_pos--;
        }
        else {
          // updating the line buffer
          int line_pos_cpy = line_pos - 1;
          while (line_pos_cpy <= (line_length + 2)) {
            line_buffer[line_pos_cpy] = line_buffer[line_pos_cpy + 1];
            line_pos_cpy++;
          }

          // updating the line
          char backspace = 8, space = ' ';
          int pos = line_pos;
          while (pos <= line_length + 1) {
            write(1, &space, 1);
            pos++;
          }

          while (pos > 0) {
            write(1, &backspace, 1);
            pos--;
          }

          line_length = strlen(line_buffer);
          write(1, line_buffer, line_length);

          // moving cursor to correct place
          line_pos--;
          for (int iter = 0; iter < (line_length - line_pos); iter++) {
            write(1, &backspace, 1);
          }
        }
      }
      prevCmdTab = false;
    }
    else if (ch == 1) {
      // cntr-A or home
      char backspace = 8;
      while (line_pos > 0) {
        write(1, &backspace, 1);
        line_pos--;
      }
    }
    else if (ch == 5) {
      // cntr-E or end
      write(1, &line_buffer[line_pos], strlen(&line_buffer[line_pos]));
      line_pos = line_length;
      prevCmdTab = false;
    }
    else if (ch>=32) {
      if (line_pos == line_length) {
        // It is a printable character, so echo
        write(1,&ch,1);

        // If max number of character reached return
        if (line_length==MAX_BUFFER_LINE-2) break;

        // add char to buffer
        line_buffer[line_length]=ch;
        line_length++;
        line_pos++;
      }
      else {
        // updating the line buffer
        int temp = line_length;
        while (temp >= line_pos) {
          line_buffer[temp] = line_buffer[temp - 1];
          temp--;
        }
        line_buffer[line_pos] = ch;

        // updating the output
        char backspace = 8;
        int line_pos_cpy = line_pos;
        while (line_pos_cpy > 0) {
          write(1, &backspace, 1);
          line_pos_cpy--;
        }
        line_length = strlen(line_buffer);
        write(1, line_buffer, line_length);
        line_pos++;

        // moving cursor to correct place
        for (int iter = 0; iter < (line_length - line_pos); iter++) {
          write(1, &backspace, 1);
        }
      }
      prevCmdTab = false;
    }
    else if (ch==10) {
      // <Enter> was typed, so print new line
      write(1,&ch,1);
      prevCmdTab = false;
      break;
    }
    else if (ch == 31) {
      // ctrl-?
      read_line_print_usage();
      line_buffer[0]=0;
      prevCmdTab = false;
      break;
    }
    else if (ch==27) {
      // Escape sequence. Read two chars more to determine key
      char ch1, ch2;
      read(0, &ch1, 1);
      read(0, &ch2, 1);
      if (ch1==91) {
        if ((ch2 == 65) || (ch2 == 66)) {
          // Erase old line
          for (int i = 0; i < line_length; i++) {
            ch = 8;
            write(1,&ch,1);
          }

          // Print spaces on top
          for (int i = 0; i < line_length; i++) {
            ch = ' ';
            write(1,&ch,1);
          }

          // Print backspaces
          for (int i = 0; i < line_length; i++) {
            ch = 8;
            write(1,&ch,1);
            line_buffer[0] = 0;
          }

          if (ch2 == 65) {
            // up arrow
            if ((history_length == 0) || (history_index == -1)) {
              strcpy(line_buffer, "");
            }
            else if (history_index >= 0) {
              strcpy(line_buffer, history[history_index--]);
              history_index = history_index % history_length;
            }
          }
          else if (ch2 == 66) {
            // down arrow
            if ((history_index >= -1) && (history_index < history_length - 1)) {
              strcpy(line_buffer, history[++history_index]);
            }
            else if (history_length == history_index) {
              history_index = history_index - 1;
              strcpy(line_buffer, "");
            }
          }

          line_length = strlen(line_buffer);
          line_pos = line_length;
          write(1, line_buffer, line_length);
        }
        else if (ch2 == 67) {
          // right arrow
          if (line_pos < line_length) {
            ch = 27;
            write(1, &ch, 1);
            ch = 91;
            write(1, &ch, 1);
            ch = 67;
            write(1, &ch, 1);
            line_pos++;
          }
        }
        else if (ch2 == 68) {
          // left arrow
          if (line_pos > 0) {
            ch = 8;
            write(1, &ch, 1);
            line_pos--;
          }
        }
      }
      prevCmdTab = false;
    }
  }


  // Add eol and null char at the end of string
  line_buffer[line_length++]=10;
  line_buffer[line_length]=0;

  char *line_buffer_store = (char *) malloc(MAX_BUFFER_LINE * sizeof(char));
  strcpy(line_buffer_store, line_buffer);
  line_buffer_store[line_length - 1] = 0;
  history[history_length++] = line_buffer_store;
  history_index = history_length - 1;

  // reseting terminal settings
  tcsetattr(0, TCSANOW, &init_terminal);

  return line_buffer;
}
