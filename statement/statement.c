#include <stdio.h>
#include <stdlib.h>
#include <string.h>
// Hay un posible problema con el planteamiento de token type. Si se llega a
// implementar la redireccion si se pasa un archivo este sera visto como un
// proceso
enum TOKEN_TYPE { PROCESS, ENV_VAR, EXEC_CONTROL, DEFAULT };
enum OPERATOR_TYPE { AND, XOR, OR, BACKGROUND, REDIRECTION, FD };

enum OPERATOR_TYPE getOperatorType(const char *op) {
  if (strcmp(op, "&&") == 0) {
    return AND;
  } else if (strcmp(op, "||") == 0) {
    return XOR;
  } else if (strcmp(op, ";") == 0) {
    return OR;
  } else if (strcmp(op, "&") == 0) {
    return BACKGROUND;
  } else if (strcmp(op, "<") == 0 || strcmp(op, ">") == 0 ||
             strcmp(op, ">>") == 0 || strcmp(op, "2>") == 0 ||
             strcmp(op, "&>") == 0) {
    return REDIRECTION;
  } else if (strcmp(op, "&1") == 0 || strcmp(op, "&2") == 0) {
    return FD;
  }
  return REDIRECTION; // Default
}
typedef struct {
  char **argv;
  uint argc;
} Command;

typedef struct {
  const char *key;
  const char *value;
} EnvVar;

typedef struct {

  union {
    Command command;
    EnvVar envVar;
    const char *execControl;
  };
  enum TOKEN_TYPE type;
} Node;

typedef struct Tree {
  Node *node;
  struct Tree *leftSon;
  struct Tree *rightSon;
} Tree;
typedef struct Stack {
  Tree *currentTree;
  struct Stack *prev;
} Stack;
void destroyStack(Stack *stack) {
  if (stack->prev) {
    destroyStack(stack->prev);
  }
  free(stack);
}
void unStack(Stack **stack) {
  if (!*stack)
    return;
  Stack *toFree = *stack;
  *stack = (*stack)->prev;
  free(toFree);
}
void addToStack(Stack **stack, Tree *tree) {
  Stack *newStack = malloc(sizeof(Stack));
  newStack->currentTree = tree;
  newStack->prev = *stack;
  *stack = newStack;
}
const char *reserved[] = {
    "<",  ">",  ">>", "2>", "&>", // Redireccion
    "&1", "&2",                   // fd
    ";",  "&&", "||",
    "&",              // Control de ejecucion
    "\"", "\'", "\\", // Comillas
    NULL              //
};

Tree *createTree(char **tokens) {
  Tree *root = malloc(sizeof(Tree));
  root->leftSon = NULL;
  root->rightSon = NULL;
  root->node = NULL;

  char *currentReserved = NULL;
  int previousWasProcess = 0;
  int wasReserved = 0;
  for (int i = 0; tokens[i] != NULL; i++) {
    for (int j = 0; reserved[j] != NULL; j++) {
      if (strcmp(tokens[i], reserved[j]) == 0) {
        wasReserved = 1;
        previousWasProcess = 0;
        Node *newNode = malloc(sizeof(Node));
        newNode->type = EXEC_CONTROL;
        newNode->execControl = reserved[j];
        if (!root->node) {
          root->node = newNode;
        } else {
          Tree *newTree = malloc(sizeof(Tree));
          newTree->node = newNode;
          newTree->leftSon = root;
          newTree->rightSon = NULL;
          root = newTree;
        }
        break;
      }
    }

    if (wasReserved) {
      wasReserved = 0;
      continue;
    }

    int exit = 0;
    Tree *currentLeaf = root;
    if (!currentLeaf->leftSon) {
      Tree *leftSon = malloc(sizeof(Tree));
      leftSon->leftSon = NULL;
      leftSon->rightSon = NULL;
      Node *newNode = malloc(sizeof(Node));
      newNode->type = PROCESS;
      newNode->command.argc = 1;
      newNode->command.argv = malloc(sizeof(char *) * 2);
      newNode->command.argv[0] = tokens[i];
      newNode->command.argv[1] = NULL;

      leftSon->node = newNode;
      currentLeaf->leftSon = leftSon;
    } else if (!previousWasProcess) {

      Tree *rightSon = malloc(sizeof(Tree));
      rightSon->leftSon = NULL;
      rightSon->rightSon = NULL;
      Node *newNode = malloc(sizeof(Node));
      newNode->type = PROCESS;
      newNode->command.argc = 1;
      newNode->command.argv = malloc(sizeof(char *) * 2);
      newNode->command.argc = 1;
      newNode->command.argv[0] = tokens[i];
      newNode->command.argv[1] = NULL;
      rightSon->node = newNode;
      currentLeaf->rightSon = rightSon;
    } else if (!currentLeaf->rightSon) {

      Node *currentNode = currentLeaf->leftSon->node;
      Command *cmd = &(currentNode->command);
      cmd->argc++;
      cmd->argv = realloc(cmd->argv, sizeof(char *) * ((cmd->argc) + 1));
      cmd->argv[cmd->argc - 1] = tokens[i];
      cmd->argv[cmd->argc] = NULL;
    } else {
      Node *currentNode = currentLeaf->rightSon->node;
      Command *cmd = &(currentNode->command);
      cmd->argc++;
      cmd->argv =
          realloc(cmd->argv, sizeof(char *) * ((cmd->argc) + 1)); // NULL
      cmd->argv[cmd->argc - 1] = tokens[i];
      cmd->argv[cmd->argc] = NULL;
    }
    previousWasProcess = 1;
  }
  return root;
}
void freeTree(Tree *tree) {
  if (!tree)
    return;

  freeTree(tree->leftSon);
  freeTree(tree->rightSon);

  if (tree->node) {
    if (tree->node->type == PROCESS && tree->node->command.argv) {
      free(tree->node->command.argv);
    }
    free(tree->node);
  }

  free(tree);
}

#ifdef TEST_TREE

void printTree(Tree *tree, int depth) {
  if (!tree)
    return;
  for (int i = 0; i < depth; i++)
    printf("  ");
  if (!tree->node) {
    printf("↳ (null)\n");
    return;
  }
  switch (tree->node->type) {
  case PROCESS:
    printf("↳ PROCESS: %s", tree->node->command.bin);
    printf("  %u args:", tree->node->command.argc);
    if (tree->node->command.argv) {
      for (uint i = 0; i <= tree->node->command.argc; i++) {
        for (int j = 0; j < depth + 1; j++)
          printf(" ");
        printf("arg[%u]: %s ", i,
               tree->node->command.argv[i] ? tree->node->command.argv[i]
                                           : "NULL");
      }
      printf("\n");
    }
    break;
  case EXEC_CONTROL:
    printf("↳ EXEC_CONTROL: %s\n", tree->node->execControl);
    break;
  case ENV_VAR:
    printf("↳ ENV_VAR: %s=%s\n", tree->node->envVar.key,
           tree->node->envVar.value);
    break;
  default:
    printf("↳ DEFAULT\n");
  }
  printTree(tree->leftSon, depth + 1);
  printTree(tree->rightSon, depth + 1);
}

// Función para liberar memoria

int main() {
  // Simulación de tokens tipo shell
  char *tokens[] = {"echo", "hola", "&&",  "ls",          "-l",
                    "&",    ";",    "cat", "archivo.txt", NULL};

  // Crear árbol
  Tree *root = createTree(tokens);

  // Función para imprimir el árbol

  // Mostrar árbol
  printf("Árbol generado:\n");
  printTree(root, 0);

  // Liberar memoria
  freeTree(root);
  return 0;
}
#endif
