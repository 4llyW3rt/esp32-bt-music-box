import math, sys, os
OUT = os.environ.get("OUT", "out")          # cikti klasoru (varsayilan: ./out)
os.makedirs(OUT, exist_ok=True)
TEXT = sys.argv[2] if len(sys.argv)>2 else ""   # kapanis yazisi (bos olabilir)
from PIL import Image, ImageDraw, ImageFont
W,H=320,240; S=2
BG=(14,16,32); ORG=(240,150,50); ORD=(200,105,25); WHT=(250,240,225); PINK=(255,140,160)
RED=(220,50,60); REDD=(150,25,40); FLOOR=(48,52,84)
INI = sys.argv[1] if len(sys.argv)>1 else "|"     # kapi ustundeki iki isim/harf, "A|B" gibi; bos = sadece kalp
a,b = INI.split("|")

def new():
    im=Image.new("RGB",(W*S,H*S),BG); return im,ImageDraw.Draw(im)
def fin(im): return im.resize((W,H),Image.LANCZOS)
def e(d,cx,cy,rx,ry,fill,outline=None):
    d.ellipse([ (cx-rx)*S,(cy-ry)*S,(cx+rx)*S,(cy+ry)*S ],fill=fill,outline=outline)
def poly(d,pts,fill): d.polygon([(x*S,y*S) for x,y in pts],fill=fill)
def line(d,p,q,w,col): d.line([p[0]*S,p[1]*S,q[0]*S,q[1]*S],fill=col,width=int(w*S))

def cat(d,x,y,k=1.0,t=0.0,walk=0.0,paw=0.0,face=1):
    # side-view cat, feet at y, facing right (face=1). x = body centre
    f=face
    def X(v): return x+v*k*f
    def Y(v): return y+v*k
    tail=math.sin(t)*10
    # tail
    pts=[(X(-26),Y(-26)),(X(-40),Y(-34+tail*0.5)),(X(-46),Y(-52+tail)),(X(-40),Y(-54+tail))]
    for i in range(len(pts)-1): line(d,pts[i],pts[i+1],6*k,ORD)
    # back legs / front legs
    for i,(lx,ph) in enumerate([(-16,0),(-8,math.pi),(14,math.pi),(22,0)]):
        sw=math.sin(walk+ph)*6
        up=max(0,math.cos(walk+ph))*3
        px=lx+sw
        if i==3 and paw>0: line(d,(X(lx),Y(-14)),(X(lx+14*paw),Y(-24*paw-4)),5*k,ORG)
        else: line(d,(X(lx),Y(-14)),(X(px),Y(-up)),5*k,ORG if i>1 else ORD)
    e(d,X(0),Y(-26),28*k,14*k,ORG)
    e(d,X(0),Y(-20),20*k,7*k,WHT)
    # head
    hx,hy=X(30),Y(-40)
    e(d,hx,hy,15*k,13*k,ORG)
    poly(d,[(X(20),Y(-50)),(X(22),Y(-66)),(X(30),Y(-53))],ORG)
    poly(d,[(X(34),Y(-53)),(X(41),Y(-66)),(X(43),Y(-49))],ORG)
    poly(d,[(X(23),Y(-52)),(X(23.5),Y(-61)),(X(28),Y(-54))],PINK)
    e(d,X(36),Y(-42),3.2*k,3.6*k,(20,20,20)); e(d,X(37),Y(-43),1.1*k,1.1*k,WHT)
    e(d,X(43),Y(-37),2*k,1.5*k,PINK)
    for dy in (-2,1): line(d,(X(41),Y(-35+dy)),(X(54),Y(-37+dy*2)),0.8*k,WHT)

def yarn(d,cx,cy,r,rot=0):
    e(d,cx,cy,r,r,RED)
    for i in range(4):
        a_=rot+i*math.pi/4
        dx,dy=math.cos(a_)*r*0.8,math.sin(a_)*r*0.8
        line(d,(cx-dx,cy-dy*0.5),(cx+dx,cy+dy*0.5),max(1,r*0.12),REDD)
    e(d,cx-r*0.3,cy-r*0.35,r*0.25,r*0.18,(255,150,150))

def ease(t): return t*t*(3-2*t)

