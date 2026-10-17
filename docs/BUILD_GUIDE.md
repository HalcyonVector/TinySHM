# TinySHM build guide: from shopping bag to demo

Everything here is done by hand. No drilling, cutting or soldering.

## What you need

- 2 steel rulers, 30 cm, same brand (plus 1 spare)
- 1 C-clamp (G-clamp), 3 or 4 inch
- 3 binder clips, 25 mm or 32 mm
- Thin double-sided tape and insulation tape
- **One** sensor module with headers **already soldered**: an MPU6050
  (GY-521 board) or an ADXL335 (GY-61 board). See Part 0 to pick.
- Female-to-female jumper wires (main use) and a few male-to-female (spare)
- A coin, a pen, and the lab's LPC1768 board, PC, Keil and Flash Magic

## Part 0: ADXL335 or MPU6050?

Both measure how hard the ruler shakes (acceleration). The code works with
either one. You only need **one** of them.

**In simple terms:**
- The **ADXL335** sends a **voltage**. More shaking means the voltage goes up
  and down more. The board measures the voltage with its ADC, the same way
  you read POT4 in the lab.
- The **MPU6050** sends **numbers** over two wires using a protocol called
  I2C. The board asks "what is the acceleration?" 100 times a second and the
  chip answers with a number.

Think of it as an analog thermometer you read by eye (ADXL335) versus a
digital thermometer that tells you the exact number (MPU6050).

| | ADXL335 (GY-61) | MPU6050 (GY-521) |
|---|---|---|
| How it talks | Voltage into the ADC | Digital numbers over I2C |
| Wires to the board | 3 (VCC, GND, Z) | 4 (VCC, GND, SDA, SCL) |
| Where it plugs in | JP18 pin (AD0.4) | Connector CNC pins 5 and 6 |
| Detail in each reading | 12-bit: about 370 steps per g | 16-bit: 8192 steps per g, about 22 times finer |
| Noise | Picks up electrical noise on the wire and the board | Converts inside the chip, so the wire carries clean numbers |
| Built-in filter | No | Yes (44 Hz), which removes fast junk before sampling |
| Extra sensors | None | A gyroscope too (not used, but nice to mention) |
| Price (approx., not checked today) | Rs 424 at ElectronicsComp | Rs 150 to 250 |
| Test before it arrives | Yes, POT4 stands in for the sensor | Only the rest of the board (flash the ADXL335 setting and use POT4) |
| If it isn't working | Readings don't change. Harder to tell why | LCD says "MPU6050 NOT FOUND", so you know it's the wiring |
| Good for the viva | Uses the ADC lab you already did | Adds an I2C driver, a real embedded protocol |
| Main risk | Wrong JP18 pin | Wrong CNC pin or SDA/SCL swapped |

**Why the MPU6050 is better here:** cleaner and finer readings mean the
small frequency and damping changes from a loose clip show up more clearly,
and that's exactly what the ML depends on. It's also cheaper and tells you
when the wiring is wrong.

**Why you might still pick the ADXL335:** it's the simplest possible
wiring (just a voltage), and it uses only things you've already done in the
lab.

**Recommendation:** MPU6050. If its I2C pins give trouble, the same code
switches to an ADXL335 by changing one line, so you're not stuck.

**Switching sensors in the code:** set `SENSOR` in `board/config.h`
(`SENSOR_MPU6050` or `SENSOR_ADXL335`) **and** `SENSOR` in `pc/config.py`
(`"mpu6050"` or `"adxl335"`). They must match. MPU6050 is the default.

### What to buy for each

| | ADXL335 | MPU6050 |
|---|---|---|
| Module | GY-61, headers soldered | GY-521, headers soldered |
| Wires | 3 female-to-female | 4 female-to-female |
| Everything else | Same: rulers, clamp, clips, tapes | Same |

## Part 1: Build the beam

You build **one** beam and use it for every test. Never rebuild it between
tests, because the board learns what this exact beam sounds like.

![Side view of the beam](img/beam_side_view.png)

1. **Clamp ruler A.** Lay it flat on the table with 10 cm sticking out past
   the edge (20 cm stays on the table). Put the C-clamp over the part still on the table, about 2 cm
   from the edge, and tighten it firmly by hand. Draw a pen line on the table
   along the ruler and at the clamp, so you can put everything back in the
   same place.
2. **Join ruler B.** Lay ruler B on top of the free end of ruler A so they
   overlap by 5 cm, both pointing the same way. Ruler B's tip should be 35 cm
   from the table edge.
