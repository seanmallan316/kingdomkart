# Blender (headless): import the raw image-to-3D model, keep the main piece, shrink it to a Wii U size, smooth it, unwrap it
import bpy, sys, bmesh
argv = sys.argv[sys.argv.index('--') + 1:]; src, dst, target = argv[0], argv[1], int(argv[2])
bpy.ops.wm.read_factory_settings(use_empty=True)
bpy.ops.import_scene.gltf(filepath=src)
obs = [o for o in bpy.context.scene.objects if o.type == 'MESH']
bpy.ops.object.select_all(action='DESELECT')
for o in obs: o.select_set(True)
bpy.context.view_layer.objects.active = obs[0]
if len(obs) > 1: bpy.ops.object.join()
ob = bpy.context.view_layer.objects.active
bpy.ops.object.transform_apply(location=True, rotation=True, scale=True)
# drop loose crumbs (tiny separate pieces)
bm = bmesh.new(); bm.from_mesh(ob.data); bm.verts.ensure_lookup_table()
import collections
seen = set(); groups = []
for v in bm.verts:
    if v.index in seen: continue
    st = [v]; g = []; seen.add(v.index)
    while st:
        x = st.pop(); g.append(x)
        for e in x.link_edges:
            w = e.other_vert(x)
            if w.index not in seen: seen.add(w.index); st.append(w)
    groups.append(g)
big = max(len(g) for g in groups); kill = [v for g in groups if len(g) < big * 0.01 for v in g]
bmesh.ops.delete(bm, geom=kill, context='VERTS'); bm.to_mesh(ob.data); bm.free()
print('pieces', len(groups), 'killed verts', len(kill), 'faces', len(ob.data.polygons))
m = ob.modifiers.new('dec', 'DECIMATE'); m.ratio = target / max(1, len(ob.data.polygons)); m.use_collapse_triangulate = True
bpy.ops.object.modifier_apply(modifier='dec')
bpy.ops.object.shade_smooth()
bpy.ops.object.mode_set(mode='EDIT'); bpy.ops.mesh.select_all(action='SELECT')
bpy.ops.mesh.quads_convert_to_tris()
bpy.ops.uv.smart_project(angle_limit=1.15, island_margin=0.004, area_weight=0.6)
bpy.ops.object.mode_set(mode='OBJECT')
print('final faces', len(ob.data.polygons))
bpy.ops.export_scene.gltf(filepath=dst, export_format='GLB', export_normals=True, export_texcoords=True, export_materials='NONE', export_yup=True)
