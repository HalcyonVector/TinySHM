# TinySHM: vibration-based structural damage detection on LPC1768

Everything here runs on a laptop **now**, using fake flicks. In the lab you
swap the fake data for real recordings and rerun the same commands.

## One-time setup (each laptop)
1. Install Python 3.10 or newer, and tick "Add to PATH" on Windows.
2. Open a terminal in this folder and run `pip install -r requirements.txt`
3. For the C check, you need gcc. On Windows, install MinGW-w64 or use WSL. This step is optional.

## Practice run with fake data (do this this week)
```
python pc/simulate.py                 # 121 fake flicks in data/sim/
python pc/simulate.py --unseen        # 20 fake faults the model never sees
python pc/build_dataset.py sim        # flicks -> features table
python pc/build_dataset.py sim_unseen
python pc/train.py sim                # trains everything, plots, writes board/model.h
python pc/check_c_parity.py sim       # board C code gives the same answers as Python
python pc/logger.py --sim --label healthy --step 0 --count 5       # fake lab session
python pc/logger.py --sim --label loose_support --step 1 --count 5
python pc/logger.py --sim --label loose_support --step 2 --count 5
python pc/trend.py sim_session        # when will it become unsafe?
python pc/llm_report.py sim_session   # AI-written inspection report
python pc/compare_methods.py sim      # FFT peak vs interpolated FFT vs zero-crossing
```
Results land in `results/sim/` and `results/sim_session/`.

**New to the hardware?** Start with [docs/BUILD_GUIDE.md](docs/BUILD_GUIDE.md): building the beam, wiring the sensor, and every lab step in order.

## Board setup (ALS-SDA-ARMCTXM3-01, from the lab manual)
The sensor is an **MPU6050 (GY-521 board)**, read over I2C. Full photos and
steps are in [docs/BUILD_GUIDE.md](docs/BUILD_GUIDE.md).

| What | Connection |
|---|---|
| LCD | FRC cable CND -> CNAD, short JP16 and JP5, POT3 = contrast (Lab 9) |
| Keypad | FRC cable CNB -> CNB3, short JP4 (1,2) (Lab 9) |
| PC link | DB9 cross cable to CN9, same as Flash Magic, 115200 baud |
| LEDs | FRC cable CNA -> CNA1 (Lab 6). LD2 to LD9 = health bar, all flash = UNSAFE |
| MPU6050 SDA | P0.19 (I2C1 SDA), CNC middle pair, pin 5 |
| MPU6050 SCL | P0.20 (I2C1 SCL), CNC middle pair, pin 6 |
| MPU6050 VCC | TP2 "3.3V" test post (POWER SUPPLY box) |
| MPU6050 GND | TP3 "GND" test post, next to TP2 |
| MPU6050 AD0, INT, XDA, XCL | Leave unconnected |

The sensor uses loose female-to-female jumper wires, not an FRC cable. On
power-up the board prints `MPU6050 found, WHO_AM_I = ...` over UART. If the
LCD says **MPU6050 NOT FOUND**, swap SDA and SCL, check VCC and GND, and
press reset. Any WHO_AM_I value is fine (104 is a genuine chip; clones often
say 112 or 114 and work the same).

**Beam:** two 30 cm steel rulers. Ruler A sticks out 10 cm past the table
edge, held down by a stack of 4 heavy books flush with the edge. Ruler B
overlaps it by 5 cm, held with 3 binder clips (35 cm free length).

**Sensor mounting:** tape the module flat on ruler B, 2 cm from the tip. The
code reads the **Z** axis, which points along the direction the ruler swings.
The MPU6050 is set to +-4 g with its built-in 44 Hz filter.

## Keil uVision 4 project
1. Create a new project for NXP LPC1768 and copy `system_LPC17xx.c` into it, as in Lab 6.
2. Add `board/main.c`, `hw.c`, `features.c`, `classify.c`, and tick "Create HEX file".
3. In `startup_LPC17xx.s` (Configuration Wizard tab) set Stack Size to 0x800.
4. Build. A gcc test build uses about 20 KB of flash (Keil limit 32 KB) and
   about 12.4 KB of RAM (the LPC1768 has 32 KB main RAM plus 32 KB more).
