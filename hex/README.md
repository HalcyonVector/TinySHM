# Ready-to-flash hex files

Built with `board/gcc/build.sh` (no Keil needed). Flash with Flash Magic:
LPC1768, COM port, 9600 baud. Before every Start: hold ISP, tap RESET, release ISP.
After flashing: close Flash Magic, press RESET.

| File | What it is |
|---|---|
| TinySHM.hex | The damage detector (UART 19200 baud for logger.py) |
| LiveTest.hex | Live sensor check: CNC wire states + WHO_AM_I ping |
| WireTest.hex | Live 0/1 on CNC 5, 6, 9 and CNA 7, 8 (wire finder) |
