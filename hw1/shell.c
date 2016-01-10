#include <ctype.h>
#include <errno.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <signal.h>
#include <sys/wait.h>
#include <termios.h>
#include <unistd.h>
#include <sys/stat.h>
#include <fcntl.h>

#include "io.h"
#include "parse.h"
#include "process.h"
#include "shell.h"

/* Whether the shell is connected to an actual terminal or not. */
bool shell_is_interactive;

/* File descriptor for the shell input */
int shell_terminal;

/* Terminal mode settings for the shell */
struct termios shell_tmodes;

/* Process group id for the shell */
pid_t shell_pgid;

int cmd_quit(tok_t arg[]);
int cmd_help(tok_t arg[]);
int cmd_pwd(tok_t arg[]);
int cmd_cd(tok_t arg[]);
void run_prog(tok_t *arg);
tok_t *redirect(tok_t *arg);

/* Built-in command functions take token array (see parse.h) and return int */
typedef int cmd_fun_t(tok_t args[]);

/* Built-in command struct and lookup table */
typedef struct fun_desc {
  cmd_fun_t *fun;
  char *cmd;
  char *doc;
} fun_desc_t;

fun_desc_t cmd_table[] = {
  {cmd_help, "?", "show this help menu"},
  {cmd_quit, "quit", "quit the command shell"},
  {cmd_pwd, "pwd", "print the current working directory"},
  {cmd_cd, "cd", "change the current working directory"},
};
/**
 * Changes the current working directory
 */
int cmd_cd(tok_t arg[]) {
	if (chdir(arg[0]) == 0) {
		cmd_pwd(NULL);
		return 1;
	} else {
		printf("Change working directory aborts!\n");
		return 0;
	}
}
/**
 * Prints the current working directory
 */
int cmd_pwd(tok_t arg[]) {
    char *buf;
    long size = pathconf(".", _PC_PATH_MAX);
    if ((buf = (char *)malloc((size_t)size)) != NULL) {
	getcwd(buf, (size_t)size);
        printf("%s\n", buf);
	free(buf);
    }   
    return 1;
}
/**
 * Prints a helpful description for the given command
 */
int cmd_help(tok_t arg[]) {
  for (int i = 0; i < sizeof(cmd_table) / sizeof(fun_desc_t); i++) {
    printf("%s - %s\n", cmd_table[i].cmd, cmd_table[i].doc);
  }
  return 1;
}

/**
 * Quits this shell
 */
int cmd_quit(tok_t arg[]) {
  exit(0);
  return 1;
}

/**
 * Looks up the built-in command, if it exists.
 */
int lookup(char cmd[]) {
  for (int i = 0; i < sizeof(cmd_table) / sizeof(fun_desc_t); i++) {
    if (cmd && (strcmp(cmd_table[i].cmd, cmd) == 0)) return i;
  }
  return -1;
}

/**
 * Intialization procedures for this shell
 */
void init_shell() {
  /* Check if we are running interactively */
  shell_terminal = STDIN_FILENO;
  shell_is_interactive = isatty(shell_terminal);

  if(shell_is_interactive){
    /* Force the shell into foreground */
    while(tcgetpgrp(shell_terminal) != (shell_pgid = getpgrp()))
      kill(-shell_pgid, SIGTTIN);

    /* Saves the shell's process id */
    shell_pgid = getpid();

    /* Take control of the terminal */
    tcsetpgrp(shell_terminal, shell_pgid);
    tcgetattr(shell_terminal, &shell_tmodes);
  }
}

tok_t *redirect(tok_t *arg) {
	int  i = 0;
	bool is_arg = true;
	int  fd;
	while (arg[i] != NULL) {
		if (!strcmp(arg[i],"<")) {
			is_arg = false;
			if ((fd = open(arg[i+1],O_RDONLY)) >= 0) 
				dup2(fd, 0);
		}
		if (!strcmp(arg[i],">")) {
			is_arg = false;
			if ((fd = open(arg[i+1],O_WRONLY|O_TRUNC|O_CREAT,S_IRUSR|S_IRGRP|S_IWGRP|S_IWUSR )) >= 0) 			
				dup2(fd, 1);
		}
		if (!is_arg) arg[i] = NULL;
		i++;
	}
	return arg;
}
void run_prog(tok_t *arg) {
	int child_pid = fork();
	if (child_pid == 0) {
		arg = redirect(arg);
		if (execv(arg[0], arg) < 0) {
			tok_t *path = get_toks(getenv("PATH"));
			char *prog_name = arg[0];
			int i = -1;
			while (path[++i] != NULL) {
				strcat(path[i],"/");
				strcat(path[i], prog_name);
				arg[0] = path[i];
				execv(arg[0],arg);			
			}		
		}
	} else {
		int pid, status;
		if ((pid = waitpid(child_pid, &status, 0)) < 0) {
			printf("waitpid() error in run_prog()!\n");
        } 
	}	
}

int shell(int argc, char *argv[]) {
  char *input_bytes;
  tok_t *tokens;
  int line_num = 0;
  int fundex = -1;

  init_shell();

  if (shell_is_interactive)
    /* Please only print shell prompts when standard input is not a tty */
    fprintf(stdout, "%d: ", line_num);

  while ((input_bytes = freadln(stdin))) {
    tokens = get_toks(input_bytes);
    fundex = lookup(tokens[0]);
    if (fundex >= 0) {
      cmd_table[fundex].fun(&tokens[1]);
    } else {
      /* REPLACE this to run commands as programs. */
      //fprintf(stdout, "This shell doesn't know how to run programs.\n");
	  run_prog(tokens);
    }

    if (shell_is_interactive)
      /* Please only print shell prompts when standard input is not a tty */
      fprintf(stdout, "%d: ", ++line_num);
  }

  return 0;
}
