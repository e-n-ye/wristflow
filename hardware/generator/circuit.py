"""Design database. Pin numbers are physical pad numbers from the V1.2 EPRO."""
import copy
import json
import re
import uuid
import zipfile
from collections import defaultdict
from pathlib import Path
from workspace import REFERENCE

here = Path(__file__).resolve().parent
archive = zipfile.ZipFile(REFERENCE)
catalog = json.loads(archive.read('project.json'))
source = {}
libs = {}
def uid(name):
    return uuid.uuid5(uuid.NAMESPACE_URL,'wristflow/r2/'+name).hex
def records(data):
    return [r for line in data.splitlines() if line.strip() and (r:=json.loads(line))]
def encode(rows):
    return ('\n'.join(json.dumps(x,ensure_ascii=False,separators=(',',':')) for x in rows)+'\n').encode()
for key,board in catalog['boards'].items():
    tag='core' if key=='核心板' else 'screen'
    sch=catalog['schematics'][board['schematic']]
    statuses={}
    for page in sch['sheets']:
        sheet=records(archive.read(f"SHEET/{board['schematic']}/{page['id']}.esch"))
        attrs=defaultdict(dict)
        for row in sheet:
            if row[0]=='ATTR':attrs[row[2]][row[3]]=row[4]
        for attr in attrs.values():
            if attr.get('Designator'):statuses[attr['Designator']]=(page['name'],attr.get('Add into BOM')=='no')
    raw=records(archive.read('PCB/'+board['pcb']+'.epcb'))
    starts=[i for i,row in enumerate(raw) if row[0]=='DOCTYPE']
    for a,b in zip(starts,starts[1:]):
        f=raw[a:b]
        libs[f[1][1]['uuid']]=f
    raw=raw[starts[-1]:]
    if tag=='core':pcb_template=raw
    for row in [r for r in raw if r[0]=='COMPONENT']:
        cid=row[1]
        attr={r[7]:r[8] for r in raw if r[0]=='ATTR' and r[3]==cid}
        ref=attr['Designator']
        d=catalog['devices'][attr['Device']]
        fp=attr.get('Footprint',d['attributes']['Footprint'])
        pin={r[2]:r[3] for r in raw if r[0]=='PAD_NET' and r[1]==cid}
        page,dnp=statuses.get(ref,('PCB only',False))
        source[tag,ref]=dict(source=tag,original=ref,device=attr['Device'],fp=fp,part=d['title'],nets=pin,
                             page=page,dnp=dnp,value=d['attributes'].get('Value',''),status='参考实装封装')

def loadfp(key):
    if key not in libs:libs[key]=records(archive.read('FOOTPRINT/'+key+'.efoo'))
    return libs[key]
parts={}
aliases={'USB_VBUS_5V':'DOCK_PROTECTED','PA_44_VBUSDET':'PA44_VBUS_DET','PA_24_SPI1_DO':'CROWN_A',
         'PA_25_SPI1_DI':'CROWN_B','PA_27_SD_DET':'PPG_INT_N','CH_RTS#':'RTS_N_OD',
         '$32N61845':'CELL_TS','$32N61853':'CHG_VDD','$2N93902':'MOD_VSYS',
         '$2N99775':'MOD_SW_OUT','$20N34914':'OLED_VBAT','$20N11228':'OLED_3V3',
         '$1N6115':'OLED_PULL10K','$1N6940':'OLED_PULL1M','$1N8098':'OLED_VCI',
         '$14N36026':'MOTOR_P','$14N37208':'MOTOR_N','$14N36235':'MOTOR_GATE'}
def net(n):
    return aliases.get(n,n.replace('$','N_'))
def keep(tag,ref,group,new=None):
    p=copy.deepcopy(source[tag,ref])
    p.update(ref=new or ref,group=group,board='main')
    p['nets']={k:net(v) for k,v in p['nets'].items()}
    parts[p['ref']]=p
    return p
