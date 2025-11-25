#include <stdio.h>
#include <stdlib.h>
#include <string.h>

char **parseInput(char *input) {
  char **tokens = NULL;
  char *localInput = input;
  char *token = NULL;

  int currentToken = 0;
  do {
    token = strtok(currentToken ? NULL : localInput, " ");
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
