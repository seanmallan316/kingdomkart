#!/usr/bin/env python3
"""meshy_racer.py name image.png : makes a racer's kart-and-driver in 3D from Sean's front picture (Meshy image to 3D,
textured), saved as <scratch>/meshy/<name>.json and .glb, ready for meshy_split.py and rig_prep.py. Key read from the scratchpad."""
import json, sys, time, base64, urllib.request
S='/tmp/claude-0/-home-claude/fecd6cea-9518-53bf-8507-8392c7159c17/scratchpad'
K=open(S+'/.meshykey').read().strip(); B='https://api.meshy.ai/openapi/v1'
def req(m,p,b=None):
    r=urllib.request.Request(B+p,data=json.dumps(b).encode() if b is not None else None,method=m,headers={'Authorization':'Bearer '+K,'Content-Type':'application/json'})
    try: return json.load(urllib.request.urlopen(r,timeout=180))
    except urllib.error.HTTPError as e: print('HTTP',e.code,e.read()[:500],flush=True); raise
def wait(ep,tid):
    while True:
        t=req('GET',ep+'/'+tid)
        if t['status'] in ('SUCCEEDED','FAILED','CANCELED'): print(ep,t['status'],t.get('task_error'),t.get('consumed_credits'),flush=True); return t
        time.sleep(8)
n,img=sys.argv[1],sys.argv[2]
uri='data:image/png;base64,'+base64.b64encode(open(img,'rb').read()).decode()
t=wait('/image-to-3d',req('POST','/image-to-3d',{'image_url':uri,'ai_model':'latest','should_texture':True,'should_remesh':True,'target_polycount':12000,'topology':'triangle'})['result'])
json.dump(t,open(S+'/meshy/'+n+'.json','w')); urllib.request.urlretrieve(t['model_urls']['glb'],S+'/meshy/'+n+'.glb'); print('saved')
