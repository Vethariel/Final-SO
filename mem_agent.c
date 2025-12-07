# include <stdio.h>
#include <stdlib.h>

void mem_proc(long* mem_data) {
    FILE *fp;
    char buffer[255];
    long mem_total = 0, mem_free = 0, swap_total = 0, swap_free = 0;

    fp = fopen("/proc/meminfo", "r");
    if (fp == NULL) {
        perror("Error opening file");
        exit(-1);
    }
    while(fgets(buffer, sizeof(buffer), fp) != NULL) {
        if (sscanf(buffer, "MemTotal: %d kB", &mem_total) == 1) {
            continue;
        }else if (sscanf(buffer, "MemFree: %d kB", &mem_free) == 1) {
            continue;
        }else if (sscanf(buffer, "SwapTotal: %d kB", &swap_total) == 1) {
            continue;
        }else if (sscanf(buffer, "SwapFree: %d kB", &swap_free) == 1) {
            continue;
        }
    }
    mem_data[0] = (mem_total- mem_free)/1024;
    mem_data[1] = mem_free/1024;
    mem_data[2] = swap_total/1024; 
    mem_data[3] = swap_free/1024;
    fclose(fp);
}

int main(){
    long mem_data[4];
    mem_proc(mem_data);
    printf("Total Memory MB: %ld kB\n", mem_data[0]);
    return 0;
}