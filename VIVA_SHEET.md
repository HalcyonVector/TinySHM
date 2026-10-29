# My viva notes: TinySHM

These are my own notes for the viva. Reference numbers [1] to [15] match our paper.

---

## Part 0. The facts I must not mix up

**Two different "100" numbers.** This is the one I keep tripping on.
- **100 MHz is the CPU clock.** The LPC1768 runs at 100 million cycles per second, so one cycle is 10 ns. The peripherals get PCLK = CCLK/4 = 25 MHz.
- **100 Hz is the sampling rate.** I take 100 accelerometer readings per second, one every 10 ms. Timer0 makes this happen: it counts at 25 MHz, and I set its match register to 25,000,000 / 100 - 1 = 249,999. Every time it reaches that value it interrupts and I read the sensor.
- The block diagram now says "CPU clock 100 MHz" and "Timer0: sample every 10 ms" so nobody reads it wrong.

**What "±4 g" means.**
- g is the acceleration of gravity, 9.81 m/s². The MPU6050 lets me pick a measuring range of ±2, ±4, ±8 or ±16 g.
- I chose ±4 g, which means it can measure accelerations up to 4 times gravity in either direction (about ±39 m/s²). Anything bigger would clip.
- The output is a 16-bit signed number (-32768 to +32767). Spread over ±4 g, that gives 32768 / 4 = **8192 counts per g**, so one count is about 0.12 mg.
- Why not ±2 g? A hard flick on the tip can go past 2 g and clip, which would spoil the damping estimate. Why not ±8 g? I would lose resolution for nothing, because my flicks never get near 8 g.

**Other hardware numbers.**
- LPC1768: ARM Cortex-M3, 100 MHz, 512 KB flash, 64 KB SRAM, no floating point unit. Board: ALS-SDA-ARMCTXM3-01.
- Keil uVision free version limits code to 32 KB. Our firmware is about 20 KB. The FFT buffers use 8 KB of RAM.
- The MPU6050 is wired to two plain GPIO pins on the CNC connector (SDA pin 5, SCL pin 6), so I drive I2C in software (bit-banging). The bus runs at about 15 kHz because the long jumper wires failed at faster speeds.
- MPU6050 internal digital low-pass filter: 44 Hz. It removes anything above my Nyquist limit of 50 Hz so it cannot alias.
- UART0 at 19200 baud to the laptop. Our USB-serial cable failed above that.
- 16×2 LCD shows class and health, 8 LEDs show a health bar, 4×4 keypad selects calibrate (key C), teach mode and display pages.
- One recording: N = 1024 samples at 100 Hz = 10.24 s. Trigger: acceleration moves more than 0.3 g from rest.

**The beam.**
- Two 30 cm steel rulers. Ruler A sticks out 18.5 cm past the table edge, held by a 10 inch screw G-clamp over a cardboard pad.
- Ruler B overlaps A by 13.5 cm and is held by three binder clips. Free length is about 35.5 cm.
- MPU6050 taped near the tip (about the 26 cm mark). The healthy beam rings at about 4.74 Hz.
- No drilling or cutting. Anyone can rebuild it.

**The faults.**
- Added mass: a Rs 20 and a Rs 5 coin taped at the 28 to 30 cm mark. This is a change but NOT damage.
- Loose joint 1: middle clip removed. Loose joint 2: middle clip and one end clip removed.
- Loose support: G-clamp screw loosened by about 1/4, 1/2 and 3/4 turn. Past 3/4 turn the beam falls off.

**The data.**
- 89 flicks in total. 68 for training, the rest held out (the 1/4 and 1/2 turn flicks plus 5 healthy re-check flicks).
- I threw away 24 flicks recorded while the ruler overlap had slipped to 15 cm instead of 13.5 cm. That had moved the healthy frequency from 4.7 to 5.0 Hz, so those flicks were not comparable.

---

## Part 1. Every paper and how I used it

### Why vibration tells me about damage

