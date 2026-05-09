<!--
Notavis SDS — PR-Template
Standard: https://github.com/Notavis-GmbH/platform-builder-docs/tree/main/100_Software-Development-Standard
-->

## Was ändert dieser PR?

<!-- Kurze Zusammenfassung in 1–3 Sätzen -->

## Verlinkte Issues / Tickets

Closes #
Refs #

## Track

- [ ] Driver / Firmware (MISRA / AUTOSAR / V4L2)
- [ ] Application (Python / TS / Docker / REST)
- [ ] Beides
- [ ] Doku / CI / Tooling

## Art der Änderung

- [ ] feat — Neues Feature
- [ ] fix — Bugfix
- [ ] hwfix — Hardware-bezogener Fix
- [ ] refactor — Refactoring ohne Verhaltensänderung
- [ ] perf — Performance-Optimierung
- [ ] docs — Doku
- [ ] test — Tests
- [ ] ci — CI/Build
- [ ] chore — Sonstiges

## Tests

- [ ] Unit-Tests vorhanden / aktualisiert
- [ ] Integrationstest vorhanden / aktualisiert
- [ ] HiL / On-Target getestet (Board: __________ )
- [ ] Manuell verifiziert (Beschreibung unten)

## Checkliste

- [ ] Code folgt dem Style-Guide des jeweiligen Tracks
- [ ] Keine neuen Linter-/Static-Analysis-Warnings
- [ ] Keine Secrets / Credentials im Diff
- [ ] Doku aktualisiert (README, ADR, Spec, Glossary, falls relevant)
- [ ] CHANGELOG-Eintrag (bei user-facing Änderung)
- [ ] Conventional Commits eingehalten

## AI-Agent-Beteiligung

- [ ] Kein Agent
- [ ] Pair-Mode (menschliche Line-by-Line-Review)
- [ ] Autonomous-Mode (Prompt-Log angehängt / committed)
- [ ] Research-Mode (nur Doku / Empfehlung)

Falls Agent: **Co-Authored-by**-Trailer in Commits gesetzt? `[ ] Ja`

## Sicherheits- / Risiko-Hinweise

<!-- Berührt der PR Auth, Crypto, Bootloader, Sensor-IRQ, Netzwerk-Exposure? -->

- [ ] Kein sicherheitskritischer Bereich
- [ ] Sicherheitskritisch — zweiter Reviewer angefragt: @