groups={
 'Module':['U1','U19','R58','R1','C1','C38','R11','R12'],
 'Charger':['U17','U3','R2','R4','R5','R14','R15','C2','C3','C6','C7','C8','D17','D18','Q1','R42','R43'],
 'Rails':['U2','U4','U15','C4','C5','C9','C10','C33','C34','R3','R7','R41','R52','R54'],
 'AMOLED':['R16','R17','R18','R19','R20','R46','R47','R48','D5','D6','D7','D8','R49','R50','C13','C14','C15'],
 'Audio':['U6','C18','C19','C20','C21','C22','C23','C24','C25','L1','L2','L3','R26','R27','R28','R29','D1','D2'],
 'Mic':['U9','C26','C27','L4','R30'],
 'Motion light':['U13','U14','U16','R44','R45','C29','C30','C31','C35'],
 'Crown motor':['R34','R35','D15','Q2','R36','R37','R38','C28','D16']}
for group,refs in groups.items():
    for ref in refs:keep('core',ref,group)
for ref in ['CN1','C1','C2','C3','R1','R2','R6']:
    p=keep('screen',ref,'AMOLED',('J' if ref=='CN1' else ref[0])+str(200+int(re.search(r'\d+',ref)[0])))
    def panelnet(n):
        reverse={'VBAT':'OLED_VBAT','VCC_3V3':'OLED_3V3'}
        for a in ['R16','R17','R18','R19','R20']:
            reverse[parts[a]['nets']['1']]=parts[a]['nets']['2']
        return reverse.get(n,n)
    p['nets']={k:panelnet(v) for k,v in p['nets'].items()}
def add(ref,part,group,nets,fp=None,status='项目库候选',board='main',value=''):
    dev=next((k for k,v in catalog['devices'].items() if v['title']==part),None)
    if not fp and dev:fp=catalog['devices'][dev]['attributes']['Footprint']
    parts[ref]=dict(ref=ref,source='B',original='',page='',group=group,board=board,part=part,device=dev,fp=fp,
                    nets={str(k):v for k,v in nets.items()},value=value,status=status,dnp=False)
def twin(ref,base,group,nets,**kwargs):
    p=source['core',base]
    add(ref,p['part'],group,nets,fp=p['fp'],status='沿用参考库',value=p['value'],**kwargs)
def passive(ref,value,group,a,b,base='R5'):
    p=source['core',base]
    add(ref,value+' / 0402',group,{1:a,2:b},p['fp'],value=value,status='规格候选')
    parts[ref]['symbol_template']=p['device']
add('Q3','CJ2301 S1','Crown motor',{1:'KEY_N',2:'VBAT',3:'PA_34_KEY1'})
twin('R61','R5','Crown motor',{1:'VBAT',2:'KEY_N'})
for i,n in enumerate(['CROWN_A','GND','CROWN_B','KEY_N']):
    add('K'+str(i+1),'S7241-45R','Crown motor',{1:n},'spring','厂家焊盘图')
for ref,n in [('R62','CROWN_A'),('R63','CROWN_B')]:
    twin(ref,'R5','Crown motor',{1:'VDD33_VOUT',2:n})
# Use the switchable sensor rail instead of VDD33_VOUT (motor LDO3 is normally off).
for ref in ['R62','R63']:parts[ref]['nets']['1']='GS_3V3'
add('J1','78171-0003','Charger',{1:'VBAT_S',2:'CELL_TS',3:'GND','MP':''},'battery','Molex / KiCad 焊盘')
passive('R60','11.3k','Charger','CHG_VDD','CELL_TS')
passive('R64','162k','Charger','CELL_TS','GND')
add('D19','B5819W SL','Charger',{1:'DOCK_PROTECTED',2:'DOCK_5V_IN'})
for ref,n in [('MT1','MOTOR_P'),('MT2','MOTOR_N')]:
    add(ref,'Solder land 0.76x1.00','Crown motor',{1:n},'motor','R2 小焊盘，线径/剥线待验')
