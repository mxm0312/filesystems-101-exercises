#include <ext2fs/ext2fs.h>
#include <errno.h>
#include <unistd.h>
#include <sys/types.h>

#include <solution.h>

#define EXT2_TOP_OFFSET 1024 /* пустое место до суперблока */

long toRead = 0;

int block_size(struct ext2_super_block* super) {
    return 1024u << super->s_log_block_size;
}

int read_indirect(int img, int out, int block, int size) {
    char buff = malloc(sizeof(char) * size);
    int *numbuff;
    if (pread(img, buff, size, block * size) < 0) {
        return -errno;
    }
    numbuff = (int*) buff;
    for (uint i = 0; i < size / sizeof(int); ++i) {
        
        int part;
        char buff_dir[size];
        if (pread(img, buff_dir, size, numbuff[i] * size) < 0) {
            return -errno;
        }
        
        if (size >= toRead) {
            part = toRead;
        }
        else {
            part = size;
        }
        
        if (write(out, buff_dir, part) < part) {
            return -errno;
        }
        
        toRead -= part;
        
        if (toRead < 0) {
            break;
        }
    }
    return 0;
}

int dump_file(int img, int inode_nr, int out) {
    
    struct ext2_super_block super;
    struct ext2_group_desc desc;
    struct ext2_inode inode;
    
    if (pread(img, &super, sizeof(super), EXT2_TOP_OFFSET) < 0) {
        return -errno;
    }
    
    int size = block_size(&super); /* размер одного блока */
    
    long off = size*(super.s_first_data_block + 1)+(inode_nr - 1)/(super.s_inodes_per_group*sizeof(desc));
    
    if (pread(img, &desc, sizeof(desc), off) < 0) {
        return -errno;
    }
    /* Once the block is identified, the local inode index for the local inode table can be identified using: */
    int index = (inode_nr - 1) % super.s_inodes_per_group;
    
    int pos = desc.bg_inode_table * size +
            (index * super.s_inode_size);
    
    if (pread(img, &inode, sizeof(inode), pos) < 0) {
        return -errno;
    }
    
    toRead = inode.i_size;
    
    for (int i = 0; i < EXT2_N_BLOCKS; ++i) {
        if (i < EXT2_NDIR_BLOCKS) {
            // direct
            char buff[size];
            int part;
            if (pread(img, buff, size, inode.i_block[i] * size) < 0) {
                return -errno;
            }
            if (size >= toRead) {
                part = toRead;
            }
            else {
                part = size;
            }
            if (write(out, buff, part) < part) {
                return -errno;
            }
            toRead -= part;
        }
        else if (i == EXT2_IND_BLOCK) {
            // indirect
            if (read_indirect(img, out, inode.i_block[i], size) < 0) {
                return -errno;
            }
        }
        else if (i == EXT2_DIND_BLOCK) {
            // double indirect
            char buff[size];
            int numbuff[size];
            if (pread(img, buff, size, inode.i_block[i] * size) < 0) {
                return -errno;
            }
            for (int i = 0; i < size; i++) {
                numbuff[i] = (int)buff[i];
            }
            for (uint i = 0; i < size / sizeof(int); i++) {
                if (read_indirect(img, out, numbuff[i], size) < 0) {
                    return -errno;
                }
                if (toRead < 0) {
                    break;
                }
            }
        }
    }
    return 0;
}
