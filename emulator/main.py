#!/usr/bin/env python
# SPDX-License-Identifier: CC0
# SPDX-FileCopyrightText: Copyright 2024 Jookia

import asyncio
import os
import sys
import json


def realpathFD(fd):
    dir = "/proc/self/fd/"
    file = str(fd)
    path = dir + file
    return os.path.realpath(path)


class SimPTY:
    def __init__(self, loop):
        self.loop = loop
        self._destroyPTYPath()
        self._createPTY()
        self._createPTYPath()

    def cleanup(self):
        self._destroyPTYPath()
        self._closeFDs()

    def _createPTY(self):
        (dev_ptmx, dev_pty) = os.openpty()
        self.controlFD = dev_ptmx
        self.consumeFD = dev_pty

    def _ptyPath(self):
        dir = os.getenv("XDG_RUNTIME_DIR")
        file = "KobraACESimulator"
        path = runtime_path = dir + "/" + file
        return path

    def _createPTYPath(self):
        srcPath = realpathFD(self.consumeFD)
        destPath = self._ptyPath()
        os.symlink(srcPath, destPath)

    def _destroyPTYPath(self):
        path = self._ptyPath()
        try:
            os.unlink(path)
        except FileNotFoundError:
            pass

    def _closeFDs(self):
        self.loop.remove_reader(self.controlFD)
        self.loop.remove_reader(self.consumeFD)
        os.close(self.controlFD)
        os.close(self.consumeFD)

    async def read(self):
        readable = asyncio.Event()
        onReadable = lambda: readable.set()
        self.loop.add_reader(self.controlFD, onReadable)
        await readable.wait()
        self.loop.remove_reader(self.controlFD)
        data = os.read(self.controlFD, 64)
        return data

    async def write(self, data):
        writable = asyncio.Event()
        onWritable = lambda: writable.set()
        self.loop.add_writer(self.controlFD, onWritable)
        await writable.wait()
        self.loop.remove_writer(self.controlFD)
        os.write(self.controlFD, data)


class Frame:
    def __init__(self, payload, crc):
        self.payload = payload
        self.crc = crc


class Parser:
    def __init__(self):
        self.buffer = bytearray()
        self.field = bytearray()
        self.state = "none"

        self.payload_length = 0
        self.payload = bytearray()
        self.payload_crc = 0

    def add_buffer(self, bytes):
        self.buffer.extend(bytes)

    def parse_byte(self):
        byte = self.buffer.pop(0)
        if self.state == "none":
            if byte == 0xFF:
                self.state = "maybe_header"
        elif self.state == "maybe_header":
            if byte == 0xAA:
                self.state = "length"
                self.field = bytearray()
                self.payload_length = 0
                self.payload = bytearray()
                self.payload_crc = 0
            else:
                self.state = "none"
        elif self.state == "length":
            self.field.append(byte)
            if len(self.field) == 2:
                length = int.from_bytes(self.field, byteorder="little")
                self.payload_length = length
                if self.payload_length == 0:
                    self.state = "crc"
                    self.field = bytearray()
                else:
                    self.state = "payload"
        elif self.state == "payload":
            self.payload.append(byte)
            if len(self.payload) == self.payload_length:
                self.state = "crc"
                self.field = bytearray()
        elif self.state == "crc":
            self.field.append(byte)
            if len(self.field) == 2:
                crc = int.from_bytes(self.field, byteorder="little")
                self.payload_crc = crc
                self.state = "trailer"
        elif self.state == "trailer":
            if byte == 0xFE:
                self.state = "none"
                frame = Frame(self.payload, self.payload_crc)
                return frame
            else:
                # Deliberately ignore extra bytes
                pass
        else:
            raise RuntimeError("Unknown parser state?")
        return None

    def parse(self):
        frames = []
        while self.buffer != b"":
            frame = self.parse_byte()
            if frame is not None:
                frames.append(frame)
        return frames


async def run_watchdog(watchdog_event):
    try:
        while True:
            wait = watchdog_event.wait()
            await asyncio.wait_for(wait, timeout=3)
            watchdog_event.clear()
    except asyncio.TimeoutError:
        return 0


# Calculates CRC-16/MCRF4XX on a bytearray
def calc_crc(data):
    crc = 0xFFFF

    for i in range(0, len(data)):
        byte = data[i]
        crc = crc ^ byte
        for i in range(0, 8):
            if crc & 1:
                crc = (crc >> 1) ^ 0x8408
            else:
                crc = crc >> 1

    return crc


def process_frame(frame):
    crc = calc_crc(frame.payload)
    if crc != frame.crc:
        return None
    payload = {}
    try:
        payload = json.loads(frame.payload)
    except ValueError:
        return None
    return b"WE DID IT"


async def run_sim(watchdog_event, parser):
    loop = asyncio.get_running_loop()
    sim = SimPTY(loop)
    try:
        while True:
            data = await sim.read()
            parser.add_buffer(data)
            frames = parser.parse()
            for f in frames:
                # TODO: Is the watchdog pinged multiple times on real hardware?
                # TODO: Is the watchdog pinged before or after frame processing?
                # TODO: Is the watchdog pinged before or after writing?
                watchdog_event.set()
                out = process_frame(f)
                if out is not None:
                    await sim.write(out)
    finally:
        sim.cleanup()
    return 0


async def run_cycle(parser):
    watchdog_event = asyncio.Event()
    sim_task = asyncio.create_task(run_sim(watchdog_event, parser))
    watchdog_task = asyncio.create_task(run_watchdog(watchdog_event))
    all_tasks = [sim_task, watchdog_task]
    (done, pending) = await asyncio.wait(all_tasks, return_when=asyncio.FIRST_COMPLETED)
    sim_task.cancel()
    watchdog_task.cancel()
    if sim_task.done() and sim_task.exception():
        raise sim_task.exception()


async def main():
    parser = Parser()
    err = 0
    try:
        while True:
            await run_cycle(parser)
            await asyncio.sleep(2)
    except (asyncio.CancelledError, KeyboardInterrupt):
        err = 1
    return err


if __name__ == "__main__":
    err = asyncio.run(main())
    sys.exit(err)
