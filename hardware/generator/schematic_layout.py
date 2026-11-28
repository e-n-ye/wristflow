"""Readable page layout for the B-R2 KiCad schematics.

Coordinates are millimetres on the KiCad page (y down). The circuit itself comes from the
reviewed EPRO; this file only decides where symbols, wires, labels and frames go.
Pins are addressed as 'REF.PIN' or 'REF@NET'. Unwired pins get a power symbol (rails in
POWER), a net label (global when the net also appears on another page), or a no-connect mark.
"""

# Rails drawn with power symbols. 'gnd' uses the ground symbol, 'sup' the supply bar.
POWER = {n: 'gnd' for n in ('GND', 'SPK_GND', 'MIC_GND', 'PPG_GND')}
POWER.update({n: 'sup' for n in (
    'VBAT', 'VBAT_S', 'VSYS', 'VSYS_S', 'VSYS_1', 'MOD_VSYS', 'VCC_3V3', 'VCC_3V3_S', 'GS_3V3',
    'PPG_1V8', 'VBUS_5V', 'CHG_VDD', 'OLED_VBAT', 'OLED_3V3', 'VDD33_VOUT', 'DOCK_5V_IN')})

# Display text for values whose reviewed name is a manufacturer code (UniOhm 4-digit code).
VALUE_TEXT = {'0402WGF1000TCE': '100Ω', '0402WGF5101TCE': '5.1kΩ'}

U1_GND = ['1', '26', '35', '48'] + [str(n) for n in range(49, 61)]
NC6 = ['1', '5', '6', '7', '8', '14']

# Pin arrangement of generated box symbols. Slots are pin numbers, lists of stacked pin
# numbers sharing one position (same net), or None for a gap.
PIN_SIDES = {
    'U1': {
        'left': ['61', '27', '17', None, U1_GND, None, '36', '37', '33', '34', None,
                 '62', '46', '68', '67', '66', '63', '64', '65', '6', '5', '22', '23', '16', '13', '12', '3'],
        'right': ['47', '45', '44', '43', '42', '41', '40', '39', '38', None, '32', '31', None, '30', '29', None,
                  '28', '18', '19', '20', '21', '24', '25', '15', '14', '11', '10', '9', '8', '7', '4', '2'],
    },
    'U19': {'left': ['A2', 'B2'], 'right': ['A1'], 'bottom': ['B1']},
    'U2': {'left': ['A2', 'B2'], 'right': ['A1'], 'bottom': ['B1']},
    'U4': {'left': ['1', '3'], 'right': ['5', '4'], 'bottom': ['2']},
    'U15': {'left': ['1', '3'], 'right': ['5', '4'], 'bottom': ['2']},
    'U17': {'left': ['1', '5', '6', None, '3', '4'], 'right': ['8', None, '7'], 'bottom': [['2', '9']]},
    'U3': {'left': ['A1', None, 'B2', None, 'C1', 'C2', None], 'right': ['A2', None, 'A3', None, 'B3', None, 'B1'],
           'bottom': ['C3']},
    'J1': {'left': ['3', None, '2', None, '1', None, 'MP']},
    'J201': {'left': ['24', '23', '22', '21', '20', None, None, '19', None, '18', '17', '16', None, None, '15', '14', '13'],
             'right': ['1', None, '2', '3', None, '4', '5', None, '6', None, '7', None, None, '8', '9', '10', '11', '12'],
             'bottom': [[str(n) for n in range(25, 41)]]},
    'U6': {'left': ['3', None, None, '4', None, '1', '2'], 'right': ['8', None, None, None, '5'],
           'top': ['6'], 'bottom': ['7']},
    'U9': {'left': ['1'], 'right': ['4'], 'bottom': [['2', '3']]},
    'U13': {'left': ['14', '13', '4', '9', None, '1', '2', '3'], 'right': ['8', '5', '12', None, '10', '11'],
            'bottom': [['6', '7']]},
    'U14': {'left': ['B2', 'A2'], 'right': ['B1'], 'bottom': ['A1']},
    'U16': {'left': ['6', '4', '5'], 'right': ['1', '2'], 'bottom': ['3']},
    'U21': {'left': ['1', '3'], 'right': ['5', '4'], 'bottom': ['2']},
    'J2': {'left': [str(n) for n in range(1, 11)], 'bottom': ['11', '12']},
    'K1': {'left': ['1', '2']}, 'K2': {'left': ['1', '2']}, 'K3': {'left': ['1', '2']}, 'K4': {'left': ['1', '2']},
    'U301': {'left': ['2', '3', '13'], 'right': [['9', '10'], '11', None] + NC6, 'bottom': [['4', '12']]},
    'JF1': {'right': [str(n) for n in range(1, 11)]},
    'ENC1': {'right': ['A', 'C', 'B', None, 'S1', 'S2', None, 'MP']},
}

