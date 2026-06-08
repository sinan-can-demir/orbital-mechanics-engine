# 🗺️ Roadmap: “Own the CLI Tool Later” (Low-effort, high ROI)

You’re not dropping it — you’re parking it intelligently

---

### 🧩 Phase 0 — Current (NOW)

**Focus:** Makefile + build workflow

**Goal:**

* clean build
* one-command run
* reproducible pipeline

👉 Do NOT touch `view.sh` beyond using it

---

### 🧩 Phase 1 — Integration (after Makefile)

**Goal:** Turn your project into a workflow system

Add Makefile targets like:

```make
run:
	./build/bin/simulator

view:
	./view.sh

view-latest:
	./view.sh --latest
```

👉 This connects:
simulation → results → viewer

Now your project becomes:

**“end-to-end system” (this is resume gold)**

---

### 🧩 Phase 2 — CLI Ownership (30–45 min total, not now)

You don’t rewrite. You just extend.

**Task 1 (10 min)**
Add:

```
./view.sh --latest
```

**Task 2 (10 min)**
Add:

```
./view.sh --list
```

(just print files, don’t launch)

**Task 3 (10 min)**
Add:

```
./view.sh <filename>
```

(auto-resolve inside `results/`)

---

### 🧩 Phase 3 — UX polish (optional, 20 min)

* add `q` to quit
* sort by date instead of name
* highlight newest file

👉 This is “nice to have”, not critical

---

### 🧩 Phase 4 — Power move (only if you want)

Replace picker with:

```
fzf
```

Now you have:

* fuzzy search
* pro-level CLI UX
