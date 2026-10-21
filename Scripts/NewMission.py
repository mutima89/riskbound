"""Scenario editor helper for player-made cases (Docs/Modding.md).

  python Scripts/NewMission.py X001 "My first case"     write a valid skeleton X001.json
  python Scripts/NewMission.py --check X001.json         structural check (the game runs the full validator on load)
"""
import json
import sys

SKELETON = {
    "schema": 1,
    "id": "",
    "title": "",
    "kind": "mission",
    "department": "Operations",
    "minLevel": 2,
    "requiresPassed": ["M001"],
    "passMark": 60,
    "xp": 120,
    "weeks": 2,
    "briefing": ["What has happened, who is asking, and what they need from the risk team."],
    "evidence": [
        {"id": "facts", "title": "The facts", "body": ["Numbers and observations the player needs."]},
        {"id": "interview", "title": "Interview notes", "body": "What a colleague says when asked.", "required": False, "at": "meeting"},
    ],
    "questions": [
        {"id": "rate", "kind": "rating", "competency": "RiskAssessment", "weight": 25, "prompt": "Rate the risk before treatment.",
         "likelihood": 3, "impact": 4, "explanation": "Why this rating is supported by the evidence."},
        {"id": "figure", "kind": "numeric", "competency": "QuantitativeAnalysis", "weight": 25, "prompt": "What is the expected annual loss?",
         "answer": 10000, "tolerance": 0.02, "unit": "a year", "explanation": "Show the working."},
        {"id": "decide", "competency": "Judgement", "weight": 30, "prompt": "What do you recommend?", "options": [
            {"id": "best", "label": "The sound recommendation", "score": 90, "feedback": "Why it is right."},
            {"id": "weak", "label": "A tempting but weaker choice", "score": 40, "feedback": "Why it falls short."},
            {"id": "poor", "label": "A poor choice", "score": 10, "feedback": "Why it is wrong."}]},
        {"id": "lessons", "kind": "multi", "competency": "RiskIdentification", "weight": 20, "prompt": "Which lessons apply? Pick all that apply.", "options": [
            {"id": "a", "label": "A correct lesson", "correct": True, "feedback": "Explain."},
            {"id": "b", "label": "Another correct lesson", "correct": True, "feedback": "Explain."},
            {"id": "c", "label": "A plausible mistake", "correct": False, "feedback": "Explain."}]},
    ],
    "lesson": "The one idea the player should take away.",
}


def check(path):
    m = json.load(open(path, encoding="utf-8"))
    problems = []
    for key in ("schema", "id", "title", "department", "briefing", "minLevel", "evidence", "questions"):
        if key not in m:
            problems.append("missing " + key)
    if sum(q.get("weight", 0) for q in m.get("questions", [])) != 100:
        problems.append("question weights must add up to 100")
    for q in m.get("questions", []):
        kind = q.get("kind", "single")
        if kind == "single" and max((o.get("score", 0) for o in q.get("options", [])), default=0) < 80:
            problems.append("question %s: no option scores 80 or more" % q.get("id"))
        if kind in ("numeric", "rating") and not q.get("explanation"):
            problems.append("question %s: needs an explanation" % q.get("id"))
    print("\n".join(problems) if problems else "Looks good. The game runs the full validator when it loads the file.")
    return 1 if problems else 0


def main():
    if len(sys.argv) == 3 and sys.argv[1] == "--check":
        sys.exit(check(sys.argv[2]))
    if len(sys.argv) != 3:
        sys.exit(__doc__)
    m = json.loads(json.dumps(SKELETON))
    m["id"], m["title"] = sys.argv[1], sys.argv[2]
    with open(sys.argv[1] + ".json", "w", encoding="utf-8", newline="\n") as f:
        json.dump(m, f, indent=2)
        f.write("\n")
    print("wrote", sys.argv[1] + ".json")


if __name__ == "__main__":
    main()
