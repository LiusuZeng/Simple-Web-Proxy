#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>

#define MY_FILE "./mutex_test.txt"

void* dosth(int* src);

pthread_mutex_t mutex;

int main()
{
  pthread_t tid1;
  pthread_t tid2;
  int* args1 = (int*)malloc(sizeof(int));
  int* args2 = (int*)malloc(sizeof(int));
  //
  pthread_mutex_init(&mutex, NULL);
  //
  *args1 = 1;
  pthread_create(&tid1, NULL, (void*)(dosth), (void*)(args1)); // Tom
  *args2 = 2;
  pthread_create(&tid2, NULL, (void*)(dosth), (void*)(args2)); // Jerry
  //
  pthread_join(tid1, NULL);
  pthread_join(tid2, NULL);
  //
  pthread_mutex_destroy(&mutex);
  //
  free(args1);
  free(args2);
  return EXIT_SUCCESS;
}

void* dosth(int* src)
{
  pthread_mutex_lock(&mutex);
  // Acquire lock
  FILE* fp;
  char buf[128];
  if((fp=fopen(MY_FILE, "a+")) == NULL) {
    printf("Thread %d: cannot open file!\n", *src);
  }
  else {
    char buf[100];
    sprintf(buf, "From thread %d\n", *src);
    fputs(buf, fp);
    //
    fclose(fp);
  }
  // Release lock
  pthread_mutex_unlock(&mutex);
  return;
}
