# Asynchronous input output library

The library provides functions to issue asynchronous input output
operations through native kernel aio api. The library offers a convenient api
for nonblocking (non-sleep) implementation of a file read/write/sync.

## Prerequisites
 * A linux kernel with AIO enabled (CONFIG_AIO = y).
 * Libaio kernel aio wrapper.
 * File should be open with O_DIRECT flag
 * IO operation should by 512 bytes aligned by position and size
 * Memory buffers should be 512 bytes aligned

## API

### `int
io_ctx_create(struct io_ctx *io_ctx, int capacity, int (*wait_cb)(int wait_fd));`

Create an AIO context.

*Parameters*:

 - io_ctx - pointer to a context structure
 - capacity - AIO queue capacity
 - wait_cb - a callback to wait for a completions eventfd

*Return*:

 - 0 for success
 - a negative errno

### `int
io_ctx_process(struct io_ctx *io_ctx);`

Process completed aio requests. Retrieve all completed requests and call corresponding completion callbacks.

*Parameters*:

 - io_ctx - pointer to a context structure

*Return*:

 - count of competed ios
 - negative errno

### `void
io_ctx_destroy(struct io_ctx *io_ctx);`

Wait until all active ios are done and destroy AIO context.

*Parameters*:

 - io_ctx - pointer to a context structure

### `int
io_ctx_write(struct io_ctx *io_ctx, int fd, void *data, size_t count, long long offset,
	     void (*complete_cb)(int result, void *data), void *data_cb);`

Send a write request.

*Parameters*:

 - io_ctx - pointer to a context structure
 - fd - file handle
 - data - source to write
 - count - count of byte to write
 - offset - write position
 - complete_cb - completion callback
 - data_cb - completion callback data

*Return*:

 - request key
 - negative errno

### `int
io_ctx_pwrite(struct io_ctx *io_ctx, int fd, const struct iovec *iov, int iovcnt,
	      long long offset,
	      void (*complete_cb)(int result, void *data), void *data_cb);`

Send a vector write request.

*Parameters*:

 - io_ctx - pointer to a context structure
 - fd - file handle
 - iov - source to write
 - iovcnt - length of a source vector
 - offset - write position
 - complete_cb - completion callback
 - data_cb - completion callback data

*Return*:

 - request key
 - negative errno

### `int
io_ctx_read(struct io_ctx *io_ctx, int fd, void *data, size_t len, long long offset,
	    void (*complete_cb)(int result, void *data), void *data_cb);`

Send a read request.

*Parameters*:

- io_ctx - pointer to a context structure
 - fd - file handle
 - data - destination to read
 - count - count of byte to read
 - offset - read position
 - complete_cb - completion callback
 - data_cb - completion callback data

*Return*:

 - request key
 - negative errno

### `int
io_ctx_pread(struct io_ctx *io_ctx, int fd, const struct iovec *iov, int iovcnt,
	     long long offset,
	     void (*complete_cb)(int result, void *data), void *data_cb);`

Send a vector read request.

*Parameters*:

 - io_ctx - pointer to a context structure
 - fd - file handle
 - iov - destination to read
 - iovcnt - length of a destination vector
 - offset - read position
 - complete_cb - completion callback
 - data_cb - completion callback data

*Return*:

 - request key
 - negative errno

### `int
io_ctx_fsync(struct io_ctx *io_ctx, int fd,
	     void (*sync_cb)(int result, void *data), void *data_cb);`

Send a file sync request.

*Parameters*:

 - io_ctx - pointer to a context structure
 - fd - file handle
 - complete_cb - completion callback
 - data_cb - completion callback data

*Return*:

 - request key
 - negative errno

### `int
io_ctx_fdsync(struct io_ctx *io_ctx, int fd,
	      void (*sync_cb)(int result, void *data), void *data_cb);`

 Send a file data sync request.

*Parameters*:

 - io_ctx - pointer to a context structure
 - fd - file handle
 - complete_cb - completion callback
 - data_cb - completion callback data

*Return*:

 - request key
 - negative errno

### `int
io_ctx_cancel(struct io_ctx *io_ctx, int key);`

Cancel an aio file request.

*Parameters*:

 - io_ctx - pointer to a context structure
 - key - key of a previous issued io

*Return*:
 - 0 for success
 - negative errno
