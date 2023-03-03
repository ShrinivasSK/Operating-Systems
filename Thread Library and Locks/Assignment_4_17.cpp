#include <bits/stdc++.h>
#include <pthread.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

#include <chrono>
#include <iostream>
// shared Memory
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/types.h>

using std::cin;
using std::cout;

#define MAX_CHILDREN 10
#define MAX_NODES 1600
#define MAX_PRODUCER_THREADS 10

int min(int a, int b)
{
    if (a < b)
        return a;
    return b;
}
pthread_mutex_t mprint;
pthread_mutexattr_t matrp;

/**
 * @brief A Node describing one job.
 * Status: (0: Not Started, 1: On Going, 2: Done)
 */
typedef struct _node
{
    int jobId, timeForCompletion, status, numDependentJobs, parent;
    int numChildJobs;
    pthread_mutex_t mlock;
    pthread_mutexattr_t matr;
    int dependentJobs[MAX_CHILDREN];
} Node;

/**
 * @brief Shared Memory Structure
 *
 */
typedef struct _sharedMemory
{
    Node tree[MAX_NODES];
    int numNodes, jobsLeft;
    pthread_mutexattr_t matr;
    pthread_mutex_t numNodesLock, jobs;
} SharedMemory;

SharedMemory *shm;

/**
 * @brief Initialise Job Node
 *
 * @param a: Node modified by reference
 */
void initialiseNode(Node &a)
{
    a.jobId = rand() % 100000000 + 1;
    a.numDependentJobs = 0;
    a.numChildJobs = 0;
    a.status = 0;
    a.timeForCompletion = rand() % 250 + 1;
    pthread_mutexattr_init(&(a.matr));
    pthread_mutexattr_setpshared(&(a.matr), PTHREAD_PROCESS_SHARED);
    pthread_mutex_init(&(a.mlock), &(a.matr));
}

/**
 * @brief Initialise Mutex Locks
 *
 */
void initialiseLocks()
{
    pthread_mutexattr_init(&(shm->matr));
    pthread_mutexattr_setpshared(&(shm->matr), PTHREAD_PROCESS_SHARED);
    pthread_mutex_init(&(shm->numNodesLock), &(shm->matr));
    pthread_mutex_init(&(shm->jobs), &(shm->matr));
}

/**
 * @brief Build Tree with the Jobs
 *
 */
void initialiseTree()
{
    shm->numNodes = rand() % 201 + 300;
    shm->jobsLeft = shm->numNodes;
    int nodesLeft = shm->numNodes - 1; // one root
    int childIndex = 1;

    for (int i = 0; i < shm->numNodes; i++)
    {
        initialiseNode(shm->tree[i]);
        int numChildren = (rand() % MAX_CHILDREN) + 1;
        numChildren = min(numChildren, nodesLeft);
        nodesLeft -= numChildren;
        shm->tree[i].numDependentJobs = numChildren;
        shm->tree[i].numChildJobs = numChildren;
        for (int j = 1; j <= numChildren; j++)
        {
            shm->tree[childIndex].parent = i;
            shm->tree[i].dependentJobs[j - 1] = childIndex++;
        }
    }
}

/**
 * @brief Producer Function that runs on a producer thread
 *
 * @param arg : ID of the Producer
 * @return void* : Required due to Thread Syntax
 */
void *producer(void *arg)
{
    int id = *((int *)arg);
    int timeLeft = rand() % 10001 + 10000; // in milliseconds
    while (timeLeft > 0)
    {
        int jobProd = -1;
        int randomJob = -1;
        while (1)
        {
            if (shm->tree[0].status == 2)
            {
                randomJob = -1;
                break;
            }
            // Look for Job that has not been started and can have more
            // children nodes
            randomJob = rand() % shm->numNodes;
            pthread_mutex_lock(&shm->tree[randomJob].mlock);
            int flag = 0;
            if (shm->tree[randomJob].status == 0 &&
                shm->tree[randomJob].numChildJobs < MAX_CHILDREN)
            {
                pthread_mutex_lock(&shm->numNodesLock);
                shm->tree[randomJob].dependentJobs[shm->tree[randomJob].numChildJobs] = shm->numNodes;
                shm->tree[shm->numNodes].parent = randomJob;
                jobProd = shm->numNodes;
                shm->numNodes++;
                pthread_mutex_unlock(&shm->numNodesLock);
                shm->tree[randomJob].numChildJobs++;
                shm->tree[randomJob].numDependentJobs++;
                flag = 1;
            }
            pthread_mutex_unlock(&shm->tree[randomJob].mlock);
            if (flag == 1)
            {
                break;
            }
        }
        if (randomJob != -1)
        {
            initialiseNode(shm->tree[jobProd]);
            // Increase Jobs Left
            // Do this inside Mutex lock so that consumer and
            // producer do not modify it together
            pthread_mutex_lock(&shm->jobs);
            shm->jobsLeft++;
            pthread_mutex_unlock(&shm->jobs);

            pthread_mutex_lock(&mprint);
            cout << "Producer " << id
                 << ": Process Added: " << shm->tree[jobProd].jobId
                 << " to parent: " << shm->tree[randomJob].jobId << "\n";
            pthread_mutex_unlock(&mprint);
            int waitTime = rand() % 301 + 200; // milliseconds
            usleep(waitTime * 1000);           // waits for microseconds
            timeLeft -= waitTime;
        }
        else
        {
            break;
        }
    }
}

