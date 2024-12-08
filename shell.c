
// Alex Nguyen

#include <ctype.h>
#include <direct.h>
#include <errno.h>
#include <fcntl.h>
#include <io.h>
#include <process.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>

#define MAX_INPUT 1024
#define MAX_ARGS 64
#define MAX_VARS 64

typedef struct {
  char name[64];
  char value[256];
} EnvVar;

EnvVar env_vars[MAX_VARS];
int env_var_count = 0;

void handle_cd(char **args);
void handle_pwd();
void handle_set(char **args);
void handle_unset(char **args);
void handle_echo(char **args);
void substitute_env_vars(char *command);
void execute_command(char **args, int bg, int input_fd, int output_fd);
void parse_and_execute(char *input);

void handle_cd(char **args) {
  if (args[1] == NULL) {
    fprintf(stderr, "cd: expected argument\n");
  } else {
    if (_chdir(args[1]) != 0) {
      perror("cd");
    }
  }
}

void handle_pwd() {
  char cwd[MAX_INPUT];
  if (_getcwd(cwd, sizeof(cwd)) != NULL) {
    printf("%s\n", cwd);
  } else {
    perror("pwd");
  }
}

void handle_set(char **args) {
  if (args[1] == NULL || args[2] == NULL) {
    fprintf(stderr, "set: expected variable and value\n");
  } else {
    _putenv_s(args[1], args[2]);
  }
}

void handle_unset(char **args) {
  if (args[1] == NULL) {
    fprintf(stderr, "unset: expected variable\n");
  } else {
    _putenv_s(args[1], "");
  }
}

void handle_echo(char **args) {
  char buffer[MAX_INPUT];

  for (int index = 1; args[index] != NULL; ++index) {
    if (index > 1)
      printf(" ");

    // Make a copy of the argument for substitution
    if (strlen(args[index]) < MAX_INPUT) {
      strcpy(buffer, args[index]);
      substitute_env_vars(buffer);
      printf("%s", buffer);
    } else {
      fprintf(stderr, "Error: argument too long\n");
    }
  }
  printf("\n");
}

void substitute_env_vars(char *command) {
  char buffer[MAX_INPUT];
  char *start, *end;
  buffer[0] = '\0';

  while ((start = strchr(command, '$'))) {
    // Copy everything before the $ symbol
    strncat(buffer, command, start - command);

    // Find the variable name after $
    end = start + 1;
    while (*end && (isalnum(*end) || *end == '_'))
      end++;

    // Extract variable name
    char var_name[128];
    strncpy(var_name, start + 1, end - start - 1);
    var_name[end - start - 1] = '\0';

    // Lookup variable value
    const char *value = getenv(var_name);

    // Append value or empty string
    strcat(buffer, value ? value : "");

    // Move to the next character after the variable
    command = end;
  }

  strcat(buffer, command);
  strcpy(command, buffer);
}

void execute_command(char **args, int bg, int input_fd, int output_fd) {
  int pid;
  int status;

  if (input_fd != STDIN_FILENO) {
    _dup2(input_fd, STDIN_FILENO);
    _close(input_fd);
  }
  if (output_fd != STDOUT_FILENO) {
    _dup2(output_fd, STDOUT_FILENO);
    _close(output_fd);
  }

  if (bg) {
    pid = _spawnvp(_P_NOWAIT, args[0], (const char *const *)args);
  } else {
    pid = _spawnvp(_P_WAIT, args[0], (const char *const *)args);
  }

  if (pid == -1) {
    perror("spawnvp");
  }
}

void parse_and_execute(char *input) {
  char *args[MAX_ARGS];
  char *token;
  int bg = 0;
  int input_fd = STDIN_FILENO;
  int output_fd = STDOUT_FILENO;

  // Check for background execution
  if (strchr(input, '&')) {
    bg = 1;
    *strchr(input, '&') = '\0';
  }

  // Check for input redirection
  char *input_redir = strchr(input, '<');
  if (input_redir) {
    *input_redir = '\0';
    char *infile = strtok(input_redir + 1, " \t\n");
    if (infile) {
      input_fd = _open(infile, _O_RDONLY);
      if (input_fd == -1) {
        perror("open");
        return;
      }
    } else {
      fprintf(stderr, "Syntax error: no input file specified\n");
      return;
    }
  }

  // Check for output redirection
  char *output_redir = strchr(input, '>');
  if (output_redir) {
    *output_redir = '\0';
    char *outfile = strtok(output_redir + 1, " \t\n");
    if (outfile) {
      output_fd =
          _open(outfile, _O_WRONLY | _O_CREAT | _O_TRUNC, S_IRUSR | S_IWUSR);
      if (output_fd == -1) {
        perror("open");
        return;
      }
    } else {
      fprintf(stderr, "Syntax error: no output file specified\n");
      return;
    }
  }

  int i = 0;
  token = strtok(input, " \t\n");
  while (token) {
    args[i++] = token;
    token = strtok(NULL, " \t\n");
  }
  args[i] = NULL;

  if (!args[0])
    return;

  if (strcmp(args[0], "cd") == 0) {
    handle_cd(args);
  } else if (strcmp(args[0], "pwd") == 0) {
    handle_pwd();
  } else if (strcmp(args[0], "set") == 0) {
    handle_set(args);
  } else if (strcmp(args[0], "unset") == 0) {
    handle_unset(args);
  } else if (strcmp(args[0], "echo") == 0) {
    handle_echo(args);
  } else {
    execute_command(args, bg, input_fd, output_fd);
  }

  if (input_fd != STDIN_FILENO)
    _close(input_fd);
  if (output_fd != STDOUT_FILENO)
    _close(output_fd);
}

int main() {
  char input[MAX_INPUT];

  while (1) {
    printf("xsh# ");
    if (!fgets(input, sizeof(input), stdin))
      break;
    if (strcmp(input, "quit\n") == 0 || strcmp(input, "exit\n") == 0)
      break;
    parse_and_execute(input);
  }

  return 0;
}
