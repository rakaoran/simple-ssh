#include "protocol.h"
#include <arpa/inet.h>
#include <asm-generic/errno-base.h>
#include <asm-generic/errno.h>
#include <errno.h>
#include <netinet/in.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>

ringbuf *rb_new(size_t size) {
	ringbuf *rb = malloc(sizeof(ringbuf));
	rb->size = size;
	rb->offset = 0;
	rb->pending = 0;
	rb->buf = malloc(size);
	return rb;
}

void rb_free(ringbuf *rb) {
	free(rb->buf);
	free(rb);
}

int rb_append(ringbuf *rb, char *buf, size_t len) {
	size_t availabe_space = rb->size - rb->pending;

	if (availabe_space < len) {
		return -1;
	}

	size_t head = (rb->offset + rb->pending) % rb->size;

	if (head + len - 1 >= rb->size) {
		size_t part1len = rb->size - head;
		size_t part2len = len - part1len;
		memcpy(rb->buf + head, buf, part1len);
		memcpy(rb->buf, buf + part1len, part2len);
	} else {
		memcpy(rb->buf + head, buf, len);
	}
	rb->pending += len;
	return 0;
}

size_t rb_copy(ringbuf *rb, char *buf, size_t len) {
	size_t min;
	if (len > rb->pending) {
		min = rb->pending;
	} else {
		min = len;
	}

	if (min == 0) {
		return 0;
	}

	if (rb->offset + min - 1 >= rb->size) {
		size_t part1len = rb->size - rb->offset;
		size_t part2len = min - part1len;
		memcpy(buf, rb->buf + rb->offset, part1len);
		memcpy(buf + part1len, rb->buf, part2len);
	} else {
		memcpy(buf, rb->buf + rb->offset, min);
	}
	return min;
}

int rb_consume(ringbuf *rb, size_t n) {
	if (n > rb->pending) {
		return -1;
	}
	rb->offset = (rb->offset + n) % rb->size;
	rb->pending -= n;
	return 0;
}

size_t rb_pending(ringbuf *rb) {
	return rb->pending;
}

size_t rb_size(ringbuf *rb) {
	return rb->size;
}

proto_conn *proto_new(int tcp_fd) {
	proto_conn *p = malloc(sizeof(proto_conn));
	p->tcp_fd = tcp_fd;
	p->inrb = rb_new(INBUF_SIZE);
	p->outrb = rb_new(OUTBUF_SIZE);
	return p;
}

void proto_free(proto_conn *conn) {
	rb_free(conn->inrb);
	rb_free(conn->outrb);
	free(conn);
}

int proto_flush(proto_conn *conn) {
	char temp[4 * KiB];
	size_t n;
	while ((n = rb_copy(conn->outrb, temp, sizeof(temp))) != 0) {
		int rv = send(conn->tcp_fd, temp, n, MSG_DONTWAIT | MSG_NOSIGNAL);
		if (rv == -1) {
			if (errno == EAGAIN || errno == EWOULDBLOCK)
				return 0;
			return -1;
		}
		rb_consume(conn->outrb, n);
	}
	return 0;
}

int proto_load(proto_conn *conn) {
	char temp[4 * KiB];
	for (;;) {
		size_t available_space = rb_size(conn->inrb) - rb_pending(conn->inrb);
		if (4 * KiB > available_space) {
			return 0;
		}

		int n = recv(conn->tcp_fd, temp, sizeof(temp), MSG_DONTWAIT);
		if (n <= 0) {
			if ((n == -1) && (errno == EAGAIN || errno == EWOULDBLOCK))
				return 0;
			return -1;
		}
		int rv = rb_append(conn->inrb, temp, n);
		if (rv == -1) {
			return -1;
		}
	}
}

int proto_read(proto_conn *conn, char *buf, size_t _) {
	uint16_t packet_len;
	int n = rb_copy(conn->inrb, (char *)&packet_len, sizeof(packet_len));
	if (n != 2) {
		return 0;
	}
	packet_len = ntohs(packet_len);
	if (packet_len > MAX_PACKET_SIZE) {
		return -1;
	}

	size_t pending_in = rb_pending(conn->inrb);
	if (pending_in < packet_len) {
		return 0;
	}
	size_t payload_len = packet_len - 2;
	rb_consume(conn->inrb, 2);
	rb_copy(conn->inrb, buf, payload_len);
	rb_consume(conn->inrb, payload_len);
	return payload_len;
}

int proto_write(proto_conn *conn, char *buf, size_t len) {
	if (len == 0) {
		return -1;
	}
	if (len > MAX_PAYLOAD_SIZE) {
		return -1;
	}
	uint16_t packlen = htons(len + 2);
	char *p = (char *)&packlen;
	int rv = rb_append(conn->outrb, p, 2);
	if (rv == -1) {
		return -1;
	}
	return rb_append(conn->outrb, buf, len);
}
