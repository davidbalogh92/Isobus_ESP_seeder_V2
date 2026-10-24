"""Minimal SVG rasteriser for the ESC icon set (path M/L/H/V/C/Q/A/Z + rel, circle, ellipse,
rect, polygon, polyline, line, g inheritance, translate/scale/rotate/matrix transforms incl. nested groups). Supersampled PIL drawing, round caps/joins."""
import re, math, xml.etree.ElementTree as ET
from PIL import Image, ImageDraw
NS='{http://www.w3.org/2000/svg}'
def col(c):
    if c in (None,'none'):return None
    if c=='white':return (255,255,255)
    if c=='black':return (0,0,0)
    c=c.lstrip('#')
    if len(c)==3:c=''.join(x*2 for x in c)
    return tuple(int(c[i:i+2],16) for i in (0,2,4))
def arc_pts(x1,y1,rx,ry,phi,fa,fs,x2,y2,n=48):
    if rx==0 or ry==0:return [(x2,y2)]
    phi=math.radians(phi);c,s=math.cos(phi),math.sin(phi)
    dx,dy=(x1-x2)/2,(y1-y2)/2
    x1p=c*dx+s*dy;y1p=-s*dx+c*dy
    rx,ry=abs(rx),abs(ry)
    l=x1p**2/rx**2+y1p**2/ry**2
    if l>1:rx*=math.sqrt(l);ry*=math.sqrt(l)
    num=rx**2*ry**2-rx**2*y1p**2-ry**2*x1p**2
    den=rx**2*y1p**2+ry**2*x1p**2
    k=math.sqrt(max(0,num/den)) if den else 0
    if fa==fs:k=-k
    cxp=k*rx*y1p/ry;cyp=-k*ry*x1p/rx
    cx=c*cxp-s*cyp+(x1+x2)/2;cy=s*cxp+c*cyp+(y1+y2)/2
    def ang(ux,uy,vx,vy):
        a=math.atan2(ux*vy-uy*vx,ux*vx+uy*vy);return a
    t1=ang(1,0,(x1p-cxp)/rx,(y1p-cyp)/ry)
    dt=ang((x1p-cxp)/rx,(y1p-cyp)/ry,(-x1p-cxp)/rx,(-y1p-cyp)/ry)
    if not fs and dt>0:dt-=2*math.pi
    if fs and dt<0:dt+=2*math.pi
    pts=[]
    for i in range(1,n+1):
        t=t1+dt*i/n
        pts.append((c*rx*math.cos(t)-s*ry*math.sin(t)+cx,s*rx*math.cos(t)+c*ry*math.sin(t)+cy))
    return pts
def parse_path(d):
    toks=re.findall(r'[A-Za-z]|-?\d*\.?\d+(?:e-?\d+)?',d)
    i=0;subs=[];cur=[];x=y=0;sx=sy=0;cmd=None
    def num():
        nonlocal i;v=float(toks[i]);i+=1;return v
    while i<len(toks):
        if re.match('[A-Za-z]',toks[i]):cmd=toks[i];i+=1
        rel=cmd.islower();C=cmd.upper()
        if C=='Z':
            if cur:cur.append((sx,sy));subs.append((cur,True));cur=[]
            x,y=sx,sy;continue
        if C=='M':
            if cur:subs.append((cur,False))
            nx,ny=num(),num()
            if rel:nx+=x;ny+=y
            x,y=nx,ny;sx,sy=x,y;cur=[(x,y)];cmd='l' if rel else 'L';continue
        if C=='L':
            nx,ny=num(),num()
            if rel:nx+=x;ny+=y
            x,y=nx,ny;cur.append((x,y))
        elif C=='H':
            nx=num();x=nx+x if rel else nx;cur.append((x,y))
        elif C=='V':
            ny=num();y=ny+y if rel else ny;cur.append((x,y))
        elif C=='C':
            v=[num() for _ in range(6)]
            if rel:v=[v[0]+x,v[1]+y,v[2]+x,v[3]+y,v[4]+x,v[5]+y]
            for k in range(1,25):
                t=k/24;u=1-t
                cur.append((u**3*x+3*u*u*t*v[0]+3*u*t*t*v[2]+t**3*v[4],u**3*y+3*u*u*t*v[1]+3*u*t*t*v[3]+t**3*v[5]))
            x,y=v[4],v[5]
        elif C=='Q':
            v=[num() for _ in range(4)]
            if rel:v=[v[0]+x,v[1]+y,v[2]+x,v[3]+y]
            for k in range(1,17):
                t=k/16;u=1-t
                cur.append((u*u*x+2*u*t*v[0]+t*t*v[2],u*u*y+2*u*t*v[1]+t*t*v[3]))
            x,y=v[2],v[3]
        elif C=='A':
            rx,ry,phi,fa,fs,nx,ny=[num() for _ in range(7)]
            if rel:nx+=x;ny+=y
            cur+=arc_pts(x,y,rx,ry,phi,int(fa),int(fs),nx,ny);x,y=nx,ny
    if cur:subs.append((cur,False))
    return subs
