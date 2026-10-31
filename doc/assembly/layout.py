"""Page layout, fonts, colours and building blocks of doc/assembly.pdf. The text lives in content.py."""
import json, math
from pathlib import Path
from PIL import Image as PILImage
from reportlab.lib.pagesizes import A4
from reportlab.lib.units import mm
from reportlab.lib.colors import HexColor, white
from reportlab.pdfbase import pdfmetrics
from reportlab.pdfbase.ttfonts import TTFont
from reportlab.pdfbase.pdfmetrics import stringWidth
from reportlab.lib.styles import ParagraphStyle
from reportlab.lib.enums import TA_CENTER
from reportlab.platypus import (BaseDocTemplate, PageTemplate, Frame, Paragraph, Spacer, Table, TableStyle, Image,
                                KeepTogether, PageBreak, NextPageTemplate, Flowable, ListFlowable, CondPageBreak)
from reportlab.platypus.doctemplate import Indenter
from reportlab.platypus.tableofcontents import TableOfContents
from reportlab.pdfgen import canvas as rl_canvas
from reportlab.lib.utils import simpleSplit

HERE = Path(__file__).resolve().parent
FIG = str(HERE / 'fig') + '/'; FD = str(HERE / 'fonts') + '/'
ROOT = HERE.parent.parent
for n, f in [('Plex','Regular'),('Plex-Med','Medium'),('Plex-SB','SemiBold'),('Plex-B','Bold'),('Plex-I','Italic')]:
    pdfmetrics.registerFont(TTFont(n, FD+f'IBMPlexSans-{f}.ttf'))
pdfmetrics.registerFont(TTFont('Mono', FD+'IBMPlexMono-Regular.ttf'))
pdfmetrics.registerFontFamily('Plex', normal='Plex', bold='Plex-SB', italic='Plex-I', boldItalic='Plex-SB')

INK=HexColor('#1d2925'); MUTED=HexColor('#5d6b66'); RULE=HexColor('#cfdad5'); ACCENT=HexColor('#2f6b5a')
ACC2=HexColor('#447766'); TINT=HexColor('#e7f0ec'); DARK=HexColor('#1b2a26'); WARN=HexColor('#d9622b')
WARN_BG=HexColor('#fdf0e7'); TIP_BG=HexColor('#eaf3ef'); INFO=HexColor('#3d6fb6'); INFO_BG=HexColor('#edf2fa')
ORANGE=HexColor('#e2701f')
W, H = A4; LM = 18*mm; FW = W - 2*LM
CHIPS = {'CA':('Sekundenkleber','#f08a24'), 'EP':('2K-Kleber','#d9534f'), 'UV':('UV-Lack','#8b6cc9'),
         'HG':('Heißkleber','#e3b23c'), 'TB':('Doppelseitiges Klebeband','#6f9ef0'), 'LK':('Lötkolben','#7a8580'),
         'SL':('Schleifen und Lack (optional)','#a9b4af'), 'EL':('Elektronik aus Kapitel 5','#2f5fa8')}

S = {}
S['body'] = ParagraphStyle('body', fontName='Plex', fontSize=9.3, leading=13.6, textColor=INK, spaceAfter=6)
S['h1'] = ParagraphStyle('h1', fontName='Plex-B', fontSize=20, leading=24, textColor=INK, spaceAfter=10)
S['h2'] = ParagraphStyle('h2', fontName='Plex-SB', fontSize=12.5, leading=16, textColor=ACCENT, spaceBefore=8, spaceAfter=6)
S['cap'] = ParagraphStyle('cap', fontName='Plex-I', fontSize=7.8, leading=10.4, textColor=MUTED, spaceBefore=2, spaceAfter=10)
S['cell'] = ParagraphStyle('cell', fontName='Plex', fontSize=8.3, leading=11.2, textColor=INK)
S['cellh'] = ParagraphStyle('cellh', fontName='Plex-SB', fontSize=8.1, leading=10.8, textColor=ACCENT)
S['h3'] = ParagraphStyle('h3', fontName='Plex-SB', fontSize=10, leading=13, textColor=INK, spaceBefore=8, spaceAfter=4)
S['pcap'] = ParagraphStyle('pcap', fontName='Plex-I', fontSize=7.4, leading=9.6, textColor=MUTED, spaceBefore=3)
S['code'] = ParagraphStyle('code', fontName='Mono', fontSize=7.5, leading=10.0, textColor=INK)
S['note'] = ParagraphStyle('note', fontName='Plex', fontSize=8.7, leading=12.4, textColor=INK)
S['li'] = ParagraphStyle('li', parent=S['body'], spaceAfter=3)
S['tile'] = ParagraphStyle('tile', fontName='Plex', fontSize=7.3, leading=9.3, textColor=MUTED, alignment=TA_CENTER)
S['toc'] = ParagraphStyle('toc', fontName='Plex-Med', fontSize=10.5, leading=19, textColor=INK)
S['cov'] = ParagraphStyle('cov', fontName='Plex', fontSize=10.3, leading=15, textColor=HexColor('#e8f1ed'))
S['covs'] = ParagraphStyle('covs', fontName='Plex', fontSize=8.4, leading=13, textColor=HexColor('#a9c9bd'))

