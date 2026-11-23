"""Writes world.json: every block-out shape and where it stands. The browser game, the bake tool
and the Godot scene all read this one file, so the paintings line up everywhere.
Part kinds: box [w,h,d] · cyl [top r, bottom r, h, sides] · gable [w,d,h] (ridge along x) · ball [r]
Run: python3 -I tools/make_world.py"""
import json, math, os
HERE = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
def P(k, s, p, hint, solid=True, rx=0.0, ry=0.0):
    # a part turns about ONE axis only (three.js and Godot combine several turns in different orders)
    assert rx == 0.0 or ry == 0.0
    return {"k": k, "s": s, "p": p, "hint": hint, "solid": solid, "rx": round(rx, 5), "ry": round(ry, 5)}
O = {}
O["cottage"] = [P("box", [7, 2.2, 3.4], [0, 1.1, 0], "#e9e4d6"), P("gable", [7.7, 4.1, 2.0], [0, 2.2, 0], "#c9a24a"),
    P("box", [0.8, 1.3, 0.8], [2.6, 3.7, 0], "#8d8578"), P("box", [0.95, 1.6, 0.08], [-0.6, 0.8, 1.72], "#3f6f8f", False),
    P("box", [0.8, 0.7, 0.08], [1.6, 1.3, 1.72], "#2b3440", False), P("box", [0.8, 0.7, 0.08], [-2.6, 1.3, 1.72], "#2b3440", False),
    P("box", [0.08, 0.7, 0.8], [3.52, 1.3, 0], "#2b3440", False), P("box", [1.2, 0.5, 0.6], [2.6, 0.25, 2.1], "#7a5a3a")]
O["tower"] = [P("box", [3.2, 7, 3.2], [0, 3.5, 0], "#8f8678"), P("cyl", [0, 2.55, 2.6, 4], [0, 8.3, 0], "#4e5a66", ry=math.pi / 4),
    P("box", [1.1, 2.0, 0.08], [0, 1.0, 1.62], "#5a3b2a", False), P("box", [0.4, 0.9, 0.08], [0, 4.2, 1.62], "#2b3440", False),
    P("box", [0.08, 0.9, 0.4], [1.62, 5.6, 0], "#2b3440", False), P("box", [3.5, 0.3, 3.5], [0, 7.0, 0], "#a39a8b")]
O["stall"] = [P("box", [3.0, 1.0, 1.2], [0, 0.5, 0], "#8a5a35")] + \
    [P("cyl", [0.07, 0.07, 2.6, 6], [x, 1.3, z], "#6b4a30") for x, z in [(-1.45, -0.55), (1.45, -0.55), (-1.45, 0.55), (1.45, 0.55)]] + \
    [P("box", [0.6, 0.4, 0.5], [-0.8, 1.2, 0], "#b98a4a"), P("box", [0.5, 0.35, 0.5], [0.1, 1.17, 0.1], "#c45a3a"),
     P("cyl", [0.25, 0.25, 0.5, 10], [0.9, 1.25, -0.1], "#7d6a4a")]
O["awning"] = [P("box", [3.5, 0.08, 1.9], [0, 0, 0], "#b23a3a", False, rx=0.32)]
O["well"] = [P("cyl", [0.95, 1.0, 0.9, 16], [0, 0.45, 0], "#8c8880"), P("cyl", [0.7, 0.7, 0.06, 16], [0, 0.9, 0], "#2d4a5a", False),
    P("box", [0.15, 2.0, 0.15], [-0.95, 1.0, 0], "#6b4a30"), P("box", [0.15, 2.0, 0.15], [0.95, 1.0, 0], "#6b4a30"),
    P("gable", [2.4, 1.7, 0.75], [0, 2.0, 0], "#4e5a66"), P("cyl", [0.2, 0.17, 0.3, 8], [0.3, 1.3, 0], "#7a5a3a", False)]
O["pine"] = [P("cyl", [0.16, 0.22, 1.4, 8], [0, 0.7, 0], "#6b4a30"), P("cyl", [0, 1.5, 2.1, 9], [0, 2.1, 0], "#2f5d3a"),
    P("cyl", [0, 1.15, 1.8, 9], [0, 3.1, 0], "#3a6d44"), P("cyl", [0, 0.75, 1.5, 9], [0, 4.0, 0], "#467a4c")]
