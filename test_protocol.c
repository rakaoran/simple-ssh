#include "protocol.h"
#include "unity.h"
#include "unity_internals.h"
#include <arpa/inet.h>
#include <errno.h>
#include <netinet/in.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

static proto_conn *end1_conn;
static proto_conn *end2_conn;

void debug_print(int n) {
	printf("This is debug line %d\n", n);
}

void tearDown() {
	close(end2_conn->tcp_fd);
	close(end1_conn->tcp_fd);
	proto_free(end2_conn);
	proto_free(end1_conn);
}

void setUp() {
	int sp[2];
	int rv = socketpair(AF_UNIX, SOCK_STREAM, 0, sp);
	if (rv == -1) {
		perror("socketpair");
		exit(EXIT_FAILURE);
	}
	end1_conn = proto_new(sp[0]);
	end2_conn = proto_new(sp[1]);
}

void test_ring_buffer(void) {
	char buf[10];
	ringbuf *rb = rb_new(5);
	TEST_ASSERT_EQUAL_INT_MESSAGE(-1, rb_consume(rb, 3), "Consuming more than existing");
	TEST_ASSERT_EQUAL_size_t_MESSAGE(0, rb_copy(rb, buf, 3), "Copying when ring is empty");

	TEST_ASSERT_EQUAL_INT_MESSAGE(0, rb_append(rb, "hi", 3), "Appending bytes correctly");
	TEST_ASSERT_EQUAL_INT_MESSAGE(3, rb_pending(rb), "Pending bytes must be 3");
	TEST_ASSERT_EQUAL_INT_MESSAGE(-1, rb_append(rb, "helloo", 6), "Appending bytes more than ring can take");

	TEST_ASSERT_EQUAL_size_t_MESSAGE(3, rb_copy(rb, buf, sizeof(buf)), "Copying bytes correctly");
	TEST_ASSERT_EQUAL_STRING_MESSAGE("hi", buf, "Comparing copied bytes to actual bytes");
	TEST_ASSERT_EQUAL_INT_MESSAGE(0, rb_consume(rb, 3), "Consuming all exising bytes");
	TEST_ASSERT_EQUAL_INT_MESSAGE(0, rb_pending(rb), "Pending bytes must be 0");

	TEST_ASSERT_EQUAL_INT_MESSAGE(0, rb_append(rb, "wrap", 5), "Bytes appended wrap at the end of the ring");

	TEST_ASSERT_EQUAL_size_t_MESSAGE(5, rb_copy(rb, buf, sizeof(buf)), "Copying bytes that wrapped");
	TEST_ASSERT_EQUAL_STRING_MESSAGE("wrap", buf, "Comparing copied wrapped bytes to actual bytes");
	TEST_ASSERT_EQUAL_INT_MESSAGE(0, rb_consume(rb, 3), "Consuming some exising bytes");

	TEST_ASSERT_EQUAL_size_t_MESSAGE(2, rb_copy(rb, buf, sizeof(buf)), "Copying left bytes");
	TEST_ASSERT_EQUAL_STRING_MESSAGE("p", buf, "Comparing copied left bytes to actual bytes");
	TEST_ASSERT_EQUAL_size_t_MESSAGE(1, rb_copy(rb, buf, 1), "Copying when buffer has less size than pending");
	TEST_ASSERT_EQUAL_CHAR_MESSAGE('p', *buf, "Comparing copied char");

	TEST_ASSERT_EQUAL_INT_MESSAGE(-1, rb_consume(rb, 3), "Consuming more than exising bytes");
	TEST_ASSERT_EQUAL_INT_MESSAGE(0, rb_consume(rb, 2), "Consuming all exising bytes");

	TEST_ASSERT_EQUAL_size_t_MESSAGE(0, rb_pending(rb), "Ring buffer back to initial state, empty");
}

