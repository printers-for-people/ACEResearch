// SPDX-License-Identifier: CC0
// SPDX-FileCopyrightText: Copyright 2024 Jookia

#define _POSIX_C_SOURCE 199309L
#define _DEFAULT_SOURCE

#define SECOND_US 1000000
#define MILLISECOND_US 1000
#define KEEPALIVE_LENGTH_US (3 * SECOND_US)
#define SLEEP_LENGTH_US (1 * SECOND_US)
#define ARRAY_SIZE(x) (sizeof(x) / sizeof(*x))

#include <fcntl.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <termios.h>
#include <time.h>
#include <unistd.h>

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
		fprintf(stderr, "unable to get time?\n");
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
	const char *data;
	size_t data_len;
	bool pings_keepalive;
	bool has_output;
};

#define NEW_TEST(x) \
	{ \
		.name = x,
#define DATA(x) .data = x, .data_len = (sizeof(x) - 1),
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

void writeTTYData(int tty, ssize_t data_len, const char *data_buf, int sleep_us) {
	while (data_len > 0) {
		ssize_t written = write(tty, data_buf, data_len);
		if (written == -1) {
			fprintf(stderr, "Unable to write data\n");
			abort();
		}
		data_len -= written;
		data_buf += written;
		sleepMicroseconds(sleep_us);
	}
	progressDot();
}

bool testFrameReconnect(bool timeout) {
	fprintf(stdout, "Frame reconnect, timeout %i ", timeout);
	fflush(stdout);

	int tty = openTTYCatchLastCycle();

	// Open the ACE and catch the last keepalive cycle
	const char data_buf1[] = "\xFF\xAA\x20\x00{\"id\":140,\"method\":";
	ssize_t data_len1 = sizeof(data_buf1) - 1; // Skip NULL
	writeTTYData(tty, data_len1, data_buf1, 0);

	// Close the TTY and reconnect
	if (timeout) {
		waitTTYClosed(tty);
	} else {
		close(tty);
	}
	tty = waitOpenACE();
	progressDot();

	// Write second half of data
	const char data_buf2[] = "\"get_status\"}\x27\xFF\xFE";
	ssize_t data_len2 = sizeof(data_buf2) - 1; // Skip NULL
	writeTTYData(tty, data_len2, data_buf2, 0);

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
	char *frame = malloc(size);
	if (!frame) {
		fprintf(stderr, "Unable to alloc frame\n");
		abort();
	}
	const char empty_frame[] = "\xFF\xAA\x00\x00\x00\x00";
	memset(frame, 0, size);
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

void benchmarkFrames(void) {
	fprintf(stdout, "-- FRAME BENCHMARKS --\n");
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

int main(void) {
	testFrames();
	benchmarkFrames();
	return 0;
}
