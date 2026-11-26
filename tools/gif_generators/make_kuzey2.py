import math, os, random
from PIL import Image, ImageDraw, ImageFilter
W,H=320,240; S=2; OFF=-12
DST=os.environ.get("OUT","out")+"/"; os.makedirs(DST, exist_ok=True)
def Y(y): return (y+OFF)*S
def X(x): return x*S
def E(d,cx,cy,rx,ry,fill,ow=0,oc=None): d.ellipse([X(cx-rx),Y(cy-ry),X(cx+rx),Y(cy+ry)],fill=fill,outline=oc,width=ow*S)
def P(d,pts,fill): d.polygon([(X(x),Y(y)) for x,y in pts],fill=fill)
def R(d,x0,y0,x1,y1,fill,r=0):
    if r: d.rounded_rectangle([X(x0),Y(y0),X(x1),Y(y1)],radius=r*S,fill=fill)
    else: d.rectangle([X(x0),Y(y0),X(x1),Y(y1)],fill=fill)
def L(d,p,q,w,c): d.line([X(p[0]),Y(p[1]),X(q[0]),Y(q[1])],fill=c,width=max(1,int(w*S)))
def tube(d,pts,w,c):
    for i in range(len(pts)-1): L(d,pts[i],pts[i+1],w,c)
    for p in pts: E(d,p[0],p[1],w/2,w/2,c)

SK=(244,208,192); SK2=(222,170,152); HAIR=(104,76,64); HAIR2=(130,98,80)
# --- arka plan: gece + sağ üstten mor ışık + bokeh ---
def background():
    im=Image.new("RGB",(W*S,H*S),(12,14,44)); d=ImageDraw.Draw(im)
    for y in range(0,H*S,4):
        t=y/(H*S); c=(int(12+8*t),int(14+6*t),int(44+14*t)); d.rectangle([0,y,W*S,y+4],fill=c)
    glow=Image.new("RGB",(W*S,H*S),(0,0,0)); g=ImageDraw.Draw(glow)
    g.ellipse([X(190),Y(-80),X(380),Y(150)],fill=(70,70,150))
    glow=glow.filter(ImageFilter.GaussianBlur(40*S/2))
    from PIL import ImageChops
    im=ImageChops.add(im,glow)
    d=ImageDraw.Draw(im); rnd=random.Random(7)
    for k in range(34):
        x=rnd.randint(150,318); y=rnd.randint(0,95); r=rnd.choice([0.8,1,1,1.4])
        E(d,x,y,r,r,(210,215,255))
    for (x,y,r) in [(268,38,5),(238,62,4),(300,70,3)]: E(d,x,y,r,r,(230,225,255))
    return im
def couch(d):
    # sırt dayanağı
    R(d,60,66,306,138,(110,72,140),12)
    R(d,126,66,200,138,(78,112,176),0); R(d,200,66,262,138,(168,128,200),0); R(d,262,66,306,138,(190,176,150),12)
    R(d,60,66,126,138,(112,60,100),12)
    # arka yastıklar
    P(d,[(40,102),(98,96),(108,152),(48,162)],(150,98,176))
    R(d,192,100,246,146,(70,110,172),8)
    P(d,[(252,100),(294,96),(298,148),(256,152)],(176,104,126))
    # sol kolçak
    R(d,6,108,80,206,(92,52,128),16); R(d,6,108,80,150,(62,104,156),16)
    # koltuk
    R(d,70,142,292,184,(70,108,160),6); R(d,70,142,140,184,(176,82,124),6); R(d,140,142,176,184,(190,150,72),0)
    # battaniye
    P(d,[(214,144),(258,134),(302,152),(290,184),(222,184)],(102,92,164))
    for k in range(7): L(d,(236+k*8,184),(236+k*8,196),1.4,(130,120,190))
    # sağ kolçak
    R(d,272,112,314,196,(112,72,152),14)
    # ön taban
    R(d,56,182,300,208,(66,42,106),6)
    R(d,70,206,84,216,(30,20,44)); R(d,278,206,292,216,(30,20,44))
def pattern():
    im=Image.new("RGB",(W*S,H*S),(246,242,248)); d=ImageDraw.Draw(im); rnd=random.Random(3)
    cols=[(240,120,150),(250,200,90),(90,170,220),(120,200,140),(250,150,80),(170,120,220)]
    for k in range(190):
        x=rnd.randint(0,W); y=rnd.randint(40,260)-OFF; r=rnd.randint(3,6)
        E(d,x,y-OFF*0,r,r*rnd.uniform(0.7,1.2),rnd.choice(cols))
    return im
