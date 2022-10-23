#include <solution.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <stddef.h>
#include <assert.h>

#include <fuse.h>

char* file_name = "hello";
 
/* Имплементация методов Fuse из документации */

static void *hello_init(struct fuse_conn_info *conn, struct fuse_config *cfg) {
    (void)conn;
    cfg->uid = getuid();
    cfg->gid = getgid();
    cfg->set_uid = 1;
    cfg->set_gid = 1;
    cfg->kernel_cache = 1;
    return NULL;
}

static int hello_read(const char *path, char *buf, size_t size, off_t offset,
                      struct fuse_file_info *fi) {
        size_t len;
        (void) fi;
    
        if(strcmp(path + 1, file_name) != 0) {
            return -ENOENT;
        }

        char out[1024]; /* с запасом */
        pid_t pid = fuse_get_context()->pid;
        sprintf(out, 1024, "hello, %d\n", pid); /* Вывод айдишников процессов */
        len = strlen(buf);

        if ((size_t) offset < len) {
            if ((size_t) offset + size > len) {
                size = len - offset;
            }
            memcpy(buf, out + offset, size);
        } else {
            size = 0;
        }
 
        return size;
}

static int hello_open(const char *path, struct fuse_file_info *fi) {
    
    if (strcmp(path+1, file_name) != 0) {
        return -ENOENT;
    }
     
    if ((fi->flags & O_ACCMODE) != O_RDONLY) {
        return -EACCES;
    }

    return 0;
}

static int hello_write(const char* req, const char *buf, size_t size,
                       off_t off, struct fuse_file_info *fi) {
    (void) req;
    (void) buf;
    (void) size;
    (void) off;
    (void) fi;
    return -EROFS;
}

static int hello_write_buf(const char *path, struct fuse_bufvec *buf,
                           off_t offset, struct fuse_file_info *fi) {
    (void) path;
    (void) buf;
    (void) offset;
    (void) fi;
    return -EROFS;
}

static int hello_getattr(const char *path, struct stat *stbuf,
                         struct fuse_file_info *fi) {
    (void) fi;
    int res = -ENOENT;
    
    memset(stbuf, 0, sizeof(struct stat));
    if (strcmp(path, "/") == 0) {
        stbuf->st_mode = S_IFDIR | 0755;
        stbuf->st_nlink = 2;
        return 0;
    } else if (strcmp(path + 1, file_name) == 0) {
        stbuf->st_mode = S_IFREG | 0444;
        stbuf->st_nlink = 1;
        stbuf->st_size = 1024;
        return 0;
    }
    return res;
}

static int hello_readdir(const char *path, void *buf, fuse_fill_dir_t filler,
                         off_t offset, struct fuse_file_info *fi,
                         enum fuse_readdir_flags flags) {
        (void) offset;
        (void) fi;
        (void) flags;
 
        if (strcmp(path, "/") != 0) {
            return -ENOENT;
        }

        filler(buf, ".", NULL, 0, 0);
        filler(buf, "..", NULL, 0, 0);
        filler(buf, file_name, NULL, 0, 0);
 
        return 0;
}

static int hello_create(const char *path, mode_t mode,
                        struct fuse_file_info *fi) {
    (void) path;
    (void) mode;
    (void) fi;
    return -EROFS;
}

static int hello_mkdir(const char *path, mode_t mode) {
    (void) path;
    (void) mode;
    return -EROFS;
}

static int hello_mknod(const char *path, mode_t mode, dev_t rdev) {
    (void) path;
    (void) mode;
    (void) rdev;
    return -EROFS;
}

static const struct fuse_operations hellofs_ops = {
        .init = hello_init,
        .read = hello_read,
        .open = hello_open,
        .write = hello_write,
        .write_buf = hello_write_buf,
        .getattr = hello_getattr,
        .readdir = hello_readdir,
        .create = hello_create,
        .mkdir = hello_mkdir,
        .mknod = hello_mknod,
};

int helloworld(const char *mntp)
{
	char *argv[] = {"exercise", "-f", (char *)mntp, NULL};
	return fuse_main(3, argv, &hellofs_ops, NULL);
}