# Reference/value text offsets for tightly stacked parts: ref -> ((dx, dy, justify), (dx, dy, justify)).
STACKED = ((-0.6, -1.4, 'right bottom'), (0.6, -1.4, 'left bottom'))
LABEL_AT = {r: STACKED for r in ('R16', 'R17', 'R18', 'R19', 'R20')}
LABEL_AT['D17'] = ((-2.0, -0.6, 'right bottom'), (-2.0, 0.6, 'right top'))
LABEL_AT['U6'] = ((1.5, -26.5, 'left bottom'), (1.5, -24.0, 'left bottom'))

PAGE_SUMMARY = {
    '01-module': ['SF32LB52 模组与全部 GPIO 去向', '模组供电循环开关 U19', '四针治具与镊子复位焊盘'],
    '02-charger': ['后盖 5 V 触点输入、防反、TVS、过压保护', 'AW32001 充电与系统路径、I2C0', 'J1 电芯接口、TS 分压、线束器件'],
    '03-rails': ['VSYS_1 开关、显示 3V3、GS_3V3 LDO', '测流桥 JP1/R49/R50 与默认 0 Ω 通路'],
    '04-display': ['原屏 CN1 直连（J201）与 QSPI 端接', '屏供电去耦、触摸 ESD 与上拉'],
    '05-audio': ['NS4150B 差分功放、输出磁珠与 ESD', '上进音麦克风 LMA2718'],
    '06-io': ['IMU / 地磁 / 光感（I2C1）', '表冠编码器与按键、振动电机', '后盖 10P 接口与 1.8 V 光学电源'],
}
OVERVIEW = {'main': [
    '读图约定：电源与地用电源符号；跨页信号和连接器上的电源脚用带框的全局标签，本页信号用普通标签；× 为不连接。',
    '网络名、位号和每个引脚的连接与 2026-11-22 评审版 EPRO 逐项一致（见 equivalence.json）。',
    'R60/R64 为暂定 TS 分压；温度保护验证前保持充电关闭。C22/C23/C25 不贴。',
    '原理图与同名 KiCad PCB 通过项目封装库 WF_B 和层次路径关联。',
]}


def lead(pg, spec, *points, net=None, d='R'):
    """Wire from a pin through points and put the net decoration on the last point."""
    pg.wire(spec, *points)
    pg.tag(*points[-1], net, d)


def at(pg, spec):
    return pg.pin(spec)


def divider(pg, upper, lower, x, y, net, d='R', run=12.7):
    """Two vertical resistors joined at `net`, with a side lead labelled `net`."""
    pg.part(upper, x, y, pin=net, rot=_rot_bottom(pg, upper, net))
    lo = pg.pin(f'{upper}@{net}')
    pg.part(lower, x, lo[1] + 5.08, top=net, pin=net)
    mid = lo[1] + 2.54
    pg.wire(f'{upper}@{net}', (x, mid), f'{lower}@{net}')
    end = (x + run, mid) if d == 'R' else (x - run, mid)
    pg.wire((x, mid), end)
    pg.tag(*end, net, d)


def _rot_bottom(pg, ref, net):
    other = next(v for v in pg.b.parts[ref]['nets'].values() if v != net)
    return pg._orient(ref, other, None)


# ------------------------------------------------------------------ main board

def main_module(b):
    pg = b.page('01-module', '01 模组、供电循环与 UART', 'A4')
    pg.box(12, 12, 150, 198, '主控模组 SF32LB52-MOD-1-N16R8')
    pg.part('U1', 75, 102, 0)
    for n in ('VBAT', 'MOD_VSYS', 'VDD33_VOUT', 'GND'):
        pg.use(f'U1@{n}', 'global')
    pg.part('C41', 26, 66, top='VBAT')
    pg.use('C41@VBAT', 'global')
    pg.text(16, 43, 'C41 靠近模组 VBATS（61 脚）。', 1.4)
    pg.text(16, 20, '模组内含 Flash、PSRAM、两颗晶振和 PCB 天线；', 1.4)
    pg.text(16, 24.5, '天线外伸区禁铺铜。未用 GPIO 标 ×。', 1.4)

    pg.box(155, 12, 287, 85, '模组供电循环（镊子短接复位）')
    pg.part('U19', 218, 52, 0)
    en = at(pg, 'U19@RTS_N_OD')
    pg.part('R58', 200, en[1], top='VSYS', pin='RTS_N_OD')
    pg.wire('U19@RTS_N_OD', 'R58@RTS_N_OD', (185, en[1]))
    pg.tag(185, en[1], 'RTS_N_OD', 'L')
    out = at(pg, 'U19@MOD_SW_OUT')
    pg.part('R1', out[0] + 12.7, out[1], left='MOD_SW_OUT', pin='MOD_SW_OUT')
    pg.wire('U19@MOD_SW_OUT', 'R1@MOD_SW_OUT')
    m = at(pg, 'R1@MOD_VSYS')
    pg.part('C1', m[0] + 7.62, m[1], top='MOD_VSYS', pin='MOD_VSYS')
    pg.part('C38', m[0] + 17.78, m[1], top='MOD_VSYS', pin='MOD_VSYS')
    lead(pg, 'R1@MOD_VSYS', at(pg, 'C1@MOD_VSYS'), at(pg, 'C38@MOD_VSYS'), (m[0] + 25.4, m[1]), net='MOD_VSYS', d='U')
    pg.text(159, 20, '用镊子将 RTS_N_OD 短接至地约 0.5 秒，关断 U19 后再释放。', 1.4)
    pg.text(159, 24.5, '平时 R58 上拉保持导通。R1 为 0 Ω 默认通路。', 1.4)

    pg.box(155, 90, 287, 160, 'UART 串联电阻与夹具探针')
    for i, (r, tp) in enumerate((('R11', 'TP14'), ('R12', 'TP13'))):
        y = 112 + i * 14
        n = b.parts[r]['nets']['1']
        pg.part(r, 235, y, left=n)
        pg.part(tp, 210, y, 0, pin='1')
        pg.wire(f'{tp}.1', f'{r}@{n}')
    pg.part('TP11', 168, 112, 0)
    pg.part('TP12', 168, 130, 270)
    pg.part('TP16', 168, 145, 270)
    pg.part('TP17', 211, 145, 0)
    pg.use('TP12.1', 'global')
    pg.text(159, 98, 'TP11 GND · TP12 MOD_VSYS（仅测量）· TP13 MCU RX', 1.4)
    pg.text(159, 102.5, 'TP14 MCU TX · TP16/TP17 镊子复位对', 1.4)


