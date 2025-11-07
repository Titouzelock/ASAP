Native Test Guide

Build & Test — Native tests and snapshots (preferred for fast iteration)
- Run: `pio test -e native`
- Output: PGM snapshots in `snapshots/` (created automatically by tests).
- Snapshots are 4-bit grayscale (P5, MaxVal 15) exported from the U8g2 full buffer.
- Treat snapshots as a contract when modifying UI rendering; update tests intentionally when UI changes.
- Snapshot naming: a single global counter across tests with action labels (e.g., `000_longpress.pgm`, `001_down.pgm`, `002_click.pgm`). See `SaveActionSnapshot` in `test/test_main.cpp`.

Agent Permissions & Expectations — Native testing
- Agents may proactively run `pio test -e native` whenever relevant to verify changes and refresh snapshots.

Verification Checklist (Quick) — Native testing
- `pio test -e native` passes and snapshots updated intentionally.