void test_proto_read(void) {
	char buf[MAX_PAYLOAD_SIZE];
	TEST_ASSERT_EQUAL_INT_MESSAGE(0, proto_read(end2_conn, buf, sizeof(buf)), "When buffer is empty");
	uint16_t packlen = htons(6);
	char *p = (char *)&packlen;
	rb_append(end2_conn->inrb, p, 1);
	TEST_ASSERT_EQUAL_INT_MESSAGE(0, proto_read(end2_conn, buf, sizeof(buf)), "When buffer has one byte");
	rb_append(end2_conn->inrb, p + 1, 1);
	TEST_ASSERT_EQUAL_INT_MESSAGE(0, proto_read(end2_conn, buf, sizeof(buf)), "When buffer has two bytes");
	rb_append(end2_conn->inrb, "hi", 2);
	TEST_ASSERT_EQUAL_INT_MESSAGE(0, proto_read(end2_conn, buf, sizeof(buf)), "Buffer has incomplete packet");
	rb_append(end2_conn->inrb, "!", 2);

	packlen = htons(11);
	rb_append(end2_conn->inrb, p, 2);
	rb_append(end2_conn->inrb, "hi bro!!", 9);

	TEST_ASSERT_EQUAL_INT_MESSAGE(4, proto_read(end2_conn, buf, sizeof(buf)), "Correct payload len");
	TEST_ASSERT_EQUAL_STRING_MESSAGE("hi!", buf, "Read correct message");

	TEST_ASSERT_EQUAL_size_t_MESSAGE(11, rb_pending(end2_conn->inrb),
					 "proto_read clears the packet read from the ring buffer");

	TEST_ASSERT_EQUAL_INT_MESSAGE(9, proto_read(end2_conn, buf, sizeof(buf)), "Correct payload len");
	TEST_ASSERT_EQUAL_STRING_MESSAGE("hi bro!!", buf, "Read correct message");
	TEST_ASSERT_EQUAL_size_t_MESSAGE(0, rb_pending(end2_conn->inrb),
					 "proto_read clears the packet read from the ring buffer");
	packlen = htons(MAX_PACKET_SIZE + 1);
	rb_append(end2_conn->inrb, p, 2);
	rb_append(end2_conn->inrb, "hi bro!!", 9);
	TEST_ASSERT_EQUAL_INT_MESSAGE(-1, proto_read(end2_conn, buf, sizeof(buf)), "Packet length is too large");
}

void test_proto_write(void) {
	char str[20] = "hello there";
	proto_write(end2_conn, str, 12);

	TEST_ASSERT_EQUAL_size_t_MESSAGE(14, rb_pending(end2_conn->outrb),
					 "proto_write must copy it to out ring buffer correctly");
	char buf[20];
	rb_copy(end2_conn->outrb, buf, 20);
	// \x00\x0e is just 14 in hex, as a 2 bytes number in network bytes order
	TEST_ASSERT_EQUAL_STRING_MESSAGE("\x00\x0ehello there", buf,
					 "proto_write must write things to ring buf correctly");

	TEST_ASSERT_EQUAL_INT_MESSAGE(-1, proto_write(end2_conn, buf, MAX_PAYLOAD_SIZE + 1),
				      "Writing too big of a packet");
	TEST_ASSERT_EQUAL_INT_MESSAGE(-1, proto_write(end2_conn, buf, 0), "Writing empty packet");
}
/*
#define MAX_PACKET_SIZE  22
#define MAX_PAYLOAD_SIZE 20
#define INBUF_SIZE       50
#define OUTBUF_SIZE      50
#define KiB               2
*/
void zero_arr(char *buf, size_t n) {
	memset(buf, 0, n);
}

int clog_fd(int fd) {
	char buf[4096];
	int total = 0;
	for (;;) {
		int n = send(fd, buf, sizeof(buf), MSG_NOSIGNAL | MSG_DONTWAIT);
		if (n == -1) {
			if (EWOULDBLOCK == errno || errno == EAGAIN) {
				return total;
			} else {
				exit(EXIT_FAILURE);
			}
		}
		total += n;
	}
}