O["beacon"] = [P("cyl", [1.1, 1.3, 0.5, 14], [0, 0.25, 0], "#8c8880"), P("cyl", [0.45, 0.6, 1.0, 10], [0, 1.0, 0], "#7a756c"),
    P("cyl", [0.9, 0.45, 0.55, 12], [0, 1.78, 0], "#3a3a3a")]
# ---- the Lochside ----
O["kirk"] = [P("box", [4.2, 3.0, 7.0], [0, 1.5, 0], "#b9b3a6"),                       # nave, long way towards the camera
    P("gable", [7.4, 5.0, 2.4], [0, 3.0, 0], "#55606b", ry=math.pi / 2),               # slate roof, ridge along z
    P("box", [1.5, 5.6, 1.5], [0, 2.8, -4.2], "#a8a295"),                             # bell tower at the back
    P("cyl", [0, 1.25, 1.6, 4], [0, 6.4, -4.2], "#55606b", ry=math.pi / 4),
    P("box", [1.1, 2.0, 0.08], [0, 1.0, 3.52], "#5a3b2a", False),                     # door
    P("box", [0.08, 1.3, 0.6], [2.12, 1.8, 1.6], "#33475a", False), P("box", [0.08, 1.3, 0.6], [2.12, 1.8, -1.2], "#33475a", False)]
O["mill"] = [P("box", [4.4, 3.2, 3.6], [0, 1.6, 0], "#9a9184"), P("gable", [5.0, 4.2, 2.0], [0, 3.2, 0], "#8a5a3a"),
    P("cyl", [1.45, 1.45, 0.5, 14], [-0.6, 1.3, 2.08], "#6b4a30", rx=math.pi / 2),     # water wheel, on the side facing the burn
    P("box", [0.08, 1.8, 1.0], [2.22, 0.9, 0.4], "#5a3b2a", False), P("box", [0.08, 0.7, 0.7], [2.22, 2.1, -0.9], "#2b3440", False),
    P("box", [0.8, 0.6, 1.3], [2.8, 0.3, -1.0], "#c9b98a")]                            # grain sacks
O["longhouse"] = [P("box", [8.5, 2.0, 3.4], [0, 1.0, 0], "#7d766a"), P("gable", [9.2, 4.2, 1.9], [0, 2.0, 0], "#6f8a4a"),
    P("box", [1.0, 1.6, 0.08], [1.5, 0.8, 1.72], "#5a3b2a", False), P("box", [0.7, 0.6, 0.08], [-1.5, 1.2, 1.72], "#2b3440", False),
    P("box", [0.08, 0.6, 0.7], [4.27, 1.2, 0], "#2b3440", False), P("cyl", [0.3, 0.35, 0.8, 8], [-2.5, 3.6, 0], "#5a544c")]
O["bridge"] = [P("box", [2.8, 0.16, 4.4], [0, 0.08, 0], "#9c958a", False),            # deck: walk across at ground level
    P("box", [0.45, 0.95, 4.6], [-1.6, 0.47, 0], "#8a8478"), P("box", [0.45, 0.95, 4.6], [1.6, 0.47, 0], "#8a8478")]
O["jetty"] = [P("box", [2.0, 0.2, 5.4], [0, 0.1, 0], "#8a6a45", False)] + \
    [P("cyl", [0.14, 0.14, 1.3, 6], [x, 0.5, z], "#5e4630", False) for x in (-0.95, 0.95) for z in (-2.2, 0.2, 2.5)]
O["boat"] = [P("box", [3.4, 0.6, 1.3], [0, 0.35, 0], "#7a4f32"), P("box", [1.0, 0.5, 1.0], [2.0, 0.42, 0], "#7a4f32", ry=math.pi / 4),
    P("box", [0.2, 0.08, 1.2], [-0.5, 0.62, 0], "#b08a5a", False), P("cyl", [0.05, 0.05, 2.6, 6], [0.3, 1.5, 0], "#5e4630", False)]
O["haystack"] = [P("cyl", [0.9, 1.2, 1.2, 12], [0, 0.6, 0], "#d2b45c"), P("cyl", [0, 0.95, 0.9, 12], [0, 1.65, 0], "#c9a94e")]
O["oak"] = [P("cyl", [0.7, 1.0, 2.6, 10], [0, 1.3, 0], "#6a4c33"), P("ball", [2.6], [0, 4.4, 0], "#4f7d3c"),
    P("ball", [1.8], [-1.9, 3.6, 0.6], "#5a8a44"), P("ball", [1.7], [1.8, 3.9, -0.4], "#467236"),
    P("box", [0.7, 1.2, 0.1], [0.1, 0.8, 0.95], "#2a1c14", False)]                     # the hollow
