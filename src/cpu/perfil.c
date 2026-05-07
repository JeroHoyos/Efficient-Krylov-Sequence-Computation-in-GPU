#ifdef _WIN32
  #include <windows.h>
  #include <psapi.h>
  #include "perfil.h"

long leer_minflt(void) {
    PROCESS_MEMORY_COUNTERS pmc;
    if (GetProcessMemoryInfo(GetCurrentProcess(), &pmc, sizeof(pmc)))
        return (long)pmc.PageFaultCount;
    return 0;
}

/* Windows no expone fallos mayores por separado; se reporta cero. */
long leer_majflt(void) {
    return 0;
}

long long leer_cpu_us_proceso(void) {
    FILETIME creation, exit, kernel, user;
    if (!GetProcessTimes(GetCurrentProcess(), &creation, &exit, &kernel, &user))
        return 0;
    ULARGE_INTEGER u, k;
    u.LowPart = user.dwLowDateTime;   u.HighPart = user.dwHighDateTime;
    k.LowPart = kernel.dwLowDateTime; k.HighPart = kernel.dwHighDateTime;
    return (long long)((u.QuadPart + k.QuadPart) / 10);
}

long leer_rss_kb(void) {
    PROCESS_MEMORY_COUNTERS pmc;
    if (GetProcessMemoryInfo(GetCurrentProcess(), &pmc, sizeof(pmc)))
        return (long)(pmc.WorkingSetSize / 1024);
    return -1;
}

#else
  #include <stdio.h>
  #include <string.h>
  #include <unistd.h>
  #include <sys/resource.h>
  #include "perfil.h"

long leer_minflt(void) {
    struct rusage ru;
    getrusage(RUSAGE_SELF, &ru);
    return ru.ru_minflt;
}

long leer_majflt(void) {
    struct rusage ru;
    getrusage(RUSAGE_SELF, &ru);
    return ru.ru_majflt;
}

long long leer_cpu_us_proceso(void) {
    FILE *f = fopen("/proc/self/stat", "r");
    if (!f) return 0;
    unsigned long utime = 0, stime = 0;
    int pid; char comm[256]; char state;
    int ppid, pgrp, session, tty_nr, tpgid;
    unsigned int flags;
    unsigned long minflt, cminflt, majflt, cmajflt;
    fscanf(f, "%d %255s %c %d %d %d %d %d %u %lu %lu %lu %lu %lu %lu",
           &pid, comm, &state, &ppid, &pgrp, &session, &tty_nr, &tpgid,
           &flags, &minflt, &cminflt, &majflt, &cmajflt, &utime, &stime);
    fclose(f);
    long clk_tck = sysconf(_SC_CLK_TCK);
    if (clk_tck <= 0) return 0;
    return (long long)((utime + stime) * 1000000LL / clk_tck);
}

long leer_rss_kb(void) {
    FILE *f = fopen("/proc/self/status", "r");
    if (!f) return -1;
    char line[256];
    long rss = -1;
    while (fgets(line, sizeof(line), f)) {
        if (strncmp(line, "VmRSS:", 6) == 0) {
            sscanf(line + 6, " %ld", &rss);
            break;
        }
    }
    fclose(f);
    return rss;
}

#endif
