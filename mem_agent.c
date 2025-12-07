# include <stdio.h>
#include <stdlib.h>

void mem_proc(char* mem_data) {
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
    sprintf(mem_data, "%ld;%ld;%ld;%ld",
         (mem_total-mem_free)/1024,
          mem_free/1024, 
          swap_total/1024,
          swap_free/1024);

    fclose(fp);
}

int main(){
    char mem_data[256];
    mem_proc(mem_data);
    printf("%s\n", mem_data);
    return 0;
}