def pn(n, size=7.9): return f'<font name="Mono" size="{size}" color="#2f6b5a">{n}</font>'
def P(t, s='body'): return Paragraph(t, S[s])
FIGN = [0]
def cap(t):
    FIGN[0] += 1; return P(f'<font name="Plex-SB">Abb. {FIGN[0]}</font>  {t}', 'cap')

class H1(Paragraph):
    def __init__(self, num, title):
        super().__init__(f'<font color="#2f6b5a">{num}</font>&nbsp;&nbsp;{title}', S['h1']); self._toc = f'{num}  {title}'; self._chap = title

def table(rows, cw, head=True, fs=None):
    data = [[c if isinstance(c, Flowable) else Paragraph(c, S['cellh'] if (head and i==0) else S['cell']) for c in r] for i, r in enumerate(rows)]
    t = Table(data, colWidths=cw, repeatRows=1 if head else 0, hAlign='LEFT')
    st = [('VALIGN',(0,0),(-1,-1),'TOP'), ('LEFTPADDING',(0,0),(-1,-1),5), ('RIGHTPADDING',(0,0),(-1,-1),5),
          ('TOPPADDING',(0,0),(-1,-1),3.4), ('BOTTOMPADDING',(0,0),(-1,-1),4), ('LINEBELOW',(0,0),(-1,-1),0.4,RULE)]
    if head: st += [('BACKGROUND',(0,0),(-1,0),TINT), ('LINEBELOW',(0,0),(-1,0),0.9,ACCENT)]
    t.setStyle(TableStyle(st)); return t

def note(text, kind='tip', width=None):
    bg, border, label = {'tip':(TIP_BG, ACCENT, 'Tipp'), 'warn':(WARN_BG, WARN, 'Achtung'), 'info':(INFO_BG, INFO, 'Hinweis')}[kind]
    p = Paragraph(f'<font name="Plex-SB" color="{border.hexval().replace("0x","#")}">{label}:</font> {text}', S['note'])
    t = Table([[p]], colWidths=[width or FW])
    t.setStyle(TableStyle([('BACKGROUND',(0,0),(-1,-1),bg), ('LINEBEFORE',(0,0),(0,-1),2.6,border),
        ('LEFTPADDING',(0,0),(-1,-1),9), ('RIGHTPADDING',(0,0),(-1,-1),8), ('TOPPADDING',(0,0),(-1,-1),6), ('BOTTOMPADDING',(0,0),(-1,-1),7)]))
    return t

def bullets(items, indent=0):
    return ListFlowable([Paragraph(t, S['li']) for t in items], bulletType='bullet', start='•', leftIndent=12+indent,
                        bulletFontName='Plex-B', bulletFontSize=9, bulletColor=ACCENT, bulletOffsetY=0)

def leader(c, x0, y0, x1, y1):
    c.saveState(); c.setLineCap(1)
    c.setStrokeColor(white); c.setLineWidth(1.9); c.line(x0, y0, x1, y1)
    c.setStrokeColor(INK); c.setLineWidth(0.55); c.line(x0, y0, x1, y1)
    c.setFillColor(white); c.circle(x0, y0, 2.4, stroke=0, fill=1)
    c.setFillColor(INK); c.circle(x0, y0, 1.45, stroke=0, fill=1); c.restoreState()

def arrow(c, x0, y0, x1, y1, col=ORANGE):
    a = math.atan2(y1-y0, x1-x0); hl, hw = 6.0, 3.0; bx, by = x1-hl*math.cos(a), y1-hl*math.sin(a)
    c.saveState(); c.setLineCap(1)
    c.setStrokeColor(white); c.setLineWidth(2.8); c.line(x0, y0, bx, by)
    c.setStrokeColor(col); c.setLineWidth(1.2); c.line(x0, y0, bx, by)
    p = c.beginPath(); p.moveTo(x1, y1); p.lineTo(bx+hw*math.sin(a), by-hw*math.cos(a)); p.lineTo(bx-hw*math.sin(a), by+hw*math.cos(a)); p.close()
    c.setFillColor(col); c.setStrokeColor(white); c.setLineWidth(0.7); c.drawPath(p, fill=1, stroke=1); c.restoreState()