**[1] Gharehbaghi et al., 2022, Springer (Arch. Comput. Methods Eng.)**
- A big review that defines SHM levels: is there damage, where is it, what type, how bad, and how much life is left. It compares physics-model methods with data-driven (ML) methods.
- How I used it: my board answers "is there damage" (CHANGED alarm), "what type" (fault class) and "how bad" (health %). The trend line is a simple version of "how much life is left".

**[2] Hou and Xia, 2021, Elsevier (J. Sound and Vibration)**
- Reviews ten years of vibration-based damage detection and groups methods by feature: frequency, mode shape, damping, frequency response. Its main point is that frequency is the easiest to measure but tells you least, and harmless things like temperature or extra mass also shift it.
- How I used it: it is the physics behind formula 1, and it is why I tested the coins separately as a harmless change.

**[3] Avci et al., 2021, Elsevier (Mech. Syst. Signal Process.)**
- Shows how the field moved from hand-set thresholds to ML. The standard structure is feature extraction followed by a classifier.
- How I used it: my pipeline follows that structure. Five features, then tree, kNN and neural network.

### Machine learning for SHM

**[4] Flah et al., 2021, Springer (Arch. Comput. Methods Eng.)**
- A systematic review of supervised and unsupervised ML in civil SHM. The biggest problem it names is the lack of labelled data from real damaged structures.
- How I used it: it is why I picked small models that work with 68 flicks, and why I added a novelty detector for faults I never trained on.

**[5] Bao and Li, 2021, SAGE (Structural Health Monitoring)**
- Argues ML is a new paradigm for SHM, covering damage identification, bad-data detection and condition assessment.
- How I used it: it supports giving a health score (condition assessment), not just yes or no.

**[6] Malekloo et al., 2022, SAGE (Structural Health Monitoring)**
- An overview of ML for SHM with new technology such as low-cost MEMS sensors and IoT nodes.
- How I used it: it supports my choice of a cheap MEMS accelerometer instead of a lab-grade sensor.

**[10] de Sousa et al., 2023, Springer (J. Vib. Eng. Technol.)**
- The closest paper to mine. They damaged cantilever beams and compared kNN, SVM, decision tree, random forest and naive Bayes. With only frequency features they got about 59 % to 94 %. With richer features they got about 95 % on real data.
- How I used it: same conclusion as mine, frequency alone is not enough. My frequency-only rule got 84.2 % and my neural network got 93.8 %. The difference is that they ran on a PC and I run on the microcontroller.

### My specific fault and my signal processing

**[11] Huang et al., 2022, Elsevier (Mech. Syst. Signal Process.)**
- Reviews every way of detecting loose bolts and fasteners. The key point for me: looseness changes stiffness and also adds energy loss through friction at the contact.
- How I used it: it explains why loosening the clamp did not change frequency but raised damping by 18 %.

**[12] Belega and Petri, 2021, IEEE (I2MTC)**
- Shows that fitting a parabola through the three biggest FFT points around a peak gives a much better frequency than the biggest bin alone.
- How I used it: formula 5. My error dropped from 0.027 Hz to 0.009 Hz.

### SHM on small chips (TinyML)

**[7] Zonzini et al., 2020, IEEE (Instrum. Meas. Magazine)**
- A sensor-to-cloud SHM system where MEMS sensor nodes pre-process data before sending it.
- How I used it: motivation for processing at the sensor. I go further and make the whole decision on the board.

**[8] Ray, 2022, Elsevier (J. King Saud Univ.)**
- A review of TinyML: ML on microcontrollers with only kilobytes of memory.
- How I used it: it defines the TinyML idea behind my project.

**[9] Capogrosso et al., 2024, IEEE Access**
- A TinyML survey from the ML side: shrinking models and the "train on PC, deploy on chip" workflow.
- How I used it: that is exactly my workflow. Train in Python, export to `model.h`, flash the board.