O["wall_x"] = [P("box", [5.0, 0.9, 0.55], [0, 0.45, 0], "#8d887c")]
O["wall_z"] = [P("box", [0.55, 0.9, 5.0], [0, 0.45, 0], "#8d887c")]

L = lambda i, x, z, y=0.0: {"id": i, "x": x, "z": z, "y": y}
layout = [L("beacon", 0, 0), L("cottage", -6.5, -5.5), L("cottage", -9.5, 6.5), L("tower", 6.5, -7), L("stall", 6, 3.5),
    L("awning", 6, 3.5, 2.55), L("well", -3.5, 4)] + [L("pine", x, z) for x, z in
    [(1.5, -10), (-12, -1), (11, 9), (-2, 11.5), (12, -2), (-12.5, -10), (3, 10)]]
layout += [L("bridge", 0, 20), L("mill", 7.0, 16.7), L("kirk", -8.5, 29), L("longhouse", 8.5, 31), L("beacon", 0, 33),
    L("jetty", 0, 42.6), L("boat", 6.5, 40.2), L("oak", -11.5, 23.5), L("haystack", -4.5, 37), L("haystack", -10.5, 36.5),
    L("haystack", 4.2, 36.6), L("wall_x", -6.5, 16.6), L("wall_x", -8.0, 41.0), L("wall_z", -12.9, 31), L("wall_z", 12.9, 26),
    L("pine", -3.5, 25), L("pine", 12, 37), L("pine", -12.2, 39), L("pine", 3.6, 27.5)]
world = {
    "view_dir": [1, 1.15, 1],
    "cutout": ["pine", "well", "beacon", "awning", "oak", "haystack", "boat", "jetty"],
    "objects": O, "layout": layout,
    "paths": [[0, 0, -4.5, -2.2], [0, 0, 4.2, -4.2], [0, 0, 4.5, 1.8], [0, 0, -2.4, 3], [0, 0, -7.5, 3.8], [0, 0, 0, 14], [0, 0, 14, 0],
              [0, 14, 0, 30], [0, 26, -6, 29], [0, 16.6, 4.4, 16.6], [0, 31, 6.5, 33], [0, 33, 0, 40]],
    "squares": [[0, 0, 3.6], [0, 33, 3.0]],          # cobbled circles: x, z, radius
    "bounds": {"x0": -13.5, "x1": 13.5, "z0": -13.5, "z1": 45.5},
    "burn": {"z": 20.0, "half": 1.3, "bridge_half": 1.2},     # a stream across the map; cross at the bridge (x near 0)
    "loch_z": 40.6,                                           # water south of this line; the jetty runs out into it
    "jetty": {"half": 0.85, "z1": 45.0},
    "gate_z": 15.2,                                           # the Veil bars the road south until the glen is freed
    "stages": [
        {"beacon": [0, 0], "flood": 17.5, "spawn": [0.5, 6.5],
         "sparks": [[3.9, -9.6], [-7.2, -8.4], [6.9, 2.2], [-11.6, 5.2], [10.5, -9.5], [-4.8, 1.2], [11.2, 5.5]],
         "wraiths": [[-9, -1], [9, -3], [9, 9.5]]},
        {"beacon": [0, 33], "flood": 34.0,
         "sparks": [[-8.5, 33.8], [10.4, 17.4], [0, 44.2], [-11.4, 26.6], [11.5, 34.5], [-2.2, 22.2], [5.2, 39.3]],
         "wraiths": [[-6, 24], [7, 28], [-8, 38], [8, 37]]}],
}
# ---- story places for the Painted Coast (Godot): written in the same small-map coordinates as above ----
world["places"] = {
    "start": [4.6, 38.4], "kessa": [-2.6, 30.6], "oak": [-9.6, 25.4], "brisa": [-9.2, 23.4], "fennick": [-1.6, 5.2],
    "spire": [6.5, -7.0], "door": [6.5, -4.6], "joran_wait": [1.6, 1.4],
    "messages": [[-8.5, 34.0], [10.8, 16.5], [-7.2, -8.6]],
    "guards": [[1.2, 24.6, "road"], [-1.6, 22.8, "road"], [-4.5, 9.5, "village"], [10.0, 1.5, "village"], [4.4, -3.6, "spire"], [8.6, -3.8, "spire"]],
    "boss_spots": [[6.5, -3.2], [2.8, -4.6], [10.2, -2.6], [5.0, -0.6]],
}

