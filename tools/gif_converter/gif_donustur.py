# gif_donustur.py - GIF / PNG / JPG dosyasini cihaz icin 320x240 GIF'e cevirir, boyutu kucuk tutar.
# Kullanim: cift tikla (pencere acilir) ya da dosyayi gif_donustur.bat uzerine surukle.
import os, sys
from PIL import Image, ImageSequence

W, H = 320, 240
MAX_KB_DEVICE = 900          # cihaz siniri

def kare_hazirla(im):
    """Ekrani dolduracak sekilde buyut, ortadan kirp."""
    im = im.convert("RGB")
    s = max(W / im.width, H / im.height)
    nw, nh = max(W, round(im.width * s)), max(H, round(im.height * s))
    im = im.resize((nw, nh), Image.LANCZOS)
    l, t = (nw - W) // 2, (nh - H) // 2
    return im.crop((l, t, l + W, t + H))

def kareleri_oku(yol, bas_sn, bit_sn):
    im = Image.open(yol)
    kareler, sureler, t = [], [], 0.0
    for fr in ImageSequence.Iterator(im):
        d = fr.info.get("duration", 80) or 80
        d = max(d, 40)
        bitis = t + d / 1000.0
        if bitis > bas_sn and (bit_sn <= 0 or t < bit_sn):
            kareler.append(kare_hazirla(fr.copy()))
            sureler.append(d)
        t = bitis
    if not kareler:
        raise ValueError("Secilen aralikta kare yok")
    return kareler, sureler

def kaydet(kareler, sureler, cikti, renk, adim):
    k, s = [], []
    for i in range(0, len(kareler), adim):
        k.append(kareler[i].quantize(renk, method=Image.MEDIANCUT, dither=Image.NONE))
        s.append(sum(sureler[i:i + adim]))
    k[0].save(cikti, save_all=True, append_images=k[1:], duration=s, loop=0, optimize=True, disposal=1)
    return os.path.getsize(cikti) / 1024.0, len(k)

def donustur(yol, hedef_kb=300, bas_sn=0.0, bit_sn=0.0, log=print):
    kareler, sureler = kareleri_oku(yol, bas_sn, bit_sn)
    taban = os.path.splitext(yol)[0]
    cikti = taban + "_320x240.gif"
    hedef_kb = min(hedef_kb, MAX_KB_DEVICE)
    log("%d kare okundu (%.1f sn)" % (len(kareler), sum(sureler) / 1000.0))
    son = None
    for adim in (1, 2, 3, 4):
        for renk in (128, 64, 32, 16):
            kb, n = kaydet(kareler, sureler, cikti, renk, adim)
            son = (kb, n, renk, adim)
            log("  deneme: %d renk, %d kareden 1 -> %.0f KB (%d kare)" % (renk, adim, kb, n))
            if kb <= hedef_kb:
                return cikti, kb, n
    return cikti, son[0], son[1]   # hedefe inemedi: en kucuk hali kalir

def komut_satiri(dosyalar):
    for d in dosyalar:
        try:
            c, kb, n = donustur(d)
            print("TAMAM: %s  %.0f KB, %d kare" % (c, kb, n))
        except Exception as e:
            print("HATA: %s: %s" % (d, e))
    input("Kapatmak icin Enter...")

def pencere():
    import tkinter as tk
    from tkinter import filedialog, messagebox
    r = tk.Tk(); r.title("GIF Donusturucu 320x240"); r.geometry("460x420")
    yol = tk.StringVar(); hedef = tk.StringVar(value="300"); bas = tk.StringVar(value="0"); bit = tk.StringVar(value="0")
    tk.Label(r, text="GIF / PNG / JPG sec:").pack(anchor="w", padx=10, pady=(10, 0))
    tk.Entry(r, textvariable=yol, width=60).pack(padx=10)
    def sec():
        p = filedialog.askopenfilename(filetypes=[("Resim", "*.gif *.png *.jpg *.jpeg *.bmp *.webp")])
        if p: yol.set(p)
    tk.Button(r, text="Dosya sec...", command=sec).pack(pady=4)
    f = tk.Frame(r); f.pack(pady=6)
    tk.Label(f, text="Baslangic (sn):").grid(row=0, column=0, sticky="e"); tk.Entry(f, textvariable=bas, width=6).grid(row=0, column=1)
    tk.Label(f, text="Bitis (sn, 0=sonuna kadar):").grid(row=1, column=0, sticky="e"); tk.Entry(f, textvariable=bit, width=6).grid(row=1, column=1)
    tk.Label(f, text="Hedef boyut (KB, en cok 900):").grid(row=2, column=0, sticky="e"); tk.Entry(f, textvariable=hedef, width=6).grid(row=2, column=1)
    kutu = tk.Text(r, height=12, width=58); kutu.pack(padx=10, pady=6)
    def yaz(m): kutu.insert("end", m + "\n"); kutu.see("end"); r.update()
    def calistir():
        if not yol.get(): messagebox.showinfo("GIF", "Once dosya sec"); return
        kutu.delete("1.0", "end")
        try:
            c, kb, n = donustur(yol.get(), float(hedef.get()), float(bas.get()), float(bit.get()), yaz)
            yaz("\nTAMAM: %s\n%.0f KB, %d kare" % (c, kb, n))
            if kb > MAX_KB_DEVICE: yaz("UYARI: 900 KB ustu, cihaz kabul etmez. Aralik kisalt.")
        except Exception as e:
            yaz("HATA: %s" % e)
    tk.Button(r, text="Donustur", command=calistir, bg="#ff8fc7").pack()
    r.mainloop()

if __name__ == "__main__":
    if len(sys.argv) > 1: komut_satiri(sys.argv[1:])
    else: pencere()