**[13] Burrello et al., 2022, IEEE (AICAS)**
- Tiny autoencoders near the sensor that flag abnormal vibration.
- How I used it: comparison. Theirs only says "anomaly". Mine names the fault, gives a health score and is explainable.

**[14] Moallemi et al., 2023, IEEE Sensors Letters**
- Runs system identification on a multi-core RISC-V microcontroller.
- How I used it: comparison. They need a powerful parallel chip. I use a basic single-core Cortex-M3 with no FPU.

**[15] Capello et al., 2025, Springer (LNEE)**
- Uses neural architecture search to fit SHM models onto small devices.
- How I used it: comparison. I hand-designed a 5-8-4 network with only 84 weights.

**My answer to "what is new?"** Edge SHM papers [13] to [15] use strong chips and only detect anomalies. Beam ML papers like [10] run on a PC. I do fault type, health score and a change alarm on a basic Cortex-M3, on real data, and I showed the board gives the same answer as the PC on all 89 flicks.

---

## Part 2. Every formula: what it does and where it comes from

Some of these are older than 2020, so their original source is not in our reference list. I can mention the original source out loud.

### 1. Natural frequency of a spring and mass
f = (1 / 2π) √(k / m)
- What it does: frequency goes down if stiffness k drops (loose joint) or mass m rises (coins).
- Where it comes from: Newton's second law. For a mass on a spring, m x'' + k x = 0. The solution is a sine wave with ω = √(k/m), and f = ω / 2π. Any vibration textbook (S. S. Rao, *Mechanical Vibrations*). In the paper I cite it through [2].
- My result: coins lowered f1 by 22 %, loose joint by about 6 %.

### 2. Cantilever beam frequency (if asked why it rings at about 4.7 Hz)
f1 = (1.875² / 2π) √(E I / (ρ A L⁴))
- What it does: frequency of a beam clamped at one end. It falls with L², so length matters a lot.
- Where it comes from: the Euler-Bernoulli beam equation. With one end clamped and one free, the first root of cos(βL) cosh(βL) = -1 is βL = 1.875.
- My note: my beam is two overlapping rulers, so it does not match exactly. That is one reason I learn from measured data instead.

### 3. Hann window
w[n] = 0.5 - 0.5 cos(2πn / (N - 1))
- What it does: fades the recording to zero at both ends so the peak does not smear into neighbouring bins (spectral leakage).
- Where it comes from: a raised cosine named after Julius von Hann, made popular by Blackman and Tukey (1958) and Harris (Proc. IEEE, 1978).

### 4. FFT and bin width
X[k] = Σ x[n] w[n] e^(-j2πkn/N), Δf = Fs / N = 100 / 1024 = 0.098 Hz
- What it does: turns 1024 time samples into a spectrum. Each bin is 0.098 Hz wide.
- Where it comes from: the Discrete Fourier Transform. Δf = Fs / N because the record lasts N / Fs = 10.24 s, and you can only separate frequencies 1 / 10.24 s apart. The fast radix-2 algorithm is Cooley and Tukey (1965), which is why N must be a power of 2.
- My result: 117 ms on the LPC1768.

### 5. Parabolic peak interpolation [12]
δ = ½ (α - γ) / (α - 2β + γ), f1 = (k_p + δ) Δf
- What it does: finds the true peak between two bins.
- Where it comes from: fit a parabola through the three points (-1, α), (0, β), (+1, γ), where β is the biggest bin. Solving for the top of the parabola gives δ.
- My result: 0.009 Hz average error, against 0.027 Hz for the plain peak bin. The reference was a 16× zero-padded FFT (1024 samples padded to 16384 points).

