# TinySHM shopping list (final, 22 October 2026)

No drilling, cutting or soldering. Everything is clamped, clipped or taped by hand.
Prices are approximate and include GST. Check the price at the shop before you pay.

## 1. Sensor: buy ONE of these (MPU6050 recommended)

| Item | Qty | Approx. price | Notes |
|---|---|---|---|
| **MPU6050 module (GY-521)**, header pins already soldered | 1 | ₹150 to ₹250 | Recommended. Digital (I2C), 16-bit, built-in filter. The code uses it by default |
| *or* ADXL335 module (GY-61), header pins already soldered | 1 | about ₹424 (ElectronicsComp) | Only if you can't get an MPU6050. Then switch `SENSOR` in `board/config.h` and `pc/config.py` |
| Spare of the same sensor | 1 | same as above | Strongly recommended. A sensor can die from a wrong wire, and you won't have time to reorder |

**Before paying, check the photo or ask the seller:** the 8 header pins (MPU6050) or 5 pins (ADXL335) must be soldered on already.

## 2. Wires

| Item | Qty | Approx. price | Used for |
|---|---|---|---|
| Female-to-female jumper wires, 20 cm | 1 pack of 40 | ₹60 to ₹70 | Sensor to board: 4 wires (MPU6050) or 3 wires (ADXL335). Lots of spares |
| Crocodile-clip test leads | 1 pack of 10 | ₹60 to ₹100 | Backup only: in case a jumper won't stay on the TP2 (3.3 V) and TP3 (GND) posts |

A breadboard and male-to-female wires are **not** needed any more. The sensor wires go straight to the board.

## 3. Beam and mounting (stationery or hardware shop)

| Item | Qty | Approx. price | Used for |
|---|---|---|---|
| 30 cm steel ruler, all the same brand | 3 | ₹120 (₹40 each) | Two make the beam, one spare |
| C-clamp (G-clamp), 3 or 4 inch | 1 | ₹150 to ₹250 | Clamps the ruler to the lab table by hand |
| Binder clips, 25 mm or 32 mm | 1 small pack (6 or more) | ₹40 | Join the two rulers (3 clips), spares |
| Thin double-sided tape (not foam tape) | 1 roll | ₹40 | Sticks the sensor to the ruler |
| Insulation tape | 1 roll | ₹20 | Holds the sensor firmly, tapes the wires along the ruler |

## 4. Already have or get from the lab (nothing to buy)

- LPC1768 board (ALS-SDA-ARMCTXM3-01), power cable and DB9 cable
- FRC cables for the LCD (CND to CNAD), keypad (CNB to CNB3) and LEDs (CNA to CNA1)
- PC with Keil uVision 4 and Flash Magic
- A coin (for the added-mass test) and a pen (to mark the clamp position)

## Total

| | Approx. amount |
|---|---|
| MPU6050 + spare | ₹300 to ₹500 |
| Wires | ₹120 to ₹170 |
| Beam and mounting | ₹370 to ₹470 |
| **Total with MPU6050 (incl. spare)** | **about ₹790 to ₹1,140** |
| Total with ADXL335 instead (incl. spare) | about ₹1,340 to ₹1,490 |

Both options stay under the Rs 2,000 limit.
