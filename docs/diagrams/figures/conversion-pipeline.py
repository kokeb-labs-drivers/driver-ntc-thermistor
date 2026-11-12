"""The conversion pipeline: from the mean ADC code to a temperature and its bound.

Reads: Top to bottom, each arrow hands one number to the next stage. Read() takes sample_count codes and their mean
(the spread check can stop it as Unstable). The mean code is classified: at or above 2^bits is CodeOutOfScale, a code
in the margin at the open rail is OpenCircuit, one in the margin at the short rail is ShortCircuit; which rail is
which depends on the divider side. The node ratio gives the thermistor's resistance from the series resistor, in 1/256
ohm. The resistance goes through the curve: a table (a bounded binary search, then linear interpolation in ln R) or
Steinhart-Hart (a cubic in ln R) or the beta equation (1/T in Q32), any of them with no floating point and no library. The temperature must lie inside
the window; the offset is added last. The uncertainty bound is computed from the same code.
The stages are the functions of inc/ntc_thermistor_conversion.hpp.
"""

from _source import CONVERSION, body, need, read

from figkit import Fig


def data():
    text = read(CONVERSION)
    convert = body(text, "ConvertCode(const Config& config, uint32_t code)")
    need(r"FineOfCode\(config, code\)", convert, "the divider stage comes first")
    need(r"CurveTemperature\(config\.curve, fine\.value\)", convert, "the curve takes the resistance in fine ohms")
    need(r"Error::BelowRange.*Error::AboveRange", convert, "the window gives BelowRange and AboveRange", )
    need(r"SaturatingAdd\(t\.value, config\.offset_mc\)", convert, "the offset is added after the window")
    need(r"Uncertainty\(config, code, fine\.value, t\.value\)", convert, "the bound comes from the same code")
    fine = body(text, "FineOfCode(const Config& config, uint32_t code)")
    need(r"ClassifyCode\(config, code\)", fine, "the code is classified before the divider")
    curve = body(text, "CurveTemperature(const Curve& c, uint64_t r)")
    need(r"TableTemperature", curve, "table mode")
    need(r"BetaTemperature", curve, "beta mode")
    need(r"ShTemperature", curve, "Steinhart-Hart mode")
    need(r"kSearchSteps\s*=\s*8U", text, "the search is bounded")
    return {"sources": [f"{CONVERSION}:ConvertCode()", f"{CONVERSION}:FineOfCode()", f"{CONVERSION}:CurveTemperature()"]}


def build(d):
    f = Fig(
        "conversion-pipeline",
        "From an ADC code to a temperature",
        alt=("Pipeline of six stages: the mean of the samples, the classification of the code as open circuit, short "
             "circuit or out of scale, the divider's node ratio giving the resistance, the curve (table, Steinhart-Hart or "
             "beta) giving the temperature, the window and offset, and the uncertainty bound."),
        width=1200,
        kicker="Architecture",
        subtitle="Integers only: ohms, milli-degrees, millivolts. Each stage can end the read with its own error.",
        profile="overview",
        legend=[("brand", "stage"), ("amber", "ends the read with an error"), ("teal", "result")],
    )
    W = f.body_w
    side = 330
    gap, row = 40, 56
    M = W - side - gap
    nx = M + gap
    stages = [
        ("1  Mean code", ["sample_count conversions; the mean, rounded"], "brand",
         "Bus, CodeOutOfScale, Unstable"),
        ("2  Classify the code", ["the rails: margins at open and at short"], "brand",
         "OpenCircuit, ShortCircuit"),
        ("3  Divider to resistance", ["node ratio and the series resistor, in 1/256 ohm"], "brand",
         "BelowRange (more than 32 bits)"),
        ("4  Curve", ["table, Steinhart-Hart (cubic in ln R) or beta: 1/T in Q32"], "brand",
         "BelowRange, AboveRange"),
        ("5  Window and offset", ["inside the window, then the offset"], "brand",
         "BelowRange, AboveRange"),
        ("6  Reading", ["temperature_mc, resistance_ohm, code, uncertainty_mc"], "teal", ""),
    ]
    prev = None
    y = 0
    for i, (title, lines, tone, errors) in enumerate(stages):
        card = f.card(0, y, M, title, lines, tone=tone, accent=True)
        if errors:
            f.note(nx, y, side, errors, tone="amber", title="Can end with")
        if prev is not None:
            f.edge([(M / 2, prev.b + 4), (M / 2, card.y - 4)], label="")
        prev = card
        y = card.b + row
    return f
