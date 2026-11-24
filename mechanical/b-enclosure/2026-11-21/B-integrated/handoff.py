"""Render the engineer handoff from the same results used for the two native files."""
import json,sys
from pathlib import Path
p=Path(sys.argv[1]);r=json.loads((p/'geometry.json').read_text(encoding='utf8'))
def num(a):return f'{a:g}'
rows=[]
for name,side in [('屏幕侧','Front'),('电池侧','Rear')]:
    for v in r['heights'][side]:
        q=v['largest_xywh']
        loc=f'({num(q[0])}, {num(q[1])})' if q else '—'
        size=f'{num(q[2])} × {num(q[3])}' if q else '—'
        rows.append(f"<tr><td>{name}</td><td>{num(v['height_mm'])}</td><td>{v['area_mm2']:.1f}</td><td>{size}</td><td>{loc}</td></tr>")
x,y,w,l,h=r['box_fit']['8x6x2']
audit=r['space_audit']
front_detail=''.join(f"<tr><td>{v['height_mm']:g}</td><td>{v['previous_area_mm2']:.2f}</td><td>{v['same_other_constraints_mm2']:.2f}</td><td>{v['photo_estimated_pending_mm2']:.2f}</td><td>{v['with_patch_held_mm2']:.2f}</td><td>{v['with_patch_clear_mm2']:.2f}</td><td>{v['extra_stack_limit_mm']:.2f}</td></tr>" for v in audit['front'])
rear3=next(v for v in audit['rear'] if v['height_mm']==3)
html=f'''<!doctype html><html lang="zh-CN"><meta charset="UTF-8"><meta name="viewport" content="width=device-width,initial-scale=1"><title>WristFlow B 整机与 PCB 结构交接</title>
<style>body{{max-width:1010px;margin:32px auto 60px;padding:0 20px;font:16px/1.65 'Microsoft YaHei',sans-serif;color:#213840}}h1{{font-size:34px}}h2{{font-size:24px;margin-top:32px}}a{{color:#12656f}}table{{border-collapse:collapse;width:100%}}td,th{{border:1px solid #c8d6d8;padding:6px 10px;text-align:left}}th{{background:#edf3f3}}img{{width:100%;height:auto}}.rf{{border-left:4px solid #b34a3d;padding:3px 12px}}li{{margin:7px 0}}</style>
<main><h1>WristFlow B 整机与主板空间</h1>
<p>本版采用 503035T 薄电池方案，成品空间暂按 35 × 30 × 5 mm，沿用既定 B 外廓、22 mm 表带与电池位置。表体 41.1 × 47.5 × 16.0 mm，含光学凸台 17.5 mm。模型按现有原屏尾的自然折返方向注册窗口和扣座。</p>
<p><a href="WristFlow-B-Integrated.FCStd">完整 FreeCAD 模型</a>　<a href="WristFlow-B-Placement.FCStd">主板双面器件包络</a>　<a href="B-board-outline.step">本版板框 STEP</a>　<a href="geometry.json">坐标与检查记录</a></p>
<img src="B-integrated-review.png" alt="完整表体与拆盖后的模块、屏尾、十触点位置">
<p>FreeCAD 整机文件默认显示合盖和两段表带。在树中选中「后盖、PPG、喇叭和十触点」「电池、可拆托板」「三线电池插头和线束」，按空格隐藏，即可看见折返段及思澈模组。图中红色为板载天线。可切换「穿屏尾纵剖面 X = −4 mm」单独看折返。<a href="section.png">纵剖面</a> · <a href="uart.png">开盖串口夹具</a></p>
<h2>屏与主板如何固定</h2>
<img src="B-mounting-stack.png" alt="屏幕周边胶与三点主板支承，电芯上下两面和绝缘试算">
<p>实物原屏与原转接板可贴合，也可沿厚度方向分开约 15 mm，排线仍有余力。穿过板窗折回 CN1 后，尾端可调整的距离较小。本版用中框台阶和 0.15 mm 名义周边模切胶承托屏，保留屏背宽片原有粘接。主板从后盖侧落在三个凸肩上，用三颗螺钉压住。锁紧高度由凸肩确定。装配时先送过带补强的头部，落板时让窄尾自然收回，再扣 CN1。窗前活动段留空，两端的承力分别落在屏框和主板上。原板的开合范围已由用户试装确认，新壳内收余线、孔边摩擦以及锁紧后是否局部顶屏仍待实装。</p>
<h2>交给硬件的基准</h2>
<p>单位 mm，以表体 XY 中心为原点，+X 朝表冠，+Y 朝串口探针端，平背外面 Z=0。PCB 面向屏幕的一面 Z=12.10，面向电池的一面 Z=11.30。裸板外接尺寸 36.8 × 43.2 × 0.80，去掉孔和边缘缺口后单面 {r['board_mm2']:.2f} mm²。</p>
<ul><li><b>屏尾。</b>长窗中心 (−0.455, 9.990)，14.0 × 3.0、R0.50，长边沿 X。CN1 中心 (10.545, 5.135)，长边沿 Y，焊在电池侧，型号 OK-118-RF024/2-35，24 针、0.35 间距、0.70 名义扣合高。原 V1.1 屏板的窗到座矢量为 (+11.000, −4.855)。本版固定这组相对位置；原板居中映射到 B 机身，绝对坐标须用实物原屏与板的装配基准复核。原厂工程中展料轮廓、照片及用户拆机确定换面路线；Z 形状、补强厚度和实际弯曲半径仍是模型估值。座的 Pin 1 和焊盘应从原 PCB 层及底面镜像规则核对。</li>
<li><b>思澈模组。</b>中心 (−3.30, −8.30)，相对上次布局转 +90°，天线朝 −X；外廓 X=−17.25…10.65、Y=−17.30…0.70。最靠近折返屏尾的位置有 {r['module_to_folded_tail_mm']:.3f} mm 名义间隙，电芯至模组电子区 {r['module_to_cell_mm']:.2f} mm。</li>
<li><b>安装与退板。</b>电机中心 (−13, 15.5) 靠左侧中框，三处主板 Ø2.48 孔中心 (9, −19.25)、(−13.6, 4.0)、(15.5, 8.4)。支承臂在屏侧沿 FPC 外形局部减薄，保留螺钉和屏尾净空。电机侧缺口开到板角，消除狭长悬边。电机压紧、螺纹强度及引线连接仍需样件核验。</li>
<li><b>后盖。</b>十个弹触分两行，X=−6.35、−4.75、−3.15、−1.55、0.05，Y=13.60 或 14.76。信号顺序为传感器供电、LED 供电、信号地、SCL、SDA，第二行 INT、SPK+、SPK−、充电输入、充电回流。承力塔支承在后盖，两排焊盘占位全部落在 PCB 上；PPG 软板上翻并分配到弹触。喇叭导音侧口和光窗沿用 B。红米两针充电触点修订为约 3.5 mm 中心距，极性、针行程、磁件方向和弹触料号待核。</li>
<li><b>维修。</b>保持屏幕与 PCB 有间距时先送补强头穿窗，再让 PCB 坐实、扣上 CN1。拆时先断电移开托板，解扣 CN1，托住屏尾并与主板退板动作协调。模型额外检查了一个 7.8 × 3.65 × 0.75 的硬头，在主板离开终位 5 mm、长轴顺窗、倾斜 65° 时通过孔口的代表姿态。尚需验证实际屏尾松量、连续翻转轨迹和装拆手感。串口、三路测流和三芯电池插拔沿用可从后盖进入的夹具位置。</li></ul>
<h2>器件高度与可用面积</h2><a href="B-placement-review.svg"><img src="B-placement-review.png" alt="同一坐标下的屏幕侧与电池侧，各板区按最高核算焊后高度分色"></a>
<p>点击图可放大矢量原图。两面都从屏侧投影，穿线窗、螺孔和侧缺口在两图上对齐。每处色块标出它通过的最高试算档，例如 0.60 mm 的器件可使用 0.60 与 0.70 mm 两种区域。屏侧高度按名义屏背叠层计算。灰色底及纹理标明尚未分配的实板和不同预留，孔窗内可见浅色底。下表按器件高度累计面积。</p>
<table><thead><tr><th>安装面</th><th>焊后高度 mm</th><th>可用面积 mm²</th><th>当档面积最大的轴对齐矩形 mm</th><th>矩形左下 XY</th></tr></thead><tbody>{''.join(rows)}</tbody></table>
<p>以 0.25 mm 整格统计，只有整个格子满足所选焊后高度才计入。各档面积彼此包含。板边留 0.30，障碍 XY 留 0.20 加 0.02 投影误差预算，Z 留 0.20，电芯另加 0.30 厚度预算。电池侧已扣除回程、补强头入口、CN1 工具区、弹触、测流和退板路径。实际封装应连同焊盘庭院一起放入相应高度区域。</p>
<h2>贴屏宽片的可用高度</h2>
<p>上一版逐高度整块扣去屏侧 FPC。本版取红叉中可识别的平贴区，只对这一部分按高度计入。胶带、屏背器件、下缘补强及能活动的窄尾继续预留。边界按照片和玻璃外廓估计。宽片按原 CAD 的 0.12 mm 名义层厚贴到 Z=13.03 的屏背，朝板表面 Z=12.91，主板屏侧 Z=12.10。扣除 0.20 mm 高度余量，局部焊后高度上限为 0.61 mm。实际凸出 p 应从相同的屏背基准量到朝板最高点。若实测基准已含宽片，p 只计额外凸出。下表的面积单位为 mm²，最右列单位为 mm。</p>
<table><thead><tr><th>器件高 mm</th><th>上版</th><th>只改平贴宽片</th><th>其中照片位置待核</th><th>暂留照片区及平贴区</th><th>暂留照片区，平贴区满足限高</th><th>允许的最大 p</th></tr></thead><tbody>{front_detail}</tbody></table>
<p>补计面积随 p 跨越限值而改变。0.60 mm 档在名义叠层下仅多 0.01 mm。新增局部保留可能使部分原先裸露的整格退出。在真实屏总成和三点锁紧的主板上复测局部净距后，可用同一张表选档。</p>
<h2>电池投影与射频预留</h2><p>主板在模组天线投影下方已开缺口。缺口两端仍有板材的射频预留以横纹表示，共 {audit['rf_host_material_mm2']:.2f} mm²，已计入单面板材面积。3 mm 档若仅解除射频预留，按原整格口径最多多出 {rear3['excluded_by_rf_only_mm2']:.2f} mm²。深灰色为模组和板上连接件的占位。电芯外沿见虚线，3 mm 档有 {rear3['usable_fully_under_cell_mm2']:.2f} mm² 整格完全位于电芯投影内。</p>
<h2>电池和主板之间的隔离</h2><p>现有可拆托板厚 0.40 mm，在电芯朝后盖的一侧。主板侧目前靠空气间隙，成品电池自带外包层的材料和厚度尚未确认。电芯到裸 PCB 为 3.60 mm，到思澈模组电子区仅 0.50 mm。整机文件可单独显示「可选试算 · 电芯朝主板侧的绝缘片」。试算取电芯大面轮廓，含胶总厚 0.10 mm，在 5.00 mm 电芯包络之外叠加。模组处名义间隙变为 0.40 mm，而本版给电芯厚度变化预留 0.30 mm、结构间隙预留 0.20 mm，两项合计后短缺 0.10 mm。表中电池侧面积仍以原 5.00 mm 包络计算。</p>
<p>按单侧托板固定电芯的方案试装，电芯在托板上防滑和防抬起的能力待实物确认。主板侧的薄片仅用于电气绝缘。定板前应核带外包、保护板和胶后最厚处，区分已含与额外增加的绝缘层，再核该电池规格允许的厚度增长和装配公差。材料及包边尚未定型。<a href="https://www.jauch.com/downloadfile/5c8fa8421814625c73ecb3a6beacac920/white_paper_guide_to_the_design_of_lipo_batteries_02-2019_en.pdf">Jauch 电池仓设计指南，第 III 节</a></p>
<p>独立 8 × 6 × 2 mm 升降压候选包络按板上 X × Y = {w:g} × {l:g} 放置，左下角 ({x:g}, {y:g})，已对比实际模型实体。器件空间文件中的「可选 · 8 × 6 × 2 mm 样块」可显示。最大矩形按单块面积排序，另一个长宽比的封装可以放在同档其他区域。</p>
<h2>保留原屏尾长度的代价</h2>
<p>长窗自身占去约 41.8 mm² 板材，空间预算还要扣除弯折、补强和可解扣的位置。固定屏尾使扣座与窗作为一组定位，后盖接口及模组需配合布置。首版直接扣主板可省一组连接和订制柔板。若之后更看重布板自由，可将 CN1 固定在原落点的小转接板上，另用定长柔板接主板，主板端即可改位置；还需留两端受力面、额外连接或焊接点、柔板和装配空间，复核触摸及 QSPI 高速连接。额外转接件、组装费和打样价格尚无报价。</p>
<p class="rf"><b>射频风险。</b>板已从左边缘挖空到模组天线末端，天线投影下方无主 PCB。电池与天线投影重合 {r['rf']['battery_projected_overlap_mm2']:.1f} / {r['rf']['radiator_area_mm2']:.1f} mm²，垂直名义距离 {r['rf']['radiator_to_cell_mm']:.2f} mm。屏背 FPC 与天线投影也有 {r['rf']['front_flex_projected_overlap_mm2']:.2f} mm² 重合。按已选取舍保留电池和机身；BLE 距离、重连和 OTA 可用性要在相同手机/方向/距离下，分别比较裸板、装电池、合盖及佩戴状态。<a href="https://downloads.sifli.com/user%20manual/DS5203-SF32LB52-MOD-1%E6%8A%80%E6%9C%AF%E8%A7%84%E6%A0%BC%E4%B9%A6%20V0p2.pdf">思澈 DS5203 §8.4 板边和天线净空建议</a></p>
<p>FreeCAD {'.'.join(r['freecad'])} 已重新打开两份模型并逐项核对实体体积，STEP 板框也已重新导入。{len(r['checks'])} 组受检几何组合未检出超过 0.0005 mm³ 的非预期穿透。需要样件验收的项目包括锁紧与板翘、屏尾穿窗与解扣、用真实电池维持工作时的测流接入和射频。<a href="geometry.json">检查范围与数值</a></p>
</main></html>'''
(p/'B-硬件交接.html').write_text(html,encoding='utf8')
