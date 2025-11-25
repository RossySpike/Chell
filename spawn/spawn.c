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
  case REDIRECTION_ERROR_OUTPUT:
    return open(filePath, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    break;
  case REDIRECTION_ALL_OUTPUT:
    return open(filePath, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    break;
  case REDIRECTION_STDERR_TO_STDOUT:
  case REDIRECTION_STDOUT_TO_STDERR:
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
  for (int i = 0; args && args[i] != NULL; i++) {
    printf("  - %s\n", args[i]);
  }
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
      if (outputfd != STDOUT_FILENO) {

        dup2(outputfd, STDOUT_FILENO);
        if (outputfd > 2)
          close(outputfd);
      }
      break;
    case REDIRECTION_ERROR_OUTPUT:
      if (outputfd != STDERR_FILENO) {

        dup2(outputfd, STDERR_FILENO);

        if (outputfd > 2)
          close(outputfd);
      }
      break;
    case REDIRECTION_ALL_OUTPUT:
      if (outputfd != STDERR_FILENO) {

        dup2(outputfd, STDERR_FILENO);
      }
      if (outputfd != STDOUT_FILENO) {

        dup2(outputfd, STDOUT_FILENO);
      }
      if (outputfd > 2)
        close(outputfd);
      break;
    case REDIRECTION_STDERR_TO_STDOUT:

      dup2(STDOUT_FILENO, STDERR_FILENO);
      break;
    case REDIRECTION_STDOUT_TO_STDERR:

      dup2(STDERR_FILENO, STDOUT_FILENO);
      break;
    case PIPE:
      if (inputfd != STDIN_FILENO)
        dup2(inputfd, STDIN_FILENO);
      if (outputfd != STDOUT_FILENO)
        dup2(outputfd, STDOUT_FILENO);
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

typedef struct PipeResult {
  int inputfd; // lectura del último pipe
} PipeResult;

PipeResult preparePipeline(Stack **stack) {
  PipeResult result = {STDIN_FILENO};
  int fds[2];
  int inputfd = STDIN_FILENO;

  while (*stack && (*stack)->currentTree && (*stack)->currentTree->node &&
         (*stack)->currentTree->node->type == EXEC_CONTROL &&
         (*stack)->currentTree->node->execControl.opType == PIPE) {

    Tree *pipeTree = (*stack)->currentTree;
    Node *leftNode = pipeTree->leftSon ? pipeTree->leftSon->node : NULL;
    Node *rightNode = pipeTree->rightSon ? pipeTree->rightSon->node : NULL;

    // crear pipe
    pipe(fds);
    int outputfd = fds[1];

    // lanzar hijo izquierdo
    if (leftNode && leftNode->type == PROCESS) {
      spawn(leftNode->command.argv, outputfd, inputfd, 0, PIPE);
    }

    // cerrar extremos en el padre
    if (outputfd > 2)
      close(outputfd);
    if (inputfd > 2)
      close(inputfd);

    // preparar input para el siguiente
    inputfd = fds[0];

    // avanzar en el stack
    unStack(stack);

    if (*stack && (*stack)->currentTree && (*stack)->currentTree->node &&
        (*stack)->prev && (*stack)->prev->currentTree &&
        (*stack)->prev->currentTree->node &&
        (*stack)->prev->currentTree->node->type == EXEC_CONTROL &&
        (*stack)->prev->currentTree->node->execControl.opType == PIPE) {

      // llegamos al último pipe: no ejecutar el proceso derecho
      result.inputfd = inputfd;
      break;
      // si el rightNode es otro PIPE, seguimos
    } else if (rightNode && rightNode->type == EXEC_CONTROL &&
               rightNode->execControl.opType == PIPE) {
      addToStack(stack, pipeTree->rightSon);
    } else {

      // llegamos al último pipe: no ejecutar el proceso derecho
      result.inputfd = inputfd;
      break;
    }
  }

  return result;
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
  while (stack && stack->currentTree) {

    PipeResult res;
    int pipeline = 0;
    currentTree = stack->currentTree;
    Node *currentNode = currentTree ? currentTree->node : NULL;
    if (currentNode && currentNode->type && currentNode->type == EXEC_CONTROL &&
        currentNode->execControl.opType == PIPE) {

      res = preparePipeline(&stack);
      inputfdLeft = res.inputfd;
      inputfdRight = res.inputfd;

      currentTree = stack->currentTree;

      currentNode = currentTree ? currentTree->node : NULL;
      pipeline = 1;
    }
    Node *leftNode =
        currentTree ? currentTree->leftSon ? currentTree->leftSon->node : NULL
                    : NULL;

    /* printNode(currentTree); */
    Node *rightNode =
        currentTree ? currentTree->rightSon ? currentTree->rightSon->node : NULL
                    : NULL;

    int leftProcessStatus = 0;
    int rightCanStart = 1;
    int leftCanStart = 1;
    currentTree = stack->prev ? stack->prev->currentTree : NULL;
    unStack(&stack);

    if (leftNode && leftNode->type == PROCESS && leftCanStart) {

      uint op = -1;

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
        break;
      case REDIRECTION_OUTPUT:
        outputfdLeft = open(rightNode ? rightNode->filePath : NULL,
                            O_WRONLY | O_CREAT | O_TRUNC, 0644);
        break;
      case REDIRECTION_APPEND:
        outputfdLeft = open(rightNode ? rightNode->filePath : NULL,
                            O_WRONLY | O_CREAT | O_APPEND, 0644);
        break;
      case REDIRECTION_ERROR_OUTPUT:
        outputfdLeft = open(rightNode ? rightNode->filePath : NULL,
                            O_WRONLY | O_CREAT | O_TRUNC, 0644);
        break;
      case REDIRECTION_ALL_OUTPUT:
        outputfdLeft = open(rightNode ? rightNode->filePath : NULL,
                            O_WRONLY | O_CREAT | O_TRUNC, 0644);
        break;
      case PIPE:
        if (!stack || !stack->currentTree || !stack->currentTree->node ||
            stack->currentTree->node->type != EXEC_CONTROL) {

          break;
        }
        switch (stack->currentTree->node->execControl.opType) {

        case REDIRECTION_OUTPUT:
          outputfdLeft = open(rightNode ? rightNode->filePath : NULL,
                              O_WRONLY | O_CREAT | O_TRUNC, 0644);
          break;
        case REDIRECTION_APPEND:
          outputfdLeft = open(rightNode ? rightNode->filePath : NULL,
                              O_WRONLY | O_CREAT | O_APPEND, 0644);
          break;
        case REDIRECTION_ERROR_OUTPUT:
          outputfdLeft = open(rightNode ? rightNode->filePath : NULL,
                              O_WRONLY | O_CREAT | O_TRUNC, 0644);
          break;
        case REDIRECTION_ALL_OUTPUT:
          outputfdLeft = open(rightNode ? rightNode->filePath : NULL,
                              O_WRONLY | O_CREAT | O_TRUNC, 0644);
        default:
          break;
        }
        break;
      case REDIRECTION_STDERR_TO_STDOUT:
      case REDIRECTION_STDOUT_TO_STDERR:
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
      uint opRight = upperNode ? upperNode->execControl.opType : -1;
      int shouldWaitRight = opRight == BACKGROUND ? 0 : 1;
      Node *upperRight =
          currentTree
              ? currentTree->rightSon ? currentTree->rightSon->node : NULL
              : NULL;

      switch (opRight) {

        /* case REDIRECTION_INPUT: */
        /*   break; */
      case REDIRECTION_INPUT:
        inputfdRight = open(upperRight ? upperRight->filePath : NULL, O_RDONLY);
        if (stack && stack->prev && stack->prev->currentTree &&
            stack->prev->currentTree->node &&
            stack->prev->currentTree->node->type == EXEC_CONTROL &&
            REDIRECTION_INPUT <
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
        }
        break;
      case REDIRECTION_OUTPUT:
        outputfdRight = open(upperRight ? upperRight->filePath : NULL,
                             O_WRONLY | O_CREAT | O_TRUNC, 0644);
        break;
      case REDIRECTION_APPEND:
        outputfdRight = open(upperRight ? upperRight->filePath : NULL,
                             O_WRONLY | O_CREAT, 0644);
        break;
      case PIPE:
        if (!stack || !stack->currentTree || !stack->currentTree->node ||
            stack->currentTree->node->type != EXEC_CONTROL) {

          break;
        }
        switch (stack->currentTree->node->execControl.opType) {

        case REDIRECTION_OUTPUT:
          outputfdRight = open(upperRight ? upperRight->filePath : NULL,
                               O_WRONLY | O_CREAT | O_TRUNC, 0644);
          break;
        case REDIRECTION_APPEND:
          outputfdRight = open(upperRight ? upperRight->filePath : NULL,
                               O_WRONLY | O_CREAT, 0644);
          break;
        default:
          break;
        }
        break;
      case REDIRECTION_ERROR_OUTPUT:
        break;
      case REDIRECTION_ALL_OUTPUT:
        break;
      case REDIRECTION_STDERR_TO_STDOUT:
        break;
      case REDIRECTION_STDOUT_TO_STDERR:
        break;
      default:
        outputfdRight = STDOUT_FILENO;
        break;
      }
      if (rightNode->type == PROCESS) {

        printf("inputfdRight: %d\n", inputfdRight);
        retValue = spawn(rightNode->command.argv, outputfdRight, inputfdRight,
                         shouldWaitRight, opRight);

        if (opRight == AND) {
          leftCanStart = leftProcessStatus == 0 ? 1 : 0;
        } else if (opRight == XOR) {
          leftCanStart = leftProcessStatus != 0 ? 1 : 0;
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
    pipeline = 0;
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
