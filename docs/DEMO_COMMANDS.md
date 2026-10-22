# Simulated demo: every command, in order

Run these in PowerShell inside the TinySHM folder. Use `py` (not `python`) on
Windows. Or skip typing and run everything at once with `.\run_sim_demo.bat`
(or double-click it).

## 0. Get the latest code
```
git pull
```

## 1. Install the libraries (once per laptop)
```
py -m pip install -r requirements.txt
```

## 2. Make simulated flicks
```
py pc/simulate.py
py pc/simulate.py --unseen
```
121 training flicks in `data/sim/`, and 20 never-seen combined faults in `data/sim_unseen/`.

## 3. Turn every flick into 5 features
```
py pc/build_dataset.py sim
py pc/build_dataset.py sim_unseen
```

## 4. Train all the models
```
py pc/train.py sim
```
Expect roughly: frequency only 71 %, decision tree 97 %, kNN 100 %, neural net 100 %,
health error 2.6 points, 9 of 20 unseen faults flagged UNKNOWN.
Writes `board/model.h` and the plots in `results/sim/`.

## 5. Check the board's C code matches Python
```
py pc/check_c_parity.py sim
```
Prints PARITY OK. Needs gcc (MinGW). If gcc is missing, skip this step.

## 6. A simulated lab session (good to show live)
```
py pc/logger.py --sim --label healthy --step 0 --count 5
py pc/logger.py --sim --label loose_support --step 1 --count 5
py pc/logger.py --sim --label loose_support --step 2 --count 5
py pc/logger.py --sim --label loose_support --step 3 --count 5
py pc/logger.py --sim --label added_mass --step 1 --count 3
```
Each flick prints its frequency, the predicted condition and the health score.
Labels you can use: `healthy`, `loose_support`, `loose_joint`, `added_mass`.

## 7. Predict when the beam becomes unsafe
```
py pc/trend.py sim_session
```
Writes `results/sim_session/trend.png`.

## 8. Compare the 3 frequency methods (objective 5)
```
py pc/compare_methods.py sim
```
Writes `results/sim/method_comparison.png`.

## 9. AI-written inspection report
```
py pc/llm_report.py sim_session
```
Uses Groq if `GROQ_API_KEY` is set, otherwise a template. Saved to
`results/sim_session/inspection_report.md`.

## Where the results are
| File | Shows |
|---|---|
| `results/sim/accuracy_comparison.png` | Frequency only vs the 3 ML models |
| `results/sim/features_f1_vs_damping.png` | Why frequency alone mixes up the coin and damage |
| `results/sim/confusion_matrices.png` | Where each model goes wrong |
| `results/sim/health_score.png` | Predicted vs true health |
| `results/sim/method_comparison.png` | FFT peak vs interpolated FFT vs zero-crossing |
| `results/sim_session/trend.png` | When the beam becomes unsafe |
| `results/sim_session/inspection_report.md` | The AI report |

## Starting fresh
To regenerate everything from zero, delete the `data` and `results` folders
first, then run the steps again.
