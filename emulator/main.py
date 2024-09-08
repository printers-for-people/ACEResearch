#!/usr/bin/env python
# SPDX-License-Identifier: CC0
# SPDX-FileCopyrightText: Copyright 2024 Jookia
import asyncio
import os
import sys


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
        self._registerEventLoop(self.loop)

    def cleanup(self):
        self._unregisterEventLoop(self.loop)
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
        os.close(self.controlFD)
        os.close(self.consumeFD)

    def _registerEventLoop(self, loop):
        self.readable = asyncio.Event()
        self.writable = asyncio.Event()
        onReadable = lambda: self.readable.set()
        onWritable = lambda: self.writable.set()
        loop.add_reader(self.controlFD, onReadable)
        loop.add_writer(self.controlFD, onWritable)

    def _unregisterEventLoop(self, loop):
        loop.remove_reader(self.controlFD)
        loop.remove_writer(self.controlFD)

    async def read(self):
        await self.readable.wait()
        self.readable.clear()
        data = os.read(self.controlFD, 64)
        return data

    async def write(self, data):
        await self.writable.wait()
        self.writable.clear()
        os.write(self.controlFD, data)


async def run_watchdog(watchdog_event):
    try:
        while True:
            wait = watchdog_event.wait()
            await asyncio.wait_for(wait, timeout=5)
            watchdog_event.clear()
    except asyncio.TimeoutError:
        return 0


async def run_sim(watchdog_event):
    loop = asyncio.get_running_loop()
    sim = SimPTY(loop)
    try:
        while True:
            data = await sim.read()
            if data == b"PING_WATCHDOG\r\n":
                watchdog_event.set()
            await sim.write(data)
    finally:
        sim.cleanup()
    return 0


async def run_cycle():
    watchdog_event = asyncio.Event()
    sim_task = asyncio.create_task(run_sim(watchdog_event))
    watchdog_task = asyncio.create_task(run_watchdog(watchdog_event))
    all_tasks = [sim_task, watchdog_task]
    (done, pending) = await asyncio.wait(all_tasks, return_when=asyncio.FIRST_COMPLETED)
    sim_task.cancel()
    watchdog_task.cancel()


async def main():
    err = 0
    try:
        while True:
            await run_cycle()
            await asyncio.sleep(2)
    except (asyncio.CancelledError, KeyboardInterrupt):
        err = 1
    return err


if __name__ == "__main__":
    err = asyncio.run(main())
    sys.exit(err)
