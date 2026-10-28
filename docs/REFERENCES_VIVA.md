# References and viva brief

Eight references are enough. Each one below says what the paper is about,
which part of our project it supports, and the formula it backs.

## Reference list (IEEE style, paste into the report)

[1] S. W. Doebling, C. R. Farrar, M. B. Prime and D. W. Shevitz, "Damage identification and health monitoring of structural and mechanical systems from changes in their vibration characteristics: A literature review," Los Alamos National Laboratory, Rep. LA-13070-MS, 1996.

[2] O. S. Salawu, "Detection of structural damage through changes in frequency: A review," *Engineering Structures*, vol. 19, no. 9, pp. 718-723, 1997.

[3] C. R. Farrar and K. Worden, "An introduction to structural health monitoring," *Philosophical Transactions of the Royal Society A*, vol. 365, no. 1851, pp. 303-315, 2007.

[4] O. Avci, O. Abdeljaber, S. Kiranyaz, M. Hussein, M. Gabbouj and D. J. Inman, "A review of vibration-based damage detection in civil structures: From traditional methods to Machine Learning and Deep Learning applications," *Mechanical Systems and Signal Processing*, vol. 147, 107077, 2021.

[5] K. Worden, G. Manson and N. R. J. Fieller, "Damage detection using outlier analysis," *Journal of Sound and Vibration*, vol. 229, no. 3, pp. 647-667, 2000.

[6] J. W. Cooley and J. W. Tukey, "An algorithm for the machine calculation of complex Fourier series," *Mathematics of Computation*, vol. 19, no. 90, pp. 297-301, 1965.

[7] T. Cover and P. Hart, "Nearest neighbor pattern classification," *IEEE Transactions on Information Theory*, vol. 13, no. 1, pp. 21-27, 1967.

[8] S. S. Rao, *Mechanical Vibrations*, 6th ed. Pearson, 2017.

Optional extras if the guide wants more on signal processing:
[9] F. J. Harris, "On the use of windows for harmonic analysis with the discrete Fourier transform," *Proceedings of the IEEE*, vol. 66, no. 1, pp. 51-83, 1978.
[10] E. Jacobsen and P. Kootsookos, "Fast, accurate frequency estimators," *IEEE Signal Processing Magazine*, vol. 24, no. 3, pp. 123-125, 2007.

## What each paper says, in two lines

- **[1] Doebling 1996.** The classic survey that started vibration-based damage detection. Damage changes stiffness, mass or damping, so it changes the natural frequencies, mode shapes and damping you can measure. *Our whole idea comes from this.*
- **[2] Salawu 1997.** Reviews using only the frequency shift to find damage. Its main warning: frequency changes are small and can also be caused by things that are not damage (mass, temperature). *This is why our "frequency only" baseline fails on the support, and why we added the coin test.*
- **[3] Farrar and Worden 2007.** Defines SHM as a pattern-recognition problem in steps: is there damage, what type, how severe. *Our board does all three: CHANGED alarm, fault class, health score.*
- **[4] Avci et al. 2021.** Review of how ML replaced hand-set thresholds in vibration damage detection: feature extraction followed by a classifier (kNN, trees, neural nets). *Justifies our pipeline: 5 features followed by tree / kNN / small neural net.*
- **[5] Worden et al. 2000.** Train only on the healthy structure, then flag anything that is a statistical outlier from it (Mahalanobis distance). *Our change detector and UNKNOWN detector are a simplified version of this.*
- **[6] Cooley and Tukey 1965.** The FFT algorithm. Computes the spectrum in N log2 N steps instead of N squared. *We wrote our own radix-2 FFT in C from this.*
- **[7] Cover and Hart 1967.** The nearest-neighbour rule: label a new point like its closest stored examples; error is at most twice the best possible. *Our kNN classifier, kNN health score and teach mode.*
- **[8] Rao, Mechanical Vibrations.** Textbook source for the cantilever frequency formula, damping ratio and logarithmic decrement.

## Formulas you may be asked about

**1. Why the frequency changes (spring-mass model) [8]**
f = (1 / 2π) √(k / m)
Less stiffness k (loose joint) or more mass m (coins) lowers f. Our coins dropped f1 by 22 %, the loose joint by about 6 %.