5. Flash with Flash Magic as in Lab 6, then **close Flash Magic** so the logger can use the COM port.

**Keypad keys:** `0` normal mode · `A` teach a new condition (5 flicks) · `C` self-calibrate the healthy baseline (10 flicks) · `F` switch the LCD between the answer and the raw numbers.

## Lab day
1. Wire the sensor and flash the board code. It sends `RES,...` and `RAW,...` once per flick over UART at 115200 baud.
2. Record the flicks. Change `COM5` to your port, which you can find in Device Manager under Ports.
   ```
   python pc/logger.py --port COM5 --label healthy     --step 0 --count 25
   python pc/logger.py --port COM5 --label loose_support --step 1 --count 12   # 3 books left
   python pc/logger.py --port COM5 --label loose_support --step 2 --count 12   # 2 books left
   python pc/logger.py --port COM5 --label loose_support --step 3 --count 10   # 1 book left
   python pc/logger.py --port COM5 --label loose_joint --step 1 --count 15   # 1 clip removed
   python pc/logger.py --port COM5 --label loose_joint --step 2 --count 12   # 2 clips removed
   python pc/logger.py --port COM5 --label added_mass  --step 1 --count 25   # coin taped on
   ```
   Put all 4 books back and re-check "healthy" a few times during the session, because the support drifts.
3. Train on the real data:
   ```
   python pc/build_dataset.py real
   python pc/train.py real
   python pc/check_c_parity.py real
   ```
4. Copy the new `board/model.h` into the Keil project, rebuild and reflash. The board now answers by itself.
5. Run the demo. Run `trend.py real` and `llm_report.py real` for the report.

## What each file does
| File | Job |
|---|---|
| `pc/config.py` | Shared settings (sample rate, classes, damage levels). Must match `board/config.h` |
| `pc/simulate.py` | Fake flicks from cantilever physics |
| `pc/features.py` | 5 features per flick: frequency, damping, amplitude, energy ratio, decay time |
| `pc/build_dataset.py` | All flick files -> one features CSV |
| `pc/train.py` | Trains: frequency-only baseline, decision tree, kNN, neural net (5-8-4), health-score regression, change and novelty detectors. Makes plots and `board/model.h` |
| `pc/export_c.py` | Writes the trained models as C arrays |
| `pc/check_c_parity.py` | Compiles the board C code with gcc and checks it matches Python |
| `pc/predict.py` | Python twin of the board's classifier, used by the logger |
| `pc/logger.py` | Records flicks from the board (or `--sim`), shows live answers |
| `pc/trend.py` | Linear regression on health vs damage step, predicts when it goes UNSAFE |
| `pc/llm_report.py` | LLM-written inspection report. Uses Groq (`GROQ_API_KEY`) first, then Gemini (`GEMINI_API_KEY`), then `--prompt-only` or a built-in template |
| `board/features.c` | Feature extraction + its own FFT (no CMSIS library needed) |
| `board/classify.c` | Runs the tree, neural net, kNN, health score, change and novelty checks, plus teach mode |
| `board/model.h` | Auto-generated by `train.py`. Never edit by hand |
| `board/main.c` | Board program: wait for a flick, record, classify, LCD + UART, keypad modes |
| `board/hw.c` | Drivers: UART0, TIMER0 + ADC sampler, 16x2 LCD, 4x4 keypad |
| `board/config.h` | Sample rate, ADC channel, LCD on/off. Must match `pc/config.py` |
| `board/host_test.c` | Runs the board code on the laptop for testing |

## Important
- **The fake-data numbers are not results.** Only numbers from `results/real/` go in the report.
- If you change a feature in `features.py`, make the same change in `board/features.c` and rerun `check_c_parity.py`.
- The board code compiles cleanly but hasn't run on real hardware yet. The first lab session is for checking the wiring, the sensor readings and the UART link.
