# Why binary is physically practical

*(Draft answer — read it, then rewrite it in your own words before treating
the Week 1 "written explanation" evidence item as done. The tracker's
completion standard is "explain it," not "read someone else's explanation.")*

Digital circuits need to represent state using a physical quantity —
voltage is the obvious/convenient one on a silicon chip. Voltage is
continuous: it can be 0.00V, 0.01V, 1.734V, anything. If we tried to encode,
say, 10 distinct symbols (like decimal digits 0-9) directly as 10 different
voltage bands, we'd need 10 narrow, precisely-spaced bands with 10 tiny
gaps between them. Any small noise (heat, electromagnetic interference,
power supply ripple, tiny manufacturing variance between transistors) could
easily push a signal from one narrow band into a neighboring one, silently
corrupting the value. Building circuitry that reliably tells 10 close-together
bands apart, at gigahertz switching speeds, across billions of transistors,
is extremely hard and unreliable.

With only **two** states, we get to do the opposite: use two *wide* bands
(e.g., roughly 0–0.8V = "0", roughly 3.0–3.3V(or whatever Vdd is) = "1")
with one *big* forbidden gap in between. Noise would have to swing a signal
across that entire gap to cause an error — which is far less likely than
crossing one of ten narrow gaps. This is why binary isn't an arbitrary
choice made for mathematical elegance — it's the encoding that gives the
maximum noise margin for the minimum circuit complexity, which is exactly
what you want when you're trying to switch billions of transistors
reliably, billions of times per second.

There's also a compounding practical benefit: a circuit element that only
has to distinguish two states (a transistor acting as a switch: fully on or
fully off) is much simpler and cheaper to build than one that has to
distinguish among many analog levels precisely. Two-state (binary) logic
lets you build the fundamental switching element (the transistor-as-switch)
as simply as possible, and then get arbitrary complexity/precision back
*for free* by combining huge numbers of these simple switches (see: Week 3,
logic gates) rather than needing each individual component to be precise.

**One-sentence version:** binary wins because it maximizes the noise margin
between states while minimizing the complexity of the switching element
needed to tell those states apart — an engineering trade-off, not a law of
mathematics.
