// SPDX-License-Identifier: CC0
// SPDX-FileCopyrightText: Copyright 2024 Jookia

#define _POSIX_C_SOURCE 199309L
#define _DEFAULT_SOURCE

#define SECOND_US 1000000
#define MILLISECOND_US 1000
#define KEEPALIVE_LENGTH_US (3 * SECOND_US)
#define SLEEP_LENGTH_US (1 * SECOND_US)
#define ARRAY_SIZE(x) (sizeof(x) / sizeof(*x))

#include <errno.h>
#include <fcntl.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <termios.h>
#include <time.h>
#include <unistd.h>

#include "mjson.h"

int tryOpenSimulator(void) {
	static char sim_path[1024];
	const char *xdg_path = getenv("XDG_RUNTIME_DIR");
	if (xdg_path == NULL) {
		return -1;
	}
	size_t xdg_len = strlen(xdg_path);
	if (xdg_len > 500) {
		return -1;
	}
	strcpy(sim_path, xdg_path);
	strcat(sim_path, "/KobraACESimulator");
	int tty = open(sim_path, O_RDWR);
	return tty;
}

int tryOpenSerial(void) {
	int tty = open("/dev/serial/by-id/usb-ANYCUBIC_ACE_0-if00", O_RDWR);
	if (tty == -1) {
		tty = open("/dev/serial/by-id/usb-ANYCUBIC_ACE_1-if00", O_RDWR);
	}
	return tty;
}

int tryOpenACE(void) {
	int tty = tryOpenSimulator();
	if (tty == -1) {
		tty = tryOpenSerial();
	}
	if (tty == -1) {
		return -1;
	}
	struct termios cfg = {0};
	cfmakeraw(&cfg);
	cfsetspeed(&cfg, B115200);
	tcsetattr(tty, 0, &cfg);
	return tty;
}

void sleepMicroseconds(int microseconds) {
	if (microseconds == 0) {
		return;
	}

	int sec = microseconds / 1000000;
	int nsec = (microseconds % 1000000) * 1000;

	struct timespec wait_time;
	wait_time.tv_sec = sec;
	wait_time.tv_nsec = nsec;
	nanosleep(&wait_time, NULL);
}

int waitOpenACE(void) {
	int tty = -1;
	while (tty == -1) {
		tty = tryOpenACE();
		sleepMicroseconds(10000);
	}
	return tty;
}

ssize_t waitTTYClosed(int tty) {
	ssize_t ret = 0;
	ssize_t count = 0;
	static char buf[1024];
	do {
		count = read(tty, &buf, sizeof(buf));
		if (count > 0) {
			ret += count;
		}
	} while (count > 0);
	return ret;
}

void getTime(struct timespec *time) {
	clockid_t clock = CLOCK_BOOTTIME;
	int err = clock_gettime(clock, time);
	if (err != 0) {
		fprintf(stdout, "unable to get time?\n");
		abort();
	}
}

int durationMicroseconds(struct timespec *start, struct timespec *end) {
	time_t sec_a = start->tv_sec;
	time_t sec_b = end->tv_sec;
	time_t nsec_a = start->tv_nsec;
	time_t nsec_b = end->tv_nsec;
	int32_t sec_delta = sec_b - sec_a;
	int32_t nsec_delta = nsec_b - nsec_a;
	if (nsec_delta < 0) {
		nsec_delta += 1000000000;
		sec_delta -= 1;
	}
	int sec_delta_ms = sec_delta * 1000000;
	int nsec_delta_ms = nsec_delta / 1000;
	int duration = sec_delta_ms + nsec_delta_ms;
	return duration;
}

int microsecondsEqual(int microseconds, int target, int error) {
	int max = target + error;
	int min = target - error;
	int in_range = min < microseconds && microseconds < max;
	return in_range;
}

void progressDot() {
	fprintf(stdout, ".");
	fflush(stdout);
}

struct frameTestData {
	const char *name;
	const unsigned char *data;
	size_t data_len;
	bool pings_keepalive;
	bool has_output;
};

