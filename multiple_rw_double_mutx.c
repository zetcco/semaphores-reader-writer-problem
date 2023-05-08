#include <sys/sem.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/wait.h>
#include <time.h>

#define WRITER_COUNT 6
#define READER_COUNT 5
#define BUFFER_LEN 10
#define READER_TEST_RUNS 12
#define WRITER_TEST_RUNS 10
#define sem_reader_mutex 0
#define sem_writer_mutex 1
#define sem_fullslot_count 2
#define sem_emptyslo_count 3

void increment(int* iter, int size);
void produce(int* buffer, int* rear, int value);
void consume(int* buffer, int* front, int i);
int get_semaphore();
void init_semaphore(int sem_id, int sem_num, int val);
void V(int sem_id, int sem_num);
void P(int sem_id, int sem_num);

int main() {
    int pid, iproc, processes[WRITER_COUNT + READER_COUNT];
    int* buffer, *front, *rear;
    int sem_id;

    clock_t begin = clock();

    if ((buffer = (int*)mmap(NULL, (BUFFER_LEN+2)*sizeof(int), PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0)) == MAP_FAILED) {
        perror("Shared memory allocation error");
        exit(1);
    }
    front = buffer + BUFFER_LEN;
    rear = front + 1;
    *front = *rear = 0;

    sem_id = get_semaphore();
    init_semaphore(sem_id, sem_reader_mutex, 1);
    init_semaphore(sem_id, sem_writer_mutex, 1);
    init_semaphore(sem_id, sem_fullslot_count, 0);
    init_semaphore(sem_id, sem_emptyslo_count, BUFFER_LEN);

    for (iproc = 0 ; iproc < READER_COUNT ; iproc++) {
        if ((pid = fork()) == -1) {
            perror("Failed to spawn reader process");
        } else if (pid == 0) {
            //printf("Reader process spawned\n");
            int i = 0;
            for (; i < READER_TEST_RUNS ; i++) {
                P(sem_id, sem_fullslot_count);
                P(sem_id, sem_reader_mutex);
                consume(buffer, front, i);
                increment(front, BUFFER_LEN);
                V(sem_id, sem_reader_mutex);
                V(sem_id, sem_emptyslo_count);
            }
            //printf("Reader process done\n");
            exit(0);
        } else {
            processes[iproc] = pid;
        }
    }
    //printf("Reader process spawning done\n");

    for (iproc = 0 ; iproc < WRITER_COUNT ; iproc++) {
        if ((pid = fork()) == -1) {
            perror("Failed to spawn writer process");
        } else if (pid == 0) {
            //printf("Writer process spawned\n");
            int i = 0;
            for (; i < WRITER_TEST_RUNS ; i++) {
                P(sem_id, sem_emptyslo_count);
                P(sem_id, sem_writer_mutex);
                produce(buffer, rear, 100*iproc+i);
                increment(rear, BUFFER_LEN);
                V(sem_id, sem_writer_mutex);
                V(sem_id, sem_fullslot_count);
            }
            //printf("Writer process done\n");
            exit(0);
        } else {
            processes[iproc + READER_COUNT] = pid;
        }
    }
    //printf("Writer process spawning done\n");

    int i = 0;
    for (; i < READER_COUNT + WRITER_COUNT ; i++)
        waitpid(processes[i], NULL, 0);
    
    clock_t end = clock();

    printf("Run time: %f\n",  (double)(end - begin) / CLOCKS_PER_SEC);

    if (munmap(buffer, (BUFFER_LEN + 2)*sizeof(int)) == -1) {
        perror("Shared memory deallocation failed");
        exit(1);
    }
}

void increment(int* iter, int size) {
    *iter = (*iter +  1) % size;
}

void produce(int* buffer, int* rear, int value) {
    *(buffer + *rear) = value;
}

void consume(int* buffer, int* front, int i) {
    //printf("Consumed buffer[%d] == %d\n", i, *(buffer + *front));
}

int get_semaphore() {
    int sem_id;

    if ((sem_id = semget(IPC_PRIVATE, 4, IPC_CREAT | 0600)) == -1) {
        perror("Getting semaphore error");
        exit(1);
    }

    return sem_id;
}

void init_semaphore(int sem_id, int sem_num, int val) {
    if (semctl(sem_id, sem_num, SETVAL, val) == -1) {
        perror("Initializing semaphore error");
        exit(1);
    }
}

void V(int sem_id, int sem_num) {
    struct sembuf semaphore_op[1];

    semaphore_op[0].sem_num = sem_num;
    semaphore_op[0].sem_op = 1;
    semaphore_op[0].sem_flg = 0;

    if (semop(sem_id, semaphore_op, 1) == -1) {
        perror("Semaphore operation (Incrementing) failed.");
        exit(1);
    }
}

void P(int sem_id, int sem_num) {
    struct sembuf semaphore_op[1];

    semaphore_op[0].sem_num = sem_num;
    semaphore_op[0].sem_op = -1;
    semaphore_op[0].sem_flg = 0;

    if (semop(sem_id, semaphore_op, 1) == -1) {
        perror("Semaphore operation (Incrementing) failed.");
        exit(1);
    }
}