def main_charger(b):
    pg = b.page('02-charger', '02 触点输入、充电与电芯', 'A4')
    pg.box(12, 12, 142, 108, '触点输入与过压保护')
    pg.box(147, 12, 287, 110, '充电与系统路径 AW32001（I2C0）')
    pg.box(12, 113, 172, 198, '电芯接口与测温（J1 / 线束）')
    y = 48
    pg.part('D19', 36, y, left='DOCK_5V_IN')
    xs = (50, 60, 70)
    for r, x in zip(('D17', 'C2', 'R2'), xs):
        pg.part(r, x, y, top='DOCK_PROTECTED', pin='DOCK_PROTECTED')
    pg.part('U17', 90, y, 0, pin='1')
    pg.wire('D19@DOCK_PROTECTED', *[(x, y) for x in xs], 'U17.1')
    g1, g2 = at(pg, 'U17.5'), at(pg, 'U17.6')
    lead(pg, 'U17.5', (g1[0] - 3.81, g1[1]), (g1[0] - 3.81, g2[1]), (g1[0] - 3.81, g2[1] + 2.54), net='GND', d='D')
    pg.wire('U17.6', (g1[0] - 3.81, g2[1]))
    iset = at(pg, 'U17@N_32N38543')
    pg.part('R4', iset[0] + 5.08, iset[1] + 17.78, top='N_32N38543', pin='N_32N38543')
    pg.wire('U17@N_32N38543', (iset[0] + 5.08, iset[1]), 'R4@N_32N38543')
    pg.tag(*at(pg, 'R4@N_32N38543'), 'N_32N38543', 'R')
    vo = at(pg, 'U17@VBUS_5V')
    pg.part('C42', vo[0] + 7.62, vo[1], top='VBUS_5V', pin='VBUS_5V')
    pg.part('C3', vo[0] + 17.78, vo[1], top='VBUS_5V', pin='VBUS_5V')
    lead(pg, 'U17@VBUS_5V', at(pg, 'C42@VBUS_5V'), at(pg, 'C3@VBUS_5V'),
         (vo[0] + 25.4, vo[1]), net='VBUS_5V', d='U')
    pg.text(16, 20, 'DOCK_5V_IN 来自后盖 10P 尾指：单路 5 V、单路回流，每触点 0.5 A。', 1.4)
    pg.text(16, 24.5, 'D19 防反接，D17 TVS 钳位，R2 泄放；U17 过压保护后输出 VBUS_5V。', 1.4)

    pg.part('U3', 228, 55, 0)
    intp = at(pg, 'U3@CHG_INT')
    pg.part('R5', 203, intp[1], top='CHG_VDD', pin='CHG_INT')
    pg.part('Q1', 188, intp[1], 0, pin='3')
    pg.wire('U3@CHG_INT', 'R5@CHG_INT', 'Q1.3')
    for n, d in (('VSYS_S', 2.54), ('VBAT', 10.16), ('CHG_VDD', 17.78)):
        pg.stub(f'U3@{n}', d)
    divider(pg, 'R14', 'R15', 160, 82, 'PA44_VBUS_DET', d='R', run=5.08)
    pg.part('R42', 205, 88, left='VCC_3V3')
    pg.part('R43', 205, 98, left='VCC_3V3')
    for i, c in enumerate(('C6', 'C8', 'C7')):
        pg.part(c, 250 + i * 12, 92, top=next(v for v in b.parts[c]['nets'].values() if v != 'GND'))
    pg.text(149, 20, 'Q1 在按键时拉低 CHG_INT，复现原 KEY1 唤醒路径。', 1.4)
    pg.text(149, 24.5, 'C6 = VSYS_S、C8 = VBAT、C7 = CHG_VDD 去耦，靠近 U3。', 1.4)

    pg.part('J1', 40, 160, 0)
    pg.part('D18', 62, 160, top='VBAT_S')
    divider(pg, 'R60', 'R64', 85, 158, 'CELL_TS', run=7.62)
    pg.box(110, 138, 168, 186, '电池线束（不上板）')
    pg.part('BT1', 125, 165, 0)
    pg.part('TH1', 150, 165, top='CELL_TS')
    pg.bend('TH1@CELL_TS', 'R')
    pg.text(16, 121, 'J1 = Molex 78171-0003：1 PACK-，2 CELL_TS，3 BAT+；', 1.4)
    pg.text(16, 125.5, '插接前核对通断与极性。R60/R64 为暂定 TS 分压，CHG_VDD 供电；', 1.4)
    pg.text(16, 130, '温度保护验证前保持充电关闭。', 1.4)
    pg.text(113, 145, '两线 503030（含 PCM）', 1.3)
    pg.text(113, 149, '外贴 103JT 至 PACK-；不进 BOM', 1.3)


