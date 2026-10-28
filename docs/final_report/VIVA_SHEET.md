# Viva sheet: papers and formulas

This matches the final report (`paper.pdf`). Reference numbers [1] to [15] are the same as in the paper.

---

## Part 1. Every paper and what it does

### Group A. Why vibration tells you about damage

**[1] Gharehbaghi et al., 2022, Springer (Arch. Comput. Methods Eng.)**
- **What it does:** a big review that defines SHM properly. It splits SHM into levels: (1) is there damage, (2) where, (3) what type, (4) how bad, (5) how long is left. It compares physics-model methods with data-driven (ML) methods.
- **Where we use it:** our board answers levels 1, 3 and 4 (CHANGED alarm, fault class, health %), and the trend line is a simple level 5.

**[2] Hou and Xia, 2021, Elsevier (J. Sound and Vibration)**
- **What it does:** reviews 10 years (2010 to 2019) of vibration-based damage identification. It groups methods by feature: natural frequency, mode shape, damping, frequency response. Main point: frequency is the easiest to measure but tells you least about the type of damage, and temperature or added mass can shift it just like damage.
- **Where we use it:** the physics behind formula 1 (f falls when stiffness drops or mass rises), and the reason we tested a coin (harmless change) separately.

**[3] Avci et al., 2021, Elsevier (Mech. Syst. Signal Process.)**
- **What it does:** shows how the field moved from hand-set thresholds to ML and deep learning. The standard structure is: extract features from the signal, then feed them to a classifier.
- **Where we use it:** justifies our pipeline: 5 features, then tree / kNN / neural net.

### Group B. Machine learning for SHM

**[4] Flah et al., 2021, Springer (Arch. Comput. Methods Eng.)**
- **What it does:** a systematic review of ML algorithms used in civil SHM (supervised and unsupervised). Biggest problem it identifies: there is very little labelled data from real damaged structures.
- **Where we use it:** explains why we used small, simple models (tree, kNN, tiny NN) that work with 68 training flicks, and why we added a novelty detector for faults we never trained on.

**[5] Bao and Li, 2021, SAGE (Structural Health Monitoring)**
- **What it does:** argues ML is a new "paradigm" for SHM, covering damage identification, finding bad sensor data, and condition assessment.
- **Where we use it:** supports the idea of giving a health score (condition assessment), not just a yes/no answer.

**[6] Malekloo et al., 2022, SAGE (Structural Health Monitoring)**
- **What it does:** overview of ML for SHM together with new technology: low-cost MEMS sensors, IoT nodes, drones, big data.
- **Where we use it:** supports using a cheap MEMS accelerometer (MPU6050) instead of lab-grade sensors.

**[10] de Sousa et al., 2023, Springer (J. Vib. Eng. Technol.)**
- **What it does:** the closest paper to ours. They put damage on cantilever beams and compared 5 classifiers (kNN, SVM, decision tree, random forest, naive Bayes). Using only natural-frequency features, accuracy was about 59 % to 94 %; using richer features it reached about 95 % on real data.
- **Where we use it:** same conclusion as ours: frequency alone is not enough. Our frequency-only rule got 84 %, our ML got 94 %. Difference: they run on a PC, we run on the microcontroller.

### Group C. Our specific fault and our signal processing

**[11] Huang et al., 2022, Elsevier (Mech. Syst. Signal Process.)**
- **What it does:** reviews all ways to detect loosened bolts and fasteners (vibration, impedance, ultrasound, cameras, tapping sound). Key point for us: a loose joint changes stiffness AND increases energy loss by friction at the contact.
- **Where we use it:** explains why loosening the G-clamp did not change frequency but raised damping by 18 %.

**[12] Belega and Petri, 2021, IEEE (I2MTC conference)**
- **What it does:** shows that fitting a parabola through the 3 biggest FFT points around a peak gives a much more accurate frequency than the biggest bin alone, and analyses the error.
- **Where we use it:** formula 4 (parabolic interpolation). Our error dropped from 0.027 Hz to 0.009 Hz.

