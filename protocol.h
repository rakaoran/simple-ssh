#include <stddef.h>
#include <sys/types.h>

#ifndef MAX_PACKET_SIZE
#define MAX_PACKET_SIZE (16 * 1024)
#endif

#ifndef MAX_PAYLOAD_SIZE
#define MAX_PAYLOAD_SIZE ((16 * 1024) - 2)
#endif

#ifndef INBUF_SIZE
#define INBUF_SIZE (1024 * 1024)
#endif

#ifndef OUTBUF_SIZE
#define OUTBUF_SIZE (5 * 1024 * 1024)
#endif

#ifndef KiB
#define KiB 1024
#endif

typedef struct ringbuf_ {
	char *buf;
	size_t size;
	size_t pending;
	size_t offset;
} ringbuf;

ringbuf *rb_new(size_t size);
void rb_free(ringbuf *rb);
int rb_append(ringbuf *rb, char *buf, size_t len);
size_t rb_copy(ringbuf *rb, char *buf, size_t len);
int rb_consume(ringbuf *rb, size_t n);
size_t rb_pending(ringbuf *rb);
size_t rb_size(ringbuf *rb);

typedef struct _proto_conn {
	int tcp_fd;
	ringbuf *inrb;
	ringbuf *outrb;
} proto_conn;

int proto_flush(proto_conn *conn);
int proto_load(proto_conn *conn);
int proto_read(proto_conn *conn, char *buf, size_t len);
int proto_write(proto_conn *conn, char *buf, size_t len);
proto_conn *proto_new(int tcp_fd);
void proto_free(proto_conn *con);
