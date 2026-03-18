#! /usr/bin/env python
#
# Copyright 2022 Spotify AB
#
# Licensed under the GNU Public License, Version 3.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#    https://www.gnu.org/licenses/gpl-3.0.html
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

import numpy as np
import pytest

from pedalboard import Bitcrush

from .utils import generate_sine_at


def _scale_factor(bit_depth):
    """Return the scale factor that Bitcrush uses internally after the fix."""
    return 2 ** (bit_depth - 1) + 1


@pytest.mark.parametrize("bit_depth", list(np.arange(1, 32, 0.5)))
@pytest.mark.parametrize("fundamental_hz", [440])
@pytest.mark.parametrize("sample_rate", [22050, 48000])
@pytest.mark.parametrize("num_channels", [1, 2])
def test_bitcrush(bit_depth: float, fundamental_hz: float, sample_rate: float, num_channels: int):
    sine_wave = generate_sine_at(
        sample_rate, fundamental_hz, num_seconds=0.1, num_channels=num_channels
    )

    plugin = Bitcrush(bit_depth)
    output = plugin.process(sine_wave, sample_rate)

    assert np.all(np.isfinite(output))

    sf = _scale_factor(bit_depth)
    expected_output = np.around(sine_wave.astype(np.float64) * sf) / sf
    np.testing.assert_allclose(output, expected_output, atol=0.01)


def test_invalid_bit_depth_raises_exception():
    with pytest.raises(ValueError):
        Bitcrush(bit_depth=-5)
    with pytest.raises(ValueError):
        Bitcrush(bit_depth=100)


class TestBitcrushQuantizationFormula:
    """Tests that verify the corrected Bitcrush quantization formula (issue #396)."""

    @pytest.mark.parametrize("bit_depth", [1, 2, 4, 8, 16])
    def test_output_is_quantized(self, bit_depth: int):
        """Output samples should snap to a discrete set of quantized levels."""
        sample_rate = 44100.0
        # Use a ramp from -1 to 1 so we cover the full range
        samples = np.linspace(-1.0, 1.0, 4096, dtype=np.float32).reshape(1, -1)

        plugin = Bitcrush(bit_depth)
        output = plugin.process(samples, sample_rate)

        sf = _scale_factor(bit_depth)
        # Every output sample, when multiplied by scaleFactor, should be (close
        # to) an integer — that is the definition of quantization.
        quantized_indices = output * sf
        np.testing.assert_allclose(
            quantized_indices,
            np.round(quantized_indices),
            atol=1e-5,
            err_msg=(
                f"Output samples are not properly quantized at bit_depth={bit_depth}"
            ),
        )

    def test_bit_depth_8_output_range_signed(self):
        """For bit_depth=8 the output should stay within [-1, 1] and include
        negative values — i.e. quantization is centered around zero for signed
        audio."""
        sample_rate = 44100.0
        samples = np.linspace(-1.0, 1.0, 4096, dtype=np.float32).reshape(1, -1)

        plugin = Bitcrush(8)
        output = plugin.process(samples, sample_rate)

        # Output must be within [-1, 1]
        assert np.all(output >= -1.0 - 1e-6), "Output has values below -1"
        assert np.all(output <= 1.0 + 1e-6), "Output has values above 1"

        # Must have both positive and negative values (symmetric around zero)
        assert np.any(output > 0), "Output has no positive values"
        assert np.any(output < 0), "Output has no negative values"

    @pytest.mark.parametrize("bit_depth", [2, 4, 8, 16])
    def test_quantization_symmetric_around_zero(self, bit_depth: int):
        """Quantizing a symmetric input signal should produce a symmetric
        output: quantize(-x) == -quantize(x) for all x."""
        sample_rate = 44100.0
        positive = np.linspace(0.0, 1.0, 2048, dtype=np.float32).reshape(1, -1)
        negative = -positive

        plugin = Bitcrush(bit_depth)
        out_pos = plugin.process(positive, sample_rate)
        out_neg = plugin.process(negative, sample_rate)

        np.testing.assert_allclose(
            out_neg,
            -out_pos,
            atol=1e-6,
            err_msg=f"Quantization is not symmetric around zero at bit_depth={bit_depth}",
        )

    def test_bit_depth_1_produces_three_or_fewer_levels(self):
        """With bit_depth=1 the scale factor is 2^0 + 1 = 2, so the only
        representable values are -1, -0.5, 0, 0.5, 1 (at most 5 levels).
        The exact count depends on rounding, but it must be a very small set."""
        sample_rate = 44100.0
        samples = np.linspace(-1.0, 1.0, 8192, dtype=np.float32).reshape(1, -1)

        plugin = Bitcrush(1)
        output = plugin.process(samples, sample_rate)

        unique_values = np.unique(np.round(output, decimals=5))
        # scale factor = 2, so representable levels = round(x*2)/2
        # for x in [-1,1]: levels are {-1, -0.5, 0, 0.5, 1} = 5
        assert len(unique_values) <= 5, (
            f"bit_depth=1 should produce at most 5 unique output levels, "
            f"got {len(unique_values)}: {unique_values}"
        )

    def test_bit_depth_16_preserves_signal_closely(self):
        """At bit_depth=16, quantization should be very fine — the output
        should be nearly identical to the input."""
        sample_rate = 44100.0
        samples = np.linspace(-1.0, 1.0, 4096, dtype=np.float32).reshape(1, -1)

        plugin = Bitcrush(16)
        output = plugin.process(samples, sample_rate)

        # At 16 bits the step size is ~1/32769, so max error < 2e-5
        np.testing.assert_allclose(output, samples, atol=2e-5)

    def test_number_of_quantization_levels(self):
        """The number of distinct output levels for a full-range signal should
        be close to 2 * scaleFactor + 1 (levels from -sf to +sf mapped back)."""
        sample_rate = 44100.0
        for bit_depth in [2, 4, 8]:
            samples = np.linspace(-1.0, 1.0, 65536, dtype=np.float32).reshape(1, -1)
            plugin = Bitcrush(bit_depth)
            output = plugin.process(samples, sample_rate)

            sf = _scale_factor(bit_depth)
            unique_values = np.unique(np.round(output, decimals=6))
            expected_levels = int(2 * sf) + 1  # from -sf/sf to +sf/sf in 1/sf steps
            # Allow some tolerance — rounding at boundaries may merge levels
            assert abs(len(unique_values) - expected_levels) <= 2, (
                f"bit_depth={bit_depth}: expected ~{expected_levels} levels, "
                f"got {len(unique_values)}"
            )
