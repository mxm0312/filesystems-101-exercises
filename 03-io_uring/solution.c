#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <sys/stat.h>
#include <liburing.h>
#include <solution.h>

#define QD 4 // запросы на чтение
#define BS (256 * 1024) // длина запроса

struct io_data {
    int read;
    int offset;
    struct iovec iov;
};


static int infd, outfd;
static int inflight;

static int setup_context(unsigned entries, struct io_uring *ring)
{
    int ret;

    ret = io_uring_queue_init(entries, ring, 0);
    if (ret < 0) {
        fprintf(stderr, "queue_init: %s\n", strerror(-ret));
        return -1;
    }

    return 0;
}
 

static int get_file_size(int fd, off_t *size)
{
    
    struct stat st;
    
    if (fstat(fd, &st) < 0) {
        return -1;
    }
    
    *size = st.st_size;
    
    return 0;
}

static int queue_read(struct io_uring *ring, off_t size, off_t offset, int fd)
{
    struct io_uring_sqe *sqe;
    struct io_data *data;

    data = malloc(size + sizeof(*data));
    if (!data)
        return 1;

    sqe = io_uring_get_sqe(ring);
    if (!sqe) {
        free(data);
        return 1;
    }

    data->read = 1;
    data->offset = data->first_offset = offset;

    data->iov.iov_base = data + 1;
    data->iov.iov_len = size;
    data->first_len = size;

    io_uring_prep_readv(sqe, fd, &data->iov, 1, offset);
    io_uring_sqe_set_data(sqe, data);
    
    return 0;
}

static void queue_prepped(struct io_uring *ring, struct io_data *data, int fd_in, int fd_out)
{
    struct io_uring_sqe *sqe;

    sqe = io_uring_get_sqe(ring);
    assert(sqe);

    if (data->read) {
        io_uring_prep_readv(sqe, fd_in, &data->iov, 1, data->offset);
    }
    else {
        io_uring_prep_writev(sqe, fd_out, &data->iov, 1, data->offset);
    }

    io_uring_sqe_set_data(sqe, data);
}



static void queue_write(struct io_uring *ring, struct io_data *data, int fd_in, int fd_out)
{
    data->read = 0;
    data->offset = data->first_offset;

    data->iov.iov_base = data + 1;
    data->iov.iov_len = data->first_len;

    queue_prepped(ring, data, fd_in, fd_out);
    io_uring_submit(ring);
}


int copy(int in, int out)
{
    struct io_uring ring;
    unsigned long reads, writes;
    struct io_uring_cqe *cqe;
    off_t write_left, offset;
    int ret;
    
    ret = setup_context(QD, &ring, 0);
    if (ret) {
        return ret
    }
    
    ret = get_file_size(in, &read_left)
    if (ret) {
        return ret
    }
    
    // имплементация как в документации
    
    write_left = insize;
    writes = reads = offset = 0;

    while (insize || write_left) {
        unsigned long had_reads;
        int got_comp;
    
        had_reads = reads;
        while (insize) {
            off_t this_size = insize;

            if (reads + writes >= QD)
                break;
            if (this_size > BS)
                this_size = BS;
            else if (!this_size)
                break;

            if (queue_read(ring, this_size, offset, in))
                break;

            insize -= this_size;
            offset += this_size;
            reads++;
        }

        if (had_reads != reads) {
            ret = io_uring_submit(ring);
            if (ret < 0) {
               
                break;
            }
        }

        got_comp = 0;
        while (write_left) {
            struct io_data *data;

            if (!got_comp) {
                ret = io_uring_wait_cqe(ring, &cqe);
                got_comp = 1;
            } else {
                ret = io_uring_peek_cqe(ring, &cqe);
                if (ret == -EAGAIN) {
                    cqe = NULL;
                    ret = 0;
                }
            }
            if (ret < 0) {
                
                return 1;
            }
            if (!cqe)
                break;

            data = io_uring_cqe_get_data(cqe);
            if (cqe->res < 0) {
                if (cqe->res == -EAGAIN) {
                    queue_prepped(ring, data, in, out);
                    io_uring_submit(ring);
                    io_uring_cqe_seen(ring, cqe);
                    continue;
                }
                
                return 1;
                
            } else if ((size_t)cqe->res != data->iov.iov_len) {
              
                data->iov.iov_base += cqe->res;
                data->iov.iov_len -= cqe->res;
                data->offset += cqe->res;
                queue_prepped(ring, data, in, out);
                io_uring_submit(ring);
                io_uring_cqe_seen(ring, cqe);
                continue;
            }

        
            if (data->read) {
                queue_write(ring, data, in, out);
                write_left -= data->first_len;
                reads--;
                writes++;
            } else {
                free(data);
                writes--;
            }
            io_uring_cqe_seen(ring, cqe);
        }
}
