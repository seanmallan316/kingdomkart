#!/usr/bin/env python3
"""Video motion capture: tracks the body in every frame of a reference video with MediaPipe Pose (heavy model) and
writes the 3D joint positions (metres, hip-centred "world" landmarks) plus 2D image positions to JSON.
Run: python3 tools/mocap_video.py <video.mp4> <pose_landmarker_heavy.task> <out.json>
The JSON feeds tools/retarget_video.js, which turns the joints into bone rotations on the family's skeletons."""
import sys, json, cv2, mediapipe as mp
from mediapipe.tasks import python as mpt
from mediapipe.tasks.python import vision

video, model, out = sys.argv[1:4]
opts = vision.PoseLandmarkerOptions(base_options=mpt.BaseOptions(model_asset_path=model), running_mode=vision.RunningMode.VIDEO,
                                    num_poses=1, min_pose_detection_confidence=0.4, min_tracking_confidence=0.4, output_segmentation_masks=False)
cap = cv2.VideoCapture(video); fps = cap.get(cv2.CAP_PROP_FPS) or 24
W, H = int(cap.get(cv2.CAP_PROP_FRAME_WIDTH)), int(cap.get(cv2.CAP_PROP_FRAME_HEIGHT))
frames = []
with vision.PoseLandmarker.create_from_options(opts) as lm:
    i = 0
    while True:
        ok, img = cap.read()
        if not ok: break
        res = lm.detect_for_video(mp.Image(image_format=mp.ImageFormat.SRGB, data=cv2.cvtColor(img, cv2.COLOR_BGR2RGB)), int(i * 1000 / fps))
        if res.pose_world_landmarks:
            w = res.pose_world_landmarks[0]; n = res.pose_landmarks[0]
            frames.append({'t': i / fps, 'w': [[round(p.x, 4), round(p.y, 4), round(p.z, 4), round(p.visibility, 3)] for p in w],
                           'n': [[round(p.x, 4), round(p.y, 4), round(p.z, 4)] for p in n]})
        else:
            frames.append({'t': i / fps, 'w': None, 'n': None})
        i += 1
json.dump({'fps': fps, 'size': [W, H], 'frames': frames}, open(out, 'w'))
print('frames', len(frames), 'tracked', sum(1 for f in frames if f['w']))