# ---------------- AĞIZ (KEDI ekranıyla aynı çizim) ----------------
def mouth(d,bi,A=150,cy=100):
    bi=max(1,int(bi))
    for dy in range(-bi,bi+1):
        r=math.sqrt(max(0.0,1-(dy*dy)/(bi*bi))); w=int(A*r)
        if w<=0: continue
        d.rectangle([(160-w)*S,(cy+dy)*S,(160+w)*S,(cy+dy+1)*S-1],fill=(123,0,0))
        ty=(dy-bi*0.55)/(bi*0.45)
        if bi>10 and -1<ty<1:
            tw=min(w,int(60*math.sqrt(1-ty*ty)))
            if tw>0: d.rectangle([(160-tw)*S,(cy+dy)*S,(160+tw)*S,(cy+dy+1)*S-1],fill=(255,129,131))
    if bi>10 and A>=140:
        ytop=cy-int(bi*0.866)
        for t in range(12):
            tw=6-t//2
            if tw>0:
                for cxx in (85,235): d.rectangle([(cxx-tw)*S,(ytop+t)*S,(cxx+tw)*S,(ytop+t+1)*S-1],fill=(255,255,255))

def sleepcat(d,x,y,k,breath):
    e(d,x-6*k,y-13*k,30*k,(13+breath)*k,ORG)
    e(d,x-4*k,y-6*k,22*k,6*k,WHT)
    e(d,x+28*k,y-9*k,13*k,11*k,ORG)
    poly(d,[(x+19*k,y-16*k),(x+22*k,y-30*k),(x+30*k,y-19*k)],ORG)
    poly(d,[(x+33*k,y-19*k),(x+40*k,y-29*k),(x+41*k,y-14*k)],ORG)
    line(d,(x+30*k,y-11*k),(x+38*k,y-11*k),1.2*k,(40,25,10))          # kapalı göz
    e(d,x+41*k,y-6*k,2*k,1.5*k,PINK)
    pts=[(x-34*k,y-12*k),(x-48*k,y-6*k),(x-40*k,y-2*k),(x-20*k,y-1*k)]
    for i in range(3): line(d,pts[i],pts[i+1],6*k,ORD)                  # kıvrık kuyruk

# ---------------- INTRO 1: uyuyan kedi, yumak, ayağa kalkma, ekrana yaklaşma, ağız ----------------
f1=[];d1=[]
N1=56
for i in range(N1):
    u=i/(N1-1); im,d=new()
    if u<0.82: line(d,(0,188),(W,188),1,FLOOR)
    if u<0.30:                                   # yumak zıplayarak gelir, kedi uyuyor
        v=u/0.30
        sleepcat(d,125,188,1.5,math.sin(u*40)*1.5)
        yx=335-135*ease(v); yy=181-abs(math.sin(math.pi*v*3))*48*(1-0.6*v)
        yarn(d,yx,yy,11,rot=-v*40)
        if v>0.5: d.text((150*S,95*S),"z",font=ImageFont.truetype("/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf",18*S),fill=(200,200,255),anchor="mm")
    elif u<0.38:                                 # yumak kediye değer, kedi uyanır
        v=(u-0.30)/0.08
        if v<0.5: sleepcat(d,125,188,1.5,0)
        else:
            cat(d,125,188,1.5,t=u*30)
            d.text((200*S,90*S),"!",font=ImageFont.truetype("/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf",30*S),fill=(255,230,80),anchor="mm")
        yarn(d,200+(1-v)*0,181,11,rot=0)
    elif u<0.50:                                 # kedi ayağa kalkar, yumağa vurur
        v=(u-0.38)/0.12
        paw=math.sin(min(1,v*1.6)*math.pi) if v<0.65 else 0
        cat(d,125,188,1.5,t=u*30,paw=paw)
        yx=200+ (0 if v<0.55 else (v-0.55)/0.45*22)
        yarn(d,yx,181,11,rot=v*10)
    elif u<0.82:                                 # yumak ve kedi ekrana doğru gelir
        v=(u-0.50)/0.32
        ev=ease(v)
        k=1.5+ev**1.6*7.0
        cat(d,125+(165-125)*ev,188+ev**1.6*300,k,t=u*30)
        r=11+ev**2.2*300
        yarn(d,222+(160-222)*ev,181+(110-181)*ev,r,rot=v*25)
    elif u<0.88:                                 # ekran kararır
        v=(u-0.82)/0.06
        c=int(150*(1-v)); d.rectangle([0,0,W*S,H*S],fill=(c,int(25*(1-v)),int(40*(1-v))))
    elif u<0.93:                                 # ağız belirir
        v=(u-0.88)/0.05
        d.rectangle([0,0,W*S,H*S],fill=(0,0,0)); mouth(d,3,A=max(2,150*ease(v)))
    else:                                        # ağız bir kez açılıp kapanır (şarkı başlıyor)
        v=(u-0.93)/0.07
        d.rectangle([0,0,W*S,H*S],fill=(0,0,0)); mouth(d,3+26*math.sin(v*math.pi)**2)
    f1.append(fin(im)); d1.append(70 if u<0.82 else 80)
