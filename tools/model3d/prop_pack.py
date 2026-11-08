#!/usr/bin/env python3
"""Packs a Meshy prop for the game as a one-piece .kkm (kind 0, the body) plus its picture, the same files a kart uses,
so the game loads it with kart3d_load and draws it with kart3d_draw.
   python3 tools/model3d/prop_pack.py <in.glb> <out base> <size m> [len|height] [max triangles]
The prop is sized so its height (or its longest side across the ground, 'len') is <size> metres, stands on y = 0,
is centred on x and z, faces +z, and its picture is shrunk to 512 x 512 to keep the SD card small."""
import os, subprocess, sys, struct, tempfile, numpy as np
from PIL import Image
HERE = os.path.dirname(os.path.abspath(__file__))
BL = r'''
import bpy, sys, numpy as np
a = sys.argv[sys.argv.index('--') + 1:]; src, out, outpng, maxt, smooth = a[0], a[1], a[2], int(a[3]), a[4] == '1'
bpy.ops.wm.read_factory_settings(use_empty=True); bpy.ops.import_scene.gltf(filepath=src)
obs = [o for o in bpy.context.scene.objects if o.type == 'MESH']; img = None
for o in obs:
    for m in o.data.materials:
        if m and m.use_nodes:
            for l in m.node_tree.links:
                if l.from_node.type == 'TEX_IMAGE' and l.to_socket.name == 'Base Color' and img is None: img = l.from_node.image
img.filepath_raw = outpng; img.file_format = 'PNG'; img.save()
tot = sum(len(o.data.polygons) for o in obs); P = []; N = []; UV = []
for o in obs:
    if smooth:
        o.data.shade_smooth()
    if tot > maxt: mod = o.modifiers.new('d', 'DECIMATE'); mod.ratio = maxt / tot; mod.use_collapse_triangulate = True
    dg = bpy.context.evaluated_depsgraph_get(); ev = o.evaluated_get(dg); m = ev.to_mesh(); m.calc_loop_triangles()
    uvl = m.uv_layers.active.data; mw = o.matrix_world; nm = mw.to_3x3()
    for lt in m.loop_triangles:
        for li in lt.loops:
            v = m.vertices[m.loops[li].vertex_index]; p = mw @ v.co; n = (nm @ (m.loops[li].normal if lt.use_smooth else lt.normal)).normalized()
            P.append((p.x, p.z, -p.y)); N.append((n.x, n.z, -n.y)); u = uvl[li].uv; UV.append((u[0], 1 - u[1]))
    ev.to_mesh_clear()
np.savez(out, P=np.array(P, np.float32), N=np.array(N, np.float32), UV=np.array(UV, np.float32))
'''
def main():
    src, base, size = sys.argv[1], sys.argv[2], float(sys.argv[3]); how = sys.argv[4] if len(sys.argv) > 4 else 'height'; maxt = int(sys.argv[5]) if len(sys.argv) > 5 else 4000; smooth = sys.argv[6] if len(sys.argv) > 6 else '0'   # 1: soft rounded shading (clouds, animals, people, trees)
    td = tempfile.mkdtemp(); sp = os.path.join(td, 'p.py'); open(sp, 'w').write(BL)
    subprocess.run(['blender', '-b', '--python', sp, '--', src, td + '/m.npz', td + '/t.png', str(maxt), smooth], check=True, stdout=subprocess.DEVNULL)
    d = np.load(td + '/m.npz'); P, N, UV = d['P'].astype(np.float64), d['N'], d['UV']
    P[:, 1] -= P[:, 1].min(); P[:, 0] -= (P[:, 0].max() + P[:, 0].min()) / 2; P[:, 2] -= (P[:, 2].max() + P[:, 2].min()) / 2
    s = size / (P[:, 1].max() if how == 'height' else max(np.ptp(P[:, 0]), np.ptp(P[:, 2]))); P *= s
    V = np.zeros((len(P), 12), np.float32); V[:, 0:3] = P; V[:, 3:6] = N; V[:, 6:8] = UV; V[:, 8:12] = 1
    with open(base + '.kkm', 'wb') as f:
        f.write(b'KKM1'); f.write(struct.pack('>I', 1)); f.write(struct.pack('>I4fI', 0, 0, 0, 0, 0, len(V))); f.write(V.astype('>f4').tobytes())
    Image.open(td + '/t.png').convert('RGB').resize((512, 512), Image.LANCZOS).save(base + '.png')
    print('%s: %d triangles, %.1f x %.1f x %.1f m, %d KB' % (os.path.basename(base), len(P) // 3, np.ptp(P[:, 0]), np.ptp(P[:, 1]), np.ptp(P[:, 2]), os.path.getsize(base + '.kkm') // 1024))
if __name__ == '__main__': main()
