import json, sys, time, base64, urllib.request
S='/tmp/claude-0/-home-claude/fecd6cea-9518-53bf-8507-8392c7159c17/scratchpad'
K=open(S+'/.meshykey').read().strip(); B='https://api.meshy.ai/openapi/v1'   # the Meshy key is kept out of the project
def req(m,p,b=None):
    r=urllib.request.Request(B+p,data=json.dumps(b).encode() if b is not None else None,method=m,headers={'Authorization':'Bearer '+K,'Content-Type':'application/json'})
    try: return json.load(urllib.request.urlopen(r,timeout=120))
    except urllib.error.HTTPError as e: print('HTTP',e.code,e.read()[:500],flush=True); raise
def wait(ep,tid):
    while True:
        t=req('GET',ep+'/'+tid)
        if t['status'] in ('SUCCEEDED','FAILED','CANCELED'): print(ep,t['status'],t.get('task_error'),flush=True); return t
        time.sleep(8)
uri='data:image/png;base64,'+base64.b64encode(open(S+'/gol/gstand_0.png','rb').read()).decode()
t=wait('/image-to-3d',req('POST','/image-to-3d',{'image_url':uri,'ai_model':'latest','should_texture':True,'should_remesh':True,'target_polycount':9000,'topology':'triangle','pose_mode':'a-pose'})['result'])
json.dump(t,open(S+'/gol/model.json','w')); urllib.request.urlretrieve(t['model_urls']['glb'],S+'/gol/model.glb')
r=wait('/rigging',req('POST','/rigging',{'input_task_id':t['id'],'height_meters':6.0})['result'])
json.dump(r,open(S+'/gol/rig.json','w'))
ids=[0,119,512,421,51,178,391,184]   # idle, walk, charge run, throw, shout, hit, head hold (dazed), fall forward
a=wait('/animations',req('POST','/animations',{'rig_task_id':r['id'],'action_ids':ids,'post_process':{'operation_type':'change_fps','fps':24}})['result'])
json.dump(a,open(S+'/gol/anim.json','w'))
res=a.get('result') or {}
u=res.get('animation_glb_url') if isinstance(res,dict) else None
if not u: u=a.get('animation_glb_url')
print('glb',bool(u)); urllib.request.urlretrieve(u,S+'/gol/anim.glb')
