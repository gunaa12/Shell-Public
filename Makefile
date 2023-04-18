# Defining compiler and compiler flags
cc= gcc
CC= g++
ccFLAGS= -g -std=c11
CCFLAGS= -g -std=c++17
WARNFLAGS= -Wall -Wextra -pedantic

# Defining lex and yacc
LEX=lex -l
YACC=yacc -y -d -t --debug

# Allowing editing of commands
EDIT_MODE_ON=YES
ifdef EDIT_MODE_ON
	EDIT_MODE_OBJECTS=tty-raw-mode.o read-line.o
endif

all: shell

# Compiling the individual parts
lex.yy.o: shell.l
	$(LEX) -o lex.yy.cc shell.l
	$(CC) $(CCFLAGS) -c lex.yy.cc

y.tab.o: shell.y
	$(YACC) -o y.tab.cc shell.y
	$(CC) $(CCFLAGS) -c y.tab.cc

command.o: command.cc command.hh
	$(CC) $(CCFLAGS) $(WARNFLAGS) -c command.cc

simpleCommand.o: simpleCommand.cc simpleCommand.hh
	$(CC) $(CCFLAGS) $(WARNFLAGS) -c simpleCommand.cc

shell.o: shell.cc shell.hh
	$(CC) $(CCFLAGS) $(WARNFLAGS) -c shell.cc

# Compiling the overall project
shell: y.tab.o lex.yy.o shell.o command.o simpleCommand.o $(EDIT_MODE_OBJECTS)
	$(CC) $(CCFLAGS) $(WARNFLAGS) -o shell lex.yy.o y.tab.o shell.o command.o simpleCommand.o $(EDIT_MODE_OBJECTS)

# Compiling the parts necessary for edit mode
tty-raw-mode.o: tty-raw-mode.c
	$(cc) $(ccFLAGS) $(WARNFLAGS) -c tty-raw-mode.c

read-line.o: read-line.cc
	$(CC) $(CCFLAGS) $(WARNFLAGS) -c read-line.cc

# Removes all the non source files
clean:
	rm -f lex.yy.cc y.tab.cc y.tab.hh shell *.o
