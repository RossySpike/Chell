#include <stdio.h>
#include <stdlib.h>
#include <string.h>

char **parseInput(char *input) {
  const char *delims[] = {" ", "\"", "'"};
  char **tokens = NULL;
  int currentDelim = 0;
  char *localInput = input;
  char *token = NULL;
  switch (localInput[0]) {
  case ' ': // space
    currentDelim = 0;
    break;
  case '"': // "
    currentDelim = 1;
    break;
  case '\'': // '
    currentDelim = 2;
    break;
  }

  int currentToken = 0;
  do {
    token = strtok(currentToken ? NULL : localInput, delims[currentDelim]);
    if (!tokens) {
      tokens = malloc(sizeof(char *));
    } else {
      tokens = realloc(tokens, sizeof(char *) * (currentToken + 1));
    }
    tokens[currentToken++] = token;

  } while (token != NULL);

  return tokens;
}
#ifdef TEST_PARSER
int main() {
  printf("->: ");
  char buffer[1024];

  fgets(buffer, sizeof(buffer), stdin);
  buffer[strcspn(buffer, "\n")] = 0; // \n -> \0
  char **tokens = parseInput(buffer);
  for (int i = 0; tokens[i] != NULL; i++) {
    printf("Parsed token: %s\n", tokens[i]);
  }
  free(tokens);
  return 0;
}
#endif
