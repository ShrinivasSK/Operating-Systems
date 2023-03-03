#include <bits/stdc++.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

#include <chrono>
#include <iostream>
// shared Memory
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/types.h>

#define QUEUE_SIZE 8
#define NUM_JOBS_PER_MATRIX 8
#define MATRIX_SIZE 100

using std::cin;
using std::cout;

/**
 * @brief Class for a Job that is to be performed by the worker
 *
 * Producer Number: is the number of the producer that made
 * this job
 *
 * Matrix Id: Is a unique id of the matrix
 *
 * Status: Array that maintains the status of the 8 jobs
 * for each matrix
 *
 * Matrix: The matrix that needs to be multiplied
 */
class Job {
 public:
  int producerNumber, matrixId;
  bool isProducedByWorker;
  int status[NUM_JOBS_PER_MATRIX];
  int matrix[MATRIX_SIZE][MATRIX_SIZE];
  int outputIndex;

  Job(int prodNum, bool isProdByWorker = false) {
    this->outputIndex = -1;
    this->matrixId = rand() % 100000 + 1;
    this->producerNumber = prodNum;
    this->isProducedByWorker = isProdByWorker;

    for (int i = 0; i < NUM_JOBS_PER_MATRIX; i++) {
      this->status[i] = 0;
    }

    for (int i = 0; i < MATRIX_SIZE; i++) {
      for (int j = 0; j < MATRIX_SIZE; j++) {
        this->matrix[i][j] = rand() % 19 - 9;
      }
    }
  }

  // default constructor
  Job() {}
};

/**
 * @brief Implementation of a Queue using a fixed size array
 *  Yeh code kar dena Rupinder
 */
class Queue {
 public:
  Job jobs[QUEUE_SIZE];
  int size, in, out, maxSize;
  Queue() {
    this->size = 0;
    this->in = 0;
    this->out = 0;
    this->maxSize = QUEUE_SIZE;
  }

  int enqueue(Job j) {
    if (this->isFull()) {
      return -1;
    }
    int ind = in;
    this->jobs[in] = j;
    this->in = (this->in + 1) % this->maxSize;
    this->size = this->size + 1;
    return ind;
  }
  Job *top() {
    if (this->isEmpty()) {
      return NULL;
    }
    return &this->jobs[out];
  }
  Job *secondTop() {
    if (this->size < 2) {
      return NULL;
    }
    int ind = (this->out + 1) % this->maxSize;
    return &this->jobs[ind];
  }

  Job *end() {
    if (!this->isEmpty()) {
      int ind = (in - 1 + this->maxSize) % this->maxSize;
      return &this->jobs[ind];
    }
    return NULL;
  }
  Job *atIndex(int ind) { return &this->jobs[ind]; }

  int dequeue() {
    if (this->isEmpty()) {
      return -1;
    }
    this->out = (this->out + 1) % this->maxSize;
    this->size--;
    return 1;
  }

  bool isEmpty() { return (this->size == 0); }
  bool isFull() { return (this->size == this->maxSize); }
  int Size() { return this->size; }

  int getUnAccessedBlock() {
    if (this->size >= 2) {
      Job *j1 = this->top();
      Job *j2 = this->secondTop();
      for (int i = 0; i < NUM_JOBS_PER_MATRIX; i++) {
        if (j1->status[i] == 0) {
          return i;
        }
      }
    }
    return -1;
  }
};

/**
 * @brief Structure that is part of the Shared Memory
 *
 * Queue maintains queue of jobs to be done
 *
 * job_created is a counter to count number of jobs created
 *
 * isAccessed is a flag to check whether all the blocks
 * of the matrices have been processed or not
 *      0: not started
 *      1: Going on
 *      2: Done
 */
typedef struct _shared_memory {
  Queue q;
  int job_created;
  int isAccessed;
  pthread_mutex_t mlockProducer, mlockWorker1, mlockWorker2, mlockCinsert;
  pthread_mutexattr_t matr;
} SharedMemory;

void initialiseSharedMemory(SharedMemory *shm) {
  shm->job_created = 0;
  shm->isAccessed = 0;
  shm->q = Queue();
  pthread_mutexattr_init(&(shm->matr));
  pthread_mutexattr_setpshared(&(shm->matr), PTHREAD_PROCESS_SHARED);
  pthread_mutex_init(&(shm->mlockProducer), &(shm->matr));
  pthread_mutex_init(&(shm->mlockWorker1), &(shm->matr));
  pthread_mutex_init(&(shm->mlockWorker2), &(shm->matr));
  pthread_mutex_init(&(shm->mlockCinsert), &(shm->matr));
}