class AnnotatedImage(Flowable):
    def __init__(self, img, meta, callouts=(), arrows=(), frac=0.6, pad=4, gap=8, width=None, place='center'):
        super().__init__(); self.img=img; self.meta=meta; self.callouts=list(callouts); self.arrows=list(arrows); self.place=place
        self.width = width or FW; self.frac=frac; self.pad=pad; self.gap=gap
        iw, ih = meta['size']; self.iw = self.width*frac; self.k = self.iw/iw; self.ih = ih*self.k; self.height = self.ih + 2*pad
    def wrap(self, aw, ah): return self.width, self.height
    def draw(self):
        c = self.canv; ix = 0 if self.place == 'left' else (self.width-self.iw)/2; iy = self.pad
        c.drawImage(self.img, ix, iy, self.iw, self.ih, mask='auto')
        A = self.meta['anchors']; Pt = lambda k: (ix + A[k][0]*self.k, iy + self.ih - A[k][1]*self.k)
        for a, b in self.arrows: arrow(c, *Pt(a), *Pt(b))
        sides = {'L':[], 'R':[]}
        for co in self.callouts:
            ax, ay = Pt(co['a']); side = 'R' if self.place == 'left' else (co.get('side') or ('L' if ax < ix + self.iw/2 else 'R'))
            avail = (ix - self.gap - 2.5) if side == 'L' else (self.width - (ix + self.iw + self.gap + 2.5))
            lines = [(l, 'Plex-Med', 7.7, INK) for l in simpleSplit(co['t'], 'Plex-Med', 7.7, avail)]
            for s_ in (co.get('s') or '').split('\n'):
                if s_: lines += [(l, 'Plex', 6.9, MUTED) for l in simpleSplit(s_, 'Plex', 6.9, avail)]
            sides[side].append(dict(ax=ax, ay=ay, want=ay + co.get('dy', 0), lines=lines, h=sum(l[2]*1.22 for l in lines)))
        for side, lst in sides.items():
            lst.sort(key=lambda d: -d['want']); cur = self.height
            for d in lst:
                d['y'] = min(d['want'], cur - d['h']/2); cur = d['y'] - d['h']/2 - 3.5
            if cur < 0:
                for d in lst: d['y'] += -cur
            for d in lst:
                rx = ix - self.gap if side == 'L' else ix + self.iw + self.gap
                leader(c, d['ax'], d['ay'], rx, d['y']); yy = d['y'] + d['h']/2
                for text, font, size, col in d['lines']:
                    yy -= size*1.22; c.setFont(font, size); c.setFillColor(col)
                    if side == 'L': c.drawRightString(rx - 2.5, yy + size*0.25, text)
                    else: c.drawString(rx + 2.5, yy + size*0.25, text)

def fig(name, callouts=(), arrows=(), frac=0.6, **kw):  # kw: width, place, gap
    path = FIG + name + ('.png' if name == 'hero' else '.jpg')
    return AnnotatedImage(path, json.load(open(FIG + name + '.json')), callouts, arrows, frac, **kw)

class ChipRow(Flowable):
    def __init__(self, keys, label=None, width=None): super().__init__(); self.keys=keys; self.label=label; self.width=width or FW; self.height=14
    def wrap(self, aw, ah): return self.width, self.height
    def draw(self):
        c = self.canv; x = 0; y = 1.5
        if self.label:
            c.setFont('Plex-Med', 7.5); c.setFillColor(MUTED); c.drawString(0, y+3.3, self.label); x += stringWidth(self.label, 'Plex-Med', 7.5) + 7
        for k in self.keys:
            name, col = CHIPS[k] if isinstance(k, str) else k; w = stringWidth(name, 'Plex-Med', 7.4) + 17
            c.setFillColor(HexColor('#f4f7f6')); c.setStrokeColor(RULE); c.setLineWidth(0.5); c.roundRect(x, y, w, 11.5, 5.75, stroke=1, fill=1)
            c.setFillColor(HexColor(col)); c.circle(x+6.4, y+5.75, 2.7, stroke=0, fill=1)
            c.setFillColor(INK); c.setFont('Plex-Med', 7.4); c.drawString(x+11.5, y+3.4, name); x += w + 5

class StepHeader(Flowable):
    def __init__(self, n, title, color=None): super().__init__(); self.n=n; self.title=title; self.color=color; self.width=FW; self.height=21
    def wrap(self, aw, ah): return self.width, self.height
    def draw(self):
        c = self.canv; r = 8.6; cy = self.height/2
        c.setFillColor(HexColor(self.color) if self.color else ACCENT); c.circle(r, cy, r, stroke=0, fill=1)
        fs = 9.8 if len(str(self.n)) <= 2 else 8.0
        c.setFillColor(white); c.setFont('Plex-SB', fs); c.drawCentredString(r, cy-fs*0.35, str(self.n))
        c.setFillColor(INK); c.setFont('Plex-SB', 12.2); c.drawString(2*r+7, cy-4.2, self.title)

IND = 24.2
def step(n, title, chips, items, notes=(), color=None, extra=()):
    body = [StepHeader(n, title, color), Spacer(1, 2), Indenter(IND)]
    if chips: body += [ChipRow(chips, width=FW-IND), Spacer(1, 5)]
    body += [bullets(items)]
    for kind, text in notes: body += [Spacer(1, 3), note(text, kind, FW-IND)]
    body += list(extra) + [Indenter(-IND), Spacer(1, 10)]
    return KeepTogether(body)

