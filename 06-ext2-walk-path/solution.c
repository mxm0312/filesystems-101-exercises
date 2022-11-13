#include <ext2fs/ext2fs.h>
#include <linux/fs.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>

#include <solution.h>

#define EXT2_TOP_OFFSET 1024 /* пустое место до суперблока */

int get_block_size(struct ext2_super_block* super) {
    return 1024u << super->s_log_block_size;
}

int read_block(int img, int out, int block, int size, int part) {
    
    char buff[part];
    
    if (pread(img, buff, part, block * size) < 0) {
        return -errno;
    }
    
    if (write(out, buff, part) < 0) {
        return -errno;
    }
    
    return 0;
}

int read_inode(int img, struct ext2_inode* inode, int inode_nr, struct ext2_super_block* super) {
    /* Once the block is identified, the local inode index for the local inode table can be identified using: */
    int index = (inode_nr - 1) % super->s_inodes_per_group;
    
    struct ext2_group_desc desc;
    // прочитать description
    int block_size = EXT2_BLOCK_SIZE(super);
        
    size_t offset = (super->s_first_data_block + 1) * block_size + sizeof(struct ext2_group_desc) * index;
        
    if (pread(img, &desc, sizeof(struct ext2_group_desc), offset) < 0) {
        return -errno;
    }
        
    int pos = desc.bg_inode_table * block_size + index * super->s_inode_size;
    
    uint inode_size = sizeof(struct ext2_inode);
    
    if (pread(img, inode, inode_size, pos)) {
        return -errno;
    }
    return 0;
}

int find_inode_dir(int img, struct ext2_super_block* super, int block, const char* path);

int find_ind(int img, struct ext2_super_block* super, int block, const char* path) {
    
    int size = EXT2_BLOCK_SIZE(super);
    
    uint32_t* buff = malloc(size);
    
    if (block == 0) {
        return -ENOENT;
    }
    else if (pread(img, buff, size, size * block) < 0) {
        return -errno;
    }
    for (uint i = 0; i < size / sizeof(int); ++i) {
        int res = find_inode_dir(img, super, buff[i], path);
        if (res) {
            return res;
        }
    }
    
    free(buff);
    return 0;
}

int find_dind(int img, struct ext2_super_block* super, int block, const char* path) {
    
    int block_size = EXT2_BLOCK_SIZE(super);
    uint32_t* dind = malloc(block_size);
    
    if (block == 0) {
        return -ENOENT;
    }
    if (pread(img, dind, block_size, block_size * block) < 0) {
        return -errno;
    }
    
    for (uint i = 0; i < block_size / sizeof(int); ++i) {
        int res = 0;
        if ((res = find_ind(img, super, dind[i], path)) != 0) {
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

int find_inode_dir(int img, struct ext2_super_block* super, int block, const char* path) {
    
    if (block == 0) {
        return -ENOENT;
    }
    
    int size = EXT2_BLOCK_SIZE(super);
    
    char* buff = malloc(size);
    
    if (pread(img, buff, size, block * size) < 0) {
        return -errno;
    }
  
    char* cp = buff;
    
    while (cp - buff < size) {
        
        struct ext2_dir_entry_2* entry = (struct ext2_dir_entry_2*) cp;
        
        int inode = entry->inode;
        
        if (inode == 0) {
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
                return inode_nr;
            }
            if (entry->file_type == EXT2_FT_DIR) {
                return find_inode(img, super, inode_nr, next);
            }
            
            return -ENOTDIR;
        }
        cp += entry->rec_len;
    }
    
    return 0;
}

int dump_file(int img, const char* path, int out) {
    
    struct ext2_super_block super;
    struct ext2_inode inode;
    
    size_t super_size = sizeof(struct ext2_super_block);
    
    if (pread(img, &super, super_size, EXT2_TOP_OFFSET) < 0) {
        return -errno;
    }
    int inode_nr = find_inode(img, &super, 2, path);
    if (inode_nr < 0) {
        return inode_nr;
    }
    
    size_t size = EXT2_BLOCK_SIZE(&super);
    
    if (read_inode(img, &inode, inode_nr, &super) < 0) {
        return -errno;
    }
    
    size_t to_read = inode.i_size;
    
    // direct
    size_t part = 0;
    
    for (int i = 0; i < EXT2_NDIR_BLOCKS; ++i) {
        
        if (to_read > size) {
            part = size;
        }
        else {
            part = to_read;
        }
        
        if (read_block(img, out, inode.i_block[i], size, part) < 0) {
          return -errno;
        }
        
        if (!(to_read -= part)) {
            break;
        }
    }
    
    // Single indirect
    
    part = 0;
    uint32_t* block = malloc(size);
    
    if (pread(img, block, size, size * inode.i_block[EXT2_IND_BLOCK]) < 0) {
        return -errno;
    }
    
    for (uint i = 0; i < size / sizeof(int); ++i) {
        part = to_read > size ? size : to_read;
        
        if (read_block(img, out, block[i], size, part) < 0) {
          free(block);
          return -errno;
        }
        
        if (!(to_read -= part)) {
            break;
        }
         
      }
    
      free(block);
    
    // Double indirect
    
    part = 0;
    size_t num = size / sizeof(int);
    uint32_t* d_block = malloc(2 * size);
    uint32_t* block_index = d_block + num;
    if (pread(img, d_block, size, inode.i_block[EXT2_DIND_BLOCK] * size) < 0) {
        free(d_block);
        return -errno;
    }
    
    for (size_t i = 0; i < num; ++i) {
        if (pread(img, block_index, size, d_block[i] * size) < 0) {
          free(d_block);
          return -errno;
        }
        for (size_t j = 0; j < num; ++j) {
            
            if (to_read > size) {
                part = size;
            }
            else {
                part = to_read;
            }
            
            if (read_block(img, out, block_index[j], size, part) < 0) {
                free(d_block);
                return -errno;
            }
            
            if (!(to_read -= part)) {
                break;
            }
        }
    }
    
    free(d_block);
    
    return 0;
}