def main_rails(b):
    pg = b.page('03-rails', '03 受控电源与测流桥', 'A4')
    pg.box(12, 12, 287, 105, '受控电源（PA38 / PA26 / PA30）')
    pg.box(12, 110, 287, 160, '测流桥与默认通路')
    pg.part('U2', 60, 60, 0)
    en = at(pg, 'U2@PA_38')
    pg.part('R3', 45, en[1], top='PA_38', pin='PA_38')
    lead(pg, 'U2@PA_38', at(pg, 'R3@PA_38'), (35, en[1]), net='PA_38', d='L')
    pg.part('C4', 40, 42, top='VSYS')
    o = at(pg, 'U2@VSYS_1')
    xs = [o[0] + 10.16 + i * 10.16 for i in range(3)]
    for c, x in zip(('C5', 'C9', 'C33'), xs):
        pg.part(c, x, o[1], top='VSYS_1', pin='VSYS_1')
    lead(pg, 'U2@VSYS_1', *[(x, o[1]) for x in xs], (xs[-1] + 10.16, o[1]), net='VSYS_1', d='U')
    pg.text(16, 20, 'PA38 使能 VSYS_1（U2）；PA26 控制显示 3V3（U4）；PA30 经 R54 控制 GS_3V3 与光学电源（U15）。', 1.4)
    pg.text(16, 24.5, '下拉 R3/R7/R41 保证 MCU 未驱动时各路关断。', 1.4)

    pg.part('U4', 190, 45, 0)
    en = at(pg, 'U4@PA_26')
    pg.part('R7', en[0] - 10.16, en[1], top='PA_26', pin='PA_26')
    lead(pg, 'U4@PA_26', at(pg, 'R7@PA_26'), (en[0] - 20.32, en[1]), net='PA_26', d='L')
    o = at(pg, 'U4@VCC_3V3_S')
    pg.part('C10', o[0] + 10.16, o[1], top='VCC_3V3_S', pin='VCC_3V3_S')
    lead(pg, 'U4@VCC_3V3_S', at(pg, 'C10@VCC_3V3_S'), (o[0] + 20.32, o[1]), net='VCC_3V3_S', d='U')

    pg.part('U15', 190, 82, 0)
    en = at(pg, 'U15@PA_30_1')
    pg.part('R41', en[0] - 7.62, en[1], top='PA_30_1', pin='PA_30_1')
    pg.part('R54', en[0] - 17.78, en[1], left='PA_30', pin='PA_30_1')
    pg.wire('U15@PA_30_1', 'R41@PA_30_1', 'R54@PA_30_1')
    o = at(pg, 'U15@N_33N9450')
    pg.part('C34', o[0] + 10.16, o[1], top='N_33N9450', pin='N_33N9450')
    pg.part('R52', o[0] + 25.4, o[1], left='N_33N9450', pin='N_33N9450')
    pg.wire('U15@N_33N9450', 'C34@N_33N9450', 'R52@N_33N9450')

    pg.text(16, 118, '三处可拆短接件 JP1/R49/R50 全部闭合才能运行；测流时拆下对应短接件串入电流表。', 1.4)
    pg.text(16, 122.5, 'R65/R66/R67 为默认 0 Ω 通路：VSYS_S→VSYS，VCC_3V3_S→VCC_3V3，GND→PPG_GND。', 1.4)
    first = {'JP1': 'VBAT_S', 'R49': 'VSYS_1', 'R50': 'VCC_3V3', 'R65': 'VSYS_S', 'R66': 'VCC_3V3_S', 'R67': 'GND'}
    for i, r in enumerate(first):
        pg.part(r, 33 + i * 44, 143, left=first[r])