class LayerDiagram(Flowable):
    def __init__(self, width=90*mm, height=40*mm): super().__init__(); self.width=width; self.height=height
    def wrap(self, aw, ah): return self.width, self.height
    def draw(self):
        c = self.canv; x0, x1 = 2, 88; rh = 15; gap = 13; y0 = 16; lx = x1 + 9
        SAL = HexColor('#eeb397'); SALD = HexColor('#d98f6e'); TAG = HexColor('#c77d3a')
        c.setFillColor(HexColor('#d5dce1')); c.rect(x0-2, y0-6, x1-x0+4, 5.4, stroke=0, fill=1)
        c.setFont('Plex', 6.7); c.setFillColor(MUTED); c.drawString(lx, y0-5.2, 'Druckbett')
        rows = [('Z 0–14,4', 'Token, Oberseite auf dem Bett', 'rest'), ('Z 14,4–14,6', 'Tasche', 'pocket'),
                ('Z 14,6–14,8', 'Tasche', 'pocket'), ('Z 14,8–15,0', 'letzte Schicht, schließt die Tasche', 'cover')]
        px0, px1 = x0 + (x1-x0)*0.2, x0 + (x1-x0)*0.8
        for i, (lab, txt, kind) in enumerate(rows):
            yy = y0 + i*rh + (gap if i == 3 else 0); hh = rh - 1.6
            c.setStrokeColor(SALD); c.setLineWidth(0.6)
            if kind == 'rest':
                c.setFillColor(HexColor('#f6d8ca')); c.rect(x0, yy, x1-x0, hh, stroke=1, fill=1)
            elif kind == 'pocket':
                c.setFillColor(SAL); c.rect(x0, yy, px0-x0, hh, stroke=1, fill=1); c.rect(px1, yy, x1-px1, hh, stroke=1, fill=1)
                if i == 1:
                    c.setFillColor(TAG); c.rect(px0+3, yy, px1-px0-6, 5.5, stroke=0, fill=1)
                    c.setFont('Plex-SB', 6.6); c.setFillColor(TAG); c.drawCentredString((px0+px1)/2, yy+7.6, 'NFC-Tag')
            else:
                c.setFillColor(HexColor('#f7e3d9')); c.setDash(2.2, 1.6); c.setStrokeColor(ORANGE); c.setLineWidth(0.9)
                c.rect(x0, yy, x1-x0, hh, stroke=1, fill=1); c.setDash()
            c.setFont('Plex-SB', 7.1); c.setFillColor(INK); c.drawString(lx, yy+hh/2-2.4, lab)
            c.setFont('Plex', 6.8); c.setFillColor(MUTED); c.drawString(lx + stringWidth(lab, 'Plex-SB', 7.1) + 5, yy+hh/2-2.4, txt)
        yp = y0 + 3*rh + gap/2 - 0.8
        c.setStrokeColor(ORANGE); c.setLineWidth(1.2); c.setDash(3, 2); c.line(0, yp, lx-4, yp); c.setDash()
        c.setFillColor(ORANGE); c.setFont('Plex-SB', 7.2); c.drawString(lx, yp-2.4, 'Pause: Tag einlegen')

class ParallelDiagram(Flowable):
    def __init__(self, width=86*mm, height=40*mm): super().__init__(); self.width=width; self.height=height
    def wrap(self, aw, ah): return self.width, self.height
    def draw(self):
        c = self.canv; WRAPC=HexColor('#3e58a6'); NIC=HexColor('#c9cdd2'); KAP=HexColor('#d9952e'); REDC=HexColor('#c8322b'); BLKC=HexColor('#2b2b2b')
        yb, ch, cw = 30, 48, 20; yt = yb+ch; ytop = yt+15; ybot = yb-13; bx0, bx1 = 150, self.width-4
        cells = [(16, 'Zelle 1', 'vorn links'), (78, 'Zelle 2', 'hinten rechts')]
        c.saveState(); c.setLineCap(1); c.setLineJoin(1); c.setLineWidth(2.2)
        x1, x2 = cells[0][0]+cw/2, cells[1][0]+cw/2
        c.setStrokeColor(REDC); p = c.beginPath(); p.moveTo(x1, yt+5); p.lineTo(x1, ytop); p.lineTo(bx0, ytop); c.drawPath(p, stroke=1, fill=0); c.line(x2, yt+5, x2, ytop)
        c.setStrokeColor(BLKC); p = c.beginPath(); p.moveTo(x1, yb-3); p.lineTo(x1, ybot); p.lineTo(bx0, ybot); c.drawPath(p, stroke=1, fill=0); c.line(x2, yb-3, x2, ybot)
        c.setFillColor(REDC); c.circle(x2, ytop, 2.6, stroke=0, fill=1); c.setFillColor(BLKC); c.circle(x2, ybot, 2.6, stroke=0, fill=1)
        for x, lab, sub in cells:
            c.setFillColor(WRAPC); c.setStrokeColor(HexColor('#2b3f7a')); c.setLineWidth(0.6); c.roundRect(x, yb, cw, ch, 2.5, stroke=1, fill=1)
            c.setFillColor(NIC); c.rect(x+6, yt, cw-12, 2.6, stroke=0, fill=1)
            c.setFillColor(KAP); c.roundRect(x+3, yt+2.6, cw-6, 4.6, 1.2, stroke=0, fill=1); c.roundRect(x+3, yb-5.2, cw-6, 4.6, 1.2, stroke=0, fill=1)
            c.setFillColor(white); c.setFont('Plex-B', 10); c.drawCentredString(x+cw/2, yt-12, '+'); c.drawCentredString(x+cw/2, yb+5, '−')
            c.setFillColor(INK); c.setFont('Plex-SB', 7.2); c.drawString(x+cw+5, yb+ch/2+2, lab)
            c.setFillColor(MUTED); c.setFont('Plex', 6.7); c.drawString(x+cw+5, yb+ch/2-7, sub)
        c.setFillColor(HexColor('#eef3f8')); c.setStrokeColor(HexColor('#2f5fa8')); c.setLineWidth(0.8)
        c.roundRect(bx0, ybot-10, bx1-bx0, (ytop+10)-(ybot-10), 4, stroke=1, fill=1)
        c.setFillColor(HexColor('#2f5fa8')); c.setFont('Plex-SB', 8.4); c.drawCentredString((bx0+bx1)/2+6, (ytop+ybot)/2-3, 'TP4056')
        c.setFont('Plex-SB', 7.6); c.setFillColor(REDC); c.drawString(bx0+6, ytop-2.7, 'B+'); c.setFillColor(BLKC); c.drawString(bx0+6, ybot-2.7, 'B−')
        c.setFillColor(KAP); c.roundRect(4, 2, 8, 4.6, 1.2, stroke=0, fill=1); c.setFillColor(MUTED); c.setFont('Plex', 6.6); c.drawString(15, 2.6, 'Kapton über den Lötstellen')
        c.restoreState()