3. **Clip the joint.** Put 3 binder clips across the overlap, spread evenly
   (one near each end of the overlap, one in the middle). Fold the clip
   handles flat or take the metal handles off, so they don't rattle.
4. **Check it.** Flick the tip gently. It should ring for several seconds
   with a clean "boing". If it buzzes or clatters, push the clips on
   harder and tighten the clamp.

Keep these lengths: they put the vibration at roughly 6 to 8 times a second
(an estimate; you'll measure the real value). A much longer beam droops and
vibrates too slowly to measure well. Measure with the ruler markings and
write the numbers down so you can rebuild it the same way.

## Part 2: Stick on the sensor

1. Put a small square of double-sided tape on the back of the GY-521 board
   (the side without the chip).
   (Same for the GY-61 if you use the ADXL335.)
2. Press it flat onto ruler B, about 2 cm from the tip, with the pins
   pointing back toward the table. The chip's flat face must be parallel to
   the ruler.
3. Wrap one turn of insulation tape over the module and around the ruler,
   so it can't peel off when you flick.

## Part 3: Wire the sensor to the board

![Wiring for both sensors](img/wiring.png)

### If you use the MPU6050 (GY-521)

The GY-521 has 8 pins. You only use 4. Use **female-to-female** wires: one
end goes on the module's pin, the other on the board's header pin (both are
male pins).

| GY-521 pin | Goes to on the LPC1768 board |
|---|---|
| VCC | **TP2, marked "3.3V"**, in the POWER SUPPLY box at the bottom right of the board |
| GND | **TP3, marked "GND"**, right next to TP2 (or pin 10 of any 10-pin FRC connector) |
| SCL | Connector **CNC** (the upright 10-pin socket left of the processor cover, beside the coin cell), middle row, pin 6 (P0.20) |
| SDA | Connector **CNC**, middle row, pin 5 (P0.19), next to SCL |
| XDA, XCL, AD0, INT | Leave unconnected |

**Pins 5 and 6 are always the middle pair** of a 10-pin socket (pairs go
1-2, 3-4, 5-6, 7-8, 9-10), so put SDA and SCL on the two middle pins. If the
LCD says MPU6050 NOT FOUND, just swap those two wires.

**Finding pin 1 on CNC (only matters for pin 10):** the 10-pin box connector has a small
triangle or "1" next to pin 1. Pins are numbered in a zigzag: 1 and 2 face
each other, then 3 and 4, and so on, with pin 10 at the far end. Check the
CNC picture in Appendix C of the lab manual (or ask the staff) to confirm
that pins 5 and 6 are P0.19 and P0.20.

### If you use the ADXL335 (GY-61)

The GY-61 has 5 pins (VCC, X, Y, Z, GND; some have a 6th, ST). You only use 3.

| GY-61 pin | Goes to on the LPC1768 board |
|---|---|
| VCC | **TP2, marked "3.3V"**, in the POWER SUPPLY box at the bottom right |
| GND | **TP3, marked "GND"**, next to TP2 |
| Z | The **middle pin of JP18** (in the RS485 box, left side of the board). Take the JP18 jumper cap off first and keep it safe |
| X, Y, ST | Leave unconnected |

**Finding the right JP18 pin:** before wiring the sensor, put the JP18
jumper on pins 2 and 3 (the manual's setting for POT4), flash, and turn POT4.
The logger readings should change. Then take the jumper off and put the Z
wire on pin 2. Flick: if the readings follow the flick, that's the pin. If
not, try pin 3.

**If JP18 doesn't work at all:** put the Z wire on CND pin 1 (P0.23,
AD0.0), set `ADC_CH 0` and `USE_LCD 0` in `board/config.h`, and read the
results on the laptop only. The LCD uses CND, so it can't be connected.

### For both sensors

![Where everything is on the lab board](img/board_annotated.png)

TP2 and TP3 are small test-point posts. If a female jumper doesn't grip, use a small crocodile-clip lead instead.

**Wire routing:**
- Tape the 4 wires along the ruler with small pieces of insulation tape,
  running back toward the clamp.
- Leave a loose loop of wire between the clamp and the board, so the wires
  never pull on the beam.
- Tape the wires down on the table just behind the clamp, so the hanging
  part doesn't swing.
- Keep the wiring exactly the same for all tests. Wires add a little weight.

**Other board connections** (same as your lab experiments):
- LCD: FRC cable from CND to CNAD, short JP16 and JP5. Use POT3 if the text
  is invisible.
- Keypad: FRC cable from CNB to CNB3, short JP4 (1,2).
- PC: DB9 cable to CN9, plus the board's power cable.

## Part 4: Load the code

1. In Keil uVision 4, create a project for the NXP LPC1768, as in Lab 6.
   Add `system_LPC17xx.c`, plus `board/main.c`, `hw.c`, `features.c` and
   `classify.c`. Tick "Create HEX file" and build.
2. Flash the HEX with Flash Magic, as in Lab 6.
3. **Close Flash Magic** (it holds the COM port), then press reset on the
   board.
4. Look at the LCD:
   - **DAMAGE DETECTOR / Flick the ruler** means the sensor is talking to
     the board. Go on to Part 5.
   - **MPU6050 NOT / CHECK WIRING** means no reply. Swap the SDA and SCL
     wires, check VCC and GND, and press reset. If it still fails, try the
     other two CNC pins next to them.
   - With the **ADXL335** the LCD always says "Flick the ruler", because an
     analog sensor can't answer back. Check it by flicking: the logger
     should show readings that swing up and down.

## Part 5: First flick

Flick by pressing the tip down about 1 to 2 cm with one finger, then sliding
your finger off sideways. Do it the same way every time. The board records
for about 10 seconds, then shows the answer and the health score on the LCD.
Don't touch the beam or the table while it records.

It will guess using the practice model trained on fake data, so don't trust
the answer yet. You're only checking that something appears.

## Part 6: Record real data

On the laptop, with the board connected (change `COM5` to your port, which
is under Ports in Device Manager):

```
python pc/logger.py --port COM5 --label healthy --step 0 --count 25
```
Flick, wait until the logger prints that it saved the flick (about 12
seconds), then flick again, until it reaches the count. Then make each
change by hand and
record it, putting the beam back to healthy after each fault:

| Condition | What to do by hand | Command (label, step, count) |
|---|---|---|
| Healthy | Nothing: clamp tight, 3 clips on | `healthy 0 25` |
| Coin | Tape a coin on ruler B next to the sensor | `added_mass 1 25` then remove the coin |
| Loose joint 1 | Remove the middle clip | `loose_joint 1 15` |
| Loose joint 2 | Also remove one end clip (1 left) | `loose_joint 2 12` then put both back |
| Loose clamp 1 | Loosen the clamp a 1/4 turn | `loose_clamp 1 12` |
| Loose clamp 2 | Loosen to 1/2 turn | `loose_clamp 2 12` |
| Loose clamp 3 | Loosen to 3/4 turn (beam still held) | `loose_clamp 3 10` then tighten again |

Each row means
`python pc/logger.py --port COM5 --label <label> --step <step> --count <count>`.

After every fault, re-tighten the clamp along your pen marks and refit the
clips before the next row.

## Part 7: Train on the real data and put it on the board

```
python pc/build_dataset.py real
python pc/train.py real
python pc/check_c_parity.py real
```
`train.py` writes the new `board/model.h`. Rebuild in Keil, flash again,
close Flash Magic and press reset. The board now uses models trained on your
beam.

Then make the trend graph and report:
```
python pc/trend.py real --fault loose_clamp --upto 2
python pc/llm_report.py real
```

## Part 8: Demo

1. Show a healthy flick: the LCD says HEALTHY with a high health score.
2. Loosen the clamp or remove clips and flick: the LCD names the fault and
   the score drops.
3. Tape on the coin: it says ADDED MASS, not damage, and the health score
   stays high. This is the point where "frequency only" gets fooled and the
   ML doesn't.
4. Press **A** and teach it a new condition in 5 flicks. Press **C** to
   recalibrate the healthy baseline in 10 flicks.
5. Show the plots in `results/real/` and the AI-written inspection report.

## If something goes wrong

| Problem | Fix |
|---|---|
| LCD blank | Turn POT3. Check the CND to CNAD cable and the JP16 and JP5 jumpers |
| MPU6050 NOT FOUND | Swap SDA and SCL, check VCC and GND, press reset |
| ADXL335 readings never change | Wrong JP18 pin, or the jumper is still on. Redo the POT4 check |
| Board does nothing after flashing | Close Flash Magic and press reset (it can hold the board in reset) |
| Logger shows nothing | Close Flash Magic, check the COM port number, baud 115200 |
| Flick doesn't start a recording | Flick a bit harder. The trigger needs a 0.3 g jump |
| Readings all the same number | The sensor is loose or a wire came off. Check the tape and wires |
| Results jump around a lot | Flick the same way every time, and check the clamp and clips haven't moved |
