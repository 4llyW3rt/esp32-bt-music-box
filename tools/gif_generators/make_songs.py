import math, os
from PIL import Image, ImageDraw, ImageFont
W,H=320,240; S=2
DST=os.environ.get("OUT","out")+"/"; os.makedirs(DST, exist_ok=True)
FB="/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf"
def new(bg): 
    im=Image.new("RGB",(W*S,H*S),bg); return im,ImageDraw.Draw(im)
def fin(im): return im.resize((W,H),Image.LANCZOS)
def E(d,cx,cy,rx,ry,fill,outline=None,w=1): d.ellipse([(cx-rx)*S,(cy-ry)*S,(cx+rx)*S,(cy+ry)*S],fill=fill,outline=outline,width=w*S)
def P(d,pts,fill): d.polygon([(x*S,y*S) for x,y in pts],fill=fill)
def R(d,x0,y0,x1,y1,fill,r=0):
    if r: d.rounded_rectangle([x0*S,y0*S,x1*S,y1*S],radius=r*S,fill=fill)
    else: d.rectangle([x0*S,y0*S,x1*S,y1*S],fill=fill)
def L(d,p,q,w,c): d.line([p[0]*S,p[1]*S,q[0]*S,q[1]*S],fill=c,width=int(w*S))
def thick(d,pts,w,c):
    for i in range(len(pts)-1): L(d,pts[i],pts[i+1],w,c)
    for p in pts: E(d,p[0],p[1],w/2,w/2,c)
def save(frames,dur,name,colors=32):
    q=[f.quantize(colors,method=Image.MEDIANCUT,dither=Image.NONE) for f in frames]
    q[0].save(DST+name,save_all=True,append_images=q[1:],duration=dur,loop=0,optimize=True,disposal=1)
    print(name,os.path.getsize(DST+name),"bayt",len(frames),"kare")

# ================= KUZEY YILDIZI: kanepede uzanan kız, sadece bacak sallanır =================
def star(d,cx,cy,r,col):
    pts=[]
    for k in range(8):
        a=k*math.pi/4; rr=r if k%2==0 else r*0.28
        pts.append((cx+rr*math.sin(a),cy-rr*math.cos(a)))
    P(d,pts,col)
def kuzey(t):
    im,d=new((18,22,52))
    # pencere + kuzey yıldızı (sabit)
    R(d,196,18,296,112,(50,62,120)); R(d,200,22,292,108,(10,14,38))
    L(d,(246,22),(246,108),3,(50,62,120)); L(d,(200,65),(292,65),3,(50,62,120))
    for (x,y) in [(215,38),(270,34),(222,90),(275,88),(258,48)]: E(d,x,y,1.2,1.2,(200,210,255))
    star(d,230,50,13,(255,244,190)); E(d,230,50,3,3,(255,255,235))
    R(d,0,205,W,H,(38,32,58))
    # kanepe
    R(d,56,92,262,150,(70,98,120),10)
    R(d,28,150,280,205,(52,130,140),8)
    R(d,40,138,270,158,(76,160,168),10)
    R(d,26,112,62,205,(44,112,122),12)
    R(d,250,112,282,205,(44,112,122),12)
    R(d,34,205,48,214,(30,24,40)); R(d,262,205,276,214,(30,24,40))
    SK=(240,208,186); SK2=(214,176,154); HAIR=(92,52,34); HAIR2=(122,76,50); DRESS=(236,150,172)
    # saç: kolçağın dışından yere doğru dalgalı akar (sabit)
    pts_l=[];pts_r=[]
    for k in range(0,23):
        y=104+k*5.2
        wv=math.sin(k*0.55)*3.2
        pts_l.append((22-3+wv-(k*0.12),y)); pts_r.append((44+wv*0.7+(k*0.1),y))
    P(d,pts_l+pts_r[::-1],HAIR)
    for off in (-9,-3,4):                                  # saç telleri (açık ton çizgiler)
        thick(d,[(30+off+math.sin(k*0.55)*3,108+k*5.2) for k in range(0,22,2)],1.6,HAIR2)
    P(d,[(20,214),(26,222),(34,216),(40,222),(46,214),(44,206),(24,206)],HAIR)   # uçlar
    # minder
    E(d,60,126,22,9,(236,226,210))
    # bükük diğer bacak (arkada, sabit): kalça -> diz yukarıda -> ayak koltukta
    thick(d,[(200,136),(228,106),(254,134)],12,SK2)
    E(d,258,137,8,4.5,SK2)
    # gövde: sırtüstü uzanmış, elbise
    P(d,[(72,126),(110,120),(180,124),(216,128),(262,130),(262,146),(216,146),(110,144),(72,142)],DRESS)
    E(d,76,134,11,9,DRESS)
    # sallanan bacak (tek hareket eden parça): üst bacak -> dizin kolçak üstünde
    thick(d,[(214,138),(262,132),(284,128)],15,SK)
    a=math.radians(22*math.sin(2*math.pi*t))
    kx,ky=284,128; ln=62
    fx,fy=kx+ln*math.sin(a), ky+ln*math.cos(a)
    thick(d,[(kx,ky),(fx,fy)],13,SK)
    E(d,fx+4*math.cos(a),fy-1+3*math.sin(a),8,5,SK)
    E(d,fx+7*math.cos(a),fy+1,5,3.2,(210,90,110))
    # baş: sırtüstü, profil, yüz yukarı
    E(d,56,114,16,15,SK)
    P(d,[(66,122),(74,126),(70,132),(60,130)],SK)                       # çene/boyun
    P(d,[(40,112),(40,98),(56,92),(72,100),(70,110),(60,104),(48,106)],HAIR)   # kafa üstü saç
    P(d,[(40,112),(36,126),(44,128),(50,112)],HAIR)
    L(d,(58,112),(66,112),1.4,(70,44,44))                                # kapalı göz
    E(d,76,114,2.2,1.6,SK2)                                              # burun ucu
    L(d,(69,122),(75,121),1.2,(190,90,100))                              # dudak
    # kol: omuz -> dirsek ÜSTTE -> önkol yüze, el yanakta
    thick(d,[(92,128),(100,100)],8.5,SK)                                 # üst kol yukarı
    E(d,100,99,5.2,5.2,SK)                                               # dirsek
    thick(d,[(100,99),(78,106),(64,112)],7.5,SK)                         # önkol yüze
    E(d,62,113,7,6,SK)                                                   # el yanakta
    for k in range(3): L(d,(56+k*2.4,108+k*0.6),(62+k*2.4,119),1.1,SK2)   # parmak çizgileri
    return fin(im)
