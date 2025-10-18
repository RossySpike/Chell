#include "../statement/statement.c"
#include <error.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

int spawn(char **args, int outputfd, int shouldWait) {
  printf("Spawning process: %s\n", args[0]);
  printf("With args:\n");
  for (int i = 0; args && args[i] != NULL; i++) {
    printf("  - %s\n", args[i]);
  }
  pid_t pid = fork();
  if (pid == 0) { // Child
    if (outputfd != STDOUT_FILENO) {

      if (dup2(outputfd, STDOUT_FILENO) == -1) {

        close(outputfd);
        perror("dup2 failed");
        exit(1);
      }
      close(outputfd);
    }
    execvp(args[0], args);
    exit(0);            // execvp failed
  } else if (pid > 0) { // Parent
    int status;
    if (!shouldWait)
      return 0;
    waitpid(pid, &status, 0);
    return WIFEXITED(status) ? WEXITSTATUS(status) : -1;
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
  while (currentTree->leftSon->leftSon != NULL) {
    addToStack(&stack, currentTree);
    currentTree = currentTree->leftSon;
  }
  while (stack) {

    Node *currentNode = currentTree->node;
    Node *leftNode = currentTree->leftSon->node;

    Node *rightNode =
        currentTree->rightSon ? currentTree->rightSon->node : NULL;
    currentTree = stack->prev ? stack->prev->currentTree : NULL;
    int leftProcessStatus = 0;
    int rightCanStart = 1;
    unStack(&stack);

    if (leftNode->type == PROCESS) {

      uint op = -1;
      if (currentNode) {

        op = currentNode->type == DEFAULT
                 ? -1
                 : getOperatorType(currentNode->execControl);
      }
      int shouldWait = op == BACKGROUND ? 0 : 1;

      leftProcessStatus =
          spawn(leftNode->command.argv, STDOUT_FILENO, shouldWait);

      if (op == AND) {
        rightCanStart = leftProcessStatus == 0 ? 1 : 0;
      } else if (op == XOR) {
        rightCanStart = leftProcessStatus != 0 ? 1 : 0;
      }
    }
    if (rightNode && rightCanStart) {
      uint opRight = rightNode->type == DEFAULT
                         ? -1
                         : getOperatorType(rightNode->execControl);
      if (rightNode->type == PROCESS) {
        int shouldWaitRight = opRight == BACKGROUND ? 0 : 1;
        spawn(rightNode->command.argv, STDOUT_FILENO, shouldWaitRight);
      }
    }
  }

  if (stack)
    destroyStack(stack);
  return retValue;
}
