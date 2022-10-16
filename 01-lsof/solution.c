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

#define MAX_SIZE 5000

/// Отобразить открытые файлы процессов
void lsof(void)
{
    DIR *dir = opendir("/proc");
    
    if (dir == NULL) {
        report_error("/proc", errno);
        return;
    }
    
    struct dirent * d = NULL;
    
    errno = 0;
    while ((d = readdir(dir)) != NULL) {
        pid_t pid = atoi(d->d_name);
        
        if (pid == 0) { continue; }
        
        char fd_path[MAX_SIZE] = {0};
        char file_path[2*MAX_SIZE] = {0};
        char absolute_path[MAX_SIZE] = {0};
        
        sprintf(fd_path, "/proc/%d/fd", pid);
        
        DIR *fd_dir = opendir(fd_path);
        
        if (fd_dir == NULL) {
            report_error(fd_path, errno);
            closedir(dir);
            return;
        }
        
        struct dirent * fd_file = NULL;
        
        while ((fd_file = readdir(fd_dir)) != NULL) {
            int fd = atoi(fd_file->d_name);
            
            if (fd == 0) { continue; }
            
            memset(file_path, 0, MAX_SIZE);
            memset(absolute_path, 0, MAX_SIZE);
            
            sprintf(file_path, "%s/%d", fd_path, fd);
            
            int count = readlink(file_path, absolute_path, MAX_SIZE);
            
            if (count < 0) {
                report_error(file_path, errno);
                return;
            }
            
            report_file(absolute_path);
        }
        
        closedir(fd_dir);
    }
    if (errno != 0) {
        report_error("/proc", errno);
    }
}
