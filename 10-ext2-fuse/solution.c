#include <ext2fs/ext2fs.h>
#include <fuse.h>
#include <sys/stat.h>
#include <unistd.h>

#include <solution.h>

#define EXT2_TOP_OFFSET 1024 /* пустое место до суперблока */

int find_inode(int img, struct ext2_super_block* super, int inode_nr, const char* path);

int get_block_size(struct ext2_super_block* super) {
    return 1024u << super->s_log_block_size;
}

ssize_t read_super_block(int img, struct ext2_super_block* super_block) {
    size_t size = sizeof(struct ext2_super_block);
    if (pread(img, super_block, size, EXT2_TOP_OFFSET) < 0) {
        return -errno;
    };
    return 0;
}

int read_inode(int img, struct ext2_inode* inode, int inode_nr, struct ext2_super_block* super) {
    /* Once the block is identified, the local inode index for the local inode table can be identified using: */
    size_t index_inode = (inode_nr - 1) % super->s_inodes_per_group;
    size_t desc_inx = (inode_nr - 1) / super->s_inodes_per_group;
    
    struct ext2_group_desc desc;
    // прочитать description
    int block_size = get_block_size(super);
        
    size_t offset = (super->s_first_data_block + 1) * block_size + sizeof(struct ext2_group_desc) * desc_inx;
        
    if (pread(img, &desc, sizeof(struct ext2_group_desc), offset) < 0) {
        return -errno;
    }
        
    int pos = desc.bg_inode_table * block_size + index_inode * super->s_inode_size;
    
    uint inode_size = sizeof(struct ext2_inode);
    
    if (pread(img, inode, inode_size, pos)) {
        return -errno;
    }
    return 0;
}

int find_inode_dir(int img, struct ext2_super_block* super, size_t block, const char* path) {
    
    if (block == 0) {
        return -ENOENT;
    }
    
    int size = get_block_size(super);
    
    char* buff = malloc(size);
    
    if (pread(img, buff, size, block * size) < 0) {
        free(buff);
        return -errno;
    }
  
    char* cp = buff;
    
    while (cp - buff < size) {
        
        struct ext2_dir_entry_2* entry = (struct ext2_dir_entry_2*) cp;
        
        int inode = entry->inode;
        
        if (inode == 0) {
            free(buff);
            return -ENOENT;
        }
        const char* next = path;
        while (*next && *next != '/') {
            ++next;
        }
        int equals = strncmp(path, entry->name, entry->name_len);
        
        if (next - path == entry->name_len && equals == 0) {
            
            int inode_nr = entry->inode;
            
            if (next[0] != '/') {
                free(buff);
                return inode_nr;
            }
            if (entry->file_type == EXT2_FT_DIR) {
                free(buff);
                return find_inode(img, super, inode_nr, next);
            }
            free(buff);
            return -ENOTDIR;
        }
        cp += entry->rec_len;
    }
    free(buff);
    return 0;
}

int find_ind(int img, struct ext2_super_block* super, size_t block, const char* path) {
    
    int size = get_block_size(super);
    
    uint32_t* buff = malloc(size);
    
    if (block == 0) {
        free(buff);
        return -ENOENT;
    }
    else if (pread(img, buff, size, size * block) < 0) {
        free(buff);
        return -errno;
    }
    for (uint i = 0; i < size / sizeof(int); ++i) {
        int res = find_inode_dir(img, super, buff[i], path);
        if (res) {
            free(buff);
            return res;
        }
    }
    
    free(buff);
    return 0;
}

int find_dind(int img, struct ext2_super_block* super, size_t block, const char* path) {
    
    int block_size = get_block_size(super);
    uint32_t* dind = malloc(block_size);
    
    if (block == 0) {
        free(dind);
        return -ENOENT;
    }
    if (pread(img, dind, block_size, block_size * block) < 0) {
        free(dind);
        return -errno;
    }
    
    for (uint i = 0; i < block_size / sizeof(int); ++i) {
        int res = 0;
        if ((res = find_ind(img, super, dind[i], path)) != 0) {
            free(dind);
            return res;
        }
    }
    
    free(dind);
    return 0;
}

