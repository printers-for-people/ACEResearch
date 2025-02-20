I'm not longer working on this
==============================

Hi, I (Jookia) am no longer working on this. There's two reasons.

The first is that the there's a Klipper plugin now! See the [DuckACE
project](https://github.com/utkabobr/DuckACE/) for more information.
It was always my intention to develop a Klipper plugin and leave it to
users to use it how they wanted and report issues.

The second larger one is that I don't have this hardware, so I can't do
much more in reverse engineering on the finer details I'd like to do.
This stuff doesn't matter too much for end users, but it would be things
like documenting the hardware. Maybe even a Klipper port. These things
aren't feasible to do remotely.

**This project is not abandoned**, I'm happy to merge contributions and
try and help figure things out. I just can't provide much value myself.

Thanks for understanding!

Anycubic ACE Pro Reverse Engineering
====================================

This GitHub repository is aimed at coordinating reverse engineering efforts and
documented the ACE Pro. If you'd like to contribute, please open an issue or
contact me!

The goal of this repository is to allow use of the ACE Pro on printers using
open source firmware.

Currently in progress:

- Writing an emulator for development
- Writing a Klipper plugin for the ACE

We currently want:

- People with an ACE connected to a printer running open source software

Related projects:

- How to root your Kobra 3: https://github.com/systemik/Kobra3-Firmware
- How to root your Kobra 3 (old): https://github.com/Bushmills/Anycubic-Kobra-3-roote
- Custom Kobra 3 firmware: https://github.com/utkabobr/DuckPro-Kobra3

Other discussions:

- https://github.com/Bushmills/Anycubic-Kobra-3-rooted/discussions/2
- https://github.com/printers-for-people/catboat/issues/42

Everything here is licensed CC0 aside from mjson which is licensed MIT.

Thanks
------

Thanks to the following people who have contributed to reverse engineering this
project (Alphabetical order):

- avx for for doing manual tests and strace dumps
- Bushmills for for helping root the Kobra 3
- Spitko for providing access to an ACE for development
- wiltwong for cross-compiling debugging binaries

Development instructions
------------------------

If you want to help with development in this repository, here's some quick
notes. A Linux development system is required, WSL may work (untested).

First, run the emulator in one terminal:

```
./emulator/main.py
```

Second, build and run the tests:

```
cd tests
./build.sh
./main
```

Third, build the tests for the Kobra:

```
cd tests
./build_cross.sh
```

You should have a 'main_armv7' file you can copy to the Kobra to run and
test the actual ACE. Running these tests on actual hardware may cause
unexpected behaviour, so be prepared to report bugs.

If you find anything interesting in your tests, please submit the test output
as a GitHub issue.
