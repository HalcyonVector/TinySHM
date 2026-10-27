# Lab day checklist: get the real sensor working

Goal for this session: the LCD shows **MPU OK**, then real flicks are recorded.

## Before you leave (on your laptop)

1. `git pull` in the TinySHM folder.
2. Open `Keil\Project.uvproj`, press **F7**. Check "0 Error(s)".
3. Copy `Keil\Project.hex` to a pen drive (the laptop has no COM port, so
   you flash from a lab PC).
4. Bring: the MPU6050 modules, female-to-female and male-to-female
   jumpers, croc clips, insulation tape, the rulers, clips, a coin, and the
   pen drive. Ask the lab for a **multimeter** when you arrive.

## Step 1: flash and look at the LCD

1. Wire the sensor as before: VCC to TP2 (3.3V), GND to TP3, SDA to CNC
   pin 5, SCL to CNC pin 6. Keep the LCD, keypad and LED cables as before.
2. Flash `Project.hex` with Flash Magic, close Flash Magic.
3. Pull the sensor's VCC wire for 5 seconds, put it back, press **RESET**.

What the LCD shows:

| LCD | Meaning | Next |
|---|---|---|
| `MPU OK on CNC` / `Z = ... mg` | The sensor works. Z is about +1000 or -1000 when the sensor lies flat | Go to Step 3 |
| `MPU6050 NOT FND` / `CNC A20 CNA ...` | No answer on CNC | Go to Step 2 |

## Step 2: try the other connector (CNA)

The board can also talk to the sensor on connector **CNA** (P0.10 and
P0.11, the LPC1768's second I2C port). The new code tries it by itself,
so there is nothing to rebuild.

1. Unplug the LED ribbon cable from CNA. CNA is the LEDs' connector, so the
   8 LEDs stay off while the sensor uses it. The LCD still shows everything
   (answer, health score, UNSAFE), so nothing else is lost.
2. Move SDA to **CNA pin 7** and SCL to **CNA pin 8**. Pins go in pairs from
   the end marked "1": 1-2, 3-4, 5-6, **7-8**, 9-10. So 7-8 is the pair next
   to the far end.
3. Pull VCC for 5 seconds, put it back, press RESET.

- `MPU OK on CNA`: it works. Keep it there and leave the LED cable out.
- Still `NOT FND`: do Step 2b and send the LCD line plus the readings.

### Step 2b: multimeter (black probe on TP3)

| Probe on | Expected |
|---|---|
| Sensor VCC pin | 3.3 V |
| Sensor SDA pin | about 3.3 V |
| Sensor SCL pin | about 3.3 V |
| CNC pin 5 and pin 6 (with the wires on) | about 3.3 V |

## Step 3: mount the sensor and check a flick

1. Tape the sensor 2 cm from the tip of ruler B, and tape the wires along the
   ruler. Use 40 cm jumpers, or a female-to-female jumper plus a
   male-to-female jumper per pin, and tape every joint.
2. Press RESET, wait for `Flick the ruler`, then flick.
3. After about 10 seconds the LCD shows an answer and a frequency. The
   frequency should be roughly 5 to 9 Hz. Write it down.

## Step 4: record data

Recording needs the board's serial output on a PC. Pick whichever works:

**A. Lab PC has Python** (check with `py --version` in a Command Prompt).
Copy the TinySHM folder from the pen drive, run `py -m pip install -r
requirements.txt`, then follow Part 6 of
`docs/BUILD_GUIDE.md` with `py pc/logger.py --port COMx ...`. Afterwards
copy the `data` folder back to your laptop on the pen drive.

**B. Lab PC has no Python (or pip is blocked).** Capture the board's output
with a terminal program and convert it on your laptop:
1. On the lab PC open PuTTY or Tera Term: Serial, the board's COM port,
   19200 baud. Turn on logging to a file (PuTTY: Session > Logging >
   "All session output"; Tera Term: File > Log).
2. Use **one log file per condition**, named like the table in Part 6,
   for example `healthy_0.txt`, `loose_support_1.txt`. Flick the right number
   of times, then close the log.
3. Copy the files to your laptop and import each one:
   `py pc/logger.py --file healthy_0.txt --label healthy --step 0`
   It saves every flick in the file exactly as a live recording would.

**C. Neither.** The board still learns by itself for the demo:
- press **C** and give 10 healthy flicks to set the baseline;
- press **A** and give 5 flicks of each fault to teach it.