d1[-1]=300

# ---------------- INTRO 2: kapı + harfler ----------------
f2=[];d2=[]
N2=40
try:
    font=ImageFont.truetype("/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf",30*S)
except: font=ImageFont.load_default()
def heart(d,cx,cy,r,col):
    pts=[]
    for k in range(60):
        t=k/60*2*math.pi
        x=16*math.sin(t)**3; y=-(13*math.cos(t)-5*math.cos(2*t)-2*math.cos(3*t)-math.cos(4*t))
        pts.append(((cx+x*r/16)*S,(cy+y*r/16)*S))
    d.polygon(pts,fill=col)
DX0,DX1,DY0,DY1=100,220,72,188
for i in range(N2):
    u=i/(N2-1); im,d=new()
    line(d,(0,188),(W,188),1,FLOOR)
    # kapı içi (aydınlık)
    d.rectangle([DX0*S,DY0*S,DX1*S,DY1*S],fill=(255,220,140))
    # kedi yürür
    if u<0.55:
        v=u/0.55
        cx=-30+ease(v)*190
        g=max(0,(cx-105)/50.0)            # kapiya yaklasinca uzaklasir (kuculur, yukari)
        g=min(1,g)
        if cx>0:
            cat(d,cx,188-28*ease(g),1.0-0.65*ease(g),t=u*14,walk=u*50)
    # kapı kanatları
    if u<0.45: open_=1.0
    elif u<0.75: open_=1-ease((u-0.45)/0.3)
    else: open_=0
    half=(DX1-DX0)/2
    lw=half*(1-open_*0.9)
    d.rectangle([DX0*S,DY0*S,(DX0+lw)*S,DY1*S],fill=(120,70,40),outline=(70,40,20),width=2*S)
    d.rectangle([(DX1-lw)*S,DY0*S,DX1*S,DY1*S],fill=(120,70,40),outline=(70,40,20),width=2*S)
    if open_<0.05:
        e(d,158,135,2.5,2.5,(255,210,80)); e(d,162,135,2.5,2.5,(255,210,80))
    # kasa
    d.rectangle([(DX0-4)*S,(DY0-4)*S,(DX1+4)*S,DY0*S],fill=(90,50,28))
    d.rectangle([(DX0-4)*S,DY0*S,DX0*S,DY1*S],fill=(90,50,28))
    d.rectangle([DX1*S,DY0*S,(DX1+4)*S,DY1*S],fill=(90,50,28))
    # harfler
    if u>0.8:
        al=min(1,(u-0.8)/0.12)
        c=(int(255*al),int(190*al+40*(1-al)),int(120*al+60*(1-al)))
        d.text((120*S,36*S),a,font=font,fill=c,anchor="mm")
        d.text((200*S,36*S),b,font=font,fill=c,anchor="mm")
        heart(d,160,36,15*(0.8+0.2*math.sin(u*60)),(255,70,110))
    f2.append(fin(im)); d2.append(85)
d2[-1]=2500

def save(frames,durs,path):
    q=[f.quantize(48,method=Image.MEDIANCUT,dither=Image.NONE) for f in frames]
    q[0].save(path,save_all=True,append_images=q[1:],duration=durs,loop=1,optimize=True,disposal=1)
    import os; print(path,os.path.getsize(path),"bayt",len(frames),"kare")