def render(svgfile,size,ss=8,bg=None):
    """Returns RGBA image of size x size (icon is square viewBox). Alpha is 0/255 after threshold? no: caller decides."""
    root=ET.parse(svgfile).getroot()
    vb=[float(v) for v in root.get('viewBox').split()]
    S=size*ss;k=S/vb[2]
    img=Image.new('RGBA',(S,S),(0,0,0,0))
    def inh(el,st):
        st=dict(st)
        for a in ('fill','stroke','stroke-width'):
            if el.get(a) is not None:st[a]=el.get(a)
        return st
    def mul(A,B):  # affine matrices as (a,b,c,d,e,f): x'=a*x+c*y+e, y'=b*x+d*y+f
        a1,b1,c1,d1,e1,f1=A;a2,b2,c2,d2,e2,f2=B
        return (a1*a2+c1*b2,b1*a2+d1*b2,a1*c2+c1*d2,b1*c2+d1*d2,a1*e2+c1*f2+e1,b1*e2+d1*f2+f1)
    def parse_tf(t):
        M=(1,0,0,1,0,0)
        for name,args in re.findall(r'(\w+)\s*\(([^)]*)\)',t or ''):
            v=[float(n) for n in re.findall(r'-?\d*\.?\d+(?:e-?\d+)?',args)]
            if name=='translate':T=(1,0,0,1,v[0],v[1] if len(v)>1 else 0)
            elif name=='scale':T=(v[0],0,0,v[1] if len(v)>1 else v[0],0,0)
            elif name=='rotate':
                a=math.radians(v[0]);c,s_=math.cos(a),math.sin(a);T=(c,s_,-s_,c,0,0)
                if len(v)==3:T=mul(mul((1,0,0,1,v[1],v[2]),T),(1,0,0,1,-v[1],-v[2]))
            elif name=='matrix':T=tuple(v[:6])
            else:raise ValueError('unsupported transform '+name)
            M=mul(M,T)
        return M
    def apply(M,pts):
        a,b,c,d,e,f=M
        return [(a*x+c*y+e,b*x+d*y+f) for x,y in pts]
    def draw_poly(pts,closed,st):
        layer=Image.new('RGBA',(S,S),(0,0,0,0));dr=ImageDraw.Draw(layer)
        P=[((px-vb[0])*k,(py-vb[1])*k) for px,py in pts]
        f=col(st.get('fill','black'))
        if f and closed and len(P)>2:dr.polygon(P,fill=f+(255,))
        sc=col(st.get('stroke'))
        if sc:
            w=float(st.get('stroke-width','1'))*k;r=w/2
            Q=P+[P[0]] if closed and P[0]!=P[-1] else P
            for a,b in zip(Q,Q[1:]):
                dr.line([a,b],fill=sc+(255,),width=max(1,round(w)))
            for p in Q:dr.ellipse([p[0]-r,p[1]-r,p[0]+r,p[1]+r],fill=sc+(255,))
        img.alpha_composite(layer)
    def walk(el,st,M=(1,0,0,1,0,0)):
        st=inh(el,st);tag=el.tag.replace(NS,'')
        M=mul(M,parse_tf(el.get('transform')))
        def draw_poly_m(pts,closed,st):draw_poly(apply(M,pts),closed,st)
        if tag=='path':
            for pts,closed in parse_path(el.get('d')):
                draw_poly_m(pts,closed,st)
        elif tag=='circle':
            cx,cy,r=float(el.get('cx')),float(el.get('cy')),float(el.get('r'))
            draw_poly_m([(cx+r*math.cos(2*math.pi*i/96),cy+r*math.sin(2*math.pi*i/96)) for i in range(96)],True,st)
        elif tag=='rect':
            x,y,w,h=[float(el.get(a)) for a in 'x y width height'.split()];rx=float(el.get('rx','0'))
            pts=[]
            if rx>0:
                for cx,cy,a0 in ((x+w-rx,y+rx,-90),(x+w-rx,y+h-rx,0),(x+rx,y+h-rx,90),(x+rx,y+rx,180)):
                    for i in range(13):
                        a=math.radians(a0+90*i/12);pts.append((cx+rx*math.cos(a),cy+rx*math.sin(a)))
            else:pts=[(x,y),(x+w,y),(x+w,y+h),(x,y+h)]
            draw_poly_m(pts,True,st)
        elif tag=='ellipse':
            cx,cy=float(el.get('cx')),float(el.get('cy'));rx,ry=float(el.get('rx')),float(el.get('ry'))
            draw_poly_m([(cx+rx*math.cos(2*math.pi*i/96),cy+ry*math.sin(2*math.pi*i/96)) for i in range(96)],True,st)
        elif tag=='line':
            draw_poly_m([(float(el.get('x1')),float(el.get('y1'))),(float(el.get('x2')),float(el.get('y2')))],False,st)
        elif tag=='polyline':
            v=[float(n) for n in re.findall(r'-?\d*\.?\d+',el.get('points'))]
            draw_poly_m(list(zip(v[::2],v[1::2])),False,st)
        elif tag=='polygon':
            v=[float(n) for n in re.findall(r'-?\d*\.?\d+',el.get('points'))]
            draw_poly_m(list(zip(v[::2],v[1::2])),True,st)
        for ch in el:walk(ch,st,M)
    walk(root,{'fill':'black'})
    return img.resize((size,size),Image.LANCZOS)
if __name__=='__main__':
    import sys
    render(sys.argv[1],int(sys.argv[2])).save(sys.argv[3])