### Group D. Running SHM on small chips (edge / TinyML)

**[7] Zonzini et al., 2020, IEEE (Instrum. Meas. Magazine)**
- **What it does:** proposes a sensor-to-cloud SHM system where MEMS sensor nodes do some processing locally before sending data.
- **Where we use it:** motivation for doing processing at the sensor. We go further and do the whole decision on the board.

**[8] Ray, 2022, Elsevier (J. King Saud Univ.)**
- **What it does:** review of TinyML: running ML on microcontrollers with KBs of memory. Covers tools, hardware and challenges.
- **Where we use it:** defines the TinyML idea behind our project.

**[9] Capogrosso et al., 2024, IEEE Access**
- **What it does:** a TinyML survey from the ML side: how to shrink models (quantisation, pruning, small architectures) and the workflow of training on a PC and deploying on a chip.
- **Where we use it:** our workflow is exactly this: train in Python, export to `model.h`, flash to the board.

**[13] Burrello et al., 2022, IEEE (AICAS conference)**
- **What it does:** trains tiny autoencoders that run near the sensor and flag abnormal vibration for SHM.
- **Where we use it:** comparison. Theirs only says "anomaly"; ours names the fault, gives a health %, and is explainable (tree).

**[14] Moallemi et al., 2023, IEEE Sensors Letters**
- **What it does:** runs system identification (AR/ARMA models of the vibration) on a multi-core RISC-V microcontroller, about 60 times faster than single core.
- **Where we use it:** comparison. They need a powerful parallel chip; we use a basic single-core Cortex-M3 with no floating-point unit.

**[15] Capello et al., 2025, Springer (LNEE)**
- **What it does:** uses neural architecture search (automatic design of the network) to fit SHM models onto resource-limited devices.
- **Where we use it:** comparison. We chose a hand-designed 5-8-4 network with only 84 weights instead.

**The gap we fill (say this if asked "what is new"):** edge SHM papers [13]-[15] use powerful chips and only detect anomalies; beam ML papers [10] run on a PC. We do fault type + health score + change alarm on a basic Cortex-M3, on real data, and prove the board gives the same answer as the PC on all 89 flicks.

---

## Part 2. Every formula: what it does and where it comes from

Some formulas are older than 2020, so their original source is not in our reference list (which must be 2020 to 2026). The table says which of our papers uses each one, and where it originally came from. You can mention the original source verbally.

### 1. Natural frequency of a spring-mass system
f = (1 / 2π) √(k / m)

- **What it does:** says frequency goes down if stiffness k goes down (loose joint) or mass m goes up (coin).
- **Where it comes from:** Newton's second law. For a mass on a spring, force = -k x, so m x'' + k x = 0. The solution is a sine wave x = A sin(ωt) with ω = √(k/m), and f = ω / 2π.
- **Source:** any vibration textbook (S. S. Rao, *Mechanical Vibrations*). In our paper cited through [2].
- **Our result:** coins: -22 %. Loose joint: -6 %.

### 2. Cantilever beam frequency (if asked why the ruler rings at about 4.7 Hz)
f1 = (1.875² / 2π) √(E I / (ρ A L⁴))

- **What it does:** frequency of a beam clamped at one end. Longer beam (L) means much lower frequency (falls with L²).
- **Where it comes from:** the Euler-Bernoulli beam equation E I ∂⁴w/∂x⁴ + ρ A ∂²w/∂t² = 0. With one end clamped and one free, the solution must satisfy cos(βL) cosh(βL) = -1, whose first root is βL = 1.875.
- **Source:** Euler and Bernoulli (18th century); textbook: Rao, *Mechanical Vibrations*.
- **Note:** our beam is two overlapped rulers, so it does not match this exactly. That is one reason we use ML on measured data instead of the formula.

### 3. Hann window
w[n] = 0.5 - 0.5 cos(2πn / (N - 1))

