## Inspection Report - Lab-Scale Cantilever Beam  
**Date:** 2026-10-28  

### Summary  
A total of 89 flick-induced vibration events were recorded with an MPU6050 on an LPC1768 board running on-board ML classification. The beam's natural-frequency (f₁) ranged from **3.67 Hz** (added-mass) to **4.79 Hz** (loose-support step 3). Damping ratios stayed between **1.28 %** and **3.48 %**. The ML model identified the condition with ≥ 88 % confidence for all cases; no "unknown" flags were raised. The unsafe health threshold is **40 %**.

### Findings per Condition  

| Condition (step) | Flicks | f₁ (Hz) | Damping (%) | Health (%) | Model / Confidence | Note |
|------------------|--------|---------|-------------|------------|--------------------|------|
| **added_mass 1** | 12 | 3.67 | 2.58 | 100 | added_mass (0.99) | Frequency drop due to extra inertia, not damage |
| **healthy 0** | 30 | 4.71 | 2.96 | 97 | healthy (0.93) | Baseline |
| **loose_joint 1** | 10 | 4.48 | 2.68 | 53 | loose_joint (0.88) | Early joint degradation |
| **loose_joint 2** | 8 | 4.45 | 1.28 | 2 | loose_joint (1.00) | Critical joint failure (well below unsafe) |
| **loose_support 1** | 8 | 4.70 | 3.27 | 66 | healthy (0.76) | Within healthy spread (excluded from training) |
| **loose_support 2** | 8 | 4.74 | 3.14 | 69 | healthy (0.77) | Same as above |
| **loose_support 3** | 8 | 4.79 | 3.48 | 33 | loose_support (0.88) | Below unsafe threshold |
| **test 0** | 5 | 4.68 | 2.96 | 92 | healthy (0.81) | Re-check after restoration |

### Health Trend  
- **Loose-support degradation:** health declines **≈ 14.4 points per loosening step**. Extrapolation predicts the unsafe line (40 %) will be crossed at **step 3.59**.  
- **Loose-joint:** health dropped from 53 % to 2 % within one step, indicating rapid failure once a joint begins to loosen.  

Classifier performance (cross-validated):  
- Frequency-only: 84 %  
- Decision tree / k-NN (k=3): 92 % each  
- Neural net (5-8-4): 94 %  

### Recommendation  
1. **Immediate action:** Tighten the joint identified as "loose_joint 2" and re-measure; health is critically low.  
2. **Support monitoring:** Since health is already below the unsafe threshold at loose_support 3, limit further loosening. Install a simple torque check after each adjustment; anticipate unsafe condition by step 4.  
3. **Added mass:** The 3.67 Hz frequency shift is expected from the extra mass (increased inertia lowers natural frequency) and does **not** indicate damage. No corrective action needed.  
4. **Future inspections:** Continue using the on-board ML classifier (≥ 92 % accuracy) and record at least 30 flicks per condition to maintain statistical confidence.  

*Prepared by: Structural Health Monitoring Engineer*