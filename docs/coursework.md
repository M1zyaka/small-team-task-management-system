# Coursework guide

Small Team Task Management System is a project for Fundamentals of Object-Oriented Programming at Novosibirsk State University. This page follows NSU's 2025 coursework guidelines for second-year Applied Artificial Intelligence students.

## Traceability

| Guideline | Material in this repository |
| --- | --- |
| §1.1 — Practical solution in C++ | [Build instructions and release walkthrough](../README.md#quick-start) |
| §1.1 — Problem statement and existing approaches | [Problem and scope](design.md#problem-and-scope) |
| §1.1 — Justified mathematical model | [States, assignment relation, ranking, and overdue criteria](design.md#mathematical-model) |
| §1.1 — Encapsulation, inheritance, and polymorphism | [Class diagram and implementation references](design.md#object-oriented-structure) |
| §1.1 — Algorithms, data structures, and design patterns | [Queue and summary complexity](design.md#algorithms-and-data-structures), [Template Method and RAII](design.md#object-oriented-structure) |
| §1.1 — Results and conclusions | [Automated verification](design.md#verification) and [reproducible examples](../README.md#walk-through-a-release) give evidence for the student's own conclusions |
| §§2.3, 3.3 — Literature study | [25 candidate sources](references.md), including 2025 and 2026 publications, ICSE papers, and articles from a Level 1 journal |
| §§2.1, 2.2 — Topic and plan approval | Use the question and outline below when discussing the scope with the supervisor |
| §§3.1–3.3 — Written report formatting | [Report assembly checklist](#report-assembly-checklist) |
| §4.1 — Defense and working demonstration | [Seven-minute rehearsal plan](#defense-rehearsal) |

## Report outline

The project question is how a small team can keep a consistent, prioritized record of work with explicit ownership and progress rules. The implementation goal is a working C++ board that records assignments, enforces a state model, and reports overdue work.

1. **Introduction.** Specify the team, the problem, the goal, and the tasks. Confirm the topic and working plan with the supervisor.
2. **Materials and methods.** Compare relevant task trackers. Describe the member-to-item relation, transition model, and urgency function. Explain the selected classes, collection, and file storage.
3. **Implementation and results.** Describe the separation between the CLI, domain, and storage. Run CTest and the release walkthrough. Record the environment, expected values, observed values, and what happens on a rejected move.
4. **Conclusion.** Evaluate the observed results against the stated goal. Address the single-board and local-file assumptions and their implications for the team.
5. **References and appendices.** Use the sources examined for the report. Include supporting evidence, large diagrams, or extended test output where they help explain the results.

## Report assembly checklist

- Use the title page from Appendix 1 of the NSU guide with the student's name and group, supervisor's credentials and name, city, and year. Use the table of contents on page 2. The recommended body length is 15–25 printed pages. Appendices are outside this count.
- Use single-sided A4 portrait pages with margins of 30 mm left, 20 mm top, 10 mm right, and 20 mm bottom. Keep the body black, 14 pt Times New Roman, justified, with 1.5 line spacing and a 15 mm first-line indent.
- Number pages in the lower-right corner from the introduction (page 3) through the references. Leave the title and contents pages without visible numbers. Leave the appendices unnumbered under the guideline.
- Center headings in bold 14 pt. Use single spacing for multiline headings and no trailing period or end-of-line word division.
- Number tables and figures in separate continuous sequences. Place small tables immediately after the first in-text reference. Put the table caption above it, left-aligned and without an indent. Follow the table with analysis and credit borrowed tables in a footnote.
- If a table spans pages, place the caption above the first portion and omit the closing horizontal rule at the page break. Center figure captions below the figure. Put tables or figures that occupy a full page or more in an appendix.
- Use quotation marks for direct quotations and page footnotes with continuous numbering. Format footnotes in justified 12 pt Times New Roman with single spacing. Alphabetize entries within the required groups.
- Follow the supervisor's submission timetable. The guide calls for a review copy, then a printed and signed final report, followed by the public defense.

## Defense rehearsal

| Time | Topic | Evidence |
| --- | --- | --- |
| 0:00–0:45 | Problem and goal | Describe how ownership, progress, and deadlines appear on one board |
| 0:45–2:00 | Model and algorithm | Explain valid state transitions and show how two items with the same deadline are ordered |
| 2:00–3:15 | Object-oriented design | Show the class diagram and compare the two implementations of `urgency_bonus()` |
| 3:15–5:45 | Live demonstration | Use the README example. Attempt `task move 2 done`, which fails while item 2 is still in `todo`. Finish item 1 with `task move 1 done`, then run the same dated summary to verify that the overdue count drops from 1 to 0. Use the same `--data build/demo.db` on every command |
| 5:45–7:00 | Results and discussion | Show the test result, explain the snapshot and lock, and state the project's scope |

Use the evidence above to prepare the required 6–8 minute presentation and rehearse how the queue, state machine, and file commit behave.
