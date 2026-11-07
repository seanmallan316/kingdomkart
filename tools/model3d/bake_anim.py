import bpy, sys, struct, numpy as np
argv=sys.argv[sys.argv.index('--')+1:]; src, out = argv[0], argv[1]
bpy.ops.wm.read_factory_settings(use_empty=True)
bpy.ops.import_scene.gltf(filepath=src)
arm=[o for o in bpy.context.scene.objects if o.type=='ARMATURE'][0]
me=[o for o in bpy.context.scene.objects if o.type=='MESH' and len(o.data.polygons)>1000][0]
for t in list(arm.animation_data.nla_tracks): arm.animation_data.nla_tracks.remove(t)
CL=[('idle','Idle_Armature',12,1),('walk','Slow_Orc_Walk_Armature',16,1),('run','Male_Head_Down_Charge_Armature',8,1),('throw','Over_Shoulder_Throw_Armature',18,0),
    ('roar','Shouting_Angrily_Armature',20,0),('hit','Hit_Reaction_Armature',8,0),('daze','Head_Hold_in_Pain_Armature',14,1),('fall','Shot_and_Fall_Forward_Armature',14,0)]
dg=bpy.context.evaluated_depsgraph_get()
# corner layout from the rest mesh
m0=me.data; m0.calc_loop_triangles(); uvl=m0.uv_layers.active.data
keys={}; cv=[]; cuv=[]; tris=[]
for lt in m0.loop_triangles:
    tri=[]
    for li in lt.loops:
        vi=m0.loops[li].vertex_index; uv=(round(uvl[li].uv[0],5), round(1.0-uvl[li].uv[1],5)); k=(vi,uv)
        if k not in keys: keys[k]=len(cv); cv.append(vi); cuv.append(uv)
        tri.append(keys[k])
    tris.append(tri)
cv=np.array(cv); print('corners',len(cv),'tris',len(tris))
def grab():
    dg.update(); ev=me.evaluated_get(dg); m=ev.to_mesh()
    P=np.array([(me.matrix_world@v.co)[:] for v in m.vertices]); N=np.array([(me.matrix_world.to_3x3()@v.normal).normalized()[:] for v in m.vertices]); ev.to_mesh_clear()
    P=P[cv]; N=N[cv]
    return np.stack([P[:,0],P[:,2],-P[:,1]],1), np.stack([N[:,0],N[:,2],-N[:,1]],1)
clips=[]
for name,act,nf,loop in CL:
    a=bpy.data.actions[act]; arm.animation_data.action=a; f0,f1=a.frame_range; frames=[]
    for i in range(nf):
        f=f0+(f1-f0)*i/(nf if loop else max(nf-1,1)); bpy.context.scene.frame_set(int(f), subframe=f-int(f)); frames.append(grab())
    if loop:   # keep it on the spot: take out any drift across the floor
        c0=frames[0][0][:,[0,2]].mean(0)
        frames=[(np.concatenate([P[:,:1]-(P[:,[0,2]].mean(0)-c0)[0],P[:,1:2],P[:,2:3]-(P[:,[0,2]].mean(0)-c0)[1]],1),N) for P,N in frames]
    clips.append((name,nf,(f1-f0)/24.0,loop,frames)); print(name,nf,(f1-f0)/24.0)
allP=np.concatenate([P for c in clips for P,N in c[4]]); y0=clips[0][4][0][0][:,1].min()
with open(out,'wb') as f:
    f.write(b'KKA2'); f.write(struct.pack('>III',len(cv),len(tris),len(clips)))
    f.write(np.array(cuv,'>f4').tobytes()); f.write(np.array(tris,'>u4').tobytes())
    for name,nf,dur,loop,frames in clips:
        f.write(name.encode().ljust(16,b'\0')); f.write(struct.pack('>IfI',nf,dur,loop))
        for P,N in frames:
            P=P.copy(); P[:,1]-=y0
            q=np.clip(np.round(P*4000),-32767,32767).astype('>i2'); n=np.clip(np.round(N*127),-127,127).astype('i1')
            rec=np.zeros(len(P),dtype=[('p','>i2',3),('n','i1',3),('pad','i1')]); rec['p']=q; rec['n']=n; f.write(rec.tobytes())
print('size ok', np.ptp(clips[0][4][0][0],0))
