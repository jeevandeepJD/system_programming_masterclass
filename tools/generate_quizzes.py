#!/usr/bin/env python3
"""Generate self-contained HTML theory checks from weekly quiz JSON files."""

from __future__ import annotations

import argparse
import html
import json
from pathlib import Path


ROOT = Path(__file__).resolve().parent.parent


def quiz_source(week: int) -> Path:
    matches = sorted(ROOT.glob(f"{week:02d}-*/quizzes.json"))
    if len(matches) != 1:
        raise SystemExit(
            f"expected one quizzes.json under {week:02d}-*, found {len(matches)}"
        )
    return matches[0]


def validate(data: dict, source: Path) -> None:
    if not isinstance(data.get("week"), int):
        raise ValueError(f"{source}: week must be an integer")
    days = data.get("days")
    if not isinstance(days, list) or not days:
        raise ValueError(f"{source}: days must be a non-empty list")

    seen: set[int] = set()
    for day in days:
        number = day.get("day")
        if not isinstance(number, int) or number < 1:
            raise ValueError(f"{source}: invalid day number {number!r}")
        if number in seen:
            raise ValueError(f"{source}: duplicate day {number}")
        seen.add(number)
        if not day.get("title"):
            raise ValueError(f"{source}: day {number} has no title")
        questions = day.get("questions")
        if not isinstance(questions, list) or len(questions) < 4:
            raise ValueError(f"{source}: day {number} needs at least 4 questions")
        for index, question in enumerate(questions, 1):
            options = question.get("options")
            answer = question.get("answer")
            if not question.get("prompt") or not question.get("explanation"):
                raise ValueError(
                    f"{source}: day {number} question {index} is incomplete"
                )
            if not isinstance(options, list) or len(options) < 2:
                raise ValueError(
                    f"{source}: day {number} question {index} needs options"
                )
            if not isinstance(answer, int) or not 0 <= answer < len(options):
                raise ValueError(
                    f"{source}: day {number} question {index} has bad answer"
                )