class LightPipeDiagram(Flowable):
    """Side section through the rear wall at the USB-C socket: a bent light pipe from the 2 mm hole to the charge LED."""
    def __init__(self, width=104*mm, height=52*mm):
        super().__init__(); self.width = width; self.height = height; self.hAlign = 'CENTER'
    def wrap(self, aw, ah): return self.width, self.height
    def draw(self):
        c = self.canv; s = 4.4; X0 = 112; Z0 = 15.0; yb = 14
        X = lambda d: X0 + d*s                      # mm from the outer wall face, positive = inside
        Y = lambda z: yb + (z - Z0)*s
        SAL = HexColor('#eeb397'); SALD = HexColor('#d98f6e'); PCBB = HexColor('#2f5fa8'); PCBG = HexColor('#3f7f4f')
        c.saveState(); c.setLineWidth(0.6)
        for z0, z1 in ((Z0, 17.5), (21.0, 23.25), (25.25, 45.0)):          # wall with USB-C slot and 2 mm hole
            c.setFillColor(SAL); c.setStrokeColor(SALD); c.rect(X(0), Y(z0), 3*s, (z1-z0)*s, stroke=1, fill=1)
        c.setFillColor(HexColor('#c8ccd1')); c.setStrokeColor(HexColor('#8e959c')); c.rect(X(0), Y(17.62), 7.35*s, 3.26*s, stroke=1, fill=1)
        c.setFillColor(PCBB); c.setStrokeColor(HexColor('#22467f')); c.rect(X(1.5), Y(16.4), 11.5*s, 1.2*s, stroke=1, fill=1)
        bx = 9.0; zl = 33.0                                                 # board distance from the outer face, LED height
        for z in (29.0, 41.5):                                              # 2K adhesive between wall and board
            c.setFillColor(HexColor('#f2d49b')); c.setStrokeColor(HexColor('#c9a35a')); c.ellipse(X(2.6), Y(z-1.6), X(bx+0.5), Y(z+1.6), stroke=1, fill=1)
        c.setFillColor(PCBB); c.setStrokeColor(HexColor('#22467f')); c.rect(X(bx), Y(27.0), 1.4*s, 17.0*s, stroke=1, fill=1)
        c.setFillColor(PCBG); c.setStrokeColor(HexColor('#2c5c38')); c.rect(X(bx+2.4), Y(26.0), 1.6*s, 18.6*s, stroke=1, fill=1)
        c.setFillColor(HexColor('#e0302a')); c.rect(X(bx-0.9), Y(zl-0.6), 0.9*s, 1.2*s, stroke=0, fill=1)
        def pipe():                                                         # centre line of the bent filament
            p = c.beginPath(); p.moveTo(X(0), Y(24.25)); p.lineTo(X(3.8), Y(24.25))
            p.curveTo(X(6.6), Y(24.25), X(5.2), Y(zl), X(bx-0.9), Y(zl)); return p
        c.setLineCap(0); c.setStrokeColor(HexColor('#7fb5c9')); c.setLineWidth(1.75*s); c.drawPath(pipe(), stroke=1, fill=0)
        c.setStrokeColor(HexColor('#d6edf5')); c.setLineWidth(1.75*s - 1.4); c.drawPath(pipe(), stroke=1, fill=0)
        c.setLineWidth(0.6); c.setFillColor(HexColor('#f6b3ae')); c.circle(X(0)-2.6, Y(24.25), 3.2, stroke=0, fill=1)
        c.setFillColor(MUTED); c.setFont('Plex-SB', 6.6)
        c.drawRightString(X(0)-3, Y(Z0)-9, 'AUSSEN'); c.drawString(X(3)+3, Y(Z0)-9, 'INNEN')
        def lab(ax, ay, lx, ly, text, sub=None, right=False):
            leader(c, ax, ay, lx, ly)
            draw = c.drawRightString if right else c.drawString; dx = -2.5 if right else 2.5
            c.setFont('Plex-Med', 7.5); c.setFillColor(INK); draw(lx + dx, ly + (1.2 if sub else -2.5), text)
            if sub:
                c.setFont('Plex', 6.8); c.setFillColor(MUTED); draw(lx + dx, ly - 7.2, sub)
        lab(X(0)-2.6, Y(24.25), X(0)-26, Y(29.0), 'Ladeanzeige', 'Filament außen bündig', right=True)
        lab(X(0)+2.0, Y(19.25), X(0)-26, Y(18.0), 'USB-C', right=True)
        rx = X(bx+4.0) + 16
        lab(X(1.5), Y(43.8), rx, Y(44.5), 'Rückwand, 3 mm')
        lab(X(6.0), Y(41.5), rx, Y(39.5), '2K-Kleber', 'hält auch, wenn es warm wird')
        lab(X(bx+0.7), Y(38.0), rx, Y(33.5), 'TP4056 auf der Power Latch', 'Modulseite zur Wand')
        lab(X(bx-0.45), Y(zl), rx, Y(27.5), 'Lade-LED')
        lab(X(5.5), Y(28.5), rx, Y(21.0), 'Filament 1,75 mm', 'darf gebogen verlaufen')
        c.restoreState()

