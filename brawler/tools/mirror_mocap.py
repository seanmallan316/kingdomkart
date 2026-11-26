#!/usr/bin/env python3
"""Mirrors a mocap JSON (tools/mocap_video.py) left-right, as if the video were flipped: a performer attacking to the
left of the frame then attacks to the right (the fighter's forward). Left and right joints swap so the body stays correct.
Run: python3 tools/mirror_mocap.py <in.json> <out.json>"""
import sys, json
d = json.load(open(sys.argv[1]))
# MediaPipe pose: 0 nose; 1-6 eyes; 7/8 ears; 9/10 mouth; then left/right pairs from 11 up
SWAP = list(range(33))
for a, b in [(1, 4), (2, 5), (3, 6), (7, 8), (9, 10)] + [(i, i + 1) for i in range(11, 33, 2)]:
    SWAP[a], SWAP[b] = b, a
for f in d['frames']:
    if not f['w']: continue
    f['w'] = [[-f['w'][SWAP[j]][0]] + f['w'][SWAP[j]][1:] for j in range(33)]
    f['n'] = [[1 - f['n'][SWAP[j]][0]] + f['n'][SWAP[j]][1:] for j in range(33)]
json.dump(d, open(sys.argv[2], 'w'))
print('mirrored', len(d['frames']), 'frames')