- **What it does:** fades the recording to zero at both ends. Without it, the sudden start and end cause "spectral leakage" (the peak smears into neighbouring bins).
- **Where it comes from:** a raised cosine, named after meteorologist Julius von Hann. Made popular for spectra by Blackman and Tukey (1958) and the review by F. J. Harris (Proc. IEEE, 1978).

### 4. FFT and frequency resolution
X[k] = Σ x[n] e^(-j2πkn/N),  Δf = Fs / N = 100 / 1024 = 0.098 Hz

- **What it does:** turns 1024 time samples into a spectrum. Each bin is 0.098 Hz wide.
- **Where it comes from:** the Discrete Fourier Transform (from Fourier series). Δf = Fs/N because the recording lasts T = N/Fs = 10.24 s and the DFT can only separate frequencies 1/T apart. The fast algorithm (N log₂N steps instead of N²) is Cooley and Tukey (1965), radix-2, which is why N must be a power of 2.
- **Our result:** 117 ms on the LPC1768.

### 5. Parabolic peak interpolation [12]
δ = ½ (α - γ) / (α - 2β + γ),  f1 = (k + δ) Δf

- **What it does:** finds the true peak between two bins.
- **Where it comes from:** fit a parabola y = a(x - p)² + c through the three points (-1, α), (0, β), (+1, γ), where β is the biggest bin. Solving the three equations for the vertex p gives exactly δ above. Belega and Petri [12] analyse its accuracy; the classic short note is Jacobsen and Kootsookos (IEEE Signal Processing Magazine, 2007).
- **Our result:** 0.009 Hz average error vs 0.027 Hz without it.

### 6. Damping ratio from the decay
x(t) = A e^(-ζωt) sin(ω_d t)  →  ln A(t) = ln A0 - ζ (2π f1) t  →  ζ = -slope / (2π f1)

- **What it does:** measures how fast the vibration dies out. Higher ζ = more energy lost (for example friction at a loose clamp).
- **Where it comes from:** add a damper to formula 1: m x'' + c x' + k x = 0, with ζ = c / (2√(km)). The solution is a sine inside a decaying exponential. Taking the log of the envelope gives a straight line whose slope is -ζω, so we fit a line (formula 11) and divide. The textbook version using only two peaks is the logarithmic decrement δ = ln(x_n / x_n+1) ≈ 2πζ; ours uses every 0.25 s window, so it is less noisy.
- **Source:** Rao, *Mechanical Vibrations*. Damping as a damage feature: [2], [11].
- **Our result:** clamp at 3/4 turn: 2.92 % → 3.45 % (+18 %).

### 7. Energy ratio
E_r = (energy in 3-40 Hz band - energy within ±1 Hz of the peak) / energy in band

- **What it does:** how much vibration is NOT the main ring (rattle, harmonics).
- **Where it comes from:** our own feature, based on Parseval's theorem (energy of a signal = sum of |X[k]|² in the spectrum).

### 8. Standardisation (z-score of each feature)
x̃ = (x - μ) / σ

- **What it does:** puts all features on the same scale, so Hz and % can be compared in one distance.
- **Where it comes from:** basic statistics (the standard score).

### 9. Change detector
z = √( (1/5) Σ ((x_i - μ_i) / σ_i)² ), alarm if z > 3

- **What it does:** how far this flick is from the healthy baseline, in standard deviations.
- **Where it comes from:** a simplified Mahalanobis distance (P. C. Mahalanobis, 1936) that ignores correlations, so it is cheap on the chip. Using it for damage detection is "outlier analysis" (Worden, Manson and Fieller, J. Sound Vib., 2000). The threshold 3 is the 3-sigma rule: 99.7 % of normal data lies within 3σ.

### 10. Decision tree split (Gini impurity)
G = 1 - Σ p_c²

