# AGENTS.md — Provenance & Attribution für AI-Agenten

> Dieses Repo folgt dem **Notavis Software Development Standard (SDS)**.
> Verbindliche Regeln: <https://github.com/Notavis-GmbH/platform-builder-docs/tree/main/100_Software-Development-Standard>

## 1. Erlaubte Agenten

| Agent | Modus | Zulässige Phasen |
|-------|-------|------------------|
| Perplexity Computer | Pair / Autonomous / Research | Requirements, Design, Implementation, Verification |
| GitHub Copilot      | Pair                         | Implementation, Verification |

Andere Agenten benötigen schriftliche Freigabe der Abteilungsleitung Softwareentwicklung.

## 2. Pflicht-Trailer in Commits

Jeder von einem Agenten unterstützte Commit MUSS folgenden Git-Trailer enthalten:

```
Co-Authored-by: Perplexity Computer <agent@notavis.com>
```

Bei Autonomous-Mode zusätzlich:

```
Agent-Mode: autonomous
Agent-Prompt-Hash: <sha256-prefix-12>
```

## 3. PR-Pflichtangaben

Jeder PR mit Agent-Beteiligung MUSS im PR-Body deklarieren:

- **Agent-Modus:** Pair | Autonomous | Research
- **Geänderte Bereiche:** Code | Tests | Doku | Config | CI
- **Menschliche Review-Tiefe:** Line-by-Line | Spot-Check | High-Level
- **Sicherheitskritischer Bereich berührt?** Ja/Nein (bei Ja: zweiter Reviewer Pflicht)

## 4. Verbotene Aktionen ohne explizite menschliche Freigabe

- `git push --force` auf geschützte Branches
- Änderungen an `.github/workflows/`, Branch-Protection-Rules, Secrets
- Hinzufügen neuer Dependencies mit Lizenz != MIT/Apache-2.0/BSD-3
- Löschen von Tests
- Bypass von `pre-commit` oder CI-Checks

## 5. Safety-Stop-Trigger

Der Agent stoppt SOFORT und fragt zurück bei:

- CVSS ≥ 7.0 in geänderter Dependency
- Test-Coverage-Drop > 2 Prozentpunkte
- Mehr als 500 LOC Diff in einem PR
- Berührung von Sicherheitsfunktionen (Auth, Crypto, Bootloader, Sensor-Treiber-IRQ)

## 6. Prompt-Logs

Bei Autonomous-Runs wird ein Prompt-Log unter `.agent-logs/<datum>-<thema>.md` ins Repo committed (oder als Artifact am PR angehängt) — siehe SDS §04_AI-Agent-Rules/06_provenance-attribution.md.