def render(week: int, day: dict) -> str:
    payload = json.dumps(day["questions"], ensure_ascii=False).replace(
        "</", "<\\/"
    )
    title = html.escape(day["title"])
    day_number = day["day"]
    return f"""<!doctype html>
<html lang="en">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>Week {week} · Day {day_number} Theory Check — {title}</title>
<style>
  :root {{
    color-scheme: dark;
    --bg:#07111b; --panel:#102031; --panel2:#162a3e; --line:#2d4861;
    --text:#eaf3fa; --muted:#9db0c2; --accent:#5bc8ff;
    --ok:#56df9b; --bad:#ff7589; --warn:#ffd06d;
  }}
  * {{ box-sizing:border-box; }}
  body {{
    margin:0; background:linear-gradient(145deg,#07111b,#0d1b29);
    color:var(--text); font:16px/1.55 system-ui,-apple-system,"Segoe UI",sans-serif;
  }}
  main {{ width:min(880px,100%); margin:auto; padding:32px 18px 80px; }}
  header, .question, .result {{
    background:var(--panel); border:1px solid var(--line);
    border-radius:14px; padding:22px; margin-bottom:18px;
  }}
  .eyebrow {{
    color:var(--accent); font:700 12px ui-monospace,monospace;
    letter-spacing:.1em; text-transform:uppercase;
  }}
  h1 {{ margin:.35rem 0 .5rem; font-size:clamp(25px,5vw,40px); line-height:1.12; }}
  .lede {{ color:var(--muted); margin:.4rem 0 0; }}
  fieldset {{ border:0; padding:0; margin:0; }}
  legend {{ font-weight:700; margin-bottom:14px; width:100%; }}
  .option {{
    display:flex; gap:11px; align-items:flex-start; padding:11px 13px;
    margin:8px 0; border:1px solid var(--line); border-radius:9px;
    background:var(--panel2); cursor:pointer;
  }}
  .option:hover {{ border-color:var(--accent); }}
  .option.correct {{ border-color:var(--ok); background:#123629; }}
  .option.wrong {{ border-color:var(--bad); background:#3a1822; }}
  input {{ margin-top:.28rem; accent-color:var(--accent); }}
  .feedback {{
    display:none; margin-top:13px; padding:11px 13px;
    border-left:3px solid var(--accent); color:var(--muted); background:#0a1723;
  }}
  .feedback.show {{ display:block; }}
  .unanswered {{ border-color:var(--warn); }}
  .actions {{ display:flex; gap:12px; flex-wrap:wrap; margin:24px 0; }}
  button {{
    border:1px solid var(--accent); border-radius:9px; padding:11px 18px;
    background:#173c55; color:var(--text); font:700 15px inherit; cursor:pointer;
  }}
  button.secondary {{ border-color:var(--line); background:transparent; }}
  button:hover {{ filter:brightness(1.14); }}
  .result {{ display:none; text-align:center; }}
  .result.show {{ display:block; }}
  .score {{ color:var(--accent); font-size:32px; font-weight:800; }}
  code {{ font-family:ui-monospace,monospace; color:#bdeaff; }}
</style>
</head>
<body>
<main>
  <header>
    <div class="eyebrow">Week {week} · Day {day_number} · Theory check</div>
    <h1>{title}</h1>
    <p class="lede">Predict first. Submit once. Explanations remain hidden until
    every question has an attempted answer.</p>
  </header>
  <form id="quiz"></form>
  <div class="actions">
    <button type="button" id="submit">Submit answers</button>
    <button type="button" class="secondary" id="reset">Reset</button>
  </div>
  <section class="result" id="result" aria-live="polite">
    <div class="score" id="score"></div>
    <p id="summary"></p>
  </section>
</main>
<script>
const questions = {payload};
const quiz = document.querySelector("#quiz");

function build() {{
  quiz.innerHTML = "";
  questions.forEach((q, qi) => {{
    const section = document.createElement("section");
    section.className = "question";
    section.dataset.question = qi;
    const fieldset = document.createElement("fieldset");
    const legend = document.createElement("legend");
    legend.textContent = `${{qi + 1}}. ${{q.prompt}}`;
    fieldset.appendChild(legend);
    q.options.forEach((option, oi) => {{
      const label = document.createElement("label");
      label.className = "option";
      const input = document.createElement("input");
      input.type = "radio";
      input.name = `q${{qi}}`;
      input.value = oi;
      label.append(input, document.createTextNode(option));
      fieldset.appendChild(label);
    }});
    const feedback = document.createElement("div");
    feedback.className = "feedback";
    section.append(fieldset, feedback);
    quiz.appendChild(section);
  }});
}}

document.querySelector("#submit").addEventListener("click", () => {{
  let score = 0;
  let unanswered = 0;
  questions.forEach((q, qi) => {{
    const section = quiz.querySelector(`[data-question="${{qi}}"]`);
    const selected = section.querySelector("input:checked");
    section.classList.toggle("unanswered", !selected);
    const feedback = section.querySelector(".feedback");
    section.querySelectorAll(".option").forEach((label, oi) => {{
      label.classList.remove("correct", "wrong");
      if (oi === q.answer) label.classList.add("correct");
      if (selected && Number(selected.value) === oi && oi !== q.answer)
        label.classList.add("wrong");
    }});
    if (!selected) {{
      unanswered++;
      feedback.textContent = "Choose an answer before reviewing this explanation.";
    }} else {{
      if (Number(selected.value) === q.answer) score++;
      feedback.textContent = q.explanation;
    }}
    feedback.classList.add("show");
  }});
  const result = document.querySelector("#result");
  result.classList.add("show");
  document.querySelector("#score").textContent = `${{score}} / ${{questions.length}}`;
  document.querySelector("#summary").textContent = unanswered
    ? `${{unanswered}} question(s) were unanswered. Review, reset, and try again.`
    : score === questions.length
      ? "All correct. Now explain the causal reason for each answer without the options."
      : "Use the explanations to identify the weak concept, not merely the missed option.";
  result.scrollIntoView({{behavior:"smooth", block:"center"}});
}});

document.querySelector("#reset").addEventListener("click", () => {{
  build();
  document.querySelector("#result").classList.remove("show");
  window.scrollTo({{top:0, behavior:"smooth"}});
}});

build();
</script>
</body>
</html>
"""


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument(
        "weeks",
        nargs="*",
        help="week numbers to generate; defaults to every quizzes.json",
    )
    args = parser.parse_args()

    if args.weeks:
        sources = [quiz_source(int(value)) for value in args.weeks]
    else:
        sources = sorted(ROOT.glob("[0-9][0-9]-*/quizzes.json"))

    if not sources:
        raise SystemExit("no weekly quiz data found")

    for source in sources:
        data = json.loads(source.read_text(encoding="utf-8"))
        validate(data, source)
        output_dir = source.parent / "challenges"
        output_dir.mkdir(parents=True, exist_ok=True)
        for day in data["days"]:
            output = output_dir / f"day-{day['day']:03d}-theory-check.html"
            output.write_text(render(data["week"], day), encoding="utf-8")
            print(f"wrote {output}")


if __name__ == "__main__":
    main()