# ---- stretch the coast south of the glen so the walk has room for the story: twice as long, a little wider ----
def st(x, z):
    return (round(x * 1.25, 2), round(14 + (z - 14) * 2.0, 2)) if z > 14 else (x, z)
for L_ in world["layout"]: L_["x"], L_["z"] = st(L_["x"], L_["z"])
world["paths"] = [[*st(a, b), *st(c, d)] for a, b, c, d in world["paths"]]
world["squares"] = [[*st(x, z), r] for x, z, r in world["squares"]]
for stg in world["stages"]:
    for k in ("beacon", "spawn"):
        if k in stg: stg[k] = list(st(*stg[k]))
    for k in ("sparks", "wraiths"): stg[k] = [list(st(*q)) for q in stg[k]]
pl = world["places"]
for k in ("start", "kessa", "oak", "brisa", "fennick", "spire", "door", "joran_wait"): pl[k] = list(st(*pl[k]))
pl["messages"] = [list(st(*q)) for q in pl["messages"]]
pl["boss_spots"] = [list(st(*q)) for q in pl["boss_spots"]]
pl["guards"] = [[*st(x, z), g] for x, z, g in pl["guards"]]
world["burn"]["z"] = st(0, world["burn"]["z"])[1]
world["loch_z"] = st(0, world["loch_z"])[1]
world["gate_z"] = st(0, world["gate_z"])[1]
world["bounds"] = {"x0": -17.5, "x1": 17.5, "z0": -13.5, "z1": 77.0}
# pieces that must sit against something keep their fit after the stretch
BZ, LZ = world["burn"]["z"], world["loch_z"]
for L_ in world["layout"]:
    if L_["id"] == "mill": L_["z"] = round(BZ - 3.3, 2)                 # its wheel dips in the burn
    if L_["id"] == "jetty": L_["z"] = round(LZ + 2.0, 2)                # from the shore out into the loch
    if L_["id"] == "wall_x" and L_["z"] > LZ - 1: L_["z"] = round(LZ - 1.7, 2)
world["jetty"]["z1"] = round(LZ + 4.3, 2)
pl["jetty_shore"] = [0.0, round(LZ - 0.6, 2)]; pl["jetty_end"] = [0.0, round(LZ + 3.8, 2)]
world["paths"] = [q for q in world["paths"] if not (abs(q[1] - q[3]) < 0.01 and q[2] > 4 and q[1] < BZ)]   # the old mill path
world["paths"] += [[0, BZ - 3.4, 5.4, BZ - 3.4], [0, 48, 6.2, 48.4], [0, 55, -8.2, 55.2], [0, 40, 0, 52], [0, 52, 0, LZ - 1]]
# ---- more to see along the longer road (existing paintings) ----
more = [("pine", -15, 20), ("pine", 14.5, 30), ("pine", -6, 44), ("pine", 15, 44.5), ("pine", -15.5, 50.5), ("pine", 4.5, 57),
        ("pine", -15.5, 62.5), ("pine", 15.5, 56), ("pine", 12, 40), ("pine", -4, 56.5), ("pine", 6.5, 33), ("pine", -8, 30.5),
        ("haystack", 9, 53), ("haystack", 12.2, 54), ("haystack", -6.5, 49.5), ("cottage", -11.5, 54),
        ("wall_x", 10.6, 44.4), ("wall_x", -10.5, 57.6), ("wall_z", -16.4, 43), ("wall_x", 9.5, 36.2), ("well", 5.4, 50.6),
        ("haystack", 12.5, 33.5)]
world["layout"] += [L(i, x, z) for i, x, z in more]
print("layout", len(world["layout"]), "paths", len(world["paths"]), "burn", BZ, "loch", LZ)
json.dump(world, open(os.path.join(HERE, "world.json"), "w"), indent=1)
print("objects", len(O), "layout", len(layout))