for i,n in enumerate(['GND','MOD_VSYS','UART_RXD','UART_TXD','RTS_N_OD']):
    add('TP'+str(11+i),'Pogo land 1.14','Service',{1:n},'pogo','R2 夹具焊盘')
for i,(a,b) in enumerate([('VBAT_S','VBAT'),('VSYS_1','OLED_VBAT'),('VCC_3V3','OLED_3V3')]):
    ref=['JP1','R49','R50'][i]
    add(ref,'Meter bridge 1.80 pitch','Service',{1:a,2:b},'bridge','可拆短接件机械占位')
for ref,a,b in [('R65','VSYS_S','VSYS'),('R66','VCC_3V3_S','VCC_3V3'),('R67','GND','PPG_GND')]:
    twin(ref,'R1','Service',{1:a,2:b})
add('U21','TLV70018DBVR','Rear interface',{1:'GS_3V3',2:'GND',3:'GS_3V3',4:'',5:'PPG_1V8'},source['core','U4']['fp'],'DBV5 候选')
parts['U21']['symbol_template']=source['core','U4']['device']
for ref,n in [('C39','GS_3V3'),('C40','PPG_1V8')]:twin(ref,'C2','Rear interface',{1:n,2:'GND'})
twin('R68','R5','Rear interface',{1:'GS_3V3',2:'PPG_INT_N'})
rear=['SPK_OUTP','SPK_OUTN','PPG_GND','GS_3V3','PPG_1V8','PA_40_I2C1_SCL','PA_39_I2C1_SDA','PPG_INT_N','GND','DOCK_5V_IN']
add('J2','FH34SRJ-10S-0.5SH(50)','Rear interface',{**dict(enumerate(rear,1)),'MP':''},'rear','Hirose 焊盘 / 方向待配对')
# Remove the previous development-board-only stubs from the main module.
other={v for r,p in parts.items() if r!='U1' for v in p['nets'].values() if v}
parts['U1']['nets']={k:v if v in other else '' for k,v in parts['U1']['nets'].items()}
# Flex and vertical crown daughterboard are separate electrical assemblies.
rear_local=[n if i!=8 else 'DOCK_RTN' for i,n in enumerate(rear)]
add('JF1','10P FPC fingers','Rear flex',dict(enumerate(rear_local,1)),'fingers','FPC 尾指，展开待定','rear')
add('U301','MAX30102EFD+T','Rear flex',dict(enumerate(['','PA_40_I2C1_SCL','PA_39_I2C1_SDA','PPG_GND','','','','','GS_3V3','GS_3V3','PPG_1V8','PPG_GND','PPG_INT_N',''],1)),'ppg','OESIP14，焊盘待图纸复核','rear')
for ref,base,n in [('C301','C6','GS_3V3'),('C302','C11','GS_3V3'),('C303','C1','PPG_1V8'),('C304','C11','PPG_1V8')]:
    twin(ref,base,'Rear flex',{1:n,2:'PPG_GND'},board='rear')
for ref,n in [('SP1','SPK_OUTP'),('SP2','SPK_OUTN'),('CH1','DOCK_RTN'),('CH2','DOCK_5V_IN')]:
    add(ref,'Termination '+ref,'Rear flex',{1:n},'flexland','接线端位置占位','rear')
add('ENC1','BMQ22-N1A1-2012-A','Crown',dict(A='CROWN_A',C='GND',B='CROWN_B',S1='KEY_N',S2='GND',MP=''), 'encoder','R2 承认图转录，焊盘待核','crown')
for i,n in enumerate(['CROWN_A','GND','CROWN_B','KEY_N']):
    add('JC'+str(i+1),'Daughter joint','Crown',{1:n},'joint','直角焊接位占位','crown')
    add('JC'+str(i+11),'Daughter joint','Contact bridge',{1:n},'joint','直角焊接位占位','bridge')
    add('P'+str(i+1),'Gold mating land','Contact bridge',{1:n},'gold','R2 弹片对接镀金面','bridge')