SharedMemory *shm;

int main() {
  int np, nw, nm;

  cout << "Enter number of producers: ";
  cin >> np;

  cout << "Enter number of workers: ";
  cin >> nw;

  cout << "Enter number of matrixes: ";
  cin >> nm;

  int shmid;

  std::map<int, std::pair<std::pair<int, int>, std::pair<int, int>>>
      indexToBlocks = {
          {0, {{0, 0}, {0, 0}}},
          {1, {{0, MATRIX_SIZE / 2}, {MATRIX_SIZE / 2, 0}}},
          {2, {{0, 0}, {0, MATRIX_SIZE / 2}}},
          {3, {{0, MATRIX_SIZE / 2}, {MATRIX_SIZE / 2, MATRIX_SIZE / 2}}},
          {4, {{MATRIX_SIZE / 2, 0}, {0, 0}}},
          {5, {{MATRIX_SIZE / 2, MATRIX_SIZE / 2}, {MATRIX_SIZE / 2, 0}}},
          {6, {{MATRIX_SIZE / 2, 0}, {0, MATRIX_SIZE / 2}}},
          {7,
           {{MATRIX_SIZE / 2, MATRIX_SIZE / 2},
            {MATRIX_SIZE / 2, MATRIX_SIZE / 2}}},

      };

  std::map<int, std::pair<int, int>> indexToBlockInC = {
      {0, {0, 0}},
      {1, {0, 0}},
      {2, {0, MATRIX_SIZE / 2}},
      {3, {0, MATRIX_SIZE / 2}},
      {4, {MATRIX_SIZE / 2, 0}},
      {5, {MATRIX_SIZE / 2, 0}},
      {6, {MATRIX_SIZE / 2, MATRIX_SIZE / 2}},
      {7, {MATRIX_SIZE / 2, MATRIX_SIZE / 2}},
  };

  if ((shmid = shmget(IPC_PRIVATE, sizeof(SharedMemory), 0666 | IPC_CREAT)) <
      0) {
    perror("Shmget failed!");
    exit(1);
  }

  shm = (SharedMemory *)shmat(shmid, NULL, 0);

  if (shm == (SharedMemory *)-1) {
    perror("Shmat Failed");
    exit(1);
  }

  initialiseSharedMemory(shm);

  printf("\nJobs Started..\n\n");

  // producer code
  for (int i = 0; i < np; i++) {
    int pid = fork();
    if (pid < 0) {
      perror("Could not create process");
      exit(1);
    }
    if (pid == 0) {
      srand(time(0) + i + 1);
      shm = (SharedMemory *)shmat(shmid, NULL, 0);
      if (shm == (SharedMemory *)-1) {
        perror("Shmat Failed");
        exit(1);
      }
      while (true) {
        int wait_time = rand() % 4;
        printf("Producer %d waiting for %d seconds\n", i + 1, wait_time);
        sleep(wait_time);
        Job j(i + 1);
        pthread_mutex_lock(&(shm->mlockProducer));
        if ((shm->q.size < shm->q.maxSize - 1) && (shm->job_created < nm)) {
          shm->q.enqueue(j);
          shm->job_created++;

          printf(
              "Job Created. Producer Number: %d, Pid no: %d, Matrix id: %d\n",
              i + 1, getpid(), j.matrixId);
        }
        pthread_mutex_unlock(&(shm->mlockProducer));
        if (shm->job_created == nm) break;
      }
      shmdt(shm);
      shmctl(shmid, IPC_RMID, 0);

      return 0;
    }
  }

  // worker code
  for (int i = 0; i < nw; i++) {
    int pid = fork();

    if (pid < 0) {
      perror("Could not create process");
      exit(1);
    } else if (pid == 0) {
      srand(time(0) + nw + i + 1);

      shm = (SharedMemory *)shmat(shmid, NULL, 0);

      if (shm == (SharedMemory *)-1) {
        perror("Shmat Failed");
        exit(1);
      }

      while (true) {
        int wait_time = rand() % 4;
        printf("Worker %d waiting for %d seconds\n", i + 1, wait_time);

        int blockIndex;
        sleep(wait_time);
        pthread_mutex_lock(&(shm->mlockWorker1));
        blockIndex = shm->q.getUnAccessedBlock();
        if (blockIndex != -1) {
          Job *j1 = shm->q.top();
          Job *j2 = shm->q.secondTop();
          j1->status[blockIndex] = 1;
          j2->status[blockIndex] = 1;
        }
        pthread_mutex_unlock(&(shm->mlockWorker1));
        if (blockIndex != -1) {
          Job *j1 = shm->q.top();
          Job *j2 = shm->q.secondTop();
          printf("Job started by worker%d:  Matrix ID: %d Block Number: %d\n",
                 i + 1, j1->matrixId, blockIndex);

          // get blocks
          auto blocks = indexToBlocks[blockIndex];
          int M1[MATRIX_SIZE / 2][MATRIX_SIZE / 2];
          int M2[MATRIX_SIZE / 2][MATRIX_SIZE / 2];
          int OP[MATRIX_SIZE / 2][MATRIX_SIZE / 2];

          for (int _i = 0; _i < MATRIX_SIZE / 2; _i++) {
            for (int _j = 0; _j < MATRIX_SIZE / 2; _j++) {
              M1[_i][_j] =
                  j1->matrix[_i + blocks.first.first][_j + blocks.first.second];
            }
          }

          for (int _i = 0; _i < MATRIX_SIZE / 2; _i++) {
            for (int _j = 0; _j < MATRIX_SIZE / 2; _j++) {
              M2[_i][_j] = j2->matrix[_i + blocks.second.first]
                                     [_j + blocks.second.second];
            }
          }

          // perform multiplication
          for (int _i = 0; _i < MATRIX_SIZE / 2; _i++) {
            for (int _j = 0; _j < MATRIX_SIZE / 2; _j++) {
              OP[_i][_j] = 0;
              for (int _k = 0; _k < MATRIX_SIZE / 2; _k++) {
                OP[_i][_j] += M1[_i][_k] * M2[_k][_j];
              }
            }
          }

          // update C
          if (blockIndex == 0) {
            Job j = Job(i + 1, false);
            for (int _i = 0; _i < MATRIX_SIZE; _i++) {
              for (int _j = 0; _j < MATRIX_SIZE; _j++) {
                if (_i >= MATRIX_SIZE / 2 || _j >= MATRIX_SIZE / 2) {
                  j.matrix[_i][_j] = 0;
                } else
                  j.matrix[_i][_j] = OP[_i][_j];
              }
            }

            pthread_mutex_lock(&(shm->mlockCinsert));
            j1->outputIndex = shm->q.enqueue(j);
            j1->status[0] = 2;
            j2->outputIndex = j1->outputIndex;
            pthread_mutex_unlock(&(shm->mlockCinsert));

            printf("Job done by worker%d:  Matrix ID: %d Block Number: %d\n",
                   i + 1, j1->matrixId, blockIndex);
          } else {
            while (j1->outputIndex == -1) continue;

            pthread_mutex_lock(&(shm->mlockWorker2));

            Job *j = shm->q.atIndex(j1->outputIndex);
            auto blockInC = indexToBlockInC[blockIndex];

            for (int _i = 0; _i < MATRIX_SIZE / 2; _i++) {
              for (int _j = 0; _j < MATRIX_SIZE / 2; _j++) {
                j->matrix[_i + blockInC.first][_j + blockInC.second] +=
                    OP[_i][_j];
              }
            }

            j1->status[blockIndex] = 2;
            j2->status[blockIndex] = 2;

            int _i;

            for (_i = 0; _i < 8; _i++) {
              if (j1->status[_i] != 2) break;
            }

            if (_i == 8) {
              shm->q.dequeue();
              shm->q.dequeue();
              printf("Job Done. 2 Matrix removed. Size of the Queue: %d\n",
                     shm->q.size);
            }

            pthread_mutex_unlock(&(shm->mlockWorker2));
          }
        }

        if (shm->q.size == 1 && shm->job_created == nm) {
          break;
        }
      }
      shmdt(shm);
      shmctl(shmid, IPC_RMID, 0);

      exit(0);
    }
  }

  auto start = std::chrono::high_resolution_clock::now();

  for (int i = 0; i < nw + np; i++) wait(NULL);

  auto stop = std::chrono::high_resolution_clock::now();
  auto duration =
      std::chrono::duration_cast<std::chrono::milliseconds>(stop - start);

  cout << "\nAll Jobs Finished\nTime Taken (miliseconds): " << duration.count()
       << "\n";
  Job *k = shm->q.top();
  long long sum = 0;
  for (int i = 0; i < MATRIX_SIZE; i++) {
    sum += 0LL + k->matrix[i][i];
  }
  printf("\nSum of Diagonal elements: %lld\n", sum);
  shmdt(shm);
  shmctl(shmid, IPC_RMID, 0);

  return 0;
}