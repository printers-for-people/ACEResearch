Anycubic ACE Pro Reverse Engineering
====================================

This GitHub repository is aimed at coordinating reverse engineering efforts and
documented the ACE Pro. If you'd like to contribute, please open an issue or
contact me!

The goal of this repository is to allow use of the ACE Pro on third party
printers using open source firmware.

Currently in progress:

- Documenting the serial protocol
- Writing an emulator for development
- Writing tests for jailbroken Kobras

We currently want:

- Testers with jailbroken Kobras
- Hardware for reverse engineering

Other discussions:

- https://github.com/Bushmills/Anycubic-Kobra-3-rooted/discussions/2
- https://github.com/printers-for-people/catboat/issues/42

Everything is licensed CC0.

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
