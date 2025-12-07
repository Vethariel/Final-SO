#include <arpa/inet.h>
#include <errno.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ipc.h>
#include <sys/sem.h>
#include <sys/shm.h>
#include <sys/types.h>
#include <unistd.h>

#define MAX_HOSTS 10

struct host_info {
    char ip[32];
    float cpu_usage;
    float cpu_user;
    float cpu_system;
    float cpu_idle;
    float mem_used;
    float mem_free;
    float swap_total;
    float swap_free;
};

static void error(const char *msg) {
    perror(msg);
    exit(1);
}

static void lock_shared(int semid) {
    struct sembuf op = {0, -1, 0};
    if (semop(semid, &op, 1) == -1) {
        error("semop lock failed");
    }
}

static void unlock_shared(int semid) {
    struct sembuf op = {0, 1, 0};
    if (semop(semid, &op, 1) == -1) {
        error("semop unlock failed");
    }
}

static void clear_screen(void) {
#ifdef _WIN32
    system("cls");
#else
    printf("\033[2J\033[H");
    fflush(stdout);
#endif
}

static int collector_alive(int shmid, int semid) {
    struct shmid_ds ds;
    if (shmctl(shmid, IPC_STAT, &ds) == -1) {
        return 0;
    }

    if (semctl(semid, 0, GETVAL) == -1) {
        return 0;
    }

    return 1;
}

int main(void) {
    key_t shm_key = ftok("collector.c", 'A');
    if (shm_key == -1) {
        error("ftok shm failed");
    }

    int shmid = shmget(shm_key, sizeof(struct host_info) * MAX_HOSTS, 0666);
    if (shmid == -1) {
        error("shmget failed");
    }

    struct host_info *hosts = (struct host_info *)shmat(shmid, NULL, 0);
    if (hosts == (void *)-1) {
        error("shmat failed");
    }

    key_t sem_key = ftok("collector.c", 'S');
    if (sem_key == -1) {
        error("ftok sem failed");
    }

    int semid = semget(sem_key, 1, 0666);
    if (semid == -1) {
        error("semget failed");
    }

    while (1) {
        clear_screen();

        lock_shared(semid);
        struct host_info snapshot[MAX_HOSTS];
        memcpy(snapshot, hosts, sizeof(snapshot));
        unlock_shared(semid);

        printf("%-15s %8s %10s %10s %10s %12s %12s %13s %12s\n",
               "IP", "CPU%", "CPU_user%", "CPU_sys%", "CPU_idle%", "Mem_used_MB", "Mem_free_MB", "Swap_total_MB", "Swap_free_MB");
        for (int i = 0; i < MAX_HOSTS; i++) {
            if (snapshot[i].ip[0] == '\0') {
                continue;
            }

            int cpu_zero = snapshot[i].cpu_usage == 0.0f && snapshot[i].cpu_user == 0.0f && snapshot[i].cpu_system == 0.0f && snapshot[i].cpu_idle == 0.0f;
            int mem_zero = snapshot[i].mem_used == 0.0f && snapshot[i].mem_free == 0.0f && snapshot[i].swap_total == 0.0f && snapshot[i].swap_free == 0.0f;

            printf("%-15s ", snapshot[i].ip);
            if (cpu_zero) {
                printf("%8s %10s %10s %10s ", "--", "--", "--", "--");
            } else {
                printf("%8.1f %10.1f %10.1f %10.1f ",
                       snapshot[i].cpu_usage, snapshot[i].cpu_user, snapshot[i].cpu_system, snapshot[i].cpu_idle);
            }

            if (mem_zero) {
                printf("%12s %12s %13s %12s\n", "--", "--", "--", "--");
            } else {
                printf("%12.0f %12.0f %13.0f %12.0f\n",
                       snapshot[i].mem_used, snapshot[i].mem_free, snapshot[i].swap_total, snapshot[i].swap_free);
            }
        }

        fflush(stdout);
        sleep(2);

        if (!collector_alive(shmid, semid)) {
            printf("\nCollector unavailable. Exiting visualizer.\n");
            break;
        }
    }

    shmdt(hosts);
    return 0;
}
