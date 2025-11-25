#include <stdio.h>
#include <stdlib.h>
#include <string.h>
// Hay un posible problema con el planteamiento de token type. Si se llega a
// implementar la redireccion si se pasa un archivo este sera visto como un
// proceso
enum TOKEN_TYPE { PROCESS, ENV_VAR, EXEC_CONTROL, FILE_PATH, DEFAULT };
enum OPERATOR_TYPE {
  AND,
  XOR,
  OR,
  BACKGROUND,
  REDIRECTION_INPUT,
  REDIRECTION_OUTPUT,
  REDIRECTION_APPEND,
  REDIRECTION_ERROR_OUTPUT,
  REDIRECTION_ALL_OUTPUT,
  REDIRECTION_STDERR_TO_STDOUT,
  REDIRECTION_STDOUT_TO_STDERR,
  PIPE
};

enum OPERATOR_TYPE getOperatorType(const char *op) {
  if (strcmp(op, "&&") == 0) {
    return AND;
  } else if (strcmp(op, "||") == 0) {
    return XOR;
  } else if (strcmp(op, ";") == 0) {
    return OR;
  } else if (strcmp(op, "&") == 0) {

    return BACKGROUND;
  } else if (strcmp(op, "<") == 0)
    return REDIRECTION_INPUT;
  else if (strcmp(op, ">") == 0) {
    return REDIRECTION_OUTPUT;
  } else if (strcmp(op, ">>") == 0) {
    return REDIRECTION_APPEND;
  } else if (strcmp(op, "2>") == 0) {
    return REDIRECTION_ERROR_OUTPUT;
  } else if (strcmp(op, "&>") == 0) {
    return REDIRECTION_ALL_OUTPUT;
  } else if (strcmp(op, "&1") == 0) {
    return REDIRECTION_STDERR_TO_STDOUT;
  } else if (strcmp(op, "&2") == 0) {
    return REDIRECTION_STDOUT_TO_STDERR;
  } else if (strcmp(op, "|") == 0) {
    return PIPE;
  }
  return -1; // ???
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

  const char *operator;
  enum OPERATOR_TYPE opType;
} ExecControl;
typedef struct {

  union {
    Command command;
    EnvVar envVar;
    ExecControl execControl;
    const char *filePath;
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
  /* printf("Current stack address: %p\n", (void *)*stack); */
  /* printf("Adding tree to stack address: %p\n", (void *)tree); */
  /* printf("Tree->node %p\n", (void *)tree->node); */
  /* printf("Tree->leftSon %p\n", (void *)tree->leftSon); */
  Stack *newStack = malloc(sizeof(Stack));
  newStack->currentTree = tree;
  newStack->prev = *stack;
  *stack = newStack;
}
const char *reserved[] = {
    "<",  ">",  ">>", "2>", "&>", "&1", "&2", // Redireccion
    ";",  "&&", "||",
    "&",              // Control de ejecucion
    "\"", "\'", "\\", // Comillas
    "|",              // Pipe
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
    enum OPERATOR_TYPE opType = getOperatorType(tokens[i]);
    /* printf("Token: %s, OpType: %d\n", tokens[i], opType); */
    if (opType != -1) {
      /* printf("Reserved token found: %s\n", tokens[i]); */
      wasReserved = 1;
      previousWasProcess = 0;
      Node *newNode = malloc(sizeof(Node));
      newNode->type = EXEC_CONTROL;
      newNode->execControl.operator= tokens[i];
      newNode->execControl.opType = opType;
      if (!root->node) {
        root->node = newNode;
      } else {
        Tree *newTree = malloc(sizeof(Tree));
        newTree->node = newNode;
        newTree->leftSon = root;
        newTree->rightSon = NULL;
        root = newTree;
      }
    }
    //

    if (wasReserved) {
      wasReserved = 0;
      continue;
    }

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
      switch (currentLeaf->node->execControl.opType) {
      case AND:
      case XOR:
      case OR:
      case BACKGROUND:
      case PIPE:
        newNode->type = PROCESS;
        newNode->command.argc = 1;
        newNode->command.argv = malloc(sizeof(char *) * 2);
        newNode->command.argc = 1;
        newNode->command.argv[0] = tokens[i];
        newNode->command.argv[1] = NULL;
        break;
      case REDIRECTION_INPUT:
      case REDIRECTION_OUTPUT:
      case REDIRECTION_APPEND:
      case REDIRECTION_ERROR_OUTPUT:
      case REDIRECTION_ALL_OUTPUT:
      case REDIRECTION_STDERR_TO_STDOUT:
      case REDIRECTION_STDOUT_TO_STDERR:
        newNode->type = FILE_PATH;
        newNode->filePath = tokens[i];
        break;
      default:
        break;
      }

      rightSon->node = newNode;
      currentLeaf->rightSon = rightSon;
    } else if (!currentLeaf->rightSon || currentLeaf->node->type == FILE_PATH) {

      Node *currentNode = currentLeaf->leftSon->node;
      Command *cmd = &(currentNode->command);
      cmd->argc++;
      cmd->argv = realloc(cmd->argv, sizeof(char *) * ((cmd->argc) + 1));
      cmd->argv[cmd->argc - 1] = tokens[i];
      cmd->argv[cmd->argc] = NULL;
    } else { // If node is filePath it never reaches here
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
    /* printf("Freeing tree node\n"); */
    /* printf("Tree node address: %p\n", (void *)tree); */
    /* printf("Tree leftSon address: %p\n", (void *)tree->leftSon); */
    /* printf("Tree rightSon address: %p\n", (void *)tree->rightSon); */
    /* printf("Tree node content address: %p\n", (void *)tree->node); */
    /**/
    /* printf("Tree node type: %d\n", tree->node->type); */
    /* printf("\n"); */
    if (tree->node->type == PROCESS) {

      if (tree->node->command.argv) {
        free(tree->node->command.argv);
      }
    }

    free(tree->node);
  }

  free(tree);
}