#define NEW_TEST(x) \
	{ \
		.name = x,
#define DATA(x) .data = (unsigned char *)x, .data_len = (sizeof(x) - 1),
#define PINGS_KEEPALIVE(x) .pings_keepalive = x,
#define HAS_OUTPUT(x) .has_output = x,
#define END_TEST() \
	} \
	,

struct frameTestData frameTestDatas[] = {
#include "frame_tests.inc"
};

#undef NEW_TEST
#undef DATA
#undef PINGS_KEEPALIVE
#undef HAS_OUTPUT
#undef END_TEST

int openTTYCatchLastCycle(void) {
	// Open the ACE and catch the last keepalive cycle
	int tty = waitOpenACE();
	progressDot();
	waitTTYClosed(tty);
	progressDot();
	close(tty);

	// Open again to start fresh
	tty = waitOpenACE();
	progressDot();
	return tty;
}

void writeTTYData(int tty, ssize_t data_len, const unsigned char *data_buf,
	int sleep_us) {
	while (data_len > 0) {
		ssize_t written = write(tty, data_buf, data_len);
		if (written == -1) {
			fprintf(stdout, "Unable to write data\n");
			abort();
		}
		data_len -= written;
		data_buf += written;
		sleepMicroseconds(sleep_us);
	}
}

int getTTYUnreadBytes(int tty) {
	int unread;
	int rc = ioctl(tty, FIONREAD, &unread);
	if (rc != 0) {
		if (errno == EIO) {
			return -1;
		}
		fprintf(stdout, "Unable to get unread TTY bytes!\n");
		abort();
	}
	return unread;
}

void testFrameHang(int size) {
	fprintf(stdout, "Frame hang, size %i ", size);
	fflush(stdout);

	// Open the ACE and catch the last keepalive cycle
	int tty = openTTYCatchLastCycle();

	// Send a frame header that accidentally hangs
	unsigned char header_buf[4];
	header_buf[0] = 0xFF;
	header_buf[1] = 0xAA;
	header_buf[2] = (size & 0x00FF) >> 0;
	header_buf[3] = (size & 0xFF00) >> 8;
	ssize_t header_len = sizeof(header_buf);
	writeTTYData(tty, header_len, header_buf, 0);
	progressDot();

	// Create status buf to test if ACE responds
	const unsigned char status_buf[] =
		"\xFF\xAA\x20\x00{\"id\":140,\"method\":"
		"\"get_status\"}\x27\xFF\xFE";
	ssize_t status_len = sizeof(status_buf) - 1; // Skip NULL

	int max_tries = 10000;
	int total_bytes = 0;
	int try = 0;
	for (try = 0; try < max_tries; ++try) {
		ssize_t wrote = write(tty, status_buf, status_len);
		total_bytes += (wrote > 0) ? wrote : 0;
		int bytes_ready = getTTYUnreadBytes(tty);
		if (wrote == -1 || bytes_ready == -1) {
			// Keepalive timed out, reconnect
			close(tty);
			tty = waitOpenACE();
		}
		if (bytes_ready > 0) {
			total_bytes -= status_len;
			break;
		}
	}

	// Cleanup
	waitTTYClosed(tty);
	progressDot();
	close(tty);

	// Print message
	if (try == max_tries) {
		fprintf(stdout, " ERROR: Failed to unhang ACE\n");
	} else {
		fprintf(stdout,
			" SUCCESS: Unhanged the ACE, took %i tries and %i "
			"bytes\n",
			try, total_bytes);
	}
}

bool testFrameReconnect(bool timeout) {
	fprintf(stdout, "Frame reconnect, timeout %i ", timeout);
	fflush(stdout);

	// Open the ACE and catch the last keepalive cycle
	int tty = openTTYCatchLastCycle();

	// Write first half of data
	const unsigned char data_buf1[] =
		"\xFF\xAA\x20\x00{\"id\":140,\"method\":";
	ssize_t data_len1 = sizeof(data_buf1) - 1; // Skip NULL
	writeTTYData(tty, data_len1, data_buf1, 0);
	progressDot();

	// Close the TTY and reconnect
	if (timeout) {
		waitTTYClosed(tty);
	} else {
		close(tty);
	}
	tty = waitOpenACE();
	progressDot();

	// Write second half of data
	const unsigned char data_buf2[] = "\"get_status\"}\x27\xFF\xFE";
	ssize_t data_len2 = sizeof(data_buf2) - 1; // Skip NULL
	writeTTYData(tty, data_len2, data_buf2, 0);
	progressDot();

	// Read output
	ssize_t output = waitTTYClosed(tty);
	progressDot();

	// Cleanup
	close(tty);

	// Check if the test was successful
	bool success = (output > 0);

	// Print the results
	const char *tag = success ? "SUCCESS" : "ERROR";
	fprintf(stdout, " %s: Read %zi bytes\n", tag, output);

	return success;
}