<<<<<<< HEAD
class PcbView(Flowable):
    """Assembly drawing of one board side from a KiCad file: outline, pads, part bodies and references.

    groups: list of (colour, label, refs) for highlighted parts; dnp: refs that stay empty (drawn crossed)."""
    def __init__(self, board, side='F', k=1.8, groups=(), dnp=(), title=None):
        super().__init__(); self.b = board; self.side = side; self.k = k*mm; self.groups = groups; self.dnp = set(dnp); self.title = title
        xs = [x for poly in board['outline'] for x, _ in poly]; ys = [y for poly in board['outline'] for _, y in poly]
        self.x0, self.x1, self.y0, self.y1 = min(xs), max(xs), min(ys), max(ys)
        self.top = 12 if title else 0
        self.width = (self.x1 - self.x0)*self.k; self.height = (self.y1 - self.y0)*self.k + self.top
    def wrap(self, aw, ah): return self.width, self.height
    def P(self, x, y):
        X = (self.x1 - x) if self.side == 'B' else (x - self.x0)
        return X*self.k, (self.y1 - y)*self.k
    def _body(self, f):
        T = f['T']
        for want in (('F.Fab', 'B.Fab'), ('F.SilkS', 'B.SilkS', 'Cmts.User'), ('F.CrtYd', 'B.CrtYd')):
            pts = [p for g in f['gfx'] if g['layer'] in want for p in g['local']]
            if pts: break
        if not pts:
            pts = [(p['lx'] + sx*p['w']/2, p['ly'] + sy*p['h']/2) for p in f['pads'] for sx in (-1, 1) for sy in (-1, 1)]
        lx0, lx1 = min(p[0] for p in pts), max(p[0] for p in pts); ly0, ly1 = min(p[1] for p in pts), max(p[1] for p in pts)
        corners = [self.P(*T(x, y)) for x, y in ((lx0, ly0), (lx1, ly0), (lx1, ly1), (lx0, ly1))]
        xs = [c[0] for c in corners]; ys = [c[1] for c in corners]
        return min(xs), min(ys), max(xs), max(ys)
    def draw(self):
        c = self.canv; side = self.side; cu = side + '.Cu'
        colour = {}
        for col, _, refs in self.groups:
            for r in refs: colour[r] = HexColor(col)
        c.saveState()
        if self.title:
            c.setFont('Plex-SB', 8); c.setFillColor(MUTED); c.drawString(0, self.height - 8, self.title)
        for poly in self.b['outline']:
            p = c.beginPath(); pts = [self.P(*q) for q in poly]; p.moveTo(*pts[0])
            for q in pts[1:]: p.lineTo(*q)
            p.close(); c.setFillColor(HexColor('#2b5d4c')); c.setStrokeColor(HexColor('#1b3f33')); c.setLineWidth(0.8); c.drawPath(p, fill=1, stroke=1)
        mine = [f for f in self.b['footprints'] if f['layer'] == cu]
        for f in mine:                                                            # part bodies
            x0, y0, x1, y1 = self._body(f); col = colour.get(f['ref'])
            if f['ref'] in self.dnp:
                c.setDash(1.5, 1.2); c.setStrokeColor(HexColor('#e05a4f')); c.setLineWidth(0.7); c.setFillColor(HexColor('#2b5d4c'))
                c.rect(x0, y0, x1-x0, y1-y0, stroke=1, fill=0); c.setDash()
                c.line(x0, y0, x1, y1); c.line(x0, y1, x1, y0)
            elif col is not None:
                big = (x1-x0)*(y1-y0) > 0.25*self.width*(self.height-self.top)
                c.setFillColor(col); c.setFillAlpha(0.45 if big else 0.82); c.setStrokeColor(white); c.setLineWidth(0.4)
                c.roundRect(x0, y0, x1-x0, y1-y0, 0.6, stroke=1, fill=1); c.setFillAlpha(1)
            else:
                c.setFillColor(white); c.setFillAlpha(0.14); c.setStrokeColor(HexColor('#cfe0d8')); c.setLineWidth(0.4)
                c.roundRect(x0, y0, x1-x0, y1-y0, 0.6, stroke=1, fill=1); c.setFillAlpha(1)
        for f in self.b['footprints']:                                           # pads
            for p in f['pads']:
                if p['kind'] == 'smd' and cu not in p['layers'] and '*.Cu' not in p['layers']: continue
                if p['kind'] == 'smd' and f['layer'] != cu: continue
                w, h = (p['h'], p['w']) if abs(abs(p['angle']) % 180 - 90) < 1 else (p['w'], p['h'])
                cx, cy = self.P(p['x'], p['y']); W, H = w*self.k, h*self.k
                c.setFillColor(HexColor('#d9b65c')); c.setStrokeColor(HexColor('#a8873a')); c.setLineWidth(0.25)
                if p['shape'] == 'circle': c.circle(cx, cy, W/2, stroke=1, fill=1)
                elif p['shape'] == 'oval': c.roundRect(cx-W/2, cy-H/2, W, H, min(W, H)/2, stroke=1, fill=1)
                elif p['shape'] == 'roundrect': c.roundRect(cx-W/2, cy-H/2, W, H, min(W, H)*max(p['rr'], 0.1), stroke=1, fill=1)
                else: c.rect(cx-W/2, cy-H/2, W, H, stroke=1, fill=1)
                if p['drill'] > 0: c.setFillColor(HexColor('#16231f')); c.circle(cx, cy, p['drill']*self.k/2, stroke=0, fill=1)
        for f in mine:                                                            # references
            x0, y0, x1, y1 = self._body(f); w, h = x1-x0, y1-y0; ref = f['ref']
            vertical = h > w*1.25; L, S = (h, w) if vertical else (w, h)
            size = max(3.4, min(7.5, S*0.62, L*0.9/max(1, len(ref))/0.62))
            dark = ref in colour; c.setFillColor(INK if dark else white); c.setFont('Plex-SB', size)
            c.saveState(); c.translate((x0+x1)/2, (y0+y1)/2)
            if vertical: c.rotate(90)
            c.drawCentredString(0, -size*0.36, ref); c.restoreState()
        for t in self.b['texts']:                                                # silkscreen texts of this side
            if not t['layer'].startswith(side + '.'): continue
            x, y = self.P(t['x'], t['y']); size = t['size']*self.k*0.95; lines = t['text'].split('\n')
            c.setFillColor(HexColor('#eef4f1')); c.setFont('Plex-Med', size)
            right = 'right' in t['justify']; left = 'left' in t['justify']
            for i, line in enumerate(reversed(lines)):
                yy = y + i*size*1.15
                if right: c.drawRightString(x, yy, line)
                elif left: c.drawString(x, yy, line)
                else: c.drawCentredString(x, yy, line)
        c.restoreState()


def codeblock(text, width=None):
    from reportlab.platypus import Preformatted
    t = Table([[Preformatted(text, S['code'])]], colWidths=[width or FW], hAlign='LEFT')
    t.setStyle(TableStyle([('BACKGROUND', (0, 0), (-1, -1), HexColor('#f3f6f5')), ('BOX', (0, 0), (-1, -1), 0.5, RULE),
                           ('LEFTPADDING', (0, 0), (-1, -1), 9), ('TOPPADDING', (0, 0), (-1, -1), 6), ('BOTTOMPADDING', (0, 0), (-1, -1), 7)]))
    return t


REPO = 'https://github.com/aaronblankenburg/timon-box'


def repo(color='#2f6b5a'):
    return f'<a href="{REPO}" color="{color}">github.com/aaronblankenburg/timon-box</a>'


def code(t, size=8.2):
    return f'<font name="Mono" size="{size}">' + t.replace('&', '&amp;').replace('<', '&lt;').replace('>', '&gt;') + '</font>'


def photo(name, width, maxpx=1500):
    """Downscaled copy of a photo from assets/ (cached), as an Image flowable of the given width."""
    src = ROOT / 'assets' / name; dst = HERE / '.cache' / 'photos' / (Path(name).stem + '.jpg')
    if not dst.exists() or dst.stat().st_mtime < src.stat().st_mtime:
        dst.parent.mkdir(parents=True, exist_ok=True)
        im = PILImage.open(src).convert('RGB'); im.thumbnail((maxpx, maxpx)); im.save(dst, quality=86)
    w, h = PILImage.open(dst).size
    return Image(str(dst), width, width*h/w)


def photos(items, cols=3, gap=5*mm, width=None):
    """Grid of photos with captions. items: [(file in assets/, caption), ...]"""
    total = width or FW; cw = (total - gap*(cols-1))/cols; rows = []; row = []
    for name, text in items:
        row.append([photo(name, cw), Paragraph(text, S['pcap'])])
        if len(row) == cols: rows.append(row); row = []
    if row: rows.append(row + [''] * (cols - len(row)))
    t = Table(rows, colWidths=[cw + (gap if i < cols-1 else 0) for i in range(cols)], hAlign='LEFT')
    t.setStyle(TableStyle([('VALIGN', (0, 0), (-1, -1), 'TOP'), ('LEFTPADDING', (0, 0), (-1, -1), 0),
                           ('RIGHTPADDING', (0, 0), (-1, -1), 0), ('BOTTOMPADDING', (0, 0), (-1, -1), 8)]))
    return t


WIRES = {'rot': '#d23a2f', 'schwarz': '#2b2b2b', 'blau': '#2f6fd0', 'gelb': '#f2c230', 'orange': '#f08a24',
         'grün': '#3a9a4a', 'weiß': '#ffffff'}


class Wire(Flowable):
    """Colour swatch with the wire colour name, for wiring tables."""
    def __init__(self, name): super().__init__(); self.name = name; self.width = 52; self.height = 9
    def wrap(self, aw, ah): return self.width, self.height
    def draw(self):
        c = self.canv; c.setFillColor(HexColor(WIRES[self.name])); c.setStrokeColor(HexColor('#8a9590')); c.setLineWidth(0.5)
        c.roundRect(0, 1, 14, 6, 3, stroke=1, fill=1); c.setFillColor(INK); c.setFont('Plex', 8.3); c.drawString(19, 1.4, self.name)