/**
 * @brief DFS to find the Independent Node
 *
 * @param curr : Curr Node while running DFS
 * @return int : The independent node, -1 if not found yet
 */
int dfs(int curr)
{
    int flag = 0;
    pthread_mutex_lock(&(shm->tree[curr].mlock));
    if (shm->tree[curr].numDependentJobs == 0 && shm->tree[curr].status == 0)
    {
        shm->tree[curr].status = 1;
        flag = 1;
    }
    pthread_mutex_unlock(&(shm->tree[curr].mlock));

    if (flag == 1)
    {
        return curr;
    }

    int ans = -1;
    // Iterate over all children
    for (int i = 0; i < MAX_CHILDREN; i++)
    {
        int val = shm->tree[curr].dependentJobs[i];
        if (val == 0)
        { // uninitialised, so not children
            continue;
        }
        if (shm->tree[val].status == 0)
        { // Not yet started
            ans = dfs(shm->tree[curr].dependentJobs[i]);
        }

        if (ans != -1)
        {
            return ans;
        }
    }
    return -1;
}

/**
 * @brief Consumer Function that runs on a Consumer thread
 *
 * @param arg : ID of the consumer
 * @return void* : Required due to Thread Syntax
 */
void *consumer(void *arg)
{
    int id = *((int *)arg);
    while (1)
    {
        if (shm->jobsLeft <= 0)
        {
            break;
        }
        // Find Independent Job by a DFS search
        int independentJob = dfs(0);

        // If no job is found then it will return -1.
        // In this case, sleep for some time and try again
        // As more processes may get complete and new nodes
        // might become independent
        if (independentJob == -1)
        {
            int waitTime = rand() % 51 + 100;
            usleep(waitTime * 1000);
            continue;
        }
        pthread_mutex_lock(&mprint);
        cout << "Consumer " << id
             << ": Job Started: " << shm->tree[independentJob].jobId << "\n";
        pthread_mutex_unlock(&mprint);

        // Stimulating the consumer Job
        usleep(shm->tree[independentJob].timeForCompletion * 1000);

        // Decrease Jobs Left
        // Do this inside Mutex lock so that consumer and
        // producer do not modify it together
        pthread_mutex_lock(&shm->jobs);
        shm->jobsLeft--;
        pthread_mutex_unlock(&shm->jobs);

        // Update status of Job and decrease number of
        // dependent jobs of parent.
        pthread_mutex_lock(&(shm->tree[independentJob].mlock));
        shm->tree[independentJob].status = 2;
        if (shm->tree[independentJob].parent != -1)
        {
            shm->tree[shm->tree[independentJob].parent].numDependentJobs--;
        }
        pthread_mutex_unlock(&(shm->tree[independentJob].mlock));

        pthread_mutex_lock(&mprint);
        cout << "Consumer " << id
             << ": Process Completed: " << shm->tree[independentJob].jobId << "\n";
        pthread_mutex_unlock(&mprint);
    }
}

int main()
{
    pthread_mutexattr_init(&(matrp));
    pthread_mutexattr_setpshared(&(matrp), PTHREAD_PROCESS_SHARED);
    pthread_mutex_init(&(mprint), &(matrp));

    srand(time(NULL));
    // Create Shared Memory
    int shmid;
    if ((shmid = shmget(IPC_PRIVATE, sizeof(SharedMemory), 0666 | IPC_CREAT)) <
        0)
    {
        perror("Shmget failed!");
        exit(1);
    }

    shm = (SharedMemory *)shmat(shmid, NULL, 0);

    if (shm == (SharedMemory *)-1)
    {
        perror("Shmat Failed");
        exit(1);
    }

    initialiseLocks();

    int p, y;

    cout << "Enter number of Producer Threads: ";
    cin >> p;

    cout << "Enter number of Worker Threads: ";
    cin >> y;

    initialiseTree();

    cout << "Number of Initial Nodes: " << shm->numNodes << "\n";

    pthread_t ptids_prod[p];
    pthread_attr_t attr;

    pthread_attr_init(&attr);

    // Create Producer Threads
    for (int i = 0; i < p; i++)
    {
        int *arg = (int *)malloc(sizeof(*arg));
        *arg = i;
        pthread_create(&ptids_prod[i], &attr, producer, arg);
    }

    int pid = fork();

    if (pid == 0)
    { // Process B
        // Attach Shared Memory to this process
        shm = (SharedMemory *)shmat(shmid, NULL, 0);
        if (shm == (SharedMemory *)-1)
        {
            perror("Shmat Failed");
            exit(1);
        }
        pthread_t ptids_cons[y];

        // Create Consumer Threads
        for (int i = 0; i < y; i++)
        {
            int *arg = (int *)malloc(sizeof(*arg));
            *arg = i;
            pthread_create(&ptids_cons[i], &attr, consumer, arg);
        }
        // Wait For completion of work of consumer threads
        for (int i = 0; i < p; i++)
        {
            pthread_join(ptids_cons[i], NULL);
        }
        exit(0);
    }
    // Wait for completion of work of Producer Threads
    for (int i = 0; i < p; i++)
    {
        pthread_join(ptids_prod[i], NULL);
    }

    // Wait for Process B
    wait(NULL);

    cout << "\nAll Jobs Completed Successfully!\n";

    return 0;
}