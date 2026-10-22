@echo off
REM TinySHM: full simulated demo. Double-click this file, or run it from
REM PowerShell inside the TinySHM folder with:  .\run_sim_demo.bat
cd /d "%~dp0"

echo === 1. Install libraries (only slow the first time) ===
py -m pip install -r requirements.txt || goto :fail

echo.
echo === 2. Make simulated flicks ===
py pc/simulate.py || goto :fail
py pc/simulate.py --unseen || goto :fail

echo.
echo === 3. Flicks to features ===
py pc/build_dataset.py sim || goto :fail
py pc/build_dataset.py sim_unseen || goto :fail

echo.
echo === 4. Train all models (writes board/model.h and plots) ===
py pc/train.py sim || goto :fail

echo.
echo === 5. Board C code vs Python (needs gcc, skipped if missing) ===
py pc/check_c_parity.py sim

echo.
echo === 6. Simulated lab session, live answers per flick ===
py pc/logger.py --sim --label healthy --step 0 --count 5 || goto :fail
py pc/logger.py --sim --label loose_support --step 1 --count 5 || goto :fail
py pc/logger.py --sim --label loose_support --step 2 --count 5 || goto :fail
py pc/logger.py --sim --label loose_support --step 3 --count 5 || goto :fail
py pc/logger.py --sim --label added_mass --step 1 --count 3 || goto :fail

echo.
echo === 7. When does it become unsafe? ===
py pc/trend.py sim_session || goto :fail

echo.
echo === 8. Frequency methods (objective 5) ===
py pc/compare_methods.py sim || goto :fail

echo.
echo === 9. AI inspection report (Groq if GROQ_API_KEY is set) ===
py pc/llm_report.py sim_session || goto :fail

echo.
echo DONE. Plots are in results\sim and results\sim_session
pause
exit /b 0

:fail
echo.
echo A step failed. Copy the error above and send it over.
pause
exit /b 1
