#!/usr/bin/env python3
"""
Drift guard: fails (exit 1) with file:line messages when constants, system files
or documented integrator lists drift apart.  Pure standard library; run from the
repo root:  python3 tools/check_drift.py

Checks
  1. Every systems/*.json with a Sun satisfies G * M_sun == GM_SUN within 1e-9
     (G and GM_SUN are read from include/constants.h).
  2. No literal "6.674" in code (.py/.cpp/.h/.json/.cmake/.txt, code cells of
     notebooks) except include/constants.h.  Prose in .md and notebook markdown
     cells may mention it.
  3. README.md and paper.md mention every member of the Integrator enum in
     include/simulation.h, and any "<number word> integrators" phrase in
     README/paper/docs matches the enum size.
  4. Every file in systems/ is referenced (by file name) from some tracked file
     outside systems/.
"""

import json
import re
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
errors = []


def err(path, line, msg):
    errors.append(f'{path}:{line}: {msg}')


def tracked():
    out = subprocess.run(['git', 'ls-files'], cwd=ROOT, capture_output=True, text=True, check=True).stdout
    return [p for p in out.splitlines() if (ROOT / p).is_file()]


def read(p):
    return (ROOT / p).read_text(errors='replace')


def const(name):
    m = re.search(r'constexpr double %s\s*=\s*([0-9.eE+-]+)\s*;' % name, read('include/constants.h'))
    if not m:
        sys.exit(f'include/constants.h:1: cannot find constexpr double {name}')
    return float(m.group(1))


def line_of(text, needle, start=0):
    return text.count('\n', 0, text.find(needle, start)) + 1


files = tracked()

# 1. Sun mass vs GM_SUN ------------------------------------------------------
G, GM_SUN = const('G'), const('GM_SUN')
for p in files:
    if not (p.startswith('systems/') and p.endswith('.json')):
        continue
    text = read(p)
    try:
        bodies = json.loads(text)['bodies']
    except (ValueError, KeyError) as e:
        err(p, 1, f'cannot read bodies: {e}')
        continue
    sun = next((b for b in bodies if b.get('name') == 'Sun'), None)
    if sun is None:
        continue
    rel = abs(G * sun['mass'] - GM_SUN) / GM_SUN
    if rel > 1e-9:
        ln = line_of(text, '"Sun"')
        err(p, ln, f'G*M_sun differs from GM_SUN (include/constants.h) by {rel:.2e} (> 1e-9); '
                   f'Sun mass should be GM_SUN/G = {GM_SUN / G!r}')

# 2. stray G literal ---------------------------------------------------------
CODE_EXT = ('.py', '.cpp', '.h', '.hpp', '.json', '.cmake', '.txt')
for p in files:
    if p == 'include/constants.h' or p.startswith(('external/', 'include/nlohmann/', 'tests/golden/', 'results/')):
        continue
    if p.endswith('.ipynb'):
        try:
            cells = json.loads(read(p))['cells']
        except ValueError:
            continue
        for ci, c in enumerate(cells):
            if c['cell_type'] != 'code':
                continue
            for li, l in enumerate(''.join(c['source']).splitlines(), 1):
                if '6.674' in l:
                    err(p, f'cell {ci + 1}, line {li}', 'literal 6.674...; use orbit.G')
    elif p.endswith(CODE_EXT) or p == 'CMakeLists.txt':
        if p == 'tools/check_drift.py':
            continue
        for li, l in enumerate(read(p).splitlines(), 1):
            if '6.674' in l:
                err(p, li, 'literal 6.674...; use physics::constants::G (C++) or orbit.G (Python)')

# 3. integrator list ---------------------------------------------------------
enum_src = read('include/simulation.h')
m = re.search(r'enum class Integrator\s*\{([^}]*)\}', enum_src)
members = [x.strip() for x in m.group(1).split(',') if x.strip()] if m else []
if not members:
    err('include/simulation.h', 1, 'cannot parse Integrator enum')
enum_line = line_of(enum_src, 'enum class Integrator')
for doc in ('README.md', 'paper.md'):
    text = read(doc)
    for mem in members:
        if not re.search(re.escape(mem), text, re.I):
            err(doc, 1, f'integrator "{mem}" (enum at include/simulation.h:{enum_line}) is not mentioned')
words = {'two': 2, 'three': 3, 'four': 4, 'five': 5, 'six': 6, 'seven': 7}
for p in files:
    if not p.endswith('.md') or p == 'docs/duplication-audit.md':
        continue
    for li, l in enumerate(read(p).splitlines(), 1):
        for w in re.findall(r'\b(two|three|four|five|six|seven)\s+(?:numerical\s+)?(?:integrators|integration methods)', l, re.I):
            if words[w.lower()] != len(members):
                err(p, li, f'says "{w} integrators" but the Integrator enum has {len(members)} members')

# 4. systems/ files referenced -----------------------------------------------
others = [p for p in files if not p.startswith('systems/') and p != 'tools/check_drift.py'
          and not p.startswith(('results/', 'external/', 'include/nlohmann/'))]
blob = {p: read(p) for p in others if not p.endswith(('.png', '.csv'))}
for p in files:
    if p.startswith('systems/') and p.endswith('.json'):
        name = Path(p).name
        if not any(name in t for t in blob.values()):
            err(p, 1, 'not referenced by any tracked file (tests, notebooks, docs, scripts, CLI help)')

if errors:
    print('Drift check FAILED:\n')
    print('\n'.join(errors))
    sys.exit(1)
print(f'Drift check passed ({len(files)} tracked files scanned; {len(members)} integrators: {", ".join(members)}).')
