# Bootloader Notes

Date: 2026-05-04

## Current package assumption

The package assumes either USB DFU or SWD-based upload depending on the board definition. It does not currently bundle or manage bootloader flashing through a bootburn-style workflow.
