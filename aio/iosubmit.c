#include "iosubmit.h"

#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>

#include <string.h>

int
io_ctx_create(struct io_ctx *io_ctx, int capacity, int (*wait_cb)(int event_fd))
{
	memset(io_ctx, 0, sizeof(*io_ctx));
	io_ctx->event_fd = eventfd(0, 0);
	if (io_ctx->event_fd < 0)
		return -errno;
	int res = io_setup(capacity, &io_ctx->ctx);
	if (res < 0) {
		close(io_ctx->event_fd);
		return res;
	}
	io_ctx->capacity = capacity;
	io_ctx->requests = (struct io_req *)calloc(capacity, sizeof(struct io_req));
	io_ctx->req_pool = (struct io_req **)calloc(capacity, sizeof(struct io_req *));
	io_ctx->wait_cb = wait_cb;
	/* Initialize the requests pool. */
	for (int i = 0; i < capacity; ++i)
		io_ctx->req_pool[i] = io_ctx->requests + i;
	return 0;
}

void
io_ctx_destroy(struct io_ctx *io_ctx)
{
	/* Waint until all requests are done. */
	while (io_ctx->active && io_ctx_process(io_ctx) >= 0);
	io_destroy(io_ctx->ctx);
	free(io_ctx->requests);
	free(io_ctx->req_pool);
	close(io_ctx->event_fd);
}

/*
 * Get a request from a requests pool.
 */
static inline int
io_ctx_get_req(struct io_ctx *io_ctx, struct io_req **io_req)
{
	while (io_ctx->active == io_ctx->capacity) {
		/* Pool is empty, wait until a some finished. */
		int res = io_ctx_process(io_ctx);
		if (res < 0)
			return res;
	}
	*io_req = io_ctx->req_pool[io_ctx->capacity - io_ctx->active - 1];
	++io_ctx->active;
	io_ctx->req_cnt = (io_ctx->req_cnt + 1) & 0x7FFFFFFFFFFFFFFF;
	return 0;
}

/*
 * Put a request to a requests pool.
 */
static inline int
io_ctx_put_req(struct io_ctx *io_ctx, struct io_req *io_req)
{
	io_ctx->req_pool[io_ctx->capacity - io_ctx->active] = io_req;
	--io_ctx->active;
}

int
io_ctx_process(struct io_ctx *io_ctx)
{
	/* Max count to finished requests fetch at once. */
	const int MAX_EVENT_CNT = 32;
	struct io_event events[MAX_EVENT_CNT];
	uint64_t finished = 0;
	/* Waint until some requests are done. */
	if (io_ctx->wait_cb(io_ctx->event_fd) < 0)
		return 0;
	/* Read finished requests count. */
	if (read(io_ctx->event_fd, &finished, sizeof(finished)) < 0)
		return -errno;

	uint64_t completed = 0;
	while (completed < finished) {
		/* Fetch all finished requests and call completion callbacks. */
		int count = finished - completed;
		if (count > MAX_EVENT_CNT)
			count = MAX_EVENT_CNT;
		int processed = io_getevents(io_ctx->ctx, count, count, events, 0);
		if (processed < 0)
			return completed > 0 ? completed : processed;
		for (int i = 0; i < processed; ++i) {
			struct io_req *io_req = (struct io_req *)events[i].data;
			io_req->complete_cb(events[i].res, io_req->data_cb);
			/* Return request to the pool. */
			io_ctx_put_req(io_ctx, io_req);
		}
		completed += processed;
	}
	return completed;
}

int
io_ctx_write(struct io_ctx *io_ctx, int fd, void *data, size_t count, long long offset,
	     void (*complete_cb)(int result, void *data), void *data_cb)
{
	struct io_req *io_req;
	int res = io_ctx_get_req(io_ctx, &io_req);
	if (res < 0)
		return res;
	io_req->complete_cb = complete_cb;
	io_req->data_cb = data_cb;
	io_req->iocb = (struct iocb){
		.data = io_req,
		.key = io_ctx->req_cnt,
		.aio_lio_opcode = IO_CMD_PWRITE,
		.aio_reqprio = 0,
		.aio_fildes = fd,
		.u.c = {
			.buf = data,
			.nbytes = count,
			.offset = offset,
			.flags = 1,
			.resfd = io_ctx->event_fd
		},
	};
	res = io_submit(io_ctx->ctx, 1, (struct iocb **)&io_req);
	if (res > 0)
		return io_req - io_ctx->requests;
	/* Return request. */
	io_ctx_put_req(io_ctx, io_req);
	return res;
}

int
io_ctx_writev(struct io_ctx *io_ctx, int fd, const struct iovec *iov, int iovcnt,
	      long long offset,
	      void (*complete_cb)(int result, void *data), void *data_cb)
{
	struct io_req *io_req;
	int res = io_ctx_get_req(io_ctx, &io_req);
	if (res < 0)
		return res;
	io_req->complete_cb = complete_cb;
	io_req->data_cb = data_cb;
	io_req->iocb = (struct iocb){
		.data = io_req,
		.key = io_ctx->req_cnt,
		.aio_lio_opcode = IO_CMD_PWRITEV,
		.aio_reqprio = 0,
		.aio_fildes = fd,
		.u.c = {
			.flags = 1,
			.resfd = io_ctx->event_fd
		},
		.u.v = {
			.vec = iov,
			.nr = iovcnt,
			.offset = offset,
		}
	};
	res = io_submit(io_ctx->ctx, 1, (struct iocb **)&io_req);
	if (res > 0)
		return io_req - io_ctx->requests;
	/* Return request. */
	io_ctx_put_req(io_ctx, io_req);
	return res;
}

