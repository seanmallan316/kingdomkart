# kkm_lod.py - makes the low-detail copy of each 3D kart (content/models/<name>_lo.kkm) for karts far away, the other
# players' karts in split-screen and the flat shadows. Same parts, same pivots, same picture; about a tenth of the shapes.
#   blender -b --python tools/model3d/kkm_lod.py -- content/models/jesus.kkm [more.kkm ...]
import bpy, bmesh, struct, sys, os
TARGET = {0: 1500, 1: 160, 2: 160, 3: 160, 4: 160, 5: 600, 6: 800}   # triangles per part kind (body, 4 wheels, torso, head); the rest: arms etc.
def read(path):
    d = open(path, 'rb').read(); assert d[:4] == b'KKM1'; n = struct.unpack('>I', d[4:8])[0]; o = 8; parts = []
    for _ in range(n):
        kind = struct.unpack('>I', d[o:o + 4])[0]; hdr = struct.unpack('>4f', d[o + 4:o + 20]); cnt = struct.unpack('>I', d[o + 20:o + 24])[0]; o += 24
        v = struct.unpack('>%df' % (12 * cnt), d[o:o + 48 * cnt]) if cnt else (); o += 48 * cnt; parts.append((kind, hdr, cnt, v))
    return parts
def decimate(cnt, v, target):
    if cnt == 0 or cnt // 3 <= target: return list(v)
    bm = bmesh.new(); uvl = bm.loops.layers.uv.new(); col = bm.loops.layers.color.new(); vmap = {}
    def vert(i):
        key = (round(v[i * 12] * 400), round(v[i * 12 + 1] * 400), round(v[i * 12 + 2] * 400))   # weld points closer than a few millimetres
        if key not in vmap: vmap[key] = bm.verts.new((v[i * 12], v[i * 12 + 1], v[i * 12 + 2]))
        return vmap[key]
    for t in range(cnt // 3):
        vs = [vert(t * 3 + k) for k in range(3)]
        if len(set(vs)) < 3: continue
        try: f = bm.faces.new(vs)
        except ValueError: continue
        for k, lp in enumerate(f.loops): i = t * 3 + k; lp[uvl].uv = (v[i * 12 + 6], v[i * 12 + 7]); lp[col] = (v[i * 12 + 8], v[i * 12 + 9], v[i * 12 + 10], v[i * 12 + 11])
    me = bpy.data.meshes.new('p'); bm.to_mesh(me); bm.free(); ob = bpy.data.objects.new('p', me); bpy.context.scene.collection.objects.link(ob)
    bpy.context.view_layer.objects.active = ob; ob.select_set(True)
    m = ob.modifiers.new('d', 'DECIMATE'); m.ratio = max(0.02, target / max(1, len(me.polygons))); m.use_collapse_triangulate = True
    bpy.ops.object.modifier_apply(modifier='d'); me = ob.data; me.calc_loop_triangles()
    for p in me.polygons: p.use_smooth = True
    uv = me.uv_layers[0].data if me.uv_layers else None; cl = me.color_attributes[0] if me.color_attributes else None
    out = []
    for tri in me.loop_triangles:
        for li in tri.loops:
            lo = me.loops[li]; p = me.vertices[lo.vertex_index]; n = lo.normal if hasattr(lo, 'normal') else p.normal
            u = uv[li].uv if uv else (0.5, 0.5); c = cl.data[li].color if (cl is not None and cl.domain == 'CORNER') else (1, 1, 1, 1)
            out += [p.co.x, p.co.y, p.co.z, n[0], n[1], n[2], u[0], u[1], c[0], c[1], c[2], c[3]]
    bpy.data.objects.remove(ob); bpy.data.meshes.remove(me); return out
def write(path, parts):
    with open(path, 'wb') as f:
        f.write(b'KKM1' + struct.pack('>I', len(parts)))
        for kind, hdr, v in parts: n = len(v) // 12; f.write(struct.pack('>I4fI', kind, *hdr, n)); f.write(struct.pack('>%df' % len(v), *v))
args = sys.argv[sys.argv.index('--') + 1:]
for path in args:
    parts = read(path); out = []; before = after = 0
    for kind, hdr, cnt, v in parts:
        lv = decimate(cnt, v, TARGET.get(kind, 160)); out.append((kind, hdr, lv)); before += cnt // 3; after += len(lv) // 36
    dst = path[:-4] + '_lo.kkm'; write(dst, out); print('%s: %d -> %d triangles' % (os.path.basename(dst), before, after), flush=True)