- **What it does:** each split picks the feature and threshold that makes the groups most "pure" (one class each).
- **Where it comes from:** CART algorithm, Breiman, Friedman, Olshen and Stone, *Classification and Regression Trees* (1984).
- **Our result:** f1 used for 71 % of decisions, damping 27 %.

### 11. kNN and Euclidean distance
d = √Σ (x̃_i - ỹ_i)², class = majority of the 3 nearest

- **What it does:** labels a new flick like its 3 most similar stored flicks.
- **Where it comes from:** Fix and Hodges (1951), proved by Cover and Hart (IEEE Trans. Inf. Theory, 1967): with enough data its error is at most twice the best possible. Distance is Pythagoras in 5 dimensions.

### 12. Neural network 5-8-4
h = ReLU(W1 x̃ + b1), p = softmax(W2 h + b2), ReLU(v) = max(0, v), softmax p_c = e^(z_c) / Σ e^(z_j)

- **What it does:** 5 inputs, 8 hidden neurons, 4 outputs (one per class). Softmax turns outputs into probabilities that add to 1; the biggest is the answer and its value is the confidence.
- **Where it comes from:** multilayer perceptron trained by backpropagation (Rumelhart, Hinton and Williams, Nature, 1986). ReLU: Nair and Hinton (2010). Softmax: Bridle (1990). Trained with scikit-learn's Adam optimiser. 5×8 + 8 + 8×4 + 4 = 84 weights.

### 13. Health score
H = 100 × (1 - average damage level of the 3 nearest flicks)

- **What it does:** turns the class into a 0-100 % health number. Below 40 % = UNSAFE.
- **Where it comes from:** our design, using kNN regression (same idea as formula 11, but averaging a number instead of voting). Damage levels are our labels: healthy 0, coin 0 (not damage), loose joint 0.5 / 1.0, loose support 0.25 / 0.5 / 0.75.
- **Our result:** average error 5.6 points (straight-line regression: 17.8).

### 14. Least-squares line (used twice: damping slope and trend)
slope b = Σ (t - t̄)(y - ȳ) / Σ (t - t̄)², intercept a = ȳ - b t̄; unsafe step = (40 - a) / b

- **What it does:** best straight line through points. For damping, y = ln RMS. For the trend, y = health per step.
- **Where it comes from:** method of least squares, Legendre (1805) and Gauss (1809): choose a, b to minimise the sum of squared errors; setting the derivatives to zero gives these formulas.
- **Our result:** predicted unsafe at step 3.6; real beam unsafe at step 3.

### 15. Sampling rule (if asked "why 100 Hz?")
Fs > 2 f_max (Nyquist-Shannon)

- **Where it comes from:** Nyquist (1928) and Shannon (1949). The beam rings at 3.6 to 4.8 Hz; 100 Hz is far above 2 × 4.8. The MPU6050's 44 Hz internal filter removes anything above 50 Hz so it cannot alias.

### 16. Cross-validation (if asked "how did you test accuracy?")
- Split the 68 training flicks into 5 parts; train on 4, test on 1; rotate; repeat 5 times with different shuffles; average. "Stratified" keeps the class mix the same in each part.
- **Source:** Stone (1974); standard in scikit-learn.

---

## Part 3. Likely questions, short answers

- **Why not just use frequency?** [2] and [10] say it is not enough, and our data proves it: loosening the clamp did not change f1 at all, only damping. Frequency-only 84 %, neural net 94 %.
- **Why is the coin not damage?** It adds mass, it does not remove stiffness. So it is its own class (ADDED MASS) with damage 0.
- **Why did the first two clamp steps fail?** The screw clamp keeps gripping until about 3/4 turn, so damping stayed inside the healthy spread. We report it as the detection limit instead of hiding it.
- **Is the ML really on the chip?** Yes. Training is on the laptop; the tree, NN weights and kNN data are exported to `model.h` and compiled in. C and Python agree on all 89 flicks. About 20 KB of flash.
- **What is new compared to the papers?** See the "gap" line at the end of Part 1.