bool frameTester(struct frameTestData *data, bool reconnect, int sleep_us) {
	struct timespec time_start;
	struct timespec time_end;
	fprintf(stdout, "%s, reconnect is %i ", data->name, reconnect);
	fflush(stdout);

	// Write first half of data
	int tty = openTTYCatchLastCycle();

	// Sleep so we don't measure from the start of the keepalive
	sleepMicroseconds(KEEPALIVE_LENGTH_US - SLEEP_LENGTH_US);
	progressDot();

	// Write test data if requested
	writeTTYData(tty, data->data_len, data->data, sleep_us);
	progressDot();

	// Re-open if needed
	if (reconnect) {
		// We might miss data during this, so don't fail
		// the test later if we have reconnect enabled
		close(tty);
		tty = waitOpenACE();
	}
	progressDot();

	// Measure the keepalive time
	getTime(&time_start);
	ssize_t output = waitTTYClosed(tty);
	getTime(&time_end);
	progressDot();

	// Cleanup
	close(tty);

	// Check if the test was successful
	int keepalive_length = durationMicroseconds(&time_start, &time_end);
	bool pinged_keepalive = microsecondsEqual(
		keepalive_length, KEEPALIVE_LENGTH_US, 500000);
	bool timed_out =
		microsecondsEqual(keepalive_length, SLEEP_LENGTH_US, 500000);
	bool success_pinged = (data->pings_keepalive == pinged_keepalive);
	bool success_timed_out = (!data->pings_keepalive == timed_out);
	bool success_keepalive = (success_pinged && success_timed_out);
	bool success_output = (data->has_output == (output > 0) || reconnect);
	bool success = (success_keepalive && success_output);

	// Print the results
	const char *tag = success ? "SUCCESS" : "ERROR";
	fprintf(stdout, " %s: Keepalive timeout is %i, read %zi bytes\n", tag,
		keepalive_length, output);

	return success;
}

void testFrames(void) {
	fprintf(stdout, "-- FRAME TESTS --\n");
	for (size_t i = 0; i < ARRAY_SIZE(frameTestDatas); ++i) {
		struct frameTestData *data = &frameTestDatas[i];
		frameTester(data, false, 0);
		frameTester(data, true, 0);
	}
	testFrameReconnect(false);
	testFrameReconnect(true);
}

const int frame_sizes[] = {
	1356, // Shouldn't work
	1025, // Should be flaky
	1024, // Should work
};

const int wait_lengths_ms[] = {
	0,
	10,
	100,
};

bool benchmarkFrame(int size, int sleep_us, int attempt) {
	char name[128];
	snprintf(name, sizeof(name), "Frame size %i wait %ius, attempt %i",
		size, sleep_us, attempt);
	unsigned char *frame = malloc(size);
	if (!frame) {
		fprintf(stdout, "Unable to alloc frame\n");
		abort();
	}
	const unsigned char empty_frame[] = "\xFF\xAA\x00\x00\x00\x00";
	memcpy(frame, empty_frame, sizeof(empty_frame));
	frame[size - 1] = '\xFE';
	struct frameTestData data = {
		.name = name,
		.data = frame,
		.data_len = size,
		.pings_keepalive = true,
		.has_output = false,
	};
	bool success = frameTester(&data, false, sleep_us);
	free(frame);
	return success;
}

const int hang_sizes[] = {
	32, // Should work
	64, // Should work
	128, // Should work
	256, // Should work
	320, // Should work
	512, // Should work
	1024, // Should work
	2048, // Shouldn't work
	4096, // Shouldn't work
	8192, // Shouldn't work
	16384, // Shouldn't work
};

