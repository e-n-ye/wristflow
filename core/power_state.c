#include "power_state.h"

bool wristflow_power_equal(const wristflow_power_snapshot_t *a, const wristflow_power_snapshot_t *b)
{
    return a->input_valid == b->input_valid && a->input_good == b->input_good &&
        a->voltage_valid == b->voltage_valid && a->millivolts == b->millivolts &&
        a->charge == b->charge && a->charge_reason == b->charge_reason &&
        a->charge_cycle == b->charge_cycle && a->estimate_valid == b->estimate_valid &&
        a->estimated_percent == b->estimated_percent;
}

const char *wristflow_power_charge_text(const wristflow_power_snapshot_t *power)
{
    switch (power->charge) {
    case WF_POWER_CHARGE_IDLE: return "未充电";
    case WF_POWER_CHARGE_ACTIVE: return "充电中";
    case WF_POWER_CHARGE_FULL: return "已充满";
    case WF_POWER_CHARGE_STOPPED: return "充电已停止";
    case WF_POWER_CHARGE_FAULT: return "充电异常";
    default: return "状态未知";
    }
}

const char *wristflow_power_charge_reason_text(const wristflow_power_snapshot_t *power)
{
    switch (power->charge_reason) {
    case WF_POWER_CHARGE_REASON_MANUAL: return "手动停止";
    case WF_POWER_CHARGE_REASON_TIMEOUT: return "充电超时";
    case WF_POWER_CHARGE_REASON_IO: return "通信失败";
    case WF_POWER_CHARGE_REASON_PROFILE: return "参数异常";
    case WF_POWER_CHARGE_REASON_INPUT: return "输入变化";
    case WF_POWER_CHARGE_REASON_FAULT: return "芯片故障";
    case WF_POWER_CHARGE_REASON_VOLTAGE: return "电压保护";
    case WF_POWER_CHARGE_REASON_REGULATION: return "芯片调节";
    default: return "";
    }
}

const char *wristflow_power_input_text(const wristflow_power_snapshot_t *power)
{
    return !power->input_valid ? "USB ?" : power->input_good ? "USB OK" : "USB --";
}
