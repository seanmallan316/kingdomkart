#!/usr/bin/env python3
"""Makes the 3D props for the cup winners' Bible story scenes with Meshy (text to 3D, preview then textured refine)
and downloads each as <scratch>/props/<name>.glb. The Meshy key is read from the scratch folder, never the project.
   python3 tools/model3d/meshy_props.py [name ...]      (no names: every prop not already downloaded)"""
import json, os, sys, time, urllib.request, threading
S = os.environ.get('KK_SCRATCH', '/tmp/claude-0/-home-claude/fecd6cea-9518-53bf-8507-8392c7159c17/scratchpad')
K = open(S + '/.meshykey').read().strip(); B = 'https://api.meshy.ai/openapi/v2'
STYLE = ', stylized 3D game asset, hand-painted cartoon style like a family kart racing game, warm vibrant colors, clean readable shapes, single object, no ground plane'
PROPS = {   # name: (what it is, triangles)
    'ark':          ("Noah's ark: a huge long wooden ship of gopher wood with a wooden house built along its deck, a side door and a row of small windows", 7000),
    'lion':         ('a male lion lying down peacefully resting, mouth closed, big golden mane', 5000),
    'sheep':        ('a fluffy white sheep standing, woolly coat, black face and legs', 3000),
    'palm':         ('a tall date palm tree with a ringed trunk, big green fronds and clusters of dates', 4000),
    'boat':         ('an ancient wooden fishing boat from the Sea of Galilee with one mast and a square cloth sail and oars', 4000),
    'pyramid':      ('an ancient Egyptian pyramid built of stacked sandstone blocks, golden capstone', 2000),
    'pillar':       ('a tall ancient stone temple column with a carved capital and square base, weathered sandstone', 2500),
    'throne':       ('a Persian royal throne of gold with purple cushions on a three-step golden dais, a golden sceptre leaning on it', 5000),
    'house':        ('an ancient Middle Eastern mud-brick house with a flat roof, an outside stair to the roof, a wooden door and a small window', 3000),
    'villager_sit': ('a Bible-times villager sitting cross-legged on the ground listening, wearing a simple robe, sash and head cloth', 4000),
    'villager':     ('a Bible-times villager standing, wearing a simple robe, sash, sandals and head cloth', 4000),
    'dove':         ('a white dove flying with its wings spread wide', 2000),
    'heaven_gate':  ('the gates of heaven: a tall arched gateway of white marble and gold with pearl gates, glowing', 6000),
    'olive':        ('a gnarled old olive tree with a twisted trunk and silvery green leaves', 4000),
    'sacks':        ('a pile of burlap sacks full of grain, some open showing wheat', 2500),
    'boulder':      ('a large rough sandstone boulder rock', 1500),
    'cloud':        ('a big fluffy white cumulus cloud, soft round puffs', 2000),
}
def req(m, p, b=None):
    r = urllib.request.Request(B + p, data=json.dumps(b).encode() if b is not None else None, method=m, headers={'Authorization': 'Bearer ' + K, 'Content-Type': 'application/json'})
    for tries in range(5):
        try: return json.load(urllib.request.urlopen(r, timeout=120))
        except urllib.error.HTTPError as e:
            msg = e.read()[:300]; print('HTTP', e.code, msg, flush=True)
            if e.code in (429, 500, 502, 503): time.sleep(20); continue
            raise
        except Exception as e: print('net', e, flush=True); time.sleep(10)
    raise RuntimeError('meshy unreachable')
def wait(tid):
    while True:
        t = req('GET', '/text-to-3d/' + tid)
        if t['status'] in ('SUCCEEDED', 'FAILED', 'CANCELED'): return t
        time.sleep(10)
def make(name):
    os.makedirs(S + '/props', exist_ok=True); out = S + '/props/' + name + '.glb'
    if os.path.exists(out): print(name, 'already here'); return
    what, tris = PROPS[name]
    p = wait(req('POST', '/text-to-3d', {'mode': 'preview', 'prompt': (what + STYLE)[:600], 'art_style': 'realistic', 'ai_model': 'latest', 'topology': 'triangle', 'target_polycount': tris, 'should_remesh': True})['result'])
    print(name, 'shape', p['status'], p.get('task_error'), flush=True)
    if p['status'] != 'SUCCEEDED': return
    r = wait(req('POST', '/text-to-3d', {'mode': 'refine', 'preview_task_id': p['id'], 'enable_pbr': False, 'texture_prompt': what + ', hand-painted cartoon texture, bright colors'})['result'])
    print(name, 'paint', r['status'], r.get('task_error'), flush=True)
    if r['status'] != 'SUCCEEDED': return
    json.dump(r, open(S + '/props/' + name + '.json', 'w')); urllib.request.urlretrieve(r['model_urls']['glb'], out); print(name, 'saved', flush=True)
if __name__ == '__main__':
    names = sys.argv[1:] or list(PROPS)
    th = []
    for i, n in enumerate(names):   # a few at a time
        t = threading.Thread(target=make, args=(n,)); t.start(); th.append(t); time.sleep(3)
        if len([x for x in th if x.is_alive()]) >= 6:
            while len([x for x in th if x.is_alive()]) >= 6: time.sleep(5)
    for t in th: t.join()
    print('ALL DONE', flush=True)
