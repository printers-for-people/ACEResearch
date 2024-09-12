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


async def run_sim():
    loop = asyncio.get_running_loop()
    sim = SimPTY(loop)
    try:
        while True:
            data = None
            try:
                data = await asyncio.wait_for(sim.read(), timeout=3)
            except asyncio.TimeoutError:
                sim.cleanup()
                await asyncio.sleep(2)
                sim = SimPTY(loop)
                continue
            await sim.write(b"OK")
    finally:
        sim.cleanup()
    return 0


async def main():
    err = 0
    try:
        await run_sim()
    except (asyncio.CancelledError, KeyboardInterrupt):
        err = 1
    return err


if __name__ == "__main__":
    err = asyncio.run(main())
    sys.exit(err)
