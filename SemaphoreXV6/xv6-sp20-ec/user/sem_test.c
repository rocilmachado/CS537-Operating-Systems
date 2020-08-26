#include "types.h"
#include "user.h"

int sem_id;

void
func(void *arg1, void *arg2)
{
  printf(1, "child\n");
  sem_post(sem_id);
  exit();
}

int main(int argc, char *argv[]){
    int arg1 = 0xABCDABCD;
    int arg2 = 0xCDEFCDEF;

    int count = 0;
    if(sem_init(&sem_id,count) < 0){
        printf(1, "main: error initializing semaphore\n");
        exit();
    }

    printf(1, "got assigned sem id %d\n", sem_id);

    int pid1 = thread_create(&func, &arg1, &arg2);
    if (pid1 < 0) {
      exit();
    }
    sem_wait(sem_id);
    printf(1, "parent: end\n");
    thread_join();
    exit();
}