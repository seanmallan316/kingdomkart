# Milestone 2 checklist: playable racing prototype

Build: kingdomkart.wuhb (version M2-0.1). Emulator-verified; not yet run on a real Wii U.

What it is: one kart on the 710 m "Test Loop" (two hills, a banked sweeper, a jump, a tight corner), 3 laps.

Controls: left stick or D-pad steers, A gas, B brake/reverse, ZR (or R) drift, hold the drift to charge a mini-turbo and release it, + restarts after the finish, HOME exits.
If nobody touches the controller for 6 seconds the kart drives itself (DEMO).

Checked in the emulator (screenshots in the ci-screenshots branch):
- [x] Track, walls, trees, sky and kart draw on the TV and GamePad
- [x] Countdown, lap counter, lap timer, best lap, speed readout
- [x] Drifting leaves skid marks and dust, drift-charge bar fills, mini-turbo message
- [x] Autopilot completes laps (about 22 s each)
Checked on a PC with no graphics (tests/sim_test.c, runs on every build):
- [x] 3 laps complete at two skill levels, jumps happen at the ramp, mini-turbos fire, no NaN
- [x] Driving backwards shows WRONG WAY and counting does not go up

Still to check on a real Wii U when one is available:
- [ ] Frame-time bar is green (60 fps). The emulator cannot measure this.
- [ ] Stick, A, B, ZR feel responsive on the real GamePad
