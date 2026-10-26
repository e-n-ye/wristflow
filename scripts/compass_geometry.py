"""Offline compass geometry references; never imported by Product firmware.

Inputs share right/forward/up axes. At rest, acceleration points toward up;
the magnetic input must already be calibrated. The result is the clockwise
azimuth of the watch's forward (+Y) horizontal projection from magnetic north.
This module neither calibrates sensors nor validates physical sample quality.
"""
from __future__ import annotations

import math


# Relative numerical conditioning only, not a device acceptance threshold.
MIN_HORIZONTAL_FRACTION = 1e-6


def _dot(a, b):
    return sum(x * y for x, y in zip(a, b))


def _cross(a, b):
    return (a[1]*b[2]-a[2]*b[1], a[2]*b[0]-a[0]*b[2], a[0]*b[1]-a[1]*b[0])


def _unit(values, name):
    if len(values) != 3 or not all(math.isfinite(value) for value in values):
        raise ValueError("invalid_" + name)
    norm = math.hypot(*values)
    if not math.isfinite(norm) or norm == 0:
        raise ValueError("zero_or_unrepresentable_" + name)
    return tuple(value / norm for value in values)


def _north(up, magnetic):
    field = _unit(magnetic, "magnetic")
    vertical = _dot(field, up)
    horizontal = tuple(field[i] - vertical*up[i] for i in range(3))
    if math.hypot(*horizontal) <= MIN_HORIZONTAL_FRACTION:
        raise ValueError("magnetic_horizontal_degenerate")
    return _unit(horizontal, "horizontal_magnetic")


def _degrees(y, x):
    degrees = math.degrees(math.atan2(y, x)) % 360
    # Modulo can round a very small negative angle up to exactly 360.0.
    return 0.0 if degrees == 360 else degrees


def watch_forward_heading(acceleration_g, calibrated_field_gauss):
    """Return degrees in [0, 360), or raise ValueError for degenerate geometry.

    No timestamp, saturation, motion, acceleration-magnitude or calibration
    quality check is possible here. Those are separate future device gates.
    """
    up = _unit(acceleration_g, "acceleration")
    forward_horizontal = tuple((1.0 if i == 1 else 0.0) - up[1]*up[i] for i in range(3))
    if math.hypot(*forward_horizontal) <= MIN_HORIZONTAL_FRACTION:
        raise ValueError("forward_horizontal_degenerate")
    north = _north(up, calibrated_field_gauss)
    east = _cross(north, up)
    return _degrees(east[1], north[1])


def shortest_arc_heading(acceleration_g, calibrated_field_gauss):
    """Diagnostic alternative: level up with the shortest rotation, then use +Y.

    This virtual forward axis differs from the watch-forward projection under
    combined tilts. It is not a claim about the vendor's internal algorithm.
    """
    up = _unit(acceleration_g, "acceleration")
    field = _unit(calibrated_field_gauss, "magnetic")
    _north(up, field)
    if 1 + up[2] <= MIN_HORIZONTAL_FRACTION:
        raise ValueError("shortest_arc_antiparallel_up")
    axis = _cross(up, (0.0, 0.0, 1.0))
    first = _cross(axis, field)
    second = _cross(axis, first)
    leveled = tuple(field[i] + first[i] + second[i]/(1 + up[2]) for i in range(3))
    return _degrees(-leveled[0], leveled[1])