void unclog_fd(int fd, int ngarbage) {
	char buf[ngarbage];
	while (ngarbage > 0) {
		int n = read(fd, buf, ngarbage);
		if (n == -1) {
			exit(EXIT_FAILURE);
		}
		ngarbage -= n;
	}
}

void test_proto_flush_and_load(void) {
	char buf[1000];
	TEST_ASSERT_EQUAL_INT_MESSAGE(0, proto_flush(end2_conn), "Flush empty buffer");
	proto_write(end2_conn, "meowwww", 8);
	proto_write(end2_conn, "goblins", 8);
	proto_write(end2_conn, "potatos", 8);
	proto_write(end2_conn, "gregory", 8);
	proto_write(end2_conn, "gregorr", 8);
	TEST_ASSERT_EQUAL_INT_MESSAGE(0, proto_flush(end2_conn), "Flush non empty buffer");

	TEST_ASSERT_EQUAL_INT_MESSAGE(0, proto_load(end1_conn), "Load data from kernel buffer");
	proto_read(end1_conn, buf, sizeof(buf));
	TEST_ASSERT_EQUAL_STRING("meowwww", buf);
	zero_arr(buf, sizeof(buf));
	proto_read(end1_conn, buf, sizeof(buf));
	TEST_ASSERT_EQUAL_STRING("goblins", buf);
	zero_arr(buf, sizeof(buf));
	proto_read(end1_conn, buf, sizeof(buf));
	TEST_ASSERT_EQUAL_STRING("potatos", buf);
	zero_arr(buf, sizeof(buf));
	proto_read(end1_conn, buf, sizeof(buf));
	TEST_ASSERT_EQUAL_STRING("gregory", buf);
	zero_arr(buf, sizeof(buf));
	TEST_ASSERT_EQUAL_size_t(0, proto_read(end1_conn, buf, sizeof(buf)));
	TEST_ASSERT_EQUAL_INT_MESSAGE(0, proto_load(end1_conn), "Load data from kernel buffer");
	proto_read(end1_conn, buf, sizeof(buf));
	TEST_ASSERT_EQUAL_STRING("gregorr", buf);
	zero_arr(buf, sizeof(buf));

	TEST_ASSERT_EQUAL_INT_MESSAGE(0, proto_load(end1_conn), "Load data from kernel buffer");
	TEST_ASSERT_EQUAL_size_t(0, proto_read(end1_conn, buf, sizeof(buf)));
	TEST_ASSERT_EQUAL_size_t_MESSAGE(0, rb_pending(end1_conn->inrb), "nothing more to read");

	int garbage = clog_fd(end2_conn->tcp_fd);
	proto_write(end2_conn, "meowwww", 8);
	TEST_ASSERT_EQUAL_INT_MESSAGE(0, proto_flush(end2_conn), "Flush with EAGAIN");
	TEST_ASSERT_EQUAL_size_t_MESSAGE(10, rb_pending(end2_conn->outrb), "nothing was flushed");
	unclog_fd(end1_conn->tcp_fd, garbage);
	TEST_ASSERT_EQUAL_INT_MESSAGE(0, proto_flush(end2_conn), "Flush after EAGAIN");
	TEST_ASSERT_EQUAL_size_t_MESSAGE(0, rb_pending(end2_conn->outrb), "flush succeeded");

	shutdown(end1_conn->tcp_fd, SHUT_WR);
	shutdown(end2_conn->tcp_fd, SHUT_WR);
	proto_write(end2_conn, "meowwww", 8);
	TEST_ASSERT_EQUAL_INT_MESSAGE(-1, proto_flush(end2_conn), "Flush after shutdown");
	TEST_ASSERT_EQUAL_INT_MESSAGE(-1, proto_load(end1_conn), "Load after shutdown");
}

int main() {
	UNITY_BEGIN();
	RUN_TEST(test_ring_buffer);
	RUN_TEST(test_proto_read);
	RUN_TEST(test_proto_write);
	RUN_TEST(test_proto_flush_and_load);
	return UNITY_END();
}
