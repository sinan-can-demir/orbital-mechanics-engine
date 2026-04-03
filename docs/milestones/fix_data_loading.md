// docs/fix_data_loading.md

# 🧠 ROADMAP: From “It Works” → “It’s Robust”

## 🎯 Goal

Turn your project into a system that:

* doesn’t crash
* handles large data
* gives predictable behavior

---

## 🧩 PHASE 1 — Stabilization (you are HERE)

### Goal:

👉 Stop crashes, make system safe

### ✅ Step 1 — Define constraints (5 min, do mentally)

Ask for each component:

* How big can input be?
* What breaks it?
* What assumptions am I making?

For your case:

* Viewer assumes → “data is small enough”
* Reality → “data can be huge”

👉 mismatch = crash

---

### ✅ Step 2 — Add guardrails (HIGH PRIORITY)

Inside viewer:

```cpp
if (data.size() > 50000) {
    stride = std::max(stride, data.size() / 50000);
}
```

Add:

* max points
* auto stride
* warning message

👉 This alone fixes your freezing issue

---

### ✅ Step 3 — Input validation at system level

Already started with:

* filtering `_out.csv` ✅

Add mindset:

> “Never trust input — even from your own system”

---

### ✅ Step 4 — Safe entrypoints

Add:

* make `view-safe`

👉 always works, no freezing

---

## 🧩 PHASE 2 — Control & Observability

### Goal:

👉 Understand what your system is doing

### ✅ Step 5 — Add logging (VERY IMPORTANT)

In viewer:

```cpp
std::cout << "Loaded rows: " << data.size() << "\n";
std::cout << "Using stride: " << stride << "\n";
```

👉 Now no more guessing

---

### ✅ Step 6 — Add warnings (UX upgrade)

```cpp
if (data.size() > 100000) {
    std::cout << "⚠️ Large dataset detected\n";
}
```

---

### ✅ Step 7 — Make behavior explicit

Instead of silent adjustments:

```
⚠️ Auto-adjusted stride to 200
```

👉 transparency = trust

---

## 🧩 PHASE 3 — Performance Thinking

### Goal:

👉 Handle scale without brute force

### ✅ Step 8 — Separate load vs render

Right now:

* load ALL
* render SOME

Better:

* load LESS
* render LESS

*(You don’t need full streaming yet)*

---

### ✅ Step 9 — Cap memory usage

Simple rule:

```cpp
if (data.size() > LIMIT) {
    drop data or sample early
}
```

---

### ✅ Step 10 — Add CLI control

* `--stride`
* `--max-points`
* `--safe`

👉 puts power in user hands

---

## 🧩 PHASE 4 — System Design Maturity

### Goal:

👉 Think in pipelines, not scripts

### ✅ Step 11 — Clean data separation

Right now:

```
results/
  *_out.csv
  *_eclipse.csv
```

Better:

```
results/
  simulation/
  analysis/
```

---

### ✅ Step 12 — Define contracts

Viewer expects:

* `x_*`, `y_*` columns

👉 enforce this clearly

---

### ✅ Step 13 — Fail gracefully

Instead of:

```
Error 255
```

Do:

```
"This file is not compatible with viewer"
```

---

## 🧩 PHASE 5 — Advanced (optional, later)

### Goal:

👉 Make it impressive

* streaming CSV (chunk loading)
* progressive rendering
* GPU acceleration (only if needed)
* CLI tool (`orbit-cli`)

---

## 🧠 Mental Model (MOST IMPORTANT PART)

Whenever something breaks:

Ask 3 questions:

1. What assumptions did I make?
2. What changed?
3. Where is the mismatch?
