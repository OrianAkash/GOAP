# GOAP Outpost Defense - Research Demo

Goal-Oriented Action Planning demo for our AI research project. 3 NPC agents
defend an outpost. Nothing is scripted: each agent picks a goal, and a
regressive A* planner searches for the cheapest action sequence that achieves
it. You can mess with the world at runtime and watch them replan.

- `sim/` - the GOAP planner + simulation (no graphics, pure C++17)
- `app/` - SFML window, rendering, input
- `contract/Contract.h` - the interface between the two
- `tests/` - planner unit tests + a headless smoke run (`build_tests.bat`)
- `docs/` - how the planner works (PLANNER.md) + presentation script (DEMO_GUIDE.md)

## Building (Windows, Visual Studio 2022)

1. Download SFML **2.6.2 - Visual C++ 17 (2022) - 64-bit** from
   https://www.sfml-dev.org/download/sfml/2.6.2/
2. Extract it so that this folder exists: `C:\SFML-2.6.2\include`
   (i.e. rename the extracted `SFML-2.6.2` folder if needed and put it at `C:\`)
3. Open `GOAP.sln`, pick **Release / x64**, press **F5**.

The post-build step copies the SFML DLLs and the `assets` folder next to the
exe, so `x64\Release\GOAP.exe` also runs on its own (that's what goes in the
submission `demo` folder).

If the build can't find `sfml-graphics.lib`, SFML is not at `C:\SFML-2.6.2` -
either move it there or change the include/library paths in
Project Properties.

Text uses `assets\DejaVuSans.ttf` if present and otherwise falls back to the
Windows Arial font. If `assets` is missing, grab DejaVuSans.ttf from
https://dejavu-fonts.github.io/ (or copy any .ttf and rename it) - mainly so
the demo also has its font when zipped up for submission.

## Controls

| input | effect |
|---|---|
| Left click | spawn a threat at the mouse |
| Right click | drop a weapon at the mouse |
| W | remove all ground weapons |
| H | injure the agent nearest the mouse |
| T | flip melee cost 20 <-> 1 |
| Space | pause |
| R | reset |
| F1 | hide/show agent labels |
| Esc | quit |

## What you're looking at

- **cyan circles** = agents (yellow dot = carrying a weapon, blinking red = hurt)
- **red circles** = threats, **yellow triangles** = weapon pickups
- **green square** = the outpost; agents heal inside the green ring
- **gray circles** = patrol points

Above each agent: its current **goal** and its **plan**, with the action it's
executing right now in [brackets]. The log at the bottom shows every goal
switch and replan as it happens. See `docs/DEMO_GUIDE.md` for a step-by-step
demo script that shows off the interesting behaviour.
