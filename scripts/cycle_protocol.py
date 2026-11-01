"""Persistent 64-byte cycle rows, strict CRC/order checks; no device access."""
import csv
import io
import re
import struct
import zlib

def decode_row(raw):
    if len(raw)!=64 or raw[:6] not in (b'WFCL\x01\x01',b'WFCL\x01\x02',b'WFCL\x01\x03'):
        raise ValueError('Cycle row format')
    if raw[7] or raw[6]&~63 or zlib.crc32(raw[:60])!=int.from_bytes(raw[60:],'little'):
        raise ValueError('Cycle row CRC/flags')
    session,sequence,elapsed,utc,revision,sampled,adc=struct.unpack_from('<IIQIIII',raw,8)
    mv,filtered=struct.unpack_from('<HH',raw,40)
    surface=struct.unpack_from('<H',raw,52)[0]
    age=struct.unpack_from('<H',raw,58)[0]
    if not session or sequence>=40000 or elapsed>172860000 or raw[56]>8 or raw[57]&~15:
        raise ValueError('Cycle row range')
    if raw[6]&1 and not 1767225600<=utc<=4102444799:
        raise ValueError('Cycle UTC')
    if raw[6]&4 and raw[44]>100: raise ValueError('Cycle percent')
    return dict(kind=raw[5],flags=raw[6],session=session,sequence=sequence,elapsed_ms=elapsed,
        utc=utc,clock_revision=revision,adc_sampled_ms=sampled,adc_sequence=adc,
        raw_mv=mv,filtered_decimillivolts=filtered,percent=raw[44],candidate=raw[45],mode=raw[46],decision=raw[47],
        charge=raw[48],charge_reason=raw[49],requested=raw[50],acknowledged=raw[51],surface=surface,
        context_flags=raw[54],phase=raw[55],stop_reason=raw[56],usb_flags=raw[57],sample_age_ms=age)

def parse_info(lines):
    pattern=r'\[cycle-info\] id=(\d+) rows=(\d+) bytes=(\d+) crc=([0-9a-fA-F]{8}) terminal=([01])$'
    rows=[re.search(pattern,line) for line in lines if '[cycle-info]' in line]
    if len(rows)!=1 or rows[0] is None: raise ValueError('One complete cycle manifest required')
    m=rows[0];sid,count,size=int(m[1]),int(m[2]),int(m[3])
    if not 0<sid<2**32 or not 0<count<=40000 or size!=count*64: raise ValueError('Cycle manifest range')
    return dict(session=sid,rows=count,bytes=size,crc=int(m[4],16),terminal=bool(int(m[5])))

def parse_page(lines,session,offset,count):
    chunks={};order=[]
    for line in lines:
        if '[cycle-data]' not in line:continue
        m=re.search(r'\[cycle-data\] id=(\d+) i=(\d+) p=([01]) hex=([0-9a-fA-F]{64})$',line)
        if not m or int(m[1])!=session: raise ValueError('Malformed/mixed cycle data')
        key=(int(m[2]),int(m[3]))
        if key in chunks:raise ValueError('Duplicate cycle data')
        chunks[key]=bytes.fromhex(m[4]);order.append(key)
    if order!=[(i,p) for i in range(offset,offset+count) for p in (0,1)]:raise ValueError('Missing/reordered cycle page')
    result=b''
    for i in range(offset,offset+count):
        raw=chunks[i,0]+chunks[i,1];r=decode_row(raw)
        if r['session']!=session or r['sequence']!=i:raise ValueError('Cycle row identity')
        result+=raw
    return result

def decode_file(raw,info):
    if len(raw)!=info['bytes'] or zlib.crc32(raw)!=info['crc']:raise ValueError('Whole cycle CRC/size')
    rows=[decode_row(raw[i:i+64]) for i in range(0,len(raw),64)]
    previous=0
    for i,row in enumerate(rows):
        if row['session']!=info['session'] or row['sequence']!=i or row['elapsed_ms']<previous:
            raise ValueError('Cycle order/time')
        if (i==0)!=(row['kind']==1) or (row['kind']==3 and i!=len(rows)-1):raise ValueError('Cycle lifecycle')
        previous=row['elapsed_ms']
    if (rows[-1]['kind']==3)!=info['terminal']:raise ValueError('Cycle terminal mismatch')
    return rows

def csv_text(rows):
    out=io.StringIO(newline='');writer=csv.DictWriter(out,fieldnames=list(rows[0]))
    writer.writeheader();writer.writerows(rows);return out.getvalue()
