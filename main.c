#include "parser/parser.c"
#include "spawn/spawn.c"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
int main() {
  char buffer[1024];
  char cwd[1024];
  while (1) {
    bzero(buffer, 1024);
    bzero(cwd, 1024);

    getcwd(cwd, sizeof(cwd));
    printf("%s $:", cwd);
    fgets(buffer, sizeof(buffer), stdin);
    buffer[strcspn(buffer, "\n")] = 0;
    char **tokens = parseInput(buffer);

    if (tokens[0] == NULL) {

      free(tokens);
      continue;
    }

    if (tokens && strcmp(tokens[0], "exit") == 0) {

      free(tokens);
      return 0;
    }
    Tree *tree = createTree(tokens);
    /* printf("\n\n"); */
    /* printTree(tree, "Root", 0); */
    /* printf("\n\n"); */
    int spawnResult = processTree(tree);

    freeTree(tree);
    free(tokens);

    if (spawnResult == EXIT) {
      break;
    }
  }
  return 0;
}
