import json, sys, time, base64, urllib.request
# meshy_split.py name : asks Meshy (Auto Split, 10 credits) to cut an approved kart into head / torso / arms / steering wheel / legs / kart,
# keeping its paint. The key is read from the scratchpad, never kept in the project.
S='/tmp/claude-0/-home-claude/fecd6cea-9518-53bf-8507-8392c7159c17/scratchpad'
K=open(S+'/.meshykey').read().strip(); B='https://api.meshy.ai/openapi/v1'
def req(m,p,b=None):
    r=urllib.request.Request(B+p,data=json.dumps(b).encode() if b is not None else None,method=m,headers={'Authorization':'Bearer '+K,'Content-Type':'application/json'})
    try: return json.load(urllib.request.urlopen(r,timeout=180))
    except urllib.error.HTTPError as e: print('HTTP',e.code,e.read()[:800],flush=True); raise
def wait(ep,tid):
    while True:
        t=req('GET',ep+'/'+tid)
        if t['status'] in ('SUCCEEDED','FAILED','CANCELED'): print(ep,t['status'],t.get('task_error'),t.get('consumed_credits'),flush=True); return t
        time.sleep(8)
def get(u,path): urllib.request.urlretrieve(u,path)
def datauri(p): return 'data:image/png;base64,'+base64.b64encode(open(p,'rb').read()).decode()
n=sys.argv[1]; tid=json.load(open(S+'/meshy/'+n+'.json'))['id']
P='head with hair, torso, left upper arm, left forearm with hand, right upper arm, right forearm with hand, steering wheel, legs, kart'
t=wait('/print/split',req('POST','/print/split',{'input_task_id':tid,'mode':'by_parts','prompt':P,'keep_texture':True})['result'])
json.dump(t,open(n+'_split2.json','w')); get(t['model_urls']['glb'],n+'_split2.glb')