### 6. Damping ratio from the decay
A(t) = A0 e^(-ζ ω1 t) → ln A(t) = ln A0 - ζ (2π f1) t → ζ = -s / (2π f1)
- What it does: measures how fast the ringing dies. Higher ζ means more energy lost, for example by friction at a loose clamp.
- Where it comes from: add a damper to formula 1: m x'' + c x' + k x = 0. The solution is a sine inside a decaying exponential. Taking the log of the envelope gives a straight line with slope s = -ζω1. I fit that line over 0.25 s RMS windows (formula 14). The two-peak textbook version is the logarithmic decrement; mine uses many windows so it is less noisy.
- My result: clamp at 3/4 turn raised damping from 2.92 % to 3.45 % (+18 %).

### 7. Energy ratio
E_r = (energy in 3 to 40 Hz band minus energy within ±1 Hz of the peak) / energy in band
- What it does: how much of the vibration is not the main ring, for example rattle and harmonics.
- Where it comes from: my own feature, based on Parseval's theorem (signal energy equals the sum of |X[k]|²).

### 8. Standardising features
x̃ = (x - μ) / σ
- What it does: puts Hz, %, g and seconds on the same scale so they can share one distance.

### 9. Change detector
z = √( (1/5) Σ ((x_i - μ_i^h) / σ_i^h)² ), alarm (CHANGED) if z > 3
- What it does: how far this flick is from the healthy baseline, in standard deviations.
- Where it comes from: a simplified Mahalanobis distance (1936) that ignores correlations, so it is cheap on the chip. Using it for damage is outlier analysis (Worden et al., J. Sound Vib., 2000). The 3 comes from the 3-sigma rule.
- The baseline can be recalibrated on the board: press C and give ten healthy flicks.

### 10. Decision tree (Gini impurity)
G = 1 - Σ p_c²
- What it does: each split picks the feature and threshold that make the groups as pure as possible.
- Where it comes from: CART, Breiman et al. (1984). Depth 3, exported as nested if statements.
- My result: f1 used for 71 % of splits, damping for 27 %.

### 11. kNN
d = √Σ (x̃_i - ỹ_i)², class = majority of the 3 nearest flicks
- Where it comes from: Fix and Hodges (1951), Cover and Hart (1967). The 68 training flicks are stored on the chip, and teach mode lets me add more.

### 12. Neural network 5-8-4
h = max(0, W1 x̃ + b1), p = softmax(W2 h + b2)
- What it does: 5 inputs, 8 hidden ReLU neurons, 4 outputs. Softmax turns outputs into probabilities. The biggest is the answer and its value is the confidence.
- Where it comes from: multilayer perceptron with backpropagation (Rumelhart, Hinton and Williams, 1986). Weights: 5×8 + 8 + 8×4 + 4 = 84.

### 13. Health score
H = 100 × (1 - average damage level of the 3 nearest flicks); below 40 = UNSAFE
- Damage levels I assigned: healthy 0, coins 0, loose joint 0.5 and 1.0, loose support up to 0.75.
- My result: average error 5.6 points (straight-line regression: 17.8). Healthy flicks averaged 95.8 and none fell below 40.

### 14. Least-squares line (used for the damping slope and the trend)
b = Σ (t - t̄)(y - ȳ) / Σ (t - t̄)², a = ȳ - b t̄, unsafe step = (40 - a) / b
- Where it comes from: Legendre (1805) and Gauss (1809).
- My result: health over support steps was 97, 66, 69, 33. The line through steps 0 to 2 predicted unsafe at step 3.6; it was really unsafe at step 3.

### 15. Novelty detector
UNKNOWN if the nearest stored flick is farther than 2 × the 95th percentile of nearest-neighbour distances in training.
- What it does: says "I have not seen this before" instead of forcing a wrong label.

### 16. Sampling rule (if asked "why 100 Hz?")
Fs > 2 f_max (Nyquist and Shannon)
- The beam rings at 3.6 to 4.8 Hz, and I look at 3 to 40 Hz. 100 Hz is well above twice that, and the 44 Hz filter stops aliasing.

### 17. Cross-validation (if asked "how did you test accuracy?")
- 5-fold stratified, repeated 5 times on the 68 training flicks. Train on 4 parts, test on 1, rotate, reshuffle, average. Stratified means each part keeps the same class mix.