int resetACE(void) {
	// This is just a shell script to toggle a smart switch,
	// nothing special. Required for destructive tests
	return system("./ace_reset.sh") == 0;
}

void testHangs(void) {
	fprintf(stdout, "-- HANG TESTS --\n");
	fprintf(stdout, "Testing if we can reset the ACE...\n");
	if (!resetACE()) {
		fprintf(stdout, "Guess not, skipping hang tests!\n");
		return;
	}
	fprintf(stdout, "We can! Proceeding with hang tests...\n");
	fprintf(stdout,
		"Note: These are informational only, ERRORs are not a "
		"problem.\n");
	for (size_t i = 0; i < ARRAY_SIZE(hang_sizes); ++i) {
		int size = hang_sizes[i];
		testFrameHang(size);
		resetACE();
	}
}

void benchmarkFrames(void) {
	fprintf(stdout, "-- FRAME BENCHMARKS --\n");
	fprintf(stdout,
		"Note: These are informational only, ERRORs are not a "
		"problem.\n");
	for (unsigned int i = 0; i < ARRAY_SIZE(frame_sizes); ++i) {
		int size = frame_sizes[i];
		for (unsigned int j = 0; j < ARRAY_SIZE(wait_lengths_ms); ++j) {
			int sleep_ms = wait_lengths_ms[j];
			int sleep_us = sleep_ms * MILLISECOND_US;
			bool succeeded1 = benchmarkFrame(size, sleep_us, 1);
			if (!succeeded1) {
				continue;
			}
			bool succeeded2 = benchmarkFrame(size, sleep_us, 2);
			if (succeeded2) {
				// Stop testing timings if this one works twice
				break;
			}
		}
	}
}

int json_count_keys(const char *json, int json_len, const char *field) {
	const char *obj;
	int obj_len;
	int ret = mjson_find(json, json_len, field, &obj, &obj_len);
	if (ret != MJSON_TOK_OBJECT && ret != MJSON_TOK_ARRAY) {
		return false;
	}
	int key_offset;
	int key_len;
	int value_offset;
	int value_len;
	int value_type;
	int json_offset = 0;
	int key_count = -1;
	do {
		++key_count;
		json_offset = mjson_next(obj, obj_len, json_offset, &key_offset,
			&key_len, &value_offset, &value_len, &value_type);
	} while (json_offset != 0);
	return key_count;
}

// Calculates CRC-16/MCRF4XX
int calc_crc(const unsigned char *data_buf, size_t data_len) {
	int crc = 0xFFFF;

	for (size_t i = 0; i < data_len; ++i) {
		unsigned char byte = data_buf[i];
		crc ^= byte;
		for (int j = 0; j < 8; ++j) {
			if (crc & 1) {
				crc = (crc >> 1) ^ 0x8408;
			} else {
				crc = (crc >> 1);
			}
		}
	}

	return crc;
}

#define FRAME_OVERHEAD 7

void writeFrame(
	int tty, ssize_t payload_len, const unsigned char *payload_buf) {
	static unsigned char frame_buf[1024];
	size_t frame_len = payload_len + FRAME_OVERHEAD;
	int checksum = calc_crc(payload_buf, payload_len);
	if (frame_len > sizeof(frame_buf)) {
		fprintf(stdout, "writeFrame buffer too larger\n");
		abort();
	}
	unsigned char *header = frame_buf + 0;
	unsigned char *payload = frame_buf + 4;
	unsigned char *trailer = payload + payload_len;
	header[0] = 0xFF;
	header[1] = 0xAA;
	header[2] = (payload_len & 0xFF);
	header[3] = ((payload_len >> 8) & 0xFF);
	trailer[0] = (checksum & 0xFF);
	trailer[1] = ((checksum >> 8) & 0xFF);
	trailer[2] = 0xFE;
	memcpy(payload, payload_buf, payload_len);
	writeTTYData(tty, frame_len, frame_buf, 0);
}

