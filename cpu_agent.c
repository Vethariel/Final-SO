#include <stdio.h>
#include <stdlib.h>


void cpu_proc(unsigned long *cpu_data)
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
        cpu_data[0] = user;
        cpu_data[1] = nice;
        cpu_data[2] = system;
        cpu_data[3] = idle;
        cpu_data[4] = iowait;
        cpu_data[5] = irq;
        cpu_data[6] = softirq;
        cpu_data[7] = steal;
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
    unsigned long cpu_data[8];
    cpu_proc(cpu_data);
    printf("User Time: %lu\n", cpu_data[0]);
    return 0;
}