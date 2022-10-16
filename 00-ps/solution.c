#include <solution.h>
#include <fcntl.h>
#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <errno.h>
#include <string.h>

#define BUF_SIZE 200
#define ARG_MAX 131072

void envsHandler(char * args, int size, char * argv[ARG_MAX]);
void argsHandler(char * args, int size, char * argv[ARG_MAX], int * argc);
char * readFile(char * name, int * sz);


/// Вывести все исполняемые процеесы из /proc
void ps(void)
{
    DIR *dir = opendir("/proc");
    
    if (dir == NULL) {
        report_error("/proc", errno); // Не удалось открыть директорию процессов
        return;
    }
    
    char **argv = calloc(ARG_MAX, sizeof(char*));
    char **argp = calloc(ARG_MAX, sizeof(char*));
    
    struct dirent *d = NULL;
    
    errno = 0;
    while ((d = readdir(dir)) != NULL) {
        pid_t pid = atoi(d->d_name);
        
        if (pid == 0) { continue; }
        
        char path[BUF_SIZE] = {0};
        memset(argv, 0, ARG_MAX*sizeof(char*));
        memset(argp, 0, ARG_MAX*sizeof(char*));
        
        int argc = 0;
        int count = 0;
        
        char cmdline_path[BUF_SIZE] = {0};
        char proc_path[BUF_SIZE] = {0};
        char environ_path[BUF_SIZE] = {0};
        
        sprintf(proc_path, "/proc/%d/exe", pid);
        sprintf(environ_path, "/proc/%d/environ", pid);
        sprintf(cmdline_path, "/proc/%d/cmdline", pid);
        
        count = readlink(proc_path, path, BUF_SIZE);
        if (count == -1) {
            report_error(proc_path, errno);
            closedir(dir);
            return;
        }
        
        char * args = readFile(cmdline_path, &count);
        if (args == NULL) { return; }
        argsHandler(args, count, argv, &argc);
        
        char * envs = readFile(environ_path, &count);
        
        if (envs == NULL) { return; }
        
        envsHandler(envs, count, argp);
        report_process(pid, path, argv, argp);
    }
    
    if (errno != 0) { report_error("/proc", errno); }
    
}

char * readFile(char * name, int * sz) {
    int fd = open(name, O_RDONLY);
    
    if (fd < 0) {
        report_error(name, errno);
        return NULL;
    }
    
    char buf[BUF_SIZE] = { 0 };
    int count = 0;
    *sz = 0;
    while ((count = read(fd, buf, BUF_SIZE)) > 0) {
        *sz += count;
    }
    if (count == -1) {
        report_error(name, errno);
        close(fd);
        return NULL;
    }
    
    char *res = calloc(*sz, sizeof(char));
    
    close(fd);
    
    fd = open(name, O_RDONLY);
    
    if (fd < 0) {
        report_error(name, errno);
        free(res);
        return NULL;
    }
    
    char * cur = res;
    while ((count = read(fd, buf, BUF_SIZE)) > 0) {
        memcpy(cur, buf, count);
        cur += count;
    }
    if (count == -1) {
        report_error(name, errno);
        free(res);
        close(fd);
        return NULL;
    }
    
    close(fd);
    
    return res;
}

void argsHandler(char *args, int size, char *argv[ARG_MAX], int *argc) {
    char *start = args;
    char *cur = start;
    int i = 0;
    while (cur < args + size) {
        if (*cur == '\0') {
            argv[i] = start;
            start = cur + 1;
            i+=1;
        }
        cur+=1;
    }
    *argc = i;
}

void envsHandler(char *args, int size, char *argv[ARG_MAX]) {
    char *start = args;
    char *cur = start;
    int i = 0;
    while (cur < args + size) {
        if (*cur == '\0') {
            argv[i] = start;
            start = cur + 1;
            i+=1;
        }
        cur+=1;
    }
}
