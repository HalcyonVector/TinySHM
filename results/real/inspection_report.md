# Inspection report (template, no LLM used)

Set GROQ_API_KEY or GEMINI_API_KEY to get an AI-written report. Raw findings:

Inspection date: 2026-10-28
Flicks analysed: 89
Per condition (label = what was actually done to the beam):
- added_mass step 1: 12 flicks, f1 3.67 Hz, damping 2.58 %, health 100 %, model says added_mass (confidence 0.99), flagged unknown 0 times
- healthy step 0: 30 flicks, f1 4.71 Hz, damping 2.96 %, health 97 %, model says healthy (confidence 0.93), flagged unknown 0 times
- loose_joint step 1: 10 flicks, f1 4.48 Hz, damping 2.68 %, health 53 %, model says loose_joint (confidence 0.88), flagged unknown 0 times
- loose_joint step 2: 8 flicks, f1 4.45 Hz, damping 1.28 %, health 2 %, model says loose_joint (confidence 1.00), flagged unknown 0 times
- loose_support step 1: 8 flicks, f1 4.70 Hz, damping 3.27 %, health 66 %, model says healthy (confidence 0.76), flagged unknown 0 times
- loose_support step 2: 8 flicks, f1 4.74 Hz, damping 3.14 %, health 69 %, model says healthy (confidence 0.77), flagged unknown 0 times
- loose_support step 3: 8 flicks, f1 4.79 Hz, damping 3.48 %, health 33 %, model says loose_support (confidence 0.88), flagged unknown 0 times
- test step 0: 5 flicks, f1 4.68 Hz, damping 2.96 %, health 92 %, model says healthy (confidence 0.81), flagged unknown 0 times
Label notes: 'test' = the healthy beam re-checked after the setup was restored (not used for training). Steps kept out of training because they sit inside the healthy spread: loose_support:1, loose_support:2.
Unsafe threshold: health below 40 %
Classifier accuracy (cross-validated): Frequency only (no ML) 84 %, Decision tree 92 %, kNN (k=3) 92 %, Neural net (5-8-4) 94 %
Trend for loose_support: health falls 14.4 points per loosening step; predicted to cross the unsafe line at step 3.59
