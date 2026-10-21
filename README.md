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
python pc/logger.py --sim --label loose_clamp --step 1 --count 5
python pc/logger.py --sim --label loose_clamp --step 2 --count 5
python pc/trend.py sim_session        # when will it become unsafe?
python pc/llm_report.py sim_session   # AI-written inspection report
python pc/compare_methods.py sim      # FFT peak vs interpolated FFT vs zero-crossing
```
Results land in `results/sim/` and `results/sim_session/`.

**New to the hardware?** Start with [docs/BUILD_GUIDE.md](docs/BUILD_GUIDE.md): building the beam, wiring the sensor, and every lab step in order.

## Board setup (ALS-SDA-ARMCTXM3-01, from the lab manual)
The code supports two sensors. **MPU6050 (GY-521) is the default.** To use an
ADXL335 (GY-61) instead, set `SENSOR` to `SENSOR_ADXL335` in `board/config.h`
**and** `SENSOR = "adxl335"` in `pc/config.py`, then retrain.

| What | Connection |
|---|---|
| LCD | FRC cable CND -> CNAD, short JP16 and JP5, POT3 = contrast (Lab 9) |
| Keypad | FRC cable CNB -> CNB3, short JP4 (1,2) (Lab 9) |
| PC link | DB9 cross cable to CN9, same as Flash Magic, 115200 baud |
| LEDs | FRC cable CNA -> CNA1 (Lab 6). LD2 to LD9 = health bar, all flash = UNSAFE |
| MPU6050 SDA | P0.19 (I2C1 SDA) on connector CNC, most likely CNC pin 5 |
| MPU6050 SCL | P0.20 (I2C1 SCL) on connector CNC, most likely CNC pin 6 |
| MPU6050 GND | Pin 10 of any CNA/CNB/CNC/CND connector is ground |
| MPU6050 VCC | 3.3 V or 5 V (the GY-521 has its own regulator and pull-up resistors). The FRC connectors have no power pin, so ask the lab staff which header gives 3.3 V or 5 V |
| MPU6050 AD0, INT, XDA, XCL | Leave unconnected |

**Checking the CNC pins (no tools):** CNC carries P0.15 to P0.22. Pin 1 is
marked with a small triangle on the connector, so P0.19 and P0.20 should be
pins 5 and 6 (inferred from the CND layout, so confirm with the connector
picture in Appendix C or the lab staff). On power-up the board prints
`MPU6050 found, WHO_AM_I = ...` over UART. If the LCD says
**MPU6050 NOT FOUND**, swap SDA and SCL, check VCC and GND, and press reset.
Any WHO_AM_I value is fine (104 is a genuine chip; clones often say 112 or 114
and work the same).

**Dry run before the sensor arrives:** set `SENSOR_ADXL335` with JP18 shorted
(2,3), flash, and turn POT4. The LCD and logger should react. This proves the
Keil build, Flash Magic, LCD, keypad and UART before any sensor wiring. Set it
back to `SENSOR_MPU6050` afterwards.

**Sensor mounting:** tape the module flat on the ruler near the tip. The code
reads the **Z** axis, which points along the direction the ruler swings. The
MPU6050 is set to +-4 g with its built-in 44 Hz filter.

### Using an ADXL335 instead
| What | Connection |
|---|---|
| ADXL335 Z out | AD0.4 = P1.30. Remove the JP18 jumper and put a jumper wire on the JP18 pin that is P1.30 (see the check below) |
| ADXL335 GND | Pin 10 of any CNA/CNB/CNC/CND connector is ground |
| ADXL335 VCC | 3.3 V (or 5 V, since the GY-61 module has its own regulator). Ask the lab staff which header gives 3.3 V or 5 V |

**Finding the P1.30 pin on JP18 (5 minutes, no tools):** the manual says "short JP18 (2,3) to use AD0.4 with POT4". Flash the firmware with the jumper on (2,3) and turn POT4: the logger should show the readings change. Then take the jumper off, connect the sensor to pin 2 (most likely P1.30), and flick. If the readings follow the flick, you're done. If they don't, try pin 3, or ask the lab staff.

**Fallback if JP18 doesn't work:** wire the sensor to CND pin 1 (AD0.0 = P0.23), set `ADC_CH 0` and `USE_LCD 0` in `board/config.h`, and read everything on the laptop. The LCD needs CND, so it can't be used in this mode. Flick gently: the ADXL335 clips past about 3 g.

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
   python pc/logger.py --port COM5 --label loose_clamp --step 1 --count 12   # 1/4 turn
   python pc/logger.py --port COM5 --label loose_clamp --step 2 --count 12   # 1/2 turn
   python pc/logger.py --port COM5 --label loose_clamp --step 3 --count 10   # 3/4 turn
   python pc/logger.py --port COM5 --label loose_joint --step 1 --count 15   # 1 clip removed
   python pc/logger.py --port COM5 --label loose_joint --step 2 --count 12   # 2 clips removed
   python pc/logger.py --port COM5 --label added_mass  --step 1 --count 25   # coin taped on
   ```
   Re-tighten and re-check "healthy" a few times during the session, because clamping drifts.
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
