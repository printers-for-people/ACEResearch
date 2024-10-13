// SPDX-License-Identifier: CC0
// SPDX-FileCopyrightText: Copyright 2024 Jookia

// Test that get_status gives a valid response
NEW_TEST("get_status")
DATA("{\"id\":100,\"method\":\"get_status\"}")
CHECK_DOUBLE("$.id", 100.0)
CHECK_STRING("$.msg", "success")
CHECK_DOUBLE("$.code", 0)
CHECK_STRING("$.result.status", "ready")
CHECK_DOUBLE("$.result.slots[0].index", 0)
END_TEST()
