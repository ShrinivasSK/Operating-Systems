#include <stdio.h>
#include <stdlib.h>
// shared Memory
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/types.h>
// wait pid
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#define MAX_SIZE 100

typedef struct _shared_memory {
  double A[MAX_SIZE][MAX_SIZE], B[MAX_SIZE][MAX_SIZE], C[MAX_SIZE][MAX_SIZE];
  int r1, c1, r2, c2;
} SharedMemory;

typedef struct _process_data {
  double **A, **B, **C;
  int veclen, i, j;
} ProcessData;

SharedMemory *shm;

void *mult(void *p) {
  ProcessData *pd = (ProcessData *)p;

  if (pd == NULL) {
    perror("Invalid Process Data.");
    exit(1);
  }
  pd->C[pd->i][pd->j] = 0;

  for (int i = 0; i < pd->veclen; i++) {
    pd->C[pd->i][pd->j] +=
        pd->A[pd->i][i] * pd->B[i][pd->j];
  }
}

void printMatrix(double d[MAX_SIZE][MAX_SIZE], int r, int c) {
  for (int i = 0; i < r; i++) {
    for (int j = 0; j < c; j++) {
      printf("%lf ", d[i][j]);
    }
    printf("\n");
  }
}

int main() {
  int shmid;

  if ((shmid = shmget(IPC_PRIVATE, sizeof(SharedMemory), 0666 | IPC_CREAT)) < 0) {
    perror("Shmget failed!");
    exit(1);
  }

  shm = (SharedMemory *)shmat(shmid, NULL, 0);

  if (shm == (SharedMemory *)-1) {
    perror("Shmat Failed");
    exit(1);
  }

  printf("Enter the dimensions of the matrices: (R1, C1, R2, C2)\n");
  scanf("%d %d %d %d", &shm->r1, &shm->c1, &shm->r2, &shm->c2);

  if (shm->r1 > MAX_SIZE || shm->c1 > MAX_SIZE || shm->r2 > MAX_SIZE ||
      shm->c2 > MAX_SIZE || shm->c1 != shm->r2) {
    perror("Invalid Dimensions");
    exit(1);
  }

  printf("Enter values in Matrix A (%d x %d). (Row Wise)\n", shm->r1, shm->c1);

  for (int i = 0; i < shm->r1; i++) {
    for (int j = 0; j < shm->c1; j++) {
      scanf("%lf", &shm->A[i][j]);
    }
  }

  printf("Enter values in Matrix B (%d x %d). (Row Wise)\n", shm->r2, shm->c2);

  for (int i = 0; i < shm->r2; i++) {
    for (int j = 0; j < shm->c2; j++) {
      scanf("%lf", &shm->B[i][j]);
    }
  }

  printf("\nCalculating Product...\n\n");

  int pids[MAX_SIZE][MAX_SIZE];

  for (int i = 0; i < shm->r1; i++) {
    for (int j = 0; j < shm->c2; j++) {
      pids[i][j] = fork();

      if (pids[i][j] < 0) {
        perror("Could not fork.");
        exit(1);
      } else if (pids[i][j] == 0) {  // child process

        // SharedMemory

        shm = (SharedMemory *)shmat(shmid, NULL, 0);

        if (shm == (SharedMemory *)-1) {
          perror("Shmat Failed");
          exit(1);
        }

        ProcessData *p = (ProcessData *)malloc(sizeof(ProcessData));

        p->A = (double **)malloc(MAX_SIZE*sizeof(double *));
        for(int i=0;i<MAX_SIZE;i++){
          p->A[i]=shm->A[i];
        }

        p->B = (double **)malloc(MAX_SIZE*sizeof(double *));
        for(int i=0;i<MAX_SIZE;i++){
          p->B[i]=shm->B[i];
        }

        p->C = (double **)malloc(MAX_SIZE*sizeof(double *));
        for(int i=0;i<MAX_SIZE;i++){
          p->C[i]=shm->C[i];
        }
        p->i = i;
        p->j = j;
        p->veclen = shm->c1;

        mult(p);
        exit(0);
      } else {
        printf("Creating Process PID: %d, for cell (%d,%d)\n", pids[i][j], i,
               j);
      }
    }
  }

  int status[MAX_SIZE][MAX_SIZE];

  do {
    for (int i = 0; i < shm->r1; i++) {
      for (int j = 0; j < shm->c2; j++) {
        int wpid = waitpid(pids[i][j], &status[i][j], WUNTRACED);
      }
    }
    int done = 1;
    for (int i = 0; i < shm->r1; i++) {
      for (int j = 0; j < shm->c2; j++) {
        if (!(WIFEXITED(status[i][j]) || WIFSIGNALED(status[i][j]))) {
          done = 0;
        } else {
          printf("Process PID %d completed for cell (%d, %d)\n", pids[i][j], i,
                 j);
        }
      }
    }
    if (done) {
      break;
    }
  } while (1);

  printf("\nProcess Completed.\n\nOutput Matrix C (%d, %d): \n", shm->r1,
         shm->c2);
  printMatrix(shm->C, shm->r1, shm->c2);

  shmdt(shm);
  shmctl(shmid,IPC_RMID,0);

  return 0;
}