int find_inode(int img, struct ext2_super_block* super, int inode_nr, const char* path) {
    
    struct ext2_inode inode;
    
    if (inode_nr == 0) {
        return -ENOENT;
    }
    if (path[0] != '/') {
        return inode_nr;
    }
    
    ++path;
    
    if (read_inode(img, &inode, inode_nr, super) < 0) {
        return -errno;
    }
    
    int res = 0;
    
    for (size_t i = 0; i < EXT2_NDIR_BLOCKS; ++i) {
        if ((res = find_inode_dir(img, super, inode.i_block[i], path))) {
            return res;
        }
    }
    
    if ((res = find_ind(img, super, inode.i_block[EXT2_IND_BLOCK], path))) {
        return res;
    }
    
    if ((res = find_dind(img, super, inode.i_block[EXT2_DIND_BLOCK], path))) {
        return res;
    }
    
    return -ENOENT;
}

void *init(struct fuse_conn_info *conn, struct fuse_config *config) {
    (void)conn;
    (void)config;
    return NULL;
}

static int fuse_write(const char *req, const char *buf, size_t size, off_t off, struct fuse_file_info *fi) {
    (void)req;
    (void)buf;
    (void)size;
    (void)off;
    (void)fi;
    return -EROFS;
}

static int fuse_create(const char *path, mode_t mode, struct fuse_file_info *fi) {
    (void)path;
    (void)mode;
    (void)fi;
    return -EROFS;
}

static int fuse_write_buf(const char *path, struct fuse_bufvec *buf, off_t offset, struct fuse_file_info *fi) {
    (void)path;
    (void)buf;
    (void)offset;
    (void)fi;
    return -EROFS;
}

static int fuse_mkdir(const char *path, mode_t mode) {
    (void)path;
    (void)mode;
    return -EROFS;
}

static int fuse_mknod(const char *path, mode_t mode, dev_t rdev) {
    (void)path;
    (void)mode;
    (void)rdev;
    return -EROFS;
}


int toRead;
int ext2_img;
struct ext2_super_block ext2_super;

static int fuse_getattr(const char *path, struct stat *stat, struct fuse_file_info *fi) {
    (void)fi;
    size_t stat_size = sizeof(struct stat);
    memset(stat, 0, stat_size);
    
    int inode_nr;
    
    if ((inode_nr = find_inode(ext2_img, &ext2_super, EXT2_ROOT_INO, path)) < 0) {
        return -ENOENT;
    }
    
    struct ext2_inode inode;
    
    if (read_inode(ext2_img, &inode, inode_nr, &ext2_super) < 0) {
        return -errno;
    }
    
    stat->st_blksize = get_block_size(&ext2_super);
    stat->st_ino = inode_nr;
    stat->st_mode = inode.i_mode;
    stat->st_nlink = inode.i_links_count;
    stat->st_uid = inode.i_uid;
    stat->st_gid = inode.i_gid;
    stat->st_size = inode.i_size;
    stat->st_blocks = inode.i_blocks;
    stat->st_atime = inode.i_atime;
    stat->st_mtime = inode.i_mtime;
    stat->st_ctime = inode.i_ctime;
    
    return 0;
}

static int fuse_open(const char *path, struct fuse_file_info *fi) {
    
    bool isAllowed = fi->flags & O_ACCMODE != O_RDONLY;
    
    if (isAllowed) {
        return -EROFS;
    }
    
    if (find_inode(ext2_img, &ext2_sb, EXT2_ROOT_INO, path) < 0) {
        return -ENOENT;
    }
    return 0;
}


static const struct fuse_operations ext2_ops = {
    .init = init,
    .write = fuse_write,
    .create = fuse_create,
    .write_buf = fuse_write_buf,
    .mkdir = fuse_mkdir,
    .mknod = fuse_mknod,
    .getattr = fuse_getattr,
    .open = fuse_open,
    //.readdir = fuse_readdir,
    //.read = fuse_read,
};

int ext2fuse(int img, const char *mntp) {
    ext2_img = img;

    char *argv[] = {"exercise", "-f", (char*) mntp, NULL};
    return fuse_main(3, argv, &ext2_ops, NULL);
}
