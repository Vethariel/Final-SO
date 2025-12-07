# include <stdio.h>

int main() {
    FILE *fp;
    char buffer[255];
    int mem_total = 0;
    int mem_free = 0;

    fp = fopen("/proc/meminfo", "r");
    if (fp == NULL) {
        perror("Error opening file");
        return -1;
    }
    while(fgets(buffer, sizeof(buffer), fp) != NULL) {
        if (sscanf(buffer, "MemTotal: %d kB", &mem_total) == 1) {
            continue;
        }
        if (sscanf(buffer, "MemFree: %d kB", &mem_free) == 1) {
            continue;
        }
    }
    printf("Total Memory MB: %d kB\n", mem_total/1024);
    printf("Free Memory MB: %d kB\n", mem_free/1024);
    fclose(fp);
    return 0;
}