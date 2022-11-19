#include <ext2fs/ext2fs.h>
#include <linux/fs.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>

#define EXT2_TOP_OFFSET 1024 /* пустое место до суперблока */

int get_block_size(struct ext2_super_block* super) {
    return 1024u << super->s_log_block_size;
}

int read_block(int img, int out, size_t block, size_t size, size_t part) {
    
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

int copy_file(int img, int out, struct ext2_super_block* super, int inode_nr) {
    
    struct ext2_inode inode;
    size_t size = get_block_size(super);
    
    if (read_inode(img, &inode, inode_nr, super) < 0) {
        return -errno;
    }
    
    size_t to_read = inode.i_size;
    
    // direct
    size_t part = 0;
    
    for (size_t i = 0; i < EXT2_NDIR_BLOCKS; ++i) {
        
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
            return 0; /* read completed */
        }
    }
    
    // Single indirect
    
    part = 0;
    uint32_t block[size];
    
    if (pread(img, block, size, size * inode.i_block[EXT2_IND_BLOCK]) < 0) {
        return -errno;
    }
    
    for (uint i = 0; i < size / sizeof(int); ++i) {
        if (to_read > size) {
            part = size;
        }
        else {
            part = to_read;
        }
        
        if (read_block(img, out, block[i], size, part) < 0) {
          return -errno;
        }
        
        if (!(to_read -= part)) {
            return 0; /* read completed */
        }
        
      }
    
      free(block);
    
    // Double indirect
    
    part = 0;
    size_t num = size / sizeof(int);
    uint32_t *d_block = malloc(2 * size);
    
    if (pread(img, d_block, size, inode.i_block[EXT2_DIND_BLOCK] * size) < 0) {
        free(d_block);
        return -errno;
    }
    
    uint32_t* block_index = d_block + num;
    
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
                free(d_block);
                return 0; /* read completed */
            }
        }
    }
    
    free(d_block);
    return 0;
}

int dump_file(int img, int inode_nr, int out)
{
    struct ext2_super_block super;
    struct ext2_inode inode;
    
    size_t super_size = sizeof(struct ext2_super_block);
    
    if (pread(img, &super, super_size, EXT2_TOP_OFFSET) < 0) {
        return -errno;
    }
    else if (read_inode(img, &inode, inode_nr, &super) < 0) {
        return -errno;
    }
    else if (copy_file(img, out, &super, inode_nr) < 0) {
        return -errno;
    }
    
	return 0;
}
