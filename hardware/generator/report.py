from place import *
import csv
import numpy as np
import shapely
import matplotlib as mpl
import matplotlib.pyplot as plt
from matplotlib import font_manager
from matplotlib.patches import PathPatch,Patch
from matplotlib.path import Path as MplPath
from shapely.geometry.polygon import orient
from workspace import FONT, OUTPUT
font_manager.fontManager.addfont(FONT)
mpl.rcParams.update({'font.family':'Microsoft YaHei','pdf.fonttype':42,'axes.unicode_minus':False})
out=OUTPUT
def table(name,cols,data):
    with (out/name).open('w',newline='',encoding='utf-8-sig') as f:
        w=csv.writer(f);w.writerow(cols);w.writerows(data)
def natur(ref):
    m=re.match(r'(\D+)(\d+)$',ref);return (m[1],int(m[2])) if m else (ref,0)
def nets(p):
    return ' | '.join(k+'='+ (v or 'NC') for k,v in p['nets'].items())
def dimensions(p):
    x,y,X,Y=p['bounds'].bounds;return round(X-x,3),round(Y-y,3)
def role(p):
    if p['group']=='Mic':return '上进音话筒及偏置/滤波，预留独立声道'
    if p['ref']=='J1':return '向后盖垂直拔出，先释放线束；配 78172-0003'
    if p['ref']=='J2':return '负Y平插，正Y后翻；尾舌0.30±0.03 mm'
    if p['ref']=='J201':return '从后盖侧垂直扣合，插头1脚仍需原屏实物确认'
    if p['ref']=='U16':return '感光面朝屏，配前框光导'
    if p['ref']=='U301':return '光面朝皮肤；补强在柔板另一侧'
    if p['ref'].startswith('K'):return '自由高1.30，上公差0.15；压合名义1.00'
    if p['ref'] in ('MT1','MT2'):return '开盖侧手焊；焊锡+线包络暂按0.7'
    if p['ref']=='U1':return '模组罩体朝屏，天线向负Y悬出；模组含存储、晶体、天线'
    if p['ref'].startswith('TP'):return '开盖后夹具接触'
    return '未布线初排'
def hsource(p):
    if p['ref'] in ('J1','J2','J201','U1','U301','ENC1') or p['ref'].startswith('K'):return '型号图或R2配合高度；非焊后实测'
    if p['ref'].startswith(('SP','CH','JF','JC','MT','TP')) or p['ref'] in ('JP1','R49','R50'):return '工艺/配合预留值'
    return '预留包络（库名/封装类别，含取整）；非厂家公差上限'
data=[]
for p in sorted(parts.values(),key=lambda p:(p['board'],p['side'],natur(p['ref']))):
    under=p['board']=='main' and p['side']=='B' and p['physical'].intersection(box(-17,-20.4,15,10.1)).area>.001
    a=devices[p['did']]['attributes'];w,h=dimensions(p)
    data.append([p['board'],p['side'],p['ref'],p['source'],p['original'],p['part'],a.get('Supplier Part',''),p['value'],footmeta[p['fp']]['title'],p['fp'],
                 p['status'],'DNP' if p['dnp'] else '初排',round(p['x'],4),round(p['y'],4),p['rot'],w,h,p['height'],hsource(p),
                 '电池投影' if under else '',round(2.15-p['height'],3) if under else '',role(p),nets(p)])
table('placement-BOM.csv',['板','面 F=屏侧 B=后盖侧（主板）','位号','来源板','原位号','型号','LCSC','数值','封装','库ID','封装依据','装配','原点X mm','原点Y mm','逆时针角°','占位X mm','占位Y mm','装配Z预算 mm','高度依据','局部区域','19.20档 到绝缘层余量 mm 未扣公差','装配说明','焊盘网络'],data)
changes=[]
for (tag,ref),q in sorted(source.items(),key=lambda item:(item[0][0],natur(item[0][1]))):
    descendants=[p for p in parts.values() if p['source']==tag and p['original']==ref]
    note='沿用电路并重排'
    if descendants:
        p=descendants[0]
        if tag=='screen':note='迁入腕表主板；跨22P的对应网络在主板合并'
    else:
        trans={('core','CN1'):('J1','两线座改三线，接外贴NTC和电池包'),('core','CN2'):('J2','差分扬声器经后盖10P'),
               ('core','CN3'):('TP11–15 / JP1 / R49 / R50 / R65 / R66','调试、测流和供电通路拆分'),
               ('core','R60'):('R60 / R64','100k固定上拉改外贴NTC分压'),
               ('core','MT1'):('MT1','缩焊盘适配R2'),('core','MT2'):('MT2','缩焊盘适配R2'),
               ('core','R49'):('R49','可拆OLED测流桥'),('core','R50'):('R50','可拆屏3V3测流桥'),
               ('core','SW1'):('Q3 / R61 / K4 / ENC1','表冠接地经P-MOS变高有效'),
               ('core','U5'):('外部夹具','USB-UART移出表体，保留RTS电源循环'),
               ('core','USB1'):('外部夹具 / J2','数据口移出，5V改后盖触点'),
               ('core','FPC1'):('J201','取消板间22P，迁入屏适配板扣座'),
               ('screen','FPC1'):('J201','取消板间22P，仅保留原屏扣座')}
        target,note=trans.get((tag,ref),('', '随原开发板专属功能省去'))
        if q['dnp'] and not target:note='参考版未装；本版不保留该选项'
        if ref.startswith('M') and tag=='screen':note='原屏板铜柱撤去'
        if ref.startswith('TP') and tag=='core':note='重分配至五针夹具及测流桥'
        if ref in ('R59','R5','R3') and q['dnp']:note='原 DNP 保持断开，去除焊位'
        if ref in ('LED1','R6') and tag=='core':note='充电指示使用屏幕，去除直流指示支路'
    dest=' / '.join(p['ref'] for p in descendants) if descendants else target
    if tag=='core' and ref in ('R49','R50','MT1','MT2','R60'):dest=ref
    changes.append([tag,q['page'],ref,q['part'],q['value'],catalog['devices'][q['device']]['attributes'].get('Supplier Part',''),q['fp'],q['dnp'],dest,note,nets(q),nets(parts[dest]) if dest in parts else ''])