#include <stdio.h>

#include <stdio.h>
#include <string.h>

void printNode(Tree *tree) {
  switch (tree->node->type) {
  case PROCESS:
    printf("PROCESS: ");
    for (int i = 0; i < tree->node->command.argc; i++) {
      printf("%s ", tree->node->command.argv[i]);
    }
    printf("\n");
    break;
  case EXEC_CONTROL:
    printf("EXEC_CONTROL: %s (opType=%d)\n", tree->node->execControl.operator,
           tree->node->execControl.opType);
    break;
  case FILE_PATH:
    printf("FILE_PATH: %s\n", tree->node->filePath);
    break;
  case ENV_VAR:
    printf("ENV_VAR: %s=%s\n", tree->node->envVar.key,
           tree->node->envVar.value);
    break;
  default:
    printf("UNKNOWN NODE\n");
  }
}
void printTree(Tree *tree, const char *relation, int depth) {
  if (!tree) {

    printf("No tree to display %d.\n", depth);
    return;
  }
  if (!tree->node) {
    printf("Empty node at depth %d.\n", depth);
    return;
  }

  // Indentación para visualizar niveles
  for (int i = 0; i < depth; i++) {
    printf("  ");
  }

  // Mostrar relación (Root, Left, Right)
  printf("%s -> ", relation);

  // Mostrar contenido del nodo
  switch (tree->node->type) {
  case PROCESS:
    printf("PROCESS: ");
    for (int i = 0; i < tree->node->command.argc; i++) {
      printf("%s ", tree->node->command.argv[i]);
    }
    printf("\n");
    break;
  case EXEC_CONTROL:
    printf("EXEC_CONTROL: %s (opType=%d)\n", tree->node->execControl.operator,
           tree->node->execControl.opType);
    break;
  case FILE_PATH:
    printf("FILE_PATH: %s\n", tree->node->filePath);
    break;
  case ENV_VAR:
    printf("ENV_VAR: %s=%s\n", tree->node->envVar.key,
           tree->node->envVar.value);
    break;
  default:
    printf("UNKNOWN NODE\n");
  }

  // Recorrer hijos
  if (tree->leftSon)
    printTree(tree->leftSon, "Left", depth + 1);
  if (tree->rightSon)
    printTree(tree->rightSon, "Right", depth + 1);
}
#ifdef TEST_TREE

// Función para liberar memoria

int main() {
  // Simulación de tokens tipo shell
  char *tokens[] = {"echo",
                    "hola",
                    "&&",
                    "ls",
                    "-l",
                    "&",
                    ";",
                    "cat",
                    "archivo.txt",
                    ">",
                    "~/melonds/pene",
                    NULL};

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