---

## Part 3. Questions I expect, and what I will say

- **Why not just use frequency?** Because loosening the clamp did not change f1 at all (4.75 Hz against 4.74 Hz), only damping. Frequency-only got 84.2 %, the neural network got 93.8 %.
- **Why is the coin not damage?** It adds mass but removes no stiffness. So it is its own class with damage level 0. It shows the system does not panic at a harmless change.
- **Why did 1/4 and 1/2 turn fail?** The screw clamp keeps gripping until about 3/4 turn, so damping stayed inside the healthy range. I report this as my detection limit instead of hiding it. On those held-out flicks the neural network still flagged 4 of 8 and 3 of 8 as loose support.
- **Is the ML really on the chip?** Yes. Training happens on the laptop. The tree, network weights and kNN data are exported to `model.h` and compiled into the firmware. I ran the same C code on the PC and it matched Python on all 89 flicks.
- **How long does one decision take?** The recording itself is 10.24 s. The FFT takes 117 ms, so processing is tiny in comparison.
- **Why not zero-crossing counting? It is faster.** It is 28 times faster (4.2 ms), but its error is 0.34 Hz, bigger than the 0.27 Hz shift a loose joint causes. So it cannot tell the classes apart.
- **What went wrong during the experiment?** After a loose-joint test I re-clipped the rulers with 15 cm overlap instead of 13.5 cm. The healthy frequency jumped from 4.7 to 5.0 Hz, so my loose-support readings looked healthy. I caught it with a healthy re-check, discarded those 24 flicks and redid the tests at 13.5 cm. After all faults, the healthy check read 4.68 Hz against the original 4.74 Hz.
- **Why 100 MHz and 100 Hz, are those the same thing?** No. 100 MHz is how fast the CPU runs. 100 Hz is how often I read the sensor. See Part 0.
- **What does ±4 g mean?** The accelerometer can measure up to 4 times gravity in either direction, at 8192 counts per g. See Part 0.
- **Limitations?** 89 flicks from one beam in one room, flicks by hand, only the first mode, and no temperature study. Next steps: a mechanical exciter, temperature compensation, a second sensor for higher modes, and data over several days.
- **What would I improve first?** More data on early support loosening, because that is where my system is weakest.

## Part 4. What changed for the demo after the report

The report describes the model trained on the first lab day. For the demo I changed three things. If asked, I say the report is the study, and these are improvements I made after it.

- **Why a new board reads lower.** The lab gave us a different LPC1768. On the same beam it reads about 4.1 to 4.4 Hz instead of 4.74 Hz. I checked the clock (6000 ticks in 60 s, so it is correct), so the cause is something in the board or sensor wiring, not the timing.
- **Calibration is now the baseline.** I press C and flick 10 times. The board takes the median frequency and damping of those 10 flicks as healthy. Every later flick is compared to that: frequency as a ratio (f1 / f1 healthy) and damping as a difference (damping minus healthy damping). So a healthy beam always reads 1.0 and 0.0, whatever board or day it is. No retraining is needed in the lab.
- **Only two features now.** The demo model uses just the frequency ratio and the damping difference, so the network is 2-8-4 instead of 5-8-4. Trained on both lab days (131 flicks): kNN 96 %, network 93 %, tree 88 %, health error 4.3 points. The board and the laptop still give identical answers on every flick.
- **What the health reads.** Healthy about 100. Coin about 85: a beam is meant to carry some load, so the coin lowers health only slightly (in the report I treated it as damage 0). Loose joint 1 about 50, loose joint 2 about 0.
- **The clamp and the B key.** Loosening the clamp does not change frequency, so the board cannot see it by itself. I press B to mark the beam as loose: health is capped at 50 %, and frequency can still lower it further. D clears the mark and frequency decides again. B and D do not touch the calibration. I am honest that B is an operator mark, not a detection.
