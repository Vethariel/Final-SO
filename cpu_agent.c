#include <stdio.h>
#include <stdlib.h>

void cpu_proc(char *cpu_data)
{
    FILE *fp;
    char cpu_label[5];
    unsigned long user, nice, system, idle, iowait, irq, softirq, steal;

    fp = fopen("/proc/stat", "r");
    if (fp == NULL)
    {
        perror("Error opening file");
        exit(-1);
    }
    if (fscanf(fp, "%4s %lu %lu %lu %lu %lu %lu %lu %lu",
               cpu_label,&user, &nice, &system, &idle, &iowait, &irq, &softirq, &steal) == 9)
    {
        sprintf(cpu_data, "%lu;%lu;%lu;%lu;%lu;%lu;%lu;%lu",
                user, nice, system, idle, iowait, irq, softirq, steal);
    }
    else
    {
        perror("Error reading CPU info");
        fclose(fp);
        exit(-1);
    }
    fclose(fp);
}

int main()
{
    char cpu_data[256];
    cpu_proc(cpu_data);
    printf("%s\n", cpu_data);
    return 0;
}