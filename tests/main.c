// SPDX-License-Identifier: CC0
// SPDX-FileCopyrightText: Copyright 2024 Jookia

#define _POSIX_C_SOURCE 199309L
#define _DEFAULT_SOURCE

#define SECOND_US 1000000
#define KEEPALIVE_LENGTH_US (3 * SECOND_US)

const char KEEPALIVE_DATA[] =
	"\377\252 \0{\"id\":140,\"method\":\"get_status\"}'\377\376";

#include <fcntl.h>
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
	return tty;
}

int tryOpenACE(void) {
	int tty = tryOpenSimulator();
	if (tty == -1) {
		tty = tryOpenSerial();
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

void waitTTYClosed(int tty) {
	ssize_t count = 0;
	static char buf[1024];
	do {
		count = read(tty, &buf, sizeof(buf));
	} while (count > 0);
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

struct keepaliveTesterParams {
	const char *testName;
	int waitTime;
	const char *dataToSend;
	int dataSize;
	int reconnect;
	int expectedLength;
};

void keepaliveTester(struct keepaliveTesterParams *params) {
	struct timespec time_start;
	struct timespec time_end;
	fprintf(stdout, "%s ", params->testName);
	fflush(stdout);

	// Open the ACE and catch the last keepalive cycle
	int tty = waitOpenACE();
	progressDot();
	waitTTYClosed(tty);
	progressDot();
	close(tty);

	// Open again to start fresh
	tty = waitOpenACE();
	progressDot();

	// Pass any required time
	sleepMicroseconds(params->waitTime);
	progressDot();

	// Write data if requested
	if (params->dataToSend && params->dataSize) {
		write(tty, params->dataToSend, params->dataSize);
	}
	progressDot();

	// Re-open if needed
	if (params->reconnect) {
		close(tty);
		tty = waitOpenACE();
	}
	progressDot();

	// Measure the keepalive time
	getTime(&time_start);
	waitTTYClosed(tty);
	getTime(&time_end);
	progressDot();

	// Cleanup
	close(tty);

	// Confirm it's the correct length
	int keepalive_length = durationMicroseconds(&time_start, &time_end);
	int is_correct_length = microsecondsEqual(
		keepalive_length, params->expectedLength, 500000);

	// Print the results
	const char *tag = is_correct_length ? "SUCCESS" : "ERROR";
	fprintf(stdout, " %s: Keepalive timeout is %i, should be around %i\n",
		tag, keepalive_length, params->expectedLength);
}

// Test that the keepalive times out after the correct amount of seconds
void testKeepaliveNoData(void) {
	struct keepaliveTesterParams params;
	params.testName = "Keepalive keepalive, no data";
	params.waitTime = 0;
	params.dataToSend = NULL;
	params.dataSize = 0;
	params.reconnect = 0;
	params.expectedLength = KEEPALIVE_LENGTH_US;
	keepaliveTester(&params);
}

// Test that the keepalive extends keepalive out after sending good data
void testKeepaliveData(void) {
	struct keepaliveTesterParams params;
	params.testName = "Keepalive keepalive, data";
	params.waitTime = (2 * SECOND_US);
	params.dataToSend = KEEPALIVE_DATA;
	params.dataSize = sizeof(KEEPALIVE_DATA);
	params.reconnect = 0;
	params.expectedLength = KEEPALIVE_LENGTH_US;
	keepaliveTester(&params);
}

// Test that the keepalive extends keepalive out after sending good data and
// persists between connects
void testKeepaliveDataPersists(void) {
	struct keepaliveTesterParams params;
	params.testName = "Keepalive keepalive, data, persists";
	params.waitTime = (2 * SECOND_US);
	params.dataToSend = KEEPALIVE_DATA;
	params.dataSize = sizeof(KEEPALIVE_DATA);
	params.reconnect = 1;
	params.expectedLength = KEEPALIVE_LENGTH_US;
	keepaliveTester(&params);
}

int main(void) {
	fprintf(stdout, "-- KEEPALIVE TESTS --\n");
	testKeepaliveNoData();
	testKeepaliveData();
	testKeepaliveDataPersists();
	return 0;
}