for p in parts.values():
    if p['source']=='B':changes.append(['B新增','', '',p['part'],p['value'],'',p['fp'],'',p['ref'],role(p),'',nets(p)])
table('reference-delta.csv',['原板','原页','原位号','型号','值','LCSC','原封装或新增封装ID','参考不装','B位号/去向','变更','原焊盘网络','B焊盘网络'],changes)

def largest_rect(g,step=.25):
    # The half diagonal inset means every emitted grid cell fits in the selected free polygon.
    xs=np.arange(-18.5,18.51,step);ys=np.arange(-22,21.76,step)
    X,Y=np.meshgrid(xs,ys);mask=shapely.contains_xy(g.buffer(-.5*step*math.sqrt(2)),X,Y)
    heights=np.zeros(len(xs),int); best=(0,None)
    for j,row in enumerate(mask):
        heights=np.where(row,heights+1,0);stack=[]
        for i,h in enumerate([*heights,0]):
            start=i
            while stack and stack[-1][1]>h:
                k,H=stack.pop();a=(i-k)*H
                if a>best[0]:best=(a,(xs[k]-step/2,ys[j]-H*step+step/2,xs[i-1]+step/2,ys[j]+step/2))
                start=k
            if not stack or stack[-1][1]<h:stack.append((start,h))
    return box(*best[1]) if best[1] else Polygon()
def polys(g):return [g] if g.geom_type=='Polygon' else [v for v in getattr(g,'geoms',[]) if v.geom_type=='Polygon']
rows=[];regions=[];primary={}
for side in ('F','B'):
    load=unary_union(layers[side])
    for trial,cellgap in [('20.90',3.55),('19.20',2.15),('18.90',2.15),('18.20',1.75)]:
        for limit in (.75,1.25,1.7,2.2):
            # Candidate superimposes R2 local structures and moves PCB relative to the stationary cell.
            geo=boardshape.buffer(-.30).difference(load.buffer(.20)).difference(reserve[side]).difference(shadow(side,limit).buffer(.15))
            motion=json.loads((CAD_INPUTS/'sweep.json').read_text(encoding='utf-8'))
            key='B0' if side=='B' else 'F'+str(next(h for h in [.75,1.25,1.7,3.3] if h>=limit))
            geo=geo.difference(shape(motion[key]).buffer(.12))
            if side=='B':
                if cellgap < limit+.20:geo=geo.difference(box(-17,-20.4,15,10.1))
                if cellgap-.5 < limit+.20:geo=geo.difference(box(10,8.15,12.1,9.15))
            reg=sorted(polys(geo),key=lambda g:g.area,reverse=True)
            rect=largest_rect(geo);x,y,X,Y=rect.bounds
            rows.append([side,trial,limit,round(load.intersection(boardshape).area,2),round(geo.area,2),round(reg[0].area,2),
                         round(X-x,2),round(Y-y,2),*[round(v,3) for v in rect.bounds],len([g for g in reg if g.area>1])])
            if trial=='19.20' and limit==1.25:primary[side]=rect
            if trial=='19.20':
                for i,g in enumerate(reg):
                    if g.area>1:regions.append(dict(type='Feature',properties=dict(side=side,height_mm=limit,index=i+1,area_mm2=round(g.area,3)),geometry=mapping(g)))
