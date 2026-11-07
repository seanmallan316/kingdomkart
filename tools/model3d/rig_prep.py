# rig_prep.py (run inside Blender) split.glb out.npz out.png [ratio] [kart ratio] : the split kart -> labelled parts (glTF axes: y up, front +z), lightly reduced
import bpy,sys,bmesh,numpy as np
argv=sys.argv[sys.argv.index('--')+1:]; src,out,outpng=argv[0],argv[1],argv[2]; ratio=float(argv[3]) if len(argv)>3 else 0.5; kratio=float(argv[4]) if len(argv)>4 else ratio
bpy.ops.wm.read_factory_settings(use_empty=True); bpy.ops.import_scene.gltf(filepath=src)
obs=[o for o in bpy.context.scene.objects if o.type=='MESH']
img=None
for o in obs:
    for m in o.data.materials:
        if m and m.use_nodes:
            for nd in m.node_tree.nodes:
                if nd.type=='TEX_IMAGE' and nd.image and img is None:
                    # base colour: the image wired into Base Color
                    for l in m.node_tree.links:
                        if l.from_node==nd and l.to_socket.name=='Base Color': img=nd.image
img.filepath_raw=outpng; img.file_format='PNG'; img.save()
D={}
biggest=max(obs,key=lambda o:len(o.data.polygons))
for o in obs:
    me=o.data
    if len(me.polygons)>800:
        mod=o.modifiers.new('d','DECIMATE'); mod.ratio=kratio if o==biggest else ratio; mod.use_collapse_triangulate=True
    dg=bpy.context.evaluated_depsgraph_get(); ev=o.evaluated_get(dg); m=ev.to_mesh(); m.calc_loop_triangles()
    uvl=m.uv_layers.active.data; mw=o.matrix_world; nm=mw.to_3x3()
    P=[];N=[];UV=[]
    for lt in m.loop_triangles:
        for li in lt.loops:
            v=m.vertices[m.loops[li].vertex_index]; p=mw@v.co; n=(nm@(m.loops[li].normal if lt.use_smooth else lt.normal)).normalized()
            P.append((p.x,p.z,-p.y)); N.append((n.x,n.z,-n.y)); u=uvl[li].uv; UV.append((u[0],1-u[1]))
    D[o.name]=(np.array(P,np.float32),np.array(N,np.float32),np.array(UV,np.float32)); ev.to_mesh_clear()
    print('PART',o.name,len(me.polygons),'->',len(P)//3)
np.savez(out,**{k+'__P':v[0] for k,v in D.items()},**{k+'__N':v[1] for k,v in D.items()},**{k+'__UV':v[2] for k,v in D.items()})
