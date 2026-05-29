/*
 * The native controller probe now builds upstream controller_src/ble_ll.c,
 * which provides the LL transport entry points directly. Keep this file as an
 * empty translation unit so the Arduino library layout remains stable without
 * exporting duplicate symbols.
 */