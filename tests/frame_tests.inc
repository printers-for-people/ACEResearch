// SPDX-License-Identifier: CC0
// SPDX-FileCopyrightText: Copyright 2024 Jookia

// Test that no data triggers nothing
NEW_TEST("No data does nothing")
DATA("")
PINGS_KEEPALIVE(false)
HAS_OUTPUT(false)
END_TEST()

// Test that garbage data doesn't do anything
NEW_TEST("Garbage data does nothing")
DATA("Abcdefghijklmnopqrstuvwxyz")
PINGS_KEEPALIVE(false)
HAS_OUTPUT(false)
END_TEST()

// Test that an empty frame with invalid length and CRC pings the keepalive
NEW_TEST("Empty frame")
DATA("\xFF\xAA\x00\x00\x00\x00\xFE")
PINGS_KEEPALIVE(true)
HAS_OUTPUT(false)
END_TEST()

// Test that 0xAA is required after 0xFF to parse a valid frame
NEW_TEST("Interrupted header")
DATA("\xFFZ\xAA\x00\x00\x00\x00\xFE")
PINGS_KEEPALIVE(false)
HAS_OUTPUT(false)
END_TEST()

// Test that a correct get_status works
NEW_TEST("Valid frame contents")
DATA("\xFF\xAA\x20\x00{\"id\":140,\"method\":\"get_status\"}\x27\xFF\xFE")
PINGS_KEEPALIVE(true)
HAS_OUTPUT(true)
END_TEST()

// Test that the keepalive is pinged even with invalid frame contents
NEW_TEST("Invalid frame contents")
DATA("\xFF\xAA\x20\x00{\"id\":140,\"method\":\"get_status\"}\x00\x00\xFE")
PINGS_KEEPALIVE(true)
HAS_OUTPUT(false)
END_TEST()

// Test that the header can be in the checksum
NEW_TEST("Checksum is header")
DATA("\xFF\xAA\x21\x00{\"id\":7376,\"method\":\"get_status\"}\xFF\xAA\xFE")
PINGS_KEEPALIVE(true)
HAS_OUTPUT(true)
END_TEST()

// Test that the trailer can be in the checksum
NEW_TEST("Checksum includes trailer")
DATA("\xFF\xAA\x22\x00{\"id\":81853,\"method\":\"get_status\"}\xFF\xFE\xFE")
PINGS_KEEPALIVE(true)
HAS_OUTPUT(true)
END_TEST()

// Test that the checksum can be 0x0000
NEW_TEST("Checksum 0x0000")
DATA("\xFF\xAA\x22\x00{\"id\":41440,\"method\":\"get_status\"}\x00\x00\xFE")
PINGS_KEEPALIVE(true)
HAS_OUTPUT(true)
END_TEST()

// Test that any bytes after the checksum are unused
NEW_TEST("Bytes are unused after checksum")
DATA("\xFF\xAA\x20\x00{\"id\":140,\"method\":\"get_status\"}\x27\xFF\xAB\xCD\xFE")
PINGS_KEEPALIVE(true)
HAS_OUTPUT(true)
END_TEST()

// Test that extra bytes in the payload fail
// Test that all bytes are being passed to the JSON
NEW_TEST("Extra bytes in payload fail")
DATA("\xFF\xAA\x22\x00{\"id\":140,\"method\":\"get_status\"}\x27\xFF\x00\x00\xFE")
PINGS_KEEPALIVE(true)
HAS_OUTPUT(false)
END_TEST()

// Test that multiple frames can be put in a single packet
NEW_TEST("Two frames in one packet")
DATA("\xFF\xAA\x00\x00\x00\x00\xFE\xFF\xAA\x20\x00{\"id\":140,\"method\":\"get_status\"}\x27\xFF\xFE")
PINGS_KEEPALIVE(true)
HAS_OUTPUT(true)
END_TEST()

// Test that frames don't begin at the start of CDC packet
// Test that frames can be split over CDC packets
NEW_TEST("One frame over multiple packets")
DATA("ZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZ\xFF\xAA\x20\x00{\"id\":140,\"method\":\"get_status\"}\x27\xFFZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZ\xFEZZZZZ")
PINGS_KEEPALIVE(true)
HAS_OUTPUT(true)
END_TEST()