**2. Cantilever natural frequency, Euler-Bernoulli beam [8]**
f1 = (1.875² / 2π) √(E I / (ρ A L⁴))
E = Young's modulus of steel, I = second moment of area of the ruler, ρA = mass per length, L = free length, 1.875 = first root of the cantilever equation. It shows f falls with 1/L², which is why our longer beam today (35.5 cm) rang lower than yesterday's.

**3. Hann window [9]**
w[n] = 0.5 - 0.5 cos(2πn / (N - 1))
Tapers the 10 s recording to zero at both ends so the FFT peak does not smear into neighbouring bins (spectral leakage).

**4. FFT and resolution [6]**
X[k] = Σ x[n] e^(-j2πkn/N), and Δf = Fs / N = 100 / 1024 = 0.098 Hz per bin.
N = 1024 is a power of 2, so the radix-2 FFT works. Measured on the LPC1768: 117 ms.

**5. Parabolic peak interpolation [10]**
δ = ½ (α - γ) / (α - 2β + γ), f1 = (k + δ) Δf
β is the biggest bin, α and γ its neighbours. A parabola through the 3 points finds the true peak between bins. On our data: 0.009 Hz average error vs 0.027 Hz without it.

**6. Damping ratio from the decay envelope [8]**
The vibration dies as A(t) = A0 e^(-ζ ω t), with ω = 2π f1. Taking logs: ln A = ln A0 - ζ 2π f1 t, a straight line.
So ζ = -(slope of ln A vs t) / (2π f1).
We fit that line to the RMS envelope (0.25 s windows). It is the same physics as the textbook logarithmic decrement δ = ln(x_n / x_n+1) ≈ 2πζ, just using every cycle instead of two. A loose clamp raised ζ from 2.9 % to 3.45 %.

**7. Change detector (z-score from healthy) [5]**
z = √( (1/5) Σ ((x_i - μ_i) / σ_i)² ), alarm if z > 3
μ and σ come from the healthy flicks (or from self-calibration with key C). 3 standard deviations is the usual outlier rule. [5] uses the full Mahalanobis distance; we use the diagonal version because it is cheap on the chip.

**8. kNN classification and health score [7]**
Distance d = √Σ (x_i - y_i)² on standardised features (x - mean) / std.
Class = majority of the 3 nearest stored flicks.
Health = 100 × (1 - average damage level of the 3 nearest), damage levels from our table (healthy 0, loose joint 0.5 / 1.0, loose support up to 0.75, coin 0).

**9. Decision tree (CART, Gini impurity) [4]**
Gini = 1 - Σ p_c². Each split picks the feature and threshold that lowers Gini most. Ours, depth 3, uses f1 for 71 % of the decisions and damping for 27 %.

**10. Neural network 5-8-4 [4]**
h = ReLU(W1 x + b1), p = softmax(W2 h + b2), with ReLU(v) = max(0, v) and softmax p_c = e^(z_c) / Σ e^(z_j).
84 weights, under 1 KB on the board. The highest p_c is the answer and its value is the confidence.

**11. Trend to unsafe (linear regression)**
health = a + b × step, fitted by least squares; unsafe step = (40 - a) / b. Predicted 3.6 from steps 0 to 2; the real beam was unsafe at step 3.

## Likely viva questions, short answers

- *Why not just use frequency?* [2] warns it is not enough, and our data proves it: loosening the clamp did not change f1 at all, only damping. Frequency-only got 84 %, the neural net 94 %.
- *Why does the coin not count as damage?* It adds mass, not a loss of stiffness, so it is a harmless change. The model has a separate ADDED MASS class with damage 0.
- *Why 100 Hz sampling?* The beam rings at 3.6 to 4.8 Hz; Nyquist needs more than 2 × that. 100 Hz with the MPU6050's 44 Hz filter avoids aliasing and gives 10 s per flick.
- *Why did loose support steps 1 and 2 fail?* The screw G-clamp still grips firmly until about 3/4 turn, so damping stayed inside the healthy spread. We report that as the detection limit.
- *Is the ML really on the chip?* Yes. Training is on the laptop; the trained tree, network weights and kNN data are exported to `model.h` and the board runs them. The C and Python answers match on all 89 flicks.
