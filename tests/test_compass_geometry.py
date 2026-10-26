"""Independent pose oracles and invalid geometry for the offline reference."""
import math
from pathlib import Path
import random
import sys
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "scripts"))
from compass_geometry import shortest_arc_heading, watch_forward_heading


def angle_error(actual, expected):
    return abs((actual - expected + 180) % 360 - 180)


def quaternion_matrix(q):
    # Body -> world from an independently generated unit quaternion.
    w, x, y, z = q
    return [[1-2*(y*y+z*z), 2*(x*y-z*w), 2*(x*z+y*w)],
            [2*(x*y+z*w), 1-2*(x*x+z*z), 2*(y*z-x*w)],
            [2*(x*z-y*w), 2*(y*z+x*w), 1-2*(x*x+y*y)]]


def body_vector(matrix, world):
    return [sum(matrix[i][j]*world[i] for i in range(3)) for j in range(3)]


class CompassGeometryTests(unittest.TestCase):
    def test_cardinals_and_wrap_boundaries(self):
        for heading in (0, .001, 45, 89.999, 90, 90.001, 180, 270, 359.999):
            yaw = math.radians(heading)
            magnetic = [-math.sin(yaw), math.cos(yaw), -.7]
            self.assertLess(angle_error(watch_forward_heading([0, 0, 1], magnetic), heading), 1e-10)

    def test_near_zero_rounding_stays_below_360(self):
        for heading in (watch_forward_heading, shortest_arc_heading):
            self.assertEqual(heading([0, 0, 1], [1e-20, 1, 0]), 0.0)

    def test_random_quaternion_oracle_and_inclination(self):
        rng = random.Random(20261026)
        for _ in range(200):
            q = [rng.gauss(0, 1) for _ in range(4)]
            norm = math.hypot(*q)
            matrix = quaternion_matrix([v/norm for v in q])
            acceleration = body_vector(matrix, [0, 0, 1])
            expected = math.degrees(math.atan2(matrix[0][1], matrix[1][1])) % 360
            for dip in (-.8, 0, .8):
                magnetic = body_vector(matrix, [0, .35, dip])
                actual = watch_forward_heading(acceleration, magnetic)
                self.assertLess(angle_error(actual, expected), 1e-9)

    def test_positive_scale_invariance(self):
        a, m = [.2, -.4, .8], [-.3, .2, -.5]
        expected = watch_forward_heading(a, m)
        for a_scale, m_scale in ((1e-100, 1e100), (1e100, 1e-100), (9.81, 100)):
            self.assertLess(angle_error(watch_forward_heading([v*a_scale for v in a], [v*m_scale for v in m]), expected), 1e-10)

    def test_inverted_watch_has_defined_forward(self):
        self.assertAlmostEqual(watch_forward_heading([0, 0, -1], [0, .35, .3]), 0)
        self.assertAlmostEqual(watch_forward_heading([0, 0, -1], [-.35, 0, .3]), 270)
        with self.assertRaisesRegex(ValueError, "antiparallel"):
            shortest_arc_heading([0, 0, -1], [0, .35, .3])

    def test_reject_nonfinite_at_every_position_and_wrong_length(self):
        for name in ("acceleration", "magnetic"):
            for index in range(3):
                for invalid in (math.nan, math.inf, -math.inf):
                    a, m = [0, 0, 1], [0, .35, -.3]
                    (a if name == "acceleration" else m)[index] = invalid
                    with self.assertRaisesRegex(ValueError, "invalid_" + name):
                        watch_forward_heading(a, m)
        for a, m in (([0, 1], [0, 1, 0]), ([0, 0, 1], [0, 1, 0, 0])):
            with self.assertRaises(ValueError):
                watch_forward_heading(a, m)

    def test_reject_zero_and_parallel_field(self):
        for a, m in (([0, 0, 0], [0, 1, 0]), ([0, 0, 1], [0, 0, 0]),
                     ([0, 0, 1], [0, 0, 1]), ([0, 0, 1], [0, 0, -1])):
            with self.assertRaises(ValueError):
                watch_forward_heading(a, m)

    def test_forward_vertical_and_numerical_conditioning(self):
        for a in ([0, 1, 0], [0, -1, 0], [0, 1, 1e-7]):
            with self.assertRaisesRegex(ValueError, "forward_horizontal"):
                watch_forward_heading(a, [1, 0, 0])
        self.assertTrue(math.isfinite(watch_forward_heading([0, 1, 1e-4], [1, 0, 0])))
        with self.assertRaisesRegex(ValueError, "magnetic_horizontal"):
            watch_forward_heading([0, 0, 1], [0, 1e-7, 1])
        self.assertAlmostEqual(watch_forward_heading([0, 0, 1], [0, 1e-4, 1]), 0)

    def test_compound_tilt_reference_difference(self):
        # 30 degrees about right, then -20 about forward: fixed independent pose.
        p, r = math.radians(30), math.radians(-20)
        matrix = [[math.cos(r), 0, math.sin(r)],
                  [math.sin(p)*math.sin(r), math.cos(p), -math.sin(p)*math.cos(r)],
                  [-math.cos(p)*math.sin(r), math.sin(p), math.cos(p)*math.cos(r)]]
        a = body_vector(matrix, [0, 0, 1])
        m = body_vector(matrix, [0, .35, -.3])
        self.assertLess(angle_error(watch_forward_heading(a, m), 0), 1e-10)
        self.assertAlmostEqual(shortest_arc_heading(a, m), 5.410046684431929, places=10)


if __name__ == "__main__":
    unittest.main()
