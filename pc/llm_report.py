"""Turn the numbers into an engineer-style inspection report using an LLM.

It summarises the session log, model accuracy and trend into a short fact
sheet, then asks an LLM to write the report.

  python pc/llm_report.py real                 # Groq if GROQ_API_KEY is set, else Gemini
                                               # if GEMINI_API_KEY is set, else a template
  python pc/llm_report.py real --prompt-only   # prints the prompt; paste into any chatbot

Keys:  Groq   https://console.groq.com/keys   (set GROQ_API_KEY=...)
       Gemini https://aistudio.google.com/apikey (set GEMINI_API_KEY=...)
Model override: GROQ_MODEL (default openai/gpt-oss-120b), GEMINI_MODEL.
Writes results/<set>/inspection_report.md
"""
import argparse
import json
import os
import urllib.request
from datetime import date
import pandas as pd
from config import DATA, RESULTS, UNSAFE_HEALTH

SYSTEM = (
    "You are a structural health monitoring engineer. Write a short inspection report "
    "(under 350 words, markdown) for a lab-scale cantilever beam (a steel ruler held "
    "to a desk) monitored by an MPU6050 accelerometer and an LPC1768 microcontroller "
    "running on-board ML. Sections: Summary, Findings per condition, Health trend, "
    "Recommendation. Use only the numbers given. Do not invent measurements. Explain in "
    "plain words why added mass lowers frequency without being damage, if relevant."
)


def facts(dset, model_set):
    log = pd.read_csv(DATA / f"{dset}_log.csv")
    lines = [f"Inspection date: {date.today()}", f"Flicks analysed: {len(log)}"]
    g = log.groupby(["label", "step"]).agg(
        flicks=("health", "size"), f1_hz=("f1_hz", "mean"), damping=("damping", "mean"),
        health=("health", "mean"), predicted=("nn", lambda s: s.mode().iat[0]),
        nn_confidence=("nn_conf", "mean"), unknown=("unknown", "sum"))
    lines.append("Per condition (label = what was actually done to the beam):")
    for (lab, step), r in g.iterrows():
        lines.append(f"- {lab} step {step}: {r.flicks} flicks, f1 {r.f1_hz:.2f} Hz, "
                     f"damping {r.damping * 100:.2f} %, health {r.health:.0f} %, "
                     f"model says {r.predicted} (confidence {r.nn_confidence:.2f}), "
                     f"flagged unknown {int(r.unknown)} times")
    lines.append(f"Unsafe threshold: health below {UNSAFE_HEALTH:.0f} %")
    met = RESULTS / model_set / "metrics.json"
    if met.exists():
        m = json.loads(met.read_text(encoding="utf-8"))
        lines.append("Classifier accuracy (cross-validated): " + ", ".join(
            f"{k} {v * 100:.0f} %" for k, v in m["accuracy"].items()))
    tr = RESULTS / dset / "trend.json"
    if tr.exists():
        t = json.loads(tr.read_text(encoding="utf-8"))
        lines.append(f"Trend for {t['fault']}: health falls {-t['slope_per_step']:.1f} points per "
                     f"loosening step; predicted to cross the unsafe line at step "
                     f"{t['predicted_unsafe_step']}")
    return "\n".join(lines)


def ask_groq(prompt):
    key = os.environ["GROQ_API_KEY"]
    model = os.environ.get("GROQ_MODEL", "openai/gpt-oss-120b")
    body = {"model": model, "temperature": 0.3,
            "messages": [{"role": "system", "content": SYSTEM},
                         {"role": "user", "content": prompt}]}
    req = urllib.request.Request(
        "https://api.groq.com/openai/v1/chat/completions", json.dumps(body).encode(),
        {"Content-Type": "application/json", "Authorization": f"Bearer {key}",
         "User-Agent": "esl-ml-kit"})
    with urllib.request.urlopen(req, timeout=60) as r:
        return json.loads(r.read())["choices"][0]["message"]["content"]


def ask_gemini(prompt):
    key = os.environ["GEMINI_API_KEY"]
    model = os.environ.get("GEMINI_MODEL", "gemini-2.5-flash")
    url = f"https://generativelanguage.googleapis.com/v1beta/models/{model}:generateContent?key={key}"
    body = {"system_instruction": {"parts": [{"text": SYSTEM}]},
            "contents": [{"parts": [{"text": prompt}]}]}
    req = urllib.request.Request(url, json.dumps(body).encode(), {"Content-Type": "application/json"})
    with urllib.request.urlopen(req, timeout=60) as r:
        return json.loads(r.read())["candidates"][0]["content"]["parts"][0]["text"]


def template(fact_text):
    return ("# Inspection report (template, no LLM used)\n\n"
            "Set GROQ_API_KEY or GEMINI_API_KEY to get an AI-written report. Raw findings:\n\n" + fact_text + "\n")


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("set", nargs="?", default="real", help="real or sim_session")
    ap.add_argument("--model", default=None, help="model set whose accuracy to quote")
    ap.add_argument("--prompt-only", action="store_true")
    args = ap.parse_args()
    model_set = args.model or ("sim" if args.set.startswith("sim") else "real")
    f = facts(args.set, model_set)
    prompt = "Measurements from today's inspection:\n" + f
    if args.prompt_only:
        print(SYSTEM + "\n\n" + prompt)
        return
    text = None
    for env, fn in (("GROQ_API_KEY", ask_groq), ("GEMINI_API_KEY", ask_gemini)):
        if os.environ.get(env) and text is None:
            try:
                text = fn(prompt)
                print(f"(report written by {fn.__name__[4:]})")
            except Exception as e:                   # no internet, bad key, model renamed
                print(f"{fn.__name__[4:]} call failed ({e})")
    if text is None:
        text = template(f)
    # LLMs like fancy dashes and quotes; the Windows console can't print them
    for fancy, plain in (("\u2010", "-"), ("\u2011", "-"), ("\u2012", "-"), ("\u2013", "-"),
                         ("\u2014", ", "), ("\u2018", "'"), ("\u2019", "'"), ("\u201c", '"'),
                         ("\u201d", '"'), ("\u2026", "..."), ("\u00a0", " "), ("\u202f", " ")):
        text = text.replace(fancy, plain)
    out = RESULTS / args.set / "inspection_report.md"
    out.parent.mkdir(parents=True, exist_ok=True)
    out.write_text(text, encoding="utf-8")
    print(text.encode("ascii", "replace").decode("ascii"))
    print(f"\nsaved {out}")


if __name__ == "__main__":
    main()
