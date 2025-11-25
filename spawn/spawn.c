#include "../statement/statement.c"
#include <error.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#define EXIT 256
int getFd(const char *filePath, enum OPERATOR_TYPE op) {
  switch (op) {
    /* case REDIRECTION_INPUT: */
    /*   break; */
  case REDIRECTION_INPUT:
    return open(filePath, O_RDONLY);
    break;
  case REDIRECTION_OUTPUT:
    return open(filePath, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    break;
  case REDIRECTION_APPEND:
    return open(filePath, O_WRONLY | O_CREAT | O_APPEND, 0644);
    break;
  default:
    break;
  }
}
int spawn(char **args, int outputfd, int inputfd, int shouldWait, int opType) {
  /* printf("opType: %d\n", opType); */
  /* printf("shouldWait: %d\n", shouldWait); */
  /* printf("outputfd: %d\n", outputfd); */
  /* printf("Spawning process: %s\n", args[0]); */
  /* printf("With args:\n"); */
  /* for (int i = 0; args && args[i] != NULL; i++) { */
  /*   printf("  - %s\n", args[i]); */
  /* } */
  if (strcmp(args[0], "exit") == 0) {
    return EXIT;
  } else if (strcmp(args[0], "cd") == 0) {
    int i = 0;
    for (; args && args[i] != NULL; i++) {
      if (i > 2) {
        fprintf(stderr, "cd: too many arguments\n");
        return -1;
      }
    }
    char *path = i == 1 ? getenv("HOME") : args[1];

    chdir(path);

    return 0;
  }
  pid_t pid = fork();
  if (pid == 0) { // Child
    if (outputfd < 0) {
      perror("open");
      exit(1);
    }
    if (inputfd != STDIN_FILENO) {
      dup2(inputfd, STDIN_FILENO);
      if (inputfd > 2)
        close(inputfd);
    }
    switch (opType) {
    case REDIRECTION_OUTPUT:
    case REDIRECTION_APPEND:
      if (outputfd != STDOUT_FILENO) {

        dup2(outputfd, STDOUT_FILENO);
        if (outputfd > 2)
          close(outputfd);
      }
      break;
    case PIPE:
    case REDIRECTION_INPUT:
      if (outputfd != STDOUT_FILENO) {
        dup2(outputfd, STDOUT_FILENO);
        if (outputfd > 2)
          close(outputfd);
      }
    }
    execvp(args[0], args);
    exit(1);            // execvp failed
  } else if (pid > 0) { // Parent
    int status;
    if (!shouldWait)
      return 0;
    waitpid(pid, &status, 0);
    int wifexited = WIFEXITED(status);
    /* printf("WIFEXITED: %d", wifexited); */
    if (wifexited) {

      int wexited = WEXITSTATUS(status);
      /* printf(" | WEXITSTATUS %d\n", wexited); */
      return wexited;
    }
    return -1;
    /* return wifexited ? wexited : -1; */
    /* return WIFEXITED(status) ? WEXITSTATUS(status) : -1; */
  } else {
    // Fork failed
    return -1;
  }
}

int processTree(Tree *tree) {
  if (!tree)
    return 0;
  Stack *stack = malloc(sizeof(*stack));
  stack->currentTree = NULL;
  stack->prev = NULL;
  int retValue = 0;
  Tree *currentTree = tree;
  while (currentTree->leftSon != NULL) {
    addToStack(&stack, currentTree);
    currentTree = currentTree->leftSon;
  }
  int outputfdLeft = STDOUT_FILENO;
  int inputfdLeft = STDIN_FILENO;
  int outputfdRight = STDOUT_FILENO;
  int inputfdRight = STDIN_FILENO;
  int fds[2];
  int piped = 0;
  int pipedLeft = 0;
  int pipedInputRight;
  int rightCanStart = 1;
  int leftCanStart = 1;
  while (stack->currentTree) {

    currentTree = stack->currentTree;
    Node *currentNode = currentTree ? currentTree->node : NULL;
    Node *leftNode = currentTree->leftSon ? currentTree->leftSon->node : NULL;

    /* printNode(currentTree); */
    Node *rightNode =
        currentTree->rightSon ? currentTree->rightSon->node : NULL;

    int leftProcessStatus = 0;
    currentTree = stack->prev ? stack->prev->currentTree : NULL;
    unStack(&stack);
    int op = -1;

    if (leftNode->type == PROCESS && leftCanStart) {

      if (currentNode) {
        op = currentNode->execControl.opType;
      }
      int shouldWait = op == BACKGROUND ? 0 : 1;

      switch (op) {
        /* case REDIRECTION_INPUT: */
        /*   break; */
      case REDIRECTION_INPUT:
        inputfdLeft = open(rightNode ? rightNode->filePath : NULL, O_RDONLY);
        if (currentTree && currentTree->node &&
            currentTree->node->type == EXEC_CONTROL &&
            REDIRECTION_INPUT < currentTree->node->execControl.opType &&
            currentTree->node->execControl.opType < PIPE) {
          outputfdLeft =
              getFd(currentTree->rightSon
                        ? currentTree->rightSon->node
                              ? currentTree->rightSon->node->type == FILE_PATH
                                    ? currentTree->rightSon->node->filePath
                                    : NULL
                              : NULL
                        : NULL,
                    currentTree->node->execControl.opType);
          op = currentTree->node->execControl.opType;
          unStack(&stack);
        }
        if (currentTree && currentTree->node &&
            currentTree->node->type == EXEC_CONTROL &&
            currentTree->node->execControl.opType == PIPE) {
          pipe(fds);
          outputfdLeft = fds[1];
          pipedInputRight = fds[0];
          piped = 1;
          pipedLeft = 1;
        }
        break;
      case REDIRECTION_OUTPUT:
        outputfdLeft = open(rightNode ? rightNode->filePath : NULL,
                            O_WRONLY | O_CREAT | O_TRUNC, 0644);
        break;
      case REDIRECTION_APPEND:
        outputfdLeft = open(rightNode ? rightNode->filePath : NULL,
                            O_WRONLY | O_CREAT | O_APPEND, 0644);
        break;
      case PIPE:
        pipe(fds);
        outputfdLeft = fds[1];
        inputfdRight = fds[0];
        break;
      default:
        break;
      }
      retValue = leftProcessStatus = spawn(leftNode->command.argv, outputfdLeft,
                                           inputfdLeft, shouldWait, op);

      if (outputfdLeft > 2)
        close(outputfdLeft);
      if (inputfdLeft > 2)
        close(inputfdLeft);
      if (retValue == EXIT) {
        if (stack)
          destroyStack(stack);
        return EXIT;
      }
      if (op == AND) {
        rightCanStart = leftProcessStatus == 0 ? 1 : 0;
      } else if (op == XOR) {
        rightCanStart = leftProcessStatus != 0 ? 1 : 0;
      }
    }

    // 2.
    if (rightNode && rightCanStart) {
      Node *upperNode =
          stack ? stack->currentTree ? stack->currentTree->node : NULL : NULL;
      int opRight = upperNode ? upperNode->execControl.opType : -1;
      int shouldWaitRight = opRight == BACKGROUND ? 0 : 1;
      Node *upperRight =
          currentTree
              ? currentTree->rightSon ? currentTree->rightSon->node : NULL
              : NULL;
      if (piped) {
        piped = 0;
        inputfdRight = pipedInputRight;
      }

      if (op == PIPE) {
      }
      switch (opRight) {

        /* case REDIRECTION_INPUT: */
        /*   break; */
      case REDIRECTION_INPUT:
        inputfdRight = open(upperRight ? upperRight->filePath : NULL, O_RDONLY);
        if (stack && stack->prev && stack->prev->currentTree &&
            stack->prev->currentTree->node &&
            stack->prev->currentTree->node->type == EXEC_CONTROL) {
          if (REDIRECTION_INPUT <
                  stack->prev->currentTree->node->execControl.opType &&
              stack->prev->currentTree->node->execControl.opType < PIPE) {

            Tree *tree = stack->prev->currentTree;
            outputfdRight = getFd(
                tree->rightSon ? tree->rightSon->node
                                     ? tree->rightSon->node->type == FILE_PATH
                                           ? tree->rightSon->node->filePath
                                           : NULL
                                     : NULL
                               : NULL,
                stack->prev->currentTree->node->execControl.opType);
            opRight = stack->prev->currentTree->node->execControl.opType;
            unStack(&stack);
          } else if (stack->prev->currentTree->node->execControl.opType ==
                     PIPE) {
            pipe(fds);
            outputfdRight = fds[1];
            pipedInputRight = fds[0];
            piped = 1;
            pipedLeft = 1;
          }
        }
        break;
      case REDIRECTION_OUTPUT:
        outputfdRight = open(upperRight ? upperRight->filePath : NULL,
                             O_WRONLY | O_CREAT | O_TRUNC, 0644);
        break;
      case REDIRECTION_APPEND:
        outputfdRight = open(upperRight ? upperRight->filePath : NULL,
                             O_WRONLY | O_CREAT | O_APPEND, 0644);
        break;
      case PIPE:{

        int fdsRight[2];
        pipe(fdsRight);

        outputfdRight = fdsRight[1];
        if (pipedLeft) {

          close(fdsRight[0]);

          pipedLeft = 0;
        } else {
          pipedInputRight = fdsRight[0];
        }

        piped = 1;
        break;
        }

      default:
        outputfdRight = STDOUT_FILENO;
        break;
      }
      if (rightNode->type == PROCESS) {

        retValue = spawn(rightNode->command.argv, outputfdRight, inputfdRight,
                         shouldWaitRight, opRight);

        if (opRight == AND) {
          rightCanStart = leftCanStart = retValue == 0 ? 1 : 0;
        } else if (opRight == XOR) {
          rightCanStart = leftCanStart = retValue != 0 ? 1 : 0;
        }
        if (outputfdRight > 2)
          close(outputfdRight);
        if (inputfdRight > 2)
          close(inputfdRight);
        if (retValue == EXIT) {
          if (stack)
            destroyStack(stack);
          return EXIT;
        }
      }
    }
  }

  if (stack)
    destroyStack(stack);
  return retValue;
}
#ifdef TEST_SPAWN
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
    printf("↳ PROCESS: %s", tree->node->command.argv[0]);
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
    printf("↳ EXEC_CONTROL: %s\n", tree->node->execControl->operator);
    break;
  case ENV_VAR:
    printf("↳ ENV_VAR: %s=%s\n", tree->node->envVar.key,
           tree->node->envVar.value);
    break;
  case FILE_PATH:
    printf("↳ FILE_PATH: %s=%s\n", tree->node->filePath,
           tree->node->envVar.value);
    break;
  default:
    printf("↳ DEFAULT\n");
  }
  printTree(tree->leftSon, depth + 1);
  printTree(tree->rightSon, depth + 1);
}
int main() {
  // Simulación de tokens tipo shell
  char *tokens[] = {"grep", "hola", "<", "todo.txt", NULL};

  // Crear árbol
  Tree *root = createTree(tokens);

  // Función para imprimir el árbol

  // Mostrar árbol

  // Liberar memoria
  processTree(root);
  freeTree(root);
  return 0;
}
#endif