unsigned char *readFrame(int tty) {
	// This function assumes a frame begins at the start of the buffer
	// and that it's well-formed. Good enough for testing?
	static unsigned char frame_buf[1024];
	ssize_t read_count = read(tty, &frame_buf, 4);
	if (read_count != 4) {
		fprintf(stdout, "readFrame can't read TTY\n");
		abort();
	}
	unsigned char *header = frame_buf + 0;
	unsigned char *payload = frame_buf + 4;
	if (header[0] != 0xFF || header[1] != 0xAA) {
		fprintf(stdout, "readFrame invalid header\n");
		abort();
	}
	unsigned int payload_len = (header[3] << 8) | header[2];
	size_t read_amount = payload_len + FRAME_OVERHEAD;
	size_t read_left = read_amount - read_count;
	while (read_left != 0) {
		unsigned char *buf_pos = frame_buf + read_amount - read_left;
		read_count = read(tty, buf_pos, read_left);
		read_left -= read_count;
		if (read_count < 0) {
			fprintf(stdout, "readFrame failed to read TTY\n");
			abort();
		}
	}
	unsigned char *trailer = payload + payload_len;
	int read_checksum = (trailer[1] << 8) | trailer[0];
	int checksum = calc_crc(payload, payload_len);
	if (trailer[2] != 0xFE) {
		fprintf(stdout, "readFrame invalid trailer\n");
		abort();
	}
	if (checksum != read_checksum) {
		fprintf(stdout, "readFrame invalid checksum\n");
		abort();
	}
	memmove(frame_buf, payload, payload_len);
	frame_buf[payload_len] = 0;
	return frame_buf;
}

const char *doRPC(const char *frame) {
	int tty = openTTYCatchLastCycle();
	writeFrame(tty, strlen(frame), (const unsigned char *)frame);
	progressDot();
	const char *result = (const char *)readFrame(tty);
	progressDot();
	close(tty);
	return result;
}

bool testRPCID(int id) {
	fprintf(stdout, "Testing ID %i ", id);
	fflush(stdout);

	// Get status with a specific ID
	char frame[128];
	snprintf(frame, sizeof(frame), "{\"id\":%i,\"method\":\"get_status\"}",
		id);
	const char *result = doRPC(frame);
	int result_len = strlen(result);
	progressDot();

	// Check the new value
	double id_value;
	int ret = mjson_get_number(result, result_len, "$.id", &id_value);
	progressDot();
	if (ret != 1) {
		fprintf(stdout, " ERROR: No ID value, frame %s, result: %s\n",
			frame, result);
		return false;
	}
	if ((int)id_value != id) {
		fprintf(stdout, " ERROR: ID was %f, frame %s, result: %s\n",
			id_value, frame, result);
		return false;
	}

	fprintf(stdout, " SUCCESS\n");
	return true;
}

int testIDs[] = {
	100,
	100,
	99,
	0,
	-1,
	50,
	101,
};

void testRPCIDs(void) {
	fprintf(stdout, "-- RPC IDs TESTS --\n");
	for (size_t i = 0; i < ARRAY_SIZE(testIDs); ++i) {
		int id = testIDs[i];
		testRPCID(id);
	}
}

void printInfo(void) {
	fprintf(stdout, "-- TEST INFO --\n");
	const char *result;

	fprintf(stdout, "Test date: Write your info here\n");
	fprintf(stdout, "ACE description: Write your info here\n");
	fprintf(stdout, "Tests version: Write your info here\n");

	fprintf(stdout, "Getting ACE info ");
	const char *frameInfo = "{\"id\":0,\"method\":\"get_info\"}";
	result = doRPC(frameInfo);
	fprintf(stdout, " %s\n", result);

	fprintf(stdout, "Getting filament info ");
	const char *frameFilamentInfo =
		"{\"id\":0,\"method\":\"get_filament_info\"}";
	result = doRPC(frameFilamentInfo);
	fprintf(stdout, " %s\n", result);

	fprintf(stdout, "Getting status ");
	const char *frameStatus = "{\"id\":0,\"method\":\"get_status\"}";
	result = doRPC(frameStatus);
	fprintf(stdout, " %s\n", result);
}

int main(void) {
	printInfo();
	testRPCIDs();
	testFrames();
	testHangs();
	benchmarkFrames();
	return 0;
}
