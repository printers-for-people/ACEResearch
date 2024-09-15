// SPDX-License-Identifier: CC0
// SPDX-FileCopyrightText: Copyright 2024 Jookia

NEW_TEST("No data")
DATA("")
PINGS_KEEPALIVE(false)
HAS_OUTPUT(false)
END_TEST()

NEW_TEST("Single byte")
DATA("x")
PINGS_KEEPALIVE(false)
HAS_OUTPUT(false)
END_TEST()

NEW_TEST("Alphabet")
DATA("abcdefghijklmnopqrstuvwxyz")
PINGS_KEEPALIVE(false)
HAS_OUTPUT(false)
END_TEST()

NEW_TEST("Longer than 64 bytes")
DATA("11111111112222222222333333333344444444445555555555666666666612345")
PINGS_KEEPALIVE(false)
HAS_OUTPUT(false)
END_TEST()

NEW_TEST("get_status")
DATA("\xFF\xAA\x20\x00{\"id\":140,\"method\":\"get_status\"}\x27\xFF\xFE")
PINGS_KEEPALIVE(true)
HAS_OUTPUT(true)
END_TEST()

NEW_TEST("get_status corruption")
DATA("\xFF\xAA\x20\x00{\"id\":140,\"method\":\"get_statux\"}\x27\xFF\xFE")
PINGS_KEEPALIVE(true)
HAS_OUTPUT(false)
END_TEST()

NEW_TEST("get_status bad CRC")
DATA("\xFF\xAA\x20\x00{\"id\":140,\"method\":\"get_status\"}\x00\x00\xFE")
PINGS_KEEPALIVE(true)
HAS_OUTPUT(false)
END_TEST()

NEW_TEST("get_status bad header")
DATA("\x00\xAA\x20\x00{\"id\":140,\"method\":\"get_status\"}\x27\xFF\xFE")
PINGS_KEEPALIVE(false)
HAS_OUTPUT(false)
END_TEST()
