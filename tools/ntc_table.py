#!/usr/bin/env python3
"""Make a validated resistance/temperature table for the NTC thermistor driver.

A part is data. This tool turns any of three descriptions into the `TablePoint` array the driver takes:

  ntc_table.py sh   A B C            --from -40 --to 125 --step 5   Steinhart-Hart, 1/T = A + B ln R + C ln(R)^3
  ntc_table.py beta R0 B [--t0 25]   --from -40 --to 125 --step 5   beta equation, 1/T = 1/T0 + ln(R/R0) / B
  ntc_table.py list FILE                                            vendor list, one "temperature_degC resistance_ohm"
                                                                    pair per line (# comments, commas allowed)
  ntc_table.py selfcheck                                            the tool's own tests (CI runs it)

Options: --name kMyPart (the array's name), --max-error K (fail when the interpolation error is larger).

The output is a C++ array, a comment with the numbers that matter, and a non-zero exit status when the table the
driver would refuse (Validate) is not valid. The interpolation error is measured the way the driver interpolates
(linear in ln R) against the source curve at the middle of every segment, and for a list against nothing (a list is
the truth): then only the validity is checked. Python's `math` is used here, offline; the driver stays integer.
"""
import argparse
import contextlib
import io
import math
import sys

K0 = 273.15
MAX_ROWS = 256
FINE = 256  # the driver carries resistance in 1/256 ohm
Q16 = 65536.0


def sh_r(a, b, c, t_c):
    """R in ohms for a temperature, by solving Steinhart-Hart (cubic in ln R, monotone) with bisection."""
    inv = 1.0 / (t_c + K0)
    lo, hi = math.log(1e-3), math.log(4e9)
    for _ in range(200):
        mid = (lo + hi) / 2
        if a + b * mid + c * mid ** 3 < inv:
            lo = mid
        else:
            hi = mid
    return math.exp((lo + hi) / 2)


def beta_r(r0, b, t0_c, t_c):
    return r0 * math.exp(b * (1.0 / (t_c + K0) - 1.0 / (t0_c + K0)))


def interp_error(rows, curve_t):
    """Largest |table - curve| in kelvin at the middle (in ln R) of every segment."""
    worst = 0.0
    for (t0, r0), (t1, r1) in zip(rows, rows[1:]):
        lr = (math.log(r0) + math.log(r1)) / 2
        t = t0 + (t1 - t0) * (math.log(r0) - lr) / (math.log(r0) - math.log(r1))
        worst = max(worst, abs(t - curve_t(math.exp(lr))))
    return worst


def problems(rows):
    """What the driver's Validate() would refuse, as a list of messages (empty = valid)."""
    out = []
    if not MIN_ROWS <= len(rows) <= MAX_ROWS:
        out.append("%d rows (the driver takes %d to %d)" % (len(rows), MIN_ROWS, MAX_ROWS))
    for t, r in rows:
        if r < 1 or r > 4294967295:
            out.append("resistance %d ohm does not fit 1 to 2^32-1" % r)
        if t <= -273.15:
            out.append("temperature %g degC is at or below absolute zero" % t)
    for (t0, r0), (t1, r1) in zip(rows, rows[1:]):
        if not t1 > t0:
            out.append("temperature must rise strictly: %g then %g" % (t0, t1))
        if not r1 < r0:
            out.append("resistance must fall strictly: %d then %d ohm (round-off: use a finer step or fewer rows)" % (r0, r1))
        elif (r0 >= 1) and (r1 >= 1) and math.floor(math.log(r0 * FINE) * Q16 + 0.5) <= math.floor(math.log(r1 * FINE) * Q16 + 0.5):
            out.append("rows %d and %d ohm have the same logarithm to 2^-16" % (r0, r1))
    return out


MIN_ROWS = 2


class ToolError(Exception):
    """A mistake of the user's, reported as one line and exit status 2."""


def build(args):
    if args.mode == "list":
        rows = []
        try:
            with open(args.file, encoding="utf-8") as handle:
                lines = handle.read().split("\n")
        except OSError as e:
            raise ToolError("cannot read %s: %s" % (args.file, e.strerror)) from e
        for number, line in enumerate(lines, 1):
            fields = line.split("#")[0].replace(",", " ").split()
            if not fields:
                continue
            try:
                if len(fields) != 2:
                    raise ValueError
                rows.append((float(fields[0]), int(round(float(fields[1])))))
            except ValueError as e:
                raise ToolError("%s:%d: expected 'temperature_degC resistance_ohm', got %r" % (args.file, number, line)) from e
        return sorted(rows), None
    if not args.step > 0:
        raise ToolError("--step must be above 0")
    if not args.hi > args.lo:
        raise ToolError("--to must be above --from")
    n = int(round((args.hi - args.lo) / args.step)) + 1
    if n > MAX_ROWS:
        raise ToolError("%d rows: the driver takes at most %d; use a larger --step or a smaller range" % (n, MAX_ROWS))
    temps = [args.lo + i * args.step for i in range(n)]
    if args.mode == "sh":
        r_of = lambda t: sh_r(args.A, args.B, args.C, t)
        curve_t = lambda r: 1.0 / (args.A + args.B * math.log(r) + args.C * math.log(r) ** 3) - K0
    else:
        r_of = lambda t: beta_r(args.R0, args.B, args.t0, t)
        curve_t = lambda r: 1.0 / (1.0 / (args.t0 + K0) + math.log(r / args.R0) / args.B) - K0
    try:
        rows = [(t, int(round(r_of(t)))) for t in temps]
    except (ArithmeticError, ValueError) as e:
        raise ToolError("the curve cannot be evaluated over %g to %g degC (%s): check the coefficients and the range"
                        % (args.lo, args.hi, e)) from e
    return rows, curve_t


