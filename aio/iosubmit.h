#ifndef IO_SUBMIT_H
#define IO_SUBMIT_H

#include <stdint.h>
#include <libaio.h>
#include <sys/eventfd.h>

/*
 * Internal aio request structure.
 * Contains iocb descriptor to submit and completion callback with data.
 */
struct io_req{
	/* Operation descriptor to submit. */
	struct iocb iocb;
	/* Completion callback. */
	void (*complete_cb)(int result, void *data);
	/* Completion callback data. */
	void *data_cb;
};

/*
 * AIO context structure.
 */
struct io_ctx {
	/* aio descriptor. */
	io_context_t ctx;
	/* Queue capacity. */
	int capacity;
	/* Io completions eventfd. */
	int event_fd;
	/* Wait callback for completions. */
	int (*wait_cb)(int wait_fd);
	/* Count of issued requests, used as key sequence for ios. */
	int64_t req_cnt;
	/* Requests storage. */
	struct io_req *requests;
	/* Available requests list. */
	struct io_req **req_pool;
	/* Count of active requests. */
	int active;
};

/*
 * Create an AIO context.
 * io_ctx - pointer to a context structure
 * capacity - AIO queue capacity
 * wait_cb - a callback to wait for a completions eventfd.
 * Return 0 for success or a negative errno.
 */
int
io_ctx_create(struct io_ctx *io_ctx, int capacity, int (*wait_cb)(int wait_fd));
/*
 * Process completed aio requests.
 * Retrieve all completed requests and call corresponding completion callbacks.
 * io_ctx - pointer to a context structure
 * Return count of competed ios or negative errno.
 */
int
io_ctx_process(struct io_ctx *io_ctx);
/*
 * Wait until all active ios are done and destroy AIO context.
 * io_ctx - pointer to a context structure
 */
void
io_ctx_destroy(struct io_ctx *io_ctx);

/*
 * Send a write request.
 * io_ctx - pointer to a context structure
 * fd - file handle
 * data - source to write
 * count - count of byte to write
 * offset - write position
 * complete_cb - completion callback
 * data_cb - completion callback data
 * Return request key or negative errno.
 */
int
io_ctx_write(struct io_ctx *io_ctx, int fd, void *data, size_t count, long long offset,
	     void (*complete_cb)(int result, void *data), void *data_cb);

/*
 * Send a vector write request.
 * io_ctx - pointer to a context structure
 * fd - file handle
 * iov - source to write
 * iovcnt - length of a source vector
 * offset - write position
 * complete_cb - completion callback
 * data_cb - completion callback data
 * Return request key or negative errno.
 */
int
io_ctx_pwrite(struct io_ctx *io_ctx, int fd, const struct iovec *iov, int iovcnt,
	      long long offset,
	      void (*complete_cb)(int result, void *data), void *data_cb);

/*
 * Send a read request.
 * io_ctx - pointer to a context structure
 * fd - file handle
 * data - destination to read
 * count - count of byte to read
 * offset - read position
 * complete_cb - completion callback
 * data_cb - completion callback data
 * Return request key or negative errno.
 */
int
io_ctx_read(struct io_ctx *io_ctx, int fd, void *data, size_t len, long long offset,
	    void (*complete_cb)(int result, void *data), void *data_cb);

/*
 * Send a vector read request.
 * io_ctx - pointer to a context structure
 * fd - file handle
 * iov - destination to read
 * iovcnt - length of a destination vector
 * offset - read position
 * complete_cb - completion callback
 * data_cb - completion callback data
 * Return request key or negative errno.
 */
int
io_ctx_pread(struct io_ctx *io_ctx, int fd, const struct iovec *iov, int iovcnt,
	     long long offset,
	     void (*complete_cb)(int result, void *data), void *data_cb);

/*
 * Send a file sync request.
 * io_ctx - pointer to a context structure
 * fd - file handle
 * complete_cb - completion callback
 * data_cb - completion callback data
 * Return request key or negative errno.
 */
int
io_ctx_fsync(struct io_ctx *io_ctx, int fd,
	     void (*sync_cb)(int result, void *data), void *data_cb);

/*
 * Send a file data sync request.
 * io_ctx - pointer to a context structure
 * fd - file handle
 * complete_cb - completion callback
 * data_cb - completion callback data
 * Return request key or negative errno.
 */
int
io_ctx_fdsync(struct io_ctx *io_ctx, int fd,
	      void (*sync_cb)(int result, void *data), void *data_cb);

/*
 * Cancel an aio file request.
 * io_ctx - pointer to a context structure
 * key - key of a previous issued io
 * Return 0 for success or negative errno.
 */
int
io_ctx_cancel(struct io_ctx *io_ctx, int key);

#endif
