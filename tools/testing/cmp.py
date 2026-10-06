import sys, glob, os
from PIL import Image, ImageChops
VIEW, PANEL = (54, 7, 584, 286), (40, 286, 600, 400)  # the table's view, the panel
def count(a, b, box):
    d = ImageChops.difference(a.crop(box), b.crop(box)).convert('L').point(lambda v: 255 if v > 24 else 0)
    return d.histogram()[255], d.getbbox()
def frame(dirn, ms):
    fs = sorted(glob.glob(dirn + '/*.bmp'))
    return min(fs, key=lambda f: abs(int(os.path.basename(f)[:5]) - ms))
if __name__ == '__main__':
    dirn, origdir = sys.argv[1], sys.argv[2]
    for arg in sys.argv[3:]:
        ms, q = arg.split(':')
        a = Image.open(frame(dirn, int(ms))).convert('RGB'); b = Image.open(f'{origdir}/{q}.png').convert('RGB')
        v = count(a, b, VIEW); p = count(a, b, PANEL)
        print(q, 'view', v, 'panel', p)
