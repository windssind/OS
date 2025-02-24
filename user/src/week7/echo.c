#include "ulib.h"

int main(int argc, char *argv[])
{
  assert(argc > 0);
  assert(strcmp(argv[0], "echo") == 0);
  assert(argv[argc] == NULL);
  for (int i = 1; i < argc; ++i)
  {
    printf("%s%c", argv[i], i == argc - 1 ? '\n' : ' ');
  }
  int pid = fork();
  printf("fork form %s\n",pid == 0? "son" : "father");
  if (pid == 0)
  {

  }else{
    int a = 1;
    printf("%d\n",a);
  }
  printf("can end\n");
  return 0;
  // printf("echo\n");

  // int a = 1;
  // int * b = &a;
  // int pid = fork();
  // if (pid == 0){
  //   printf(" form son %p\n",b);
  // }else{
  //   printf(" form daddy %p\n",b);
  // }
}
