# Debug List

Found bugs will be listed here to tracking and solution. Also document the solutions.

## 🔴 PHASE 1 — Fix correctness & trust (HIGH PRIORITY)

### ✅ 1. Fix CLI help inconsistency

**Problem:**
`orbit-sim run --help` doesn’t work properly

**Task:**

* Detect `--help` AFTER subcommand
* Print command-specific help

**Definition of Done:**

```bash
orbit-sim run --help   # works
orbit-sim validate --help   # works
```

---

### ✅ 2. Sync README with actual CLI

**Problem:** Docs lie slightly

**Task:**

* Run every command from README
* Fix flags (`--target` vs `--body`, etc.)
* Remove outdated examples

**Definition of Done:**
👉 Every command in README works exactly as written

---

### ✅ 3. Rename CMake project

**Problem:**

```cmake
project(EarthAndMoonOrbits)
```

**Fix:**

```cmake
project(OrbitalMechanicsEngine)
```

---

## 🟠 PHASE 2 — Strengthen core reliability

### ✅ 4. Improve JSON validation

**Add checks for:**

* required fields exist
* position/velocity size == 3
* mass > 0
* no NaN / inf
* unique names

**Definition of Done:**
Invalid system files fail clearly with useful messages

---

### ✅ 5. Fix numerical edge cases

**Problem: division by zero risk**

**Fix pattern:**

```cpp
double safe_div(double num, double denom) {
    const double eps = 1e-12;
    return std::abs(denom) > eps ? num / denom : 0.0;
}
```

Apply to:

* energy drift
* angular momentum drift

---

## 🟡 PHASE 3 — Clean architecture

### ✅ 6. Refactor CLI parser

**Problem:** `parseCLI()` exits directly

**Fix:**

* Return struct or result
* Let `main()` handle exit

**Definition of Done:**
Parser is testable independently

---

### ✅ 7. Fix `.gitignore`

**Problem:** starts with `*`

**Fix:**
Use standard ignore:

```
build/
*.o
*.csv
```

---

### ✅ 8. Make viewer truly optional

Only include:

```cmake
add_subdirectory(external/glad)
```

IF `BUILD_VIEWER=ON`

---

### ✅ 9. Apply compiler warnings globally

Create something like:

```cmake
target_compile_options(target PRIVATE -Wall -Wextra -Wpedantic)
```

Apply to ALL targets

---

## 🔵 PHASE 4 — Product polish

### ✅ 10. Fix hardcoded output paths

Replace:

```cpp
build/eclipse_log.csv
```

With:

* derived path OR
* CLI argument

---

### ✅ 11. Clean naming leftovers

Search & fix:

* "three_body"
* "EarthMoon"
* outdated defaults

---

### ✅ 12. Clarify README (important)

Split into:

* ✅ Current features
* 🚧 Future work

---

## 🟣 PHASE 5 — Level-up (this is where you grow most)

### ✅ 13. Add unit tests (START SMALL)

Test:

* vector math
* JSON parsing
* CLI parsing
* conservation calculations

**Definition of Done:**
You have at least 5–10 tests

---

### ✅ 14. Add CLI tests (super valuable)

Test:

```bash
orbit-sim --help
orbit-sim run --help
orbit-sim validate bad.json
```

---

# 🚀 Suggested Execution Plan (IMPORTANT)

Don’t do everything at once.

### Week 1:

* Phase 1 (CLI + README + CMake)

### Week 2:

* Phase 2 (validation + numerical safety)

### Week 3:

* Phase 3 (architecture cleanup)

### Week 4:

* Phase 5 (tests)

---