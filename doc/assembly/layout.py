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
         'SL':('Schleifen und Lack (optional)','#a9b4af'), 'EL':('Elektronik aus hardware.md','#2f5fa8')}

S = {}
S['body'] = ParagraphStyle('body', fontName='Plex', fontSize=9.3, leading=13.6, textColor=INK, spaceAfter=6)
S['h1'] = ParagraphStyle('h1', fontName='Plex-B', fontSize=20, leading=24, textColor=INK, spaceAfter=10)
S['h2'] = ParagraphStyle('h2', fontName='Plex-SB', fontSize=12.5, leading=16, textColor=ACCENT, spaceBefore=8, spaceAfter=6)
S['cap'] = ParagraphStyle('cap', fontName='Plex-I', fontSize=7.8, leading=10.4, textColor=MUTED, spaceBefore=2, spaceAfter=10)
S['cell'] = ParagraphStyle('cell', fontName='Plex', fontSize=8.3, leading=11.2, textColor=INK)
S['cellh'] = ParagraphStyle('cellh', fontName='Plex-SB', fontSize=8.1, leading=10.8, textColor=ACCENT)
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
            name, col = CHIPS[k]; w = stringWidth(name, 'Plex-Med', 7.4) + 17
            c.setFillColor(HexColor('#f4f7f6')); c.setStrokeColor(RULE); c.setLineWidth(0.5); c.roundRect(x, y, w, 11.5, 5.75, stroke=1, fill=1)
            c.setFillColor(HexColor(col)); c.circle(x+6.4, y+5.75, 2.7, stroke=0, fill=1)
            c.setFillColor(INK); c.setFont('Plex-Med', 7.4); c.drawString(x+11.5, y+3.4, name); x += w + 5

class StepHeader(Flowable):
    def __init__(self, n, title): super().__init__(); self.n=n; self.title=title; self.width=FW; self.height=21
    def wrap(self, aw, ah): return self.width, self.height
    def draw(self):
        c = self.canv; r = 8.6; cy = self.height/2
        c.setFillColor(ACCENT); c.circle(r, cy, r, stroke=0, fill=1)
        c.setFillColor(white); c.setFont('Plex-SB', 9.8); c.drawCentredString(r, cy-3.4, str(self.n))
        c.setFillColor(INK); c.setFont('Plex-SB', 12.2); c.drawString(2*r+7, cy-4.2, self.title)

IND = 24.2
def step(n, title, chips, items, notes=()):
    body = [StepHeader(n, title), Spacer(1, 2), Indenter(IND)]
    if chips: body += [ChipRow(chips, width=FW-IND), Spacer(1, 5)]
    body += [bullets(items)]
    for kind, text in notes: body += [Spacer(1, 3), note(text, kind, FW-IND)]
    body += [Indenter(-IND), Spacer(1, 10)]
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
    _, qh = q.wrap(50*mm, 40*mm); q.drawOn(c, LM, H-64*mm-ph-7*mm-qh)
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

