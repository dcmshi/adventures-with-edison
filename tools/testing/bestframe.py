"""bestframe.py PORTDIR ORIG.png [x0 y0 x1 y1]: the port frame closest to the original (in the view without the tubes)."""
import sys, glob
from PIL import Image, ImageChops
box=tuple(map(int,sys.argv[3:7])) if len(sys.argv)>6 else (60,12,600,300)
o=Image.open(sys.argv[2]).convert('RGB').crop(box)
best=None
for f in sorted(glob.glob(sys.argv[1]+'/*.bmp')):
    df=ImageChops.difference(Image.open(f).convert('RGB').crop(box),o).convert('L').point(lambda v:255 if v>24 else 0)
    c=sum(1 for v in df.getdata() if v)
    if best is None or c<best[0]: best=(c,f,df.getbbox())
print(best[0], best[1][-9:], best[2])