table('free-space.csv',['主板面','总厚比较 mm','新器件高度 mm','已排本体与焊盘包络占板面积 mm²','余区合计 mm²','最大连续区 mm²','最大网格矩形X mm','矩形Y mm','x_min','y_min','x_max','y_max','大于1mm²的连续区数'],rows)
(out/'free-regions.geojson').write_text(json.dumps(dict(type='FeatureCollection',features=regions),separators=(',',':')),encoding='utf8')
palette={'Module':'#315467','Charger':'#d38a42','Rails':'#cfaa68','AMOLED':'#875c85','Audio':'#527fa6','Mic':'#4a8498',
         'Motion light':'#5c9075','Crown motor':'#ab6556','Rear interface':'#6f81af','Service':'#7d8582'}
def paint(ax,g,fc,ec='none',lw=.45,ls='-',alpha=1):
    for geom in polys(g):
        q=orient(geom,sign=1)
        verts=[];codes=[]
        for ring in [q.exterior,*q.interiors]:
            pts=list(ring.coords);verts+=pts;codes +=[MplPath.MOVETO]+[MplPath.LINETO]*(len(pts)-2)+[MplPath.CLOSEPOLY]
        ax.add_patch(PathPatch(MplPath(verts,codes),facecolor=fc,edgecolor=ec,linewidth=lw,linestyle=ls,alpha=alpha))
labels={'U1','U3','U4','U6','U9','U13','U14','U15','U16','U17','U19','U21','J1','J2','J201','Q3','Q2','K1','K4','MT1','MT2'}
def draw(ax,side,real=False):
    ax.set_aspect('equal');ax.set_xlim((-20.8,20.8) if side=='F' else (20.8,-20.8));ax.set_ylim(-24,23.8);ax.axis('off')
    paint(ax,boardshape,'#e8efea','#526b61',.65)
    paint(ax,boardshape.intersection(reserve[side]),'#d7dfd9')
    paint(ax,primary[side],'#e8f6f3','#158776',.65,'--')
    if side=='B':
        paint(ax,boardshape.intersection(box(-17,-20.4,15,10.1)),'none','#b98d54',.55,'--')
    for p in parts.values():
        if p['board']!='main' or p['side']!=side:continue
        color=palette[p['group']]
        paint(ax,p['physical'],color,'#233c40',.26)
        for _,pg in p['padshapes']:paint(ax,transform(pg,p['x'],p['y'],p['rot'],side),'#eac081','#785b38',.18)
        if p['ref']=='U1':
            paint(ax,box(-15.5,-22.2,2.5,-16.1),'#bcac78','#695b37',.4)
            ax.text(-6.5,-19.2,'天线',ha='center',va='center',fontsize=4 if real else 8,color='#3b3624')
        if p['ref'] in labels and not real:
            b=p['physical'].bounds;ax.text((b[0]+b[2])/2,(b[1]+b[3])/2,p['ref'],ha='center',va='center',fontsize=5 if p['ref']!='U1' else 10,color='white',
                  bbox=dict(facecolor='#263d41',edgecolor='none',alpha=.78,pad=.45))
    for s in polys(added):paint(ax,s,'none','#e64849',.6)
    ax.plot([-15,-5],[-23,-23],color='#293e40',lw=1.2);ax.text(-10,-23.5,'10 mm',fontsize=4 if real else 7,ha='center',va='top')
fig,axs=plt.subplots(1,2,figsize=(10.2,7.1));fig.patch.set_facecolor('#fbfaf6')
for ax,side,title in zip(axs,['F','B'],['屏侧 · 从正面看','后盖侧 · 翻板后看']):
    draw(ax,side);ax.set_title(title,fontsize=13,color='#345252')
fig.suptitle('WristFlow B   |   R2 双面初排',fontsize=19,y=.96)
fig.subplots_adjust(top=.855,bottom=.16,left=.035,right=.965,wspace=.08)
fig.text(.5,.085,'金色为实际焊盘。绿色虚框为 19.20 mm 档可放 1.25 mm 高器件的连续矩形。',ha='center',fontsize=10)
fig.text(.5,.045,'灰色为装拆保留区，棕色虚线为电池投影，红线为本次局部补板。两面同倍率，单位 mm。',ha='center',fontsize=9,color='#48605c')
fig.savefig(out/'both-sides.png',dpi=205);plt.close(fig)
fig=plt.figure(figsize=(210/25.4,148/25.4))
for x,side,title in [(37,'F','屏侧'),(129,'B','后盖侧（翻板视图）')]:
    ax=fig.add_axes([x/210,47/148,41.6/210,47.8/148]);draw(ax,side,True)
    fig.text((x+20.8)/210,105/148,title,ha='center',fontsize=10)
fig.text(.5,.86,'WristFlow B  ·  R2   /   主板 1:1',fontsize=17,ha='center')
fig.text(.5,.19,'打印选择“实际大小 / 100%”。下方校准线应为 20.00 mm。',fontsize=9,ha='center')
fig.add_artist(plt.Line2D([95/210,115/210],[.13,.13],transform=fig.transFigure,color='black',lw=1))
fig.savefig(out/'layout-1to1.pdf');plt.close(fig)
print('FREE', [r for r in rows if r[1]=='19.20' and r[2]==1.25])