class PdfPage(Flowable):
    """First page of a PDF (e.g. a KiCad schematic), rotated by 90 degrees to fill a portrait area."""
    def __init__(self, path, width, height):
        from pdfrw import PdfReader
        from pdfrw.buildxobj import pagexobj
        super().__init__(); self.xobj = pagexobj(PdfReader(str(path)).pages[0]); self.width = width; self.height = height
    def wrap(self, aw, ah): return self.width, self.height
    def draw(self):
        from pdfrw.toreportlab import makerl
        c = self.canv; bb = [float(v) for v in self.xobj.BBox]; pw, ph = bb[2]-bb[0], bb[3]-bb[1]
        s = min(self.width/ph, self.height/pw); name = makerl(c, self.xobj)
        c.saveState(); c.translate(self.width/2, self.height/2); c.rotate(90); c.scale(s, s); c.translate(-pw/2-bb[0], -ph/2-bb[1])
        c.doForm(name); c.restoreState()
        c.setStrokeColor(RULE); c.setLineWidth(0.5)
        c.rect(self.width/2 - ph*s/2, self.height/2 - pw*s/2, ph*s, pw*s, stroke=1, fill=0)


class SvgPage(Flowable):
    """SVG drawing (e.g. the WireViz diagram), rotated by 90 degrees to fill a portrait area."""
    def __init__(self, path, width, height):
        from svglib.svglib import svg2rlg
        super().__init__(); self.d = svg2rlg(str(path)); self.width = width; self.height = height
    def wrap(self, aw, ah): return self.width, self.height
    def draw(self):
        from reportlab.graphics import renderPDF
        c = self.canv; dw, dh = self.d.width, self.d.height; s = min(self.width/dh, self.height/dw)
        c.saveState(); c.translate(self.width/2, self.height/2); c.rotate(90); c.scale(s, s); c.translate(-dw/2, -dh/2)
        renderPDF.draw(self.d, c, 0, 0); c.restoreState()

=======
>>>>>>> 25e2960d7aa34102d2d0a27d80930b01dce578b1
class NumberedCanvas(rl_canvas.Canvas):
    def __init__(self, *a, **k): super().__init__(*a, **k); self._states = []
    def showPage(self): self._states.append(dict(self.__dict__)); self._startPage()
    def save(self):
        n = len(self._states)
        for st in self._states:
            self.__dict__.update(st)
            if self._pageNumber > 1:
                self.setFont('Plex', 7.4); self.setFillColor(MUTED); self.drawRightString(W-LM, 9*mm, f'Seite {self._pageNumber} von {n}')
            super().showPage()
        super().save()

def on_cover(c, doc):
    band = 140*mm; c.saveState()
    c.setFillColor(DARK); c.rect(0, H-band, W, band, stroke=0, fill=1)
    c.setFillColor(ACC2); c.rect(0, H-band, W, 1.8*mm, stroke=0, fill=1)
    c.saveState(); t = c.beginText(LM, H-24*mm); t.setFont('Plex-SB', 8.6); t.setCharSpace(1.8); t.setFillColor(HexColor('#8fc0ae'))
    t.textLine(doc.meta['kicker']); t.setCharSpace(0); c.drawText(t); c.restoreState()
    c.setFillColor(white); c.setFont('Plex-B', 38); c.drawString(LM, H-41*mm, doc.meta['name'])
    c.setFillColor(HexColor('#d7e6e0')); c.setFont('Plex-Med', 15.5); c.drawString(LM, H-50.5*mm, doc.meta['subtitle'])
    hw = 128*mm; hh = hw*1250/1400
    c.drawImage(FIG+'hero.png', W-10*mm-hw, H-band-hh*0.30, hw, hh, mask='auto')
    p = Paragraph(doc.meta['intro'], S['cov'])
    _, ph = p.wrap(50*mm, 60*mm); p.drawOn(c, LM, H-64*mm-ph)
    q = Paragraph('<br/>'.join(doc.meta['facts']), S['covs'])
    _, qh = q.wrap(58*mm, 40*mm); q.drawOn(c, LM, H-64*mm-ph-7*mm-qh)
    c.restoreState()

def on_body(c, doc):
    c.saveState(); c.setFont('Plex-Med', 7.8); c.setFillColor(MUTED)
    c.drawString(LM, H-13*mm, doc.meta['header']); c.drawRightString(W-LM, H-13*mm, getattr(doc, 'chapter', ''))
    c.setStrokeColor(RULE); c.setLineWidth(0.5); c.line(LM, H-15*mm, W-LM, H-15*mm); c.line(LM, 13*mm, W-LM, 13*mm)
    c.setFont('Plex', 7.4); c.drawString(LM, 9*mm, doc.meta['footer']); c.restoreState()

class Doc(BaseDocTemplate):
    def __init__(self, fn, meta):
        super().__init__(fn, pagesize=A4, leftMargin=LM, rightMargin=LM, topMargin=22*mm, bottomMargin=19*mm,
                         title=meta['title'], author='', subject=meta['subject'], creator='timon-box doc/assembly')
        self.meta = meta
        self.addPageTemplates([
            PageTemplate('cover', [Frame(LM, 16*mm, FW, 100*mm, id='cv', leftPadding=0, rightPadding=0, topPadding=0, bottomPadding=0)], onPage=on_cover),
            PageTemplate('body', [Frame(LM, 19*mm, FW, H-41*mm, id='bd', leftPadding=0, rightPadding=0, topPadding=0, bottomPadding=0)], onPageEnd=on_body)])
        self.chapter = ''
    def afterFlowable(self, f):
        if isinstance(f, H1):
            self.chapter = f._chap; self.notify('TOCEntry', (0, f._toc, self.page))