def main_display(b):
    pg = b.page('04-display', '04 屏幕直连与触摸', 'A4')
    pg.box(12, 12, 200, 125, '原屏 CN1 直连（J201 = 原转接板 CN1）')
    pg.part('J201', 90, 72, 0)
    pg.text(16, 20, '24 路信号 + 外壳焊盘；QSPI 保留 22 Ω 串联端接；', 1.4)
    pg.text(16, 24.5, 'R206 选择 OLED_VBAT 供 VCI；不并联 3V3。', 1.4)
    g = [at(pg, f'J201.{n}') for n in ('18', '17', '16')]
    xb = g[0][0] - 3.81
    lead(pg, 'J201.18', (xb, g[0][1]), (xb, g[2][1]), (xb, g[2][1] + 3.81), net='GND', d='D')
    pg.wire('J201.17', (xb, g[1][1]))
    pg.wire('J201.16', (xb, g[2][1]))
    g2, g3 = at(pg, 'J201.2'), at(pg, 'J201.3')
    xr = g2[0] + 3.81
    pg.wire('J201.2', (xr, g2[1]), (xr, g3[1]), 'J201.3')
    pg.wire((xr, g2[1]), (xr + 5.08, g2[1]))
    pg.tag(xr + 5.08, g2[1], 'GND', 'R')
    p1 = at(pg, 'J201.1')
    pg.part('R202', p1[0] + 22.86, p1[1], left='OLED_PULL10K', pin='OLED_PULL10K')
    pg.wire('J201.1', 'R202@OLED_PULL10K')
    p4, p5 = at(pg, 'J201.4'), at(pg, 'J201.5')
    pg.part('R206', p4[0] + 22.86, p4[1], left='OLED_VCI', pin='OLED_VCI')
    pg.wire('J201.4', 'R206@OLED_VCI')
    pg.wire('J201.5', (p5[0] + 3.81, p5[1]), (p5[0] + 3.81, p4[1]))
    p6 = at(pg, 'J201.6')
    pg.part('R201', p6[0] + 22.86, p6[1], left='OLED_PULL1M', pin='OLED_PULL1M')
    pg.wire('J201.6', 'R201@OLED_PULL1M')
    y0 = at(pg, 'J201.8')[1]
    for k, pin in enumerate(['8', '9', '10', '11', '12']):
        p = at(pg, 'J201.' + pin)
        net = b.parts['J201']['nets'][pin]
        r = next(ref for ref in ('R16', 'R17', 'R18', 'R19', 'R20') if net in b.parts[ref]['nets'].values())
        yk = y0 + 5.08 * k
        xk = p[0] + 15.24 - 2.54 * k
        pg.part(r, p[0] + 27.94, yk, left=net, pin=net)
        pg.wire('J201.' + pin, (xk, p[1]), (xk, yk), f'{r}@{net}')

    pg.box(12, 130, 172, 198, '屏供电去耦')
    for i, c in enumerate(('C13', 'C201')):
        pg.part(c, 30 + i * 13, 168, top='OLED_VBAT')
    for i, c in enumerate(('C14', 'C15', 'C202', 'C203')):
        pg.part(c, 75 + i * 13, 168, top='OLED_3V3')
    pg.text(16, 138, 'C13/C201 靠近 OLED_VBAT 入口；', 1.4)
    pg.text(16, 142.5, 'C14/C15/C202/C203 靠近 J201 第 19 脚（OLED_3V3）。', 1.4)

    pg.box(205, 12, 287, 160, '触摸 ESD 与 I2C 上拉')
    for i, r in enumerate(('R46', 'R47', 'R48')):
        pg.part(r, 225, 40 + i * 12, left='VCC_3V3')
    for i, d in enumerate(('D5', 'D6', 'D7', 'D8')):
        pg.part(d, 220 + (i % 2) * 33, 95 + (i // 2) * 30, top=b.parts[d]['nets']['2'])
        pg.bend(f'{d}.2', 'R')
    pg.text(209, 20, '触摸 RSTN/INT/SCL/SDA 各一颗 ESD5451N；', 1.4)
    pg.text(209, 24.5, 'INT/SCL/SDA 由 R46–R48 上拉到 VCC_3V3。', 1.4)


def main_audio(b):
    pg = b.page('05-audio', '05 功放与麦克风', 'A4')
    pg.box(12, 12, 287, 120, '差分功放 NS4150B 与输出滤波')
    pg.box(12, 125, 172, 198, '上进音麦克风')
    pg.part('U6', 110, 68, 0)
    inp = at(pg, 'U6@N_31N9060')
    pg.part('R28', inp[0] - 12.7, inp[1], left='N_31N9147', pin='N_31N9060')
    pg.part('C21', at(pg, 'R28@N_31N9147')[0] - 12.7, inp[1], left='DAC1P', pin='N_31N9147')
    pg.wire('C21@N_31N9147', 'R28@N_31N9147')
    pg.wire('R28@N_31N9060', 'U6@N_31N9060')
    inn = at(pg, 'U6@N_31N9056')
    pg.part('R27', inn[0] - 12.7, inn[1], left='N_31N9143', pin='N_31N9056')
    pg.part('C20', at(pg, 'R27@N_31N9143')[0] - 12.7, inn[1], left='DAC1N', pin='N_31N9143')
    pg.wire('C20@N_31N9143', 'R27@N_31N9143')
    pg.wire('R27@N_31N9056', 'U6@N_31N9056')
    ctl = at(pg, 'U6@PA_42')
    pg.part('R29', ctl[0] - 25.4, ctl[1], top='PA_42', pin='PA_42')
    lead(pg, 'U6@PA_42', at(pg, 'R29@PA_42'), (ctl[0] - 35.56, ctl[1]), net='PA_42', d='L')
    byp = at(pg, 'U6@N_31N9186')
    pg.part('C24', byp[0] - 12.7, byp[1], top='N_31N9186', pin='N_31N9186')
    pg.wire('U6@N_31N9186', 'C24@N_31N9186')
    vcc = at(pg, 'U6@N_31N8386')
    ytop = vcc[1] - 10.16
    pg.part('R26', vcc[0] - 15.24, ytop, left='VSYS', pin='N_31N8386')
    pg.part('C18', vcc[0] + 12.7, ytop, top='N_31N8386', pin='N_31N8386')
    pg.part('C19', vcc[0] + 25.4, ytop, top='N_31N8386', pin='N_31N8386')
    pg.wire('R26@N_31N8386', (vcc[0], ytop), 'C18@N_31N8386', 'C19@N_31N8386')
    pg.wire('U6@N_31N8386', (vcc[0], ytop))
    vop, von = at(pg, 'U6@N_31N8717'), at(pg, 'U6@N_31N8678')
    pg.part('L2', vop[0] + 10.16, vop[1], left='N_31N8717', pin='N_31N8717')
    pg.part('L1', von[0] + 10.16, von[1], left='N_31N8678', pin='N_31N8678')
    pg.wire('U6@N_31N8717', 'L2@N_31N8717')
    pg.wire('U6@N_31N8678', 'L1@N_31N8678')
    xp = at(pg, 'L2@SPK_OUTP')[0]
    yn = von[1] + 17.78
    xs = [xp + 12.7, xp + 25.4, xp + 38.1]
    pg.part('D2', xs[0], vop[1], top='SPK_OUTP', pin='SPK_OUTP')
    pg.part('C25', xs[1], vop[1], top='SPK_OUTP', pin='SPK_OUTP')
    pg.part('C23', xs[2], vop[1], top='SPK_OUTP', pin='SPK_OUTP')
    pg.part('D1', xs[0], yn, top='SPK_OUTN', pin='SPK_OUTN')
    pg.part('C22', xs[1], yn, top='SPK_OUTN', pin='SPK_OUTN')
    pg.wire('L2@SPK_OUTP', *[(x, vop[1]) for x in xs], (xs[2] + 15.24, vop[1]))
    pg.wire('L1@SPK_OUTN', (xp + 5.08, von[1]), (xp + 5.08, yn), (xs[0], yn), (xs[1], yn), (xs[2], yn), (xs[2] + 15.24, yn))
    pg.wire('C23@SPK_OUTN', (xs[2], yn))
    pg.tag(xs[2] + 15.24, vop[1], 'SPK_OUTP', 'R')
    pg.tag(xs[2] + 15.24, yn, 'SPK_OUTN', 'R')
    pg.part('L3', 40, 105, left='GND')
    pg.text(16, 20, 'BTL 输出经后盖 10P 尾指到喇叭；SPK_OUTN 为驱动输出，不接地。', 1.4)
    pg.text(16, 24.5, 'C22/C23/C25 为可选滤波位（不贴）。SPK_GND 经磁珠 L3 单点接 GND。', 1.4)

    pg.part('U9', 95, 160, 0)
    out = at(pg, 'U9@ADCIN')
    pg.part('C27', out[0] - 10.16, out[1], top='ADCIN', pin='ADCIN')
    lead(pg, 'U9@ADCIN', at(pg, 'C27@ADCIN'), (out[0] - 22.86, out[1]), net='ADCIN', d='L')
    vdd = at(pg, 'U9@N_31N10127')
    pg.part('C26', vdd[0] + 10.16, vdd[1], top='N_31N10127', pin='N_31N10127')
    pg.part('R30', vdd[0] + 25.4, vdd[1], left='N_31N10127', pin='N_31N10127')
    pg.wire('U9@N_31N10127', 'C26@N_31N10127', 'R30@N_31N10127')
    pg.part('L4', 35, 185, left='GND')
    pg.text(16, 133, '顶部进音孔须正对独立声道。U9 输出接 ADCIN，MIC_BIAS 经 R30 供电。', 1.4)
    pg.text(16, 137.5, '启用音频前确认密封垫和外壳；MIC_GND 经磁珠 L4 接 GND。', 1.4)


def main_io(b):
    pg = b.page('06-io', '06 传感器、表冠、电机与后盖接口', 'A4')
    pg.box(12, 12, 148, 105, '姿态、地磁与前向环境光（I2C1）')
    pg.box(153, 12, 287, 105, '后盖接口与光学电源')
    pg.box(12, 110, 148, 198, '表冠（编码器 + 按键）')
    pg.box(153, 110, 287, 162, '振动电机')

    pg.part('U13', 50, 50, 0)
    v = [at(pg, f'U13.{n}') for n in ('8', '5', '12')]
    xv = v[0][0] + 3.81
    pg.wire('U13.8', (xv, v[0][1]), (xv, v[2][1]), 'U13.12')
    pg.wire('U13.5', (xv, v[1][1]))
    pg.wire((xv, v[0][1]), (xv, v[0][1] - 3.81))
    pg.tag(xv, v[0][1] - 3.81, 'GS_3V3', 'U')
    g = [at(pg, f'U13.{n}') for n in ('1', '2', '3')]
    xg = g[0][0] - 3.81
    pg.wire('U13.1', (xg, g[0][1]), (xg, g[2][1]), 'U13.3')
    pg.wire('U13.2', (xg, g[1][1]))
    pg.wire((xg, g[2][1]), (xg, g[2][1] + 3.81))
    pg.tag(xg, g[2][1] + 3.81, 'GND', 'D')
    pg.part('U14', 45, 90, 0)
    pg.part('U16', 115, 90, 0)
    for i, r in enumerate(('R44', 'R45')):
        pg.part(r, 108, 33 + i * 10, left='GS_3V3')
    for i, c in enumerate(('C29', 'C30', 'C31', 'C35')):
        pg.part(c, 90 + i * 12, 60, top='GS_3V3')
    pg.text(16, 20, 'U16 朝前光导；U14 的位置与航向须结合电机、', 1.4)
    pg.text(16, 24.5, '喇叭和充电夹具实测标定。', 1.4)

    pg.part('J2', 268, 58, 0)
    for n in ('PPG_GND', 'GS_3V3', 'PPG_1V8', 'GND', 'DOCK_5V_IN'):
        pg.use(f'J2@{n}', 'global')
    pg.part('U21', 190, 48, 0)
    a, c = at(pg, 'U21.1'), at(pg, 'U21.3')
    xa = a[0] - 3.81
    pg.wire('U21.1', (xa, a[1]), (xa, c[1]), 'U21.3')
    pg.part('C39', xa - 7.62, a[1], top='GS_3V3', pin='GS_3V3')
    pg.wire((xa, a[1]), at(pg, 'C39@GS_3V3'), (xa - 15.24, a[1]))
    pg.tag(xa - 15.24, a[1], 'GS_3V3', 'U')
    o = at(pg, 'U21@PPG_1V8')
    pg.part('C40', o[0] + 10.16, o[1], top='PPG_1V8', pin='PPG_1V8')
    lead(pg, 'U21@PPG_1V8', at(pg, 'C40@PPG_1V8'), (o[0] + 20.32, o[1]), net='PPG_1V8', d='U')
    pg.part('R68', 190, 85, left='GS_3V3')
    pg.text(157, 20, '每触点 0.5 A；单路 5 V、单路回流。核对负载加充电电流、', 1.4)
    pg.text(157, 24.5, '堵转/反接、插入方向与柔板弯折。U21 提供 PPG_1V8。', 1.4)

    for i, k in enumerate(('K1', 'K3', 'K4', 'K2')):
        y = 134 + i * 13
        pg.part(k, 50, y, 0)
        p1, p2 = at(pg, f'{k}.1'), at(pg, f'{k}.2')
        x = p1[0] - 3.81
        net = b.parts[k]['nets']['1']
        pg.wire(f'{k}.1', (x, p1[1]), (x, p2[1]), f'{k}.2')
        pg.wire((x, p1[1]), (x - 5.08, p1[1]))
        pg.tag(x - 5.08, p1[1], net, 'L')
    pg.part('R62', 92, 134, left='GS_3V3')
    pg.part('R63', 92, 146, left='GS_3V3')
    pg.part('Q3', 88, 186, 0)
    g = at(pg, 'Q3@KEY_N')
    pg.part('R61', g[0] - 10.16, g[1], top='VBAT', pin='KEY_N')
    lead(pg, 'Q3@KEY_N', at(pg, 'R61@KEY_N'), (g[0] - 20.32, g[1]), net='KEY_N', d='L')
    pg.bend('Q3@VBAT', 'R')
    d = at(pg, 'Q3@PA_34_KEY1')
    yk = d[1] - 5.08
    pg.part('D15', d[0] + 10.16, yk, top='PA_34_KEY1', pin='PA_34_KEY1')
    pg.part('R34', d[0] + 17.78, yk, left='PA_34_KEY1', pin='PA_34_KEY1')
    pg.wire('Q3@PA_34_KEY1', (d[0], yk), 'D15@PA_34_KEY1', 'R34@PA_34_KEY1')
    k2 = at(pg, 'R34@PA_34_PWRKEY')
    pg.part('R35', k2[0] + 5.08, yk, top='PA_34_PWRKEY', pin='PA_34_PWRKEY')
    lead(pg, 'R34@PA_34_PWRKEY', at(pg, 'R35@PA_34_PWRKEY'), (k2[0] + 10.16, yk), net='PA_34_PWRKEY', d='R')
    pg.text(16, 117, 'K1–K4 为表冠弹片：A/B 接 PA24/PA25，上拉取自 GS_3V3。', 1.4)
    pg.text(16, 121.5, 'KEY_N 拉低 Q3 栅极，输出 KEY1 正脉冲，经 R34/R35 送 PA34。', 1.4)

    y = 125
    pg.part('R36', 172, y, left='VDD33_VOUT')
    mp = at(pg, 'R36@MOTOR_P')
    pg.part('C28', mp[0] + 10.16, y, top='MOTOR_P', pin='MOTOR_P')
    pg.part('D16', mp[0] + 25.4, y, top='MOTOR_P', pin='MOTOR_P')
    pg.part('MT1', mp[0] + 45.72, y, 0, pin='1')
    pg.wire('R36@MOTOR_P', 'C28@MOTOR_P', 'D16@MOTOR_P', 'MT1.1')
    a = at(pg, 'D16@MOTOR_N')
    yn = a[1] + 7.62
    pg.part('Q2', a[0], yn + 7.62, 0, pin='3')
    pg.part('MT2', mp[0] + 45.72, yn, 0, pin='1')
    pg.wire('D16@MOTOR_N', (a[0], yn), 'Q2.3')
    pg.wire((a[0], yn), 'MT2.1')
    gt = at(pg, 'Q2@MOTOR_GATE')
    pg.part('R38', gt[0] - 7.62, gt[1], top='MOTOR_GATE', pin='MOTOR_GATE')
    pg.part('R37', gt[0] - 15.24, gt[1], left='PA_20_VIB_PWM', pin='MOTOR_GATE')
    pg.wire('R37@MOTOR_GATE', 'R38@MOTOR_GATE', 'Q2@MOTOR_GATE')
    pg.text(157, 117, 'VDD33_VOUT 经 R36 供电，Q2 低边 PWM，D16 续流；MT1/MT2 在后盖侧。', 1.3)


def main(b):
    main_module(b)
    main_charger(b)
    main_rails(b)
    main_display(b)
    main_audio(b)
    main_io(b)


# ------------------------------------------------------------------ small boards

def rear(b):
    pg = b.page('rear', '后盖柔板：尾指、光学岛与端子', 'A4')
    pg.box(12, 12, 100, 160, '10P 尾指 JF1')
    pg.part('JF1', 35, 85, 0)
    for n in ('PPG_GND', 'GS_3V3', 'PPG_1V8', 'DOCK_5V_IN'):
        pg.use(f'JF1@{n}', 'global')
    pg.text(16, 20, '尾指 1–10 与主板 J2 同序；', 1.4)
    pg.text(16, 24.5, '9 脚为充电回流 DOCK_RTN。', 1.4)
    pg.box(105, 12, 287, 110, 'MAX30102 光学岛')
    pg.part('U301', 160, 58, 0)
    pg.stub('U301.9', 5.08)
    pg.stub('U301.11', 15.24)
    for i, c in enumerate(('C301', 'C302', 'C303', 'C304')):
        pg.part(c, 215 + i * 15, 80, top=b.parts[c]['nets']['1'])
    pg.text(109, 20, '光学岛与端子位置仅为研究；后盖轮廓与尾指折弯须按弯曲中性层展开。', 1.4)
    pg.text(109, 24.5, 'C301/C302 靠近 VLED+，C303/C304 靠近 VDD。', 1.4)
    pg.box(105, 115, 287, 160, '喇叭与充电端子')
    for i, r in enumerate(('SP1', 'SP2', 'CH1', 'CH2')):
        pg.part(r, 140 + i * 38, 140, 270)


def crown(b):
    pg = b.page('crown', '表冠立板：编码器与直角焊端', 'A4')
    pg.box(15, 15, 282, 150, '表冠立板（局部 Y/Z 立放）')
    pg.part('ENC1', 90, 85, 0)
    for j, n in (('JC1', 'CROWN_A'), ('JC3', 'CROWN_B'), ('JC4', 'KEY_N')):
        p = at(pg, f'ENC1@{n}')
        pg.part(j, p[0] + 40.64, p[1], 270, pin='1')
        pg.wire(f'ENC1@{n}', f'{j}.1')
    pg.part('JC2', 190, 120, 0)
    pg.text(22, 26, 'BMQ22 管脚和定位孔须按 A 后缀承认图核对；四个直角焊端连到水平触点板。', 1.5)


def bridge(b):
    pg = b.page('bridge', '表冠水平触点板', 'A4')
    pg.box(15, 15, 282, 150, '水平四触点（底面金面 + 四个侧边焊端）')
    for i, (j, p) in enumerate((('JC11', 'P1'), ('JC12', 'P2'), ('JC13', 'P3'), ('JC14', 'P4'))):
        x = 50 + i * 60
        net = b.parts[j]['nets']['1']
        if net == 'GND':
            pg.part(j, x, 85, 0)
            pg.part(p, x + 25.4, 85, 0)
            continue
        pg.part(j, x, 85, 0)
        pg.part(p, x + 25.4, 85, 0)
        pg.wire(f'{j}.1', (x, 95), (x + 25.4, 95), f'{p}.1')
    pg.text(22, 26, '底面四个镀金面对接弹片；侧边四个焊端连到立式编码器板。', 1.5)


LAYOUTS = {'main': main, 'rear': rear, 'crown': crown, 'bridge': bridge}
