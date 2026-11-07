import json, struct, numpy as np
def load_glb(path):
    b = open(path, 'rb').read(); assert b[:4] == b'glTF'
    off = 12; js = None; bin_ = None
    while off < len(b):
        ln, ty = struct.unpack('<II', b[off:off+8]); ch = b[off+8:off+8+ln]; off += 8 + ln
        if ty == 0x4E4F534A: js = json.loads(ch)
        elif ty == 0x004E4942: bin_ = ch
    def acc(i):
        a = js['accessors'][i]; bv = js['bufferViews'][a['bufferView']]; o = bv.get('byteOffset', 0) + a.get('byteOffset', 0)
        dt = {5126: np.float32, 5125: np.uint32, 5123: np.uint16, 5121: np.uint8}[a['componentType']]; nc = {'SCALAR': 1, 'VEC2': 2, 'VEC3': 3, 'VEC4': 4}[a['type']]
        return np.frombuffer(bin_, dt, a['count'] * nc, o).reshape(a['count'], nc) if nc > 1 else np.frombuffer(bin_, dt, a['count'], o)
    P = []; I = []; base = 0
    for m in js['meshes']:
        for pr in m['primitives']:
            p = acc(pr['attributes']['POSITION']).astype(np.float64); idx = acc(pr['indices']).astype(np.int64).reshape(-1, 3) if 'indices' in pr else np.arange(len(p)).reshape(-1, 3)
            P.append(p); I.append(idx + base); base += len(p)
    return np.vstack(P), np.vstack(I), js
def cluster_decimate(P, F, cells=90):   # vertex clustering: quick and robust
    lo, hi = P.min(0), P.max(0); s = (hi - lo).max() / cells
    key = np.floor((P - lo) / s).astype(np.int64); k = key[:, 0] * 1000003 + key[:, 1] * 1009 + key[:, 2]
    uk, inv = np.unique(k, return_inverse=True); NP = np.zeros((len(uk), 3)); cnt = np.zeros(len(uk))
    np.add.at(NP, inv, P); np.add.at(cnt, inv, 1); NP /= cnt[:, None]
    NF = inv[F]; ok = (NF[:, 0] != NF[:, 1]) & (NF[:, 1] != NF[:, 2]) & (NF[:, 0] != NF[:, 2]); NF = NF[ok]
    NF = np.unique(np.sort(NF, 1) * 0 + NF, axis=0)
    return NP, NF
