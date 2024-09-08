// SPDX-License-Identifier: CC0
// SPDX-FileCopyrightText: Copyright 2024 Jookia

#define _POSIX_C_SOURCE 199309L

#define WATCHDOG_LENGTH_US 5000000

#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
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
	return tty;
}

void sleepMicroseconds(int microseconds) {
	struct timespec wait_time;
	wait_time.tv_sec = 0;
	wait_time.tv_nsec = microseconds * 1000;
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
	int err = 0;
	char buf;
	do {
		err = read(tty, &buf, 1);
	} while (err == 0);
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

// Test that the watchdog times out after the correct amount of seconds
void testTimeout(void) {
	fprintf(stdout, "Timeout test ");
	struct timespec time_start;
	struct timespec time_end;

	// Open the ACE and catch the last watchdog cycle
	int tty = waitOpenACE();
	progressDot();
	waitTTYClosed(tty);
	progressDot();
	close(tty);

	// Open again and measure a new cycle's length
	tty = waitOpenACE();
	progressDot();
	getTime(&time_start);
	waitTTYClosed(tty);
	progressDot();
	close(tty);
	getTime(&time_end);

	// Confirm it's the correct length
	int watchdog_length = durationMicroseconds(&time_start, &time_end);
	int is_correct_length =
		microsecondsEqual(watchdog_length, WATCHDOG_LENGTH_US, 500000);

	// Print the results
	const char *tag = is_correct_length ? "SUCCESS" : "ERROR";
	fprintf(stdout, " %s: Watchdog timeout is %i, should be around %i\n",
		tag, watchdog_length, WATCHDOG_LENGTH_US);
}

int main(void) {
	fprintf(stdout, "-- WATCHDOG TESTS --\n");
	testTimeout();
	return 0;
}