int
io_ctx_read(struct io_ctx *io_ctx, int fd, void *data, size_t count, long long offset,
	    void (*complete_cb)(int result, void *data), void *data_cb)
{
	struct io_req *io_req;
	int res = io_ctx_get_req(io_ctx, &io_req);
	if (res < 0)
		return res;
	io_req->complete_cb = complete_cb;
	io_req->data_cb = data_cb;
	io_req->iocb = (struct iocb){
		.data = io_req,
		.key = io_ctx->req_cnt,
		.aio_lio_opcode = IO_CMD_PREAD,
		.aio_reqprio = 0,
		.aio_fildes = fd,
		.u.c = {
			.buf = data,
			.nbytes = count,
			.offset = offset,
			.flags = 1,
			.resfd = io_ctx->event_fd
		}
	};
	res = io_submit(io_ctx->ctx, 1, (struct iocb **)&io_req);
	if (res > 0)
		return io_req - io_ctx->requests;
	/* Return request. */
	io_ctx_put_req(io_ctx, io_req);
	return res;
}

int
io_ctx_readv(struct io_ctx *io_ctx, int fd, const struct iovec *iov, int iovcnt,
	     long long offset,
	     void (*complete_cb)(int result, void *data), void *data_cb)
{
	struct io_req *io_req;
	int res = io_ctx_get_req(io_ctx, &io_req);
	if (res < 0)
		return res;
	io_req->complete_cb = complete_cb;
	io_req->data_cb = data_cb;
	io_req->iocb = (struct iocb){
		.data = io_req,
		.key = io_ctx->req_cnt,
		.aio_lio_opcode = IO_CMD_PREADV,
		.aio_reqprio = 0,
		.aio_fildes = fd,
		.u.c = {
			.flags = 1,
			.resfd = io_ctx->event_fd
		},
		.u.v = {
			.vec = iov,
			.nr = iovcnt,
			.offset = offset,
		}
	};
	res = io_submit(io_ctx->ctx, 1, (struct iocb **)&io_req);
	if (res > 0)
		return io_req - io_ctx->requests;
	/* Return request. */
	io_ctx_put_req(io_ctx, io_req);
	return res;
}

int
io_ctx_fsync(struct io_ctx *io_ctx, int fd,
	    void (*complete_cb)(int result, void *data), void *data_cb)
{
	struct io_req *io_req;
	int res = io_ctx_get_req(io_ctx, &io_req);
	if (res < 0)
		return res;
	io_req->complete_cb = complete_cb;
	io_req->data_cb = data_cb;
	io_req->iocb = (struct iocb){
		.data = io_req,
		.key = io_ctx->req_cnt,
		.aio_lio_opcode = IO_CMD_FSYNC,
		.aio_reqprio = 0,
		.aio_fildes = fd,
		.u.c = {
			.flags = 1,
			.resfd = io_ctx->event_fd
		}
	};
	res = io_submit(io_ctx->ctx, 1, (struct iocb **)&io_req);
	if (res > 0)
		return io_req - io_ctx->requests;
	/* Return request. */
	io_ctx_put_req(io_ctx, io_req);
	return res;
}

int
io_ctx_fdsync(struct io_ctx *io_ctx, int fd,
	      void (*complete_cb)(int result, void *data), void *data_cb)
{
	struct io_req *io_req;
	int res = io_ctx_get_req(io_ctx, &io_req);
	if (res < 0)
		return res;
	io_req->complete_cb = complete_cb;
	io_req->data_cb = data_cb;
	io_req->iocb = (struct iocb){
		.data = io_req,
		.key = io_ctx->req_cnt,
		.aio_lio_opcode = IO_CMD_FSYNC,
		.aio_reqprio = 0,
		.aio_fildes = fd,
		.u.c = {
			.flags = 1,
			.resfd = io_ctx->event_fd
		}
	};
	res = io_submit(io_ctx->ctx, 1, (struct iocb **)&io_req);
	if (res > 0)
		return io_req - io_ctx->requests;
	/* Return request. */
	io_ctx_put_req(io_ctx, io_req);
	return res;
}

int
io_ctx_cancel(struct io_ctx *io_ctx, int key)
{
	struct io_event event;
	struct io_req *io_req;
	io_req = io_ctx->requests + key;
	int res = io_cancel(io_ctx->ctx, &io_req->iocb, &event);

	if (res == 0) {
		io_req->complete_cb(event.res, io_req->data_cb);
		/* Return request to the pool. */
		io_ctx_put_req(io_ctx, io_req);
		return 0;
	}
	return res;
}
