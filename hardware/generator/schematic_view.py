import json,math,zipfile
import matplotlib as mpl
import matplotlib.pyplot as plt
from matplotlib.backends.backend_pdf import PdfPages
from matplotlib import font_manager
from matplotlib.patches import Circle,Ellipse,Rectangle
from collections import defaultdict
from workspace import FONT, OUTPUT
font_manager.fontManager.addfont(FONT)
mpl.rcParams.update({'font.family':'Microsoft YaHei','pdf.fonttype':42})
out=OUTPUT
def rows(z,n):return [r for line in z.read(n).splitlines() if line.strip() and (r:=json.loads(line))]
with PdfPages(out/'schematics.pdf') as pdf:
    for stem in ['WristFlow-B-R2','WristFlow-B-Rear-FPC','WristFlow-B-Crown','WristFlow-B-Crown-Contacts']:
        with zipfile.ZipFile(out/(stem+'.epro')) as z:
            proj=json.loads(z.read('project.json'))
            sid,sch=next(iter(proj['schematics'].items()))
            for page in sch['sheets']:
                rs=rows(z,f"SHEET/{sid}/{page['id']}.esch")
                fig,ax=plt.subplots(figsize=(16.54,11.69))
                fig.subplots_adjust(left=.03,right=.97,top=.88,bottom=.06)
                fig.text(.035,.95,page['name'],fontsize=19,color='#213f4b')
                fig.text(.965,.955,stem,fontsize=11,ha='right',color='#416575')
                noted=[r[5] for r in rs if r[0]=='TEXT']
                fig.text(.035,.915,noted[-1],fontsize=9,color='#486557')
                at=defaultdict(dict)
                for r in rs:
                    if r[0]=='ATTR':at[r[2]][r[3]]=r
                colors={'pin':'#6c4c50','net':'#15624c','body':'#62454d'}
                def line(xs,ys,color,lw=.65):ax.plot(xs,ys,color=color,lw=lw,solid_capstyle='round')
                texts=[]
                for r in rs:
                    if r[0]=='COMPONENT':
                        x,y=r[3:5];s=rows(z,'SYMBOL/'+at[r[1]]['Symbol'][4]+'.esym');active=False;pa=defaultdict(dict)
                        for v in s:
                            if v[0]=='PART':active=v[1]==r[2]
                            if not active:continue
                            k=v[0]
                            if k=='RECT':
                                x0,y0,x1,y1=v[2:6]
                                ax.add_patch(Rectangle((x+x0,y+y0),x1-x0,y1-y0,fill=False,ec=colors['body'],lw=.75))
                            if k in ('CIRCLE','ELLIPSE'):
                                a=Circle((x+v[2],y+v[3]),v[4]) if k=='CIRCLE' else Ellipse((x+v[2],y+v[3]),2*v[4],2*v[5])
                                a.set(fill=False,edgecolor=colors['body'],linewidth=.65);ax.add_patch(a)
                            if k=='POLY':
                                pts=v[2];line([x+i for i in pts[::2]],[y+i for i in pts[1::2]],colors['body'])
                            if k=='PIN':
                                px,py=x+v[4],y+v[5];a=math.radians(v[7]);line([px,px+v[6]*math.cos(a)],[py,py+v[6]*math.sin(a)],colors['pin'])
                            if k=='ATTR' and v[3] in ('NUMBER','NAME') and v[6] and v[7] is not None:
                                texts.append((x+v[7],y+v[8],str(v[4]),v[9] or 0,4.3,colors['pin']))
                        for key,size in [('Designator',7.8),('Name',6.5)]:
                            a=at[r[1]][key];texts.append((a[7],a[8],str(a[4]),0,size,'#173359'))
                    if r[0]=='WIRE':
                        for x,y,X,Y in r[2]:line([x,X],[y,Y],colors['net'])
                        a=at[r[1]]['NET']
                        if a[6]:texts.append((a[7],a[8],str(a[4]),a[9],6.4,colors['net']))
                ax.set_aspect('equal');ax.autoscale();ax.margins(x=.065,y=.08);ax.axis('off')
                fig.canvas.draw()
                scale=(ax.transData.transform((1,0))[0]-ax.transData.transform((0,0))[0])*72/fig.dpi
                for x,y,label,rot,size,color in texts:ax.text(x,y,label,rotation=rot,ha='left',va='baseline',fontsize=size*scale,color=color,clip_on=False)
                pdf.savefig(fig)
                if stem=='WristFlow-B-R2' and page['id']==2:
                    fig.savefig(out/'schematic-qa.png',dpi=90)
                plt.close(fig)