PAT=pattern()
BASE=background(); couch(ImageDraw.Draw(BASE))

def frame(t):
    im=BASE.copy()
    # --- pijama maskesi (statik + sarkan bacak) ---
    m=Image.new("L",(W*S,H*S),0); md=ImageDraw.Draw(m)
    def mp(pts): md.polygon([(X(x),Y(y)) for x,y in pts],fill=255)
    def mt(pts,w):
        for i in range(len(pts)-1): md.line([X(pts[i][0]),Y(pts[i][1]),X(pts[i+1][0]),Y(pts[i+1][1])],fill=255,width=int(w*S))
        for p in pts: md.ellipse([X(p[0]-w/2),Y(p[1]-w/2),X(p[0]+w/2),Y(p[1]+w/2)],fill=255)
    mp([(104,124),(140,118),(196,150),(178,176),(112,162)])         # gövde
    mt([(112,134),(88,152)],14); mt([(88,152),(97,128)],12)         # yanağa dayalı kol
    mt([(136,128),(160,144)],12)                                    # karındaki kol
    mt([(172,160),(214,114)],22); mt([(214,114),(243,150)],18)      # bükük bacak
    mt([(168,162),(188,182)],22)                                    # sarkan bacağın üstü
    a=math.radians(17*math.sin(2*math.pi*t)); kx,ky=188,182; ln=40
    fx,fy=kx+ln*math.sin(a),ky+ln*math.cos(a)
    mt([(kx,ky),(fx,fy)],17)                                        # sarkan bacak (HAREKET)
    im.paste(PAT,(0,0),m)
    d=ImageDraw.Draw(im)
    # pembe paçalar/manşetler
    E(d,243,149,10,5,(214,96,160)); E(d,97,132,7,3.4,(214,96,160))
    E(d,fx+ln*0+0.0,fy-1,9.5,4.4,(214,96,160))
    # çıplak ayaklar
    E(d,252,156,12,6.5,SK)
    for k in range(4): E(d,261+k*0.0,152+k*2.4,2.2,2.2,SK2)
    E(d,fx+4*math.cos(a)+2,fy+13,8.8,13.5,SK)
    # --- baş ---
    E(d,112,82,39,34,HAIR)                                          # arka saç
    P(d,[(76,88),(70,112),(80,124),(88,104)],HAIR); P(d,[(146,86),(156,108),(148,120),(140,100)],HAIR)   # yan dalgalar
    E(d,118,98,26.5,28.5,SK)                                        # yüz
    P(d,[(90,86),(96,66),(120,58),(146,70),(146,88),(130,76),(112,74),(100,84)],HAIR)                    # kakül
    for (x0,y0,x1,y1) in [(86,70,78,92),(94,64,86,84),(144,66,152,86),(148,76,158,98)]: L(d,(x0,y0),(x1,y1),2,HAIR2)
    E(d,100,112,6,4,(244,176,172)); E(d,138,112,6,4,(244,176,172))   # yanaklar
    for ex in (108,132):
        E(d,ex,98,7.8,9,(250,250,252),1,(120,90,80))
        E(d,ex-2,96,5.4,6.2,(72,104,86)); E(d,ex-2,96,3,3.4,(10,10,12)); E(d,ex-3.2,93.6,1.5,1.5,(255,255,255))
    L(d,(100,90),(112,86),1.5,(80,56,48)); L(d,(126,86),(140,89),1.5,(80,56,48))   # kaşlar
    E(d,119,112,3.2,2.6,(230,150,150)); E(d,119,122,3.6,2.6,(196,96,108))
    # yanağı tutan el
    E(d,98,122,9,8,SK)
    for k in range(3): L(d,(91+k*3.4,116+k*0.8),(95+k*3.4,127),1.1,SK2)
    # karındaki el
    E(d,152,141,7,5.5,SK)
    return im.resize((W,H),Image.LANCZOS)

fr=[frame(i/20) for i in range(20)]
q=[f.quantize(48,method=Image.MEDIANCUT,dither=Image.NONE) for f in fr]
q[0].save(DST+"kuzey.gif",save_all=True,append_images=q[1:],duration=80,loop=0,optimize=True,disposal=1)
print("kuzey.gif",os.path.getsize(DST+"kuzey.gif"),"bayt")
out=Image.new("RGB",(W*3,H*2))
for c,i in enumerate([0,5,10,15,3,13]):
    q[i].seek(0) if False else None
    out.paste(fr[i],((c%3)*W,(c//3)*H))
out.save(DST+"kuzey_preview.png")