fr=[kuzey(i/20) for i in range(20)]
save(fr,80,"kuzey.gif",32)

# ================= KANUNSUZLAR: bıçak yalayan adam (stilize, çizgi film) =================
def kanun(t, n):
    # arka plan: koyu kırmızı degrade + spot
    im,d=new((22,0,8))
    for y in range(0,H,4):
        c=int(18+40*(1-abs(y-90)/150)); R(d,0,y,W,y+4,(max(c,16),0,max(6,c//4)))
    E(d,150,95,120,150,(46,0,12))
    # omuz/takım elbise
    P(d,[(40,240),(70,190),(130,172),(190,172),(250,190),(280,240)],(14,14,18))
    P(d,[(138,172),(160,215),(182,172)],(235,235,235))                 # gömlek
    P(d,[(152,176),(168,176),(172,232),(160,240),(148,232)],(150,10,24))   # kırmızı kravat
    R(d,138,150,182,178,(214,184,166))                                 # boyun
    # kafa
    E(d,160,100,45,53,(226,196,176))
    E(d,116,102,8,13,(214,182,162)); E(d,204,102,8,13,(214,182,162))   # kulaklar
    P(d,[(114,92),(120,58),(150,44),(190,46),(206,62),(208,92),(198,72),(160,62),(128,74)],(10,10,12))   # saç
    # gözler (deli bakış, bıçağa bakıyor), seğirme
    tw=1.5*math.sin(t*2*math.pi*3)
    for ex in (142,180):
        E(d,ex,92,11,8,(245,245,245),(60,20,20),1)
        E(d,ex+5,92+tw*0.3,3.4,3.4,(10,10,10)); E(d,ex+5,92+tw*0.3,1,1,(240,240,240))
    L(d,(130,76),(152,82),3,(10,10,12))                                 # kaş sol (öfkeli)
    L(d,(170,74),(192,68),3,(10,10,12))                                 # kaş sağ (kalkık)
    L(d,(158,98),(154,112),1.6,(190,150,134)); L(d,(154,112),(160,114),1.6,(190,150,134))   # burun
    # sırıtış
    P(d,[(138,126),(160,132),(184,124),(186,128),(160,142),(136,130)],(70,10,16))
    for k in range(6): L(d,(142+k*7,130+ (1 if k<3 else 0)),(143+k*7,136),2.2,(245,245,240))
    # bıçak (sağda, dikey, hafif eğik)
    P(d,[(236,36),(252,70),(250,168),(222,168),(222,70)],(205,210,222))
    P(d,[(236,36),(252,70),(244,70)],(240,244,252))                     # parıltı ucu
    L(d,(246,72),(246,166),2,(235,240,250))
    R(d,218,166,254,176,(60,50,50)); R(d,222,176,250,222,(40,26,22),3)   # siperlik + sap
    E(d,236,206,22,15,(226,196,176)); 
    for k in range(3): L(d,(222,196+k*8),(250,196+k*8),2,(190,150,134))  # parmaklar
    # dil: ağızdan bıçağa uzanır, bıçak boyunca yukarı kayar
    ph=0.5-0.5*math.cos(t*2*math.pi)
    cy=140-62*ph                                                         # temas noktası (bıçak boyunca)
    mx,my=182,130
    thick(d,[(mx,my),(197,(my+cy)/2+6),(222,cy)],9,(214,70,96))
    E(d,222,cy,5,5,(230,90,112))
    # bıçaktaki kırmızı iz: dilin çıktığı yere kadar uzar
    ytop=int(cy)
    P(d,[(222,ytop),(232,ytop-2),(240,ytop+4),(236,168),(222,168)],(150,0,22))
    # damlalar: bıçak sapının önüne düşer
    for j in range(2):
        tt=(t*1.0+j*0.5)%1.0
        dx=232+j*10; dy=160+tt*70
        E(d,dx,dy,3.2,3.2+tt*3,(170,0,24))
    # vinyet
    return fin(im)
fr=[kanun(i/24,24) for i in range(24)]
save(fr,80,"kanunsuz.gif",32)

# ---- önizleme ----
def sheet(path,names,idx):
    ims=[Image.open(DST+n) for n in names]
    out=Image.new("RGB",(W*len(idx),H*len(names)))
    for r,im in enumerate(ims):
        for c,i in enumerate(idx):
            im.seek(i); out.paste(im.convert("RGB"),(c*W,r*H))
    out.save(path)
sheet(DST+"songs_preview.png",["kuzey.gif","kanunsuz.gif"],[0,5,10,15])