save(f1,d1,OUT+"/intro1.gif")
save(f2,d2,OUT+"/intro2_unused.gif")
# onizleme
for nm,fr in (("p1",f1),("p2",f2)):
    idx=[0,len(fr)//4,len(fr)//2,3*len(fr)//4,len(fr)-1]
    sh=Image.new("RGB",(W*5,H)); [sh.paste(fr[j],(n*W,0)) for n,j in enumerate(idx)]
    sh.save(f"{OUT}/{nm}_preview.png")

# ---------------- OUTRO: ağız kapanır, yumak zıplayarak gelir, kedi arkasından, kapı kapanır ----------------
f3=[];d3=[]
N3=60
SX0,SX1,SY0,SY1=215,285,72,188
def clamp01(x): return max(0.0,min(1.0,x))
def sdoor(d,open_):
    d.rectangle([SX0*S,SY0*S,SX1*S,SY1*S],fill=(255,220,140))
    lw=(SX1-SX0)*(1-open_*0.9)
    return lw
for i in range(N3):
    u=i/(N3-1); im,d=new()
    if u<0.10:                                    # ağız daralıp kaybolur
        v=u/0.10
        mouth(d,3+6*(1-v),A=max(2,150*(1-ease(v))))
        f3.append(fin(im)); d3.append(80); continue
    line(d,(0,188),(W,188),1,FLOOR)
    d.rectangle([SX0*S,SY0*S,SX1*S,SY1*S],fill=(255,220,140))
    # yumak: zıplayarak gelir, önce kapıdan geçer
    vy=clamp01((u-0.10)/0.45)
    yx=-12+262*ease(vy)
    gy=clamp01((u-0.52)/0.12)
    yy=181-abs(math.sin(math.pi*vy*5))*55*(1-0.7*vy)-26*ease(gy)
    yr=11*(1-0.65*ease(gy))
    if u<0.66: yarn(d,yx,yy,yr,rot=u*60)
    # kedi: arkasından, daha geç gelir
    vc=clamp01((u-0.28)/0.52)
    cx=-45+295*ease(vc)
    g=clamp01((cx-190)/55.0)
    if u<0.84 and cx>-40: cat(d,cx,188-28*ease(g),1.0-0.65*ease(g),t=u*14,walk=u*60)
    # tek kanatlı kapı: kedi geçtikten sonra kapanır
    open_=1.0 if u<0.84 else 1-ease(clamp01((u-0.84)/0.10))
    lw=(SX1-SX0)*(1-open_*0.9)
    d.rectangle([(SX1-lw)*S,SY0*S,SX1*S,SY1*S],fill=(120,70,40),outline=(70,40,20),width=2*S)
    if open_<0.05: e(d,SX0+8,135,3,3,(255,210,80))
    d.rectangle([(SX0-4)*S,(SY0-4)*S,(SX1+4)*S,SY0*S],fill=(90,50,28))
    d.rectangle([(SX0-4)*S,SY0*S,SX0*S,SY1*S],fill=(90,50,28))
    d.rectangle([SX1*S,SY0*S,(SX1+4)*S,SY1*S],fill=(90,50,28))
    # A ♥ B kapı kapanınca belirir
    if u>0.92:
        al=clamp01((u-0.92)/0.06)
        c=(int(255*al),int(190*al+40*(1-al)),int(120*al+60*(1-al)))
        if a: d.text((210*S,36*S),a,font=font,fill=c,anchor="mm")
        if b: d.text((290*S,36*S),b,font=font,fill=c,anchor="mm")
        heart(d,250,36,15*(0.85+0.15*math.sin(u*60)),(255,70,110))
    if u>0.95 and TEXT:
        al=clamp01((u-0.95)/0.04)
        tc=(int(255*al),int(170*al+40*(1-al)),int(190*al+60*(1-al)))
        ft=ImageFont.truetype("/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf",20*S)
        d.text((160*S,218*S),TEXT,font=ft,fill=tc,anchor="mm")
    f3.append(fin(im)); d3.append(85)
d3[-1]=1500
save(f3,d3,OUT+"/outro.gif")
for nm,fr in (("p1",f1),("p3",f3)):
    n=7; idx=[round(j*(len(fr)-1)/(n-1)) for j in range(n)]
    sh=Image.new("RGB",(W*4,H*2))
    for q,j in enumerate(idx[:7]): sh.paste(fr[j],((q%4)*W,(q//4)*H))
    sh.save(f"{OUT}/{nm}_preview.png")