def render(rows, name, err):
    lines = ["// Generated by tools/ntc_table.py. %d rows, %g to %g degC." % (len(rows), rows[0][0], rows[-1][0])]
    if err is not None:
        step = max(t1 - t0 for (t0, _), (t1, _) in zip(rows, rows[1:]))
        lines.append("// Interpolation error against the source curve: %.4f K at the middle of a segment" % err)
        lines.append("// (the driver's bound for a smooth curve is 0.0013 * step^2 + 0.003 K = %.4f K at step %g)."
                     % (0.0013 * step * step + 0.003, step))
    lines.append("// Declare it as Curve{.table = %s}; the rows must outlive the driver." % name)
    lines.append("inline constexpr ntc_thermistor::TablePoint %s[] = {" % name)
    for t, r in rows:
        lines.append("    {.resistance_ohm = %d, .temperature_mc = %d}," % (r, int(round(t * 1000))))
    lines.append("};")
    return "\n".join(lines)


def selfcheck():
    ok = True

    def check(name, cond):
        nonlocal ok
        print(("PASS " if cond else "FAIL ") + name)
        ok = ok and cond

    # A beta curve, 5 K rows: valid, strictly falling, within the driver's stated bound.
    rows = [(t, int(round(beta_r(10000, 3950, 25, t)))) for t in range(-40, 126, 5)]
    check("beta table is valid", problems(rows) == [])
    err = interp_error(rows, lambda r: 1.0 / (1.0 / (25 + K0) + math.log(r / 10000) / 3950) - K0)
    check("beta table error %.4f K within 0.0013*25+0.003" % err, err <= 0.0013 * 25 + 0.003)
    check("beta 10k/3950 at 25 degC is 10000 ohm", dict(rows)[25] == 10000)
    # Steinhart-Hart: the round trip of the solver, and a valid table.
    a, b, c = 0.8767633e-3, 2.527899e-4, 1.906681e-7
    r = sh_r(a, b, c, 25.0)
    t = 1.0 / (a + b * math.log(r) + c * math.log(r) ** 3) - K0
    check("Steinhart-Hart solver round trip (%.6f degC)" % t, abs(t - 25.0) < 1e-6)
    rows = [(t, int(round(sh_r(a, b, c, t)))) for t in range(-40, 151, 5)]
    check("Steinhart-Hart table is valid", problems(rows) == [])
    # The checks themselves: a duplicate resistance, a rising resistance, too few rows.
    check("duplicate resistance is refused", problems([(0, 100), (5, 100)]) != [])
    check("rising resistance is refused", problems([(0, 100), (5, 120)]) != [])
    check("one row is refused", problems([(0, 100)]) != [])
    check("temperature must rise", problems([(5, 100), (0, 50)]) != [])
    check("rendering names the array", "kPart[] = {" in render(rows, "kPart", None))
    # Degenerate input ends in one line of text and a non-zero status, never a traceback.
    cases = {
        "step 0": (["beta", "10000", "3950", "--from", "0", "--to", "50", "--step", "0"], "--step"),
        "negative step": (["beta", "10000", "3950", "--from", "0", "--to", "50", "--step", "-5"], "--step"),
        "to below from": (["beta", "10000", "3950", "--from", "50", "--to", "0", "--step", "5"], "--to"),
        "more than 256 rows": (["beta", "10000", "3950", "--from", "-40", "--to", "125", "--step", "0.1"], "at most 256"),
        "rows that round to 0 ohm": (["beta", "10", "3950", "--from", "100", "--to", "150", "--step", "0.5"], "must fall strictly"),
        "no such file": (["list", "/nonexistent/ntc-list.txt"], "cannot read"),
    }
    for name, (argv, text) in cases.items():
        err = io.StringIO()
        with contextlib.redirect_stderr(err), contextlib.redirect_stdout(io.StringIO()):
            code = main(argv)
        check("clean error for %s" % name, code != 0 and text in err.getvalue() and "Traceback" not in err.getvalue())
    return 0 if ok else 1


def main(argv=None):
    p = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    sub = p.add_subparsers(dest="mode", required=True)
    for mode in ("sh", "beta", "list", "selfcheck"):
        q = sub.add_parser(mode)
        q.add_argument("--name", default="kPart")
        q.add_argument("--max-error", type=float, default=None)
        if mode in ("sh", "beta"):
            q.add_argument("--from", dest="lo", type=float, required=True)
            q.add_argument("--to", dest="hi", type=float, required=True)
            q.add_argument("--step", type=float, required=True)
        if mode == "sh":
            for k in "ABC":
                q.add_argument(k, type=float)
        if mode == "beta":
            q.add_argument("R0", type=float)
            q.add_argument("B", type=float)
            q.add_argument("--t0", type=float, default=25.0)
        if mode == "list":
            q.add_argument("file")
    args = p.parse_args(argv)
    if args.mode == "selfcheck":
        return selfcheck()
    try:
        rows, curve_t = build(args)
    except ToolError as e:
        print("error: %s" % e, file=sys.stderr)
        return 2
    bad = problems(rows)
    unique = list(dict.fromkeys(bad))
    for line in unique[:5]:
        print("error: " + line, file=sys.stderr)
    if len(unique) > 5:
        print("error: ... and %d more" % (len(unique) - 5), file=sys.stderr)
    err = interp_error(rows, curve_t) if (curve_t and not bad) else None
    if args.max_error is not None and err is not None and err > args.max_error:
        print("error: interpolation error %.4f K is above %.4f K: use a smaller step" % (err, args.max_error),
              file=sys.stderr)
        return 1
    if bad:
        return 1  # no table is printed: it would be refused
    print(render(rows, args.name, err))
    return 0


if __name__ == "__main__":
    sys.exit(main())
