# Release preparation — 2026-10-21

## Scope and changes

The repository already contained an MIT licence (copyright 2026 Joe Green),
contribution guidelines, third-party notices, safety documentation and a Nano
compilation workflow. These were reviewed and retained. The ignore rules now
also cover legacy KiCad backups, symbol/footprint backups, additional editor
state and operating-system metadata.

Firmware remains in `firmware/firmware.ino`, with contents byte-for-byte
unchanged. Build/upload instructions and CI compile the whole `firmware/`
sketch directory. No tracked content was discarded. Hardware files and the
rendered SVG are unchanged. The SVG is an intentional generated documentation
asset; it should remain tracked. The PCB file is an empty placeholder.

## Sensitive information and history

- The initial audit inspected all 13 tracked files and all three commits then reachable from
  local branches, remote-tracking refs and reflogs (23 unique blobs/commit
  objects). `git fsck --full --no-reflogs --unreachable` reported no unreachable
  objects. This covers local Git data, not unadvertised remote refs or GitHub
  caches, issues, attachments or Actions logs.
- Pattern scans covered private keys, GitHub/AWS/Google credentials, secret
  assignments, credential-bearing URLs, home-directory paths, email addresses
  and IPv4 addresses. No credential or machine-specific path match was found.
  Manual review found no other sensitive content. Pattern scans cannot guarantee
  the absence of every possible secret format.
- Joe Green's copyright attribution and the commit author's GitHub noreply
  identity are intentionally retained public identifying information. No personal
  email address, telephone number or postal address was found.
- No history was rewritten or repository visibility changed during the audit.
  Historical assertions in the previous audit about earlier cleanup were not
  independently verifiable and have been replaced by this audit's scope/results.

## Validation

- Nano ATmega328P compilation passed with Arduino CLI 1.5.1, AVR core 1.8.8,
  U8g2 2.36.19, Adafruit INA228 3.0.0 and BusIO 1.17.4. Flash: 24,146 / 30,720
  bytes; static RAM: 1,023 / 2,048 bytes. Existing tools and pinned dependencies
  under `/tmp` were reused; build output was written outside the repository.
- Workflow syntax passed actionlint. The workflow runs for pushes to `main` and
  pull requests targeting `main`, uses only `contents: read`, installs the required
  AVR core and three libraries, and fails on compile errors. INA228's only
  declared library dependency is BusIO; Wire/SPI are supplied by the AVR core.
- Local Markdown targets, workflow structure, ignore rules, project JSON,
  schematic/PCB S-expressions and SVG XML were checked. No generated build files,
  KiCad recovery files, local preferences, locks or automatic ZIP backups are
  tracked. The deliberate schematic SVG is retained.
- All eight external Markdown link targets returned HTTP 200 when checked.
- All five symbol identifiers used in the schematic have embedded definitions;
  no external custom symbol library, subsheet or simulation-model dependency was
  found. Assigned footprints use standard KiCad library identifiers. Install the
  standard KiCad libraries when editing footprints.
- KiCad CLI is unavailable. Structural checks support the KiCad 9 format and
  self-contained symbols but do not establish that KiCad can open/export the
  design successfully. Opening in KiCad 9 and a fresh SVG export remain manual
  release checks; the command is in [README.md](../README.md).
- Firmware upload, electrical rule checking and bench validation were not
  performed. Firmware compilation does not validate hardware safety or readings.
- Existing library licence files and selected font headers were inspected; KiCad's
  library licence/exception was checked against its official documentation. No
  source-publication conflict was identified. Before distributing compiled
  binaries, review LGPL relinking obligations and required library/font notices;
  see [THIRD_PARTY_NOTICES.md](../THIRD_PARTY_NOTICES.md).

## GitHub configuration — verified 2026-10-21

Read-only authenticated GitHub API checks confirmed the repository remains private
and its default branch is `main`. The maintainer enabled the active
[Protect main ruleset](https://github.com/JoeyBobbles/lifepo4-battery-monitor/rules/25589712)
after the initial audit. No GitHub settings or repository visibility were changed
by the audit or this documentation update.

| Protection | Observed state |
| --- | --- |
| Main branch protection | Active ruleset targeting the default branch |
| Force pushes blocked | Yes: non-fast-forward updates prohibited |
| Branch deletion restricted | Yes: deletion prohibited |
| Pull requests required | Yes; squash merging only, zero approving reviews required |
| Required compilation | `Compile Nano firmware`, provided by GitHub Actions |
| Branch must be up to date | Yes: strict required status checks enabled |
| Bypass actors | None; current administrator cannot bypass the ruleset |
| Secret scanning | Disabled; built-in scanning is unavailable for this ordinary personally owned private repository |
| Dependabot alerts | Enabled: vulnerability-alerts endpoint returned HTTP 204 |

The zero-review policy permits a sole maintainer to merge their own pull request
once required checks pass. Changes must still go through a pull request, including
documentation updates. Classic branch protection is not configured separately;
the active ruleset supplies these protections. A “Branch not protected” response
from the classic protection endpoint does not describe the effective ruleset.

GitHub secret scanning is free for public repositories. Private organization
repositories require an eligible GitHub Team or Enterprise Cloud plan with Secret
Protection; ordinary personally owned private repositories are not eligible.
See [GitHub's secret-scanning availability and setup instructions](https://docs.github.com/en/code-security/how-tos/secure-your-secrets/detect-secret-leaks/enable-secret-scanning).
If publishing later, verify secret scanning and push protection in the settings
available at that time. Keeping the repository private is compatible with using a
separate local or CI scanner; none was added as part of this audit.

## Remaining manual release checks

- Open the schematic in KiCad 9 and verify a fresh SVG export; KiCad CLI was
  unavailable during the audit. See [README.md](../README.md) for the command.
- Complete firmware upload, electrical rule checking and hardware bench
  validation before describing the prototype as electrically validated.
- Before distributing compiled firmware, review the LGPL relinking obligations
  and required library/font notices in [THIRD_PARTY_NOTICES.md](../THIRD_PARTY_NOTICES.md).
- Recheck the protections and applicable secret-scanning options before publication.
  Dependabot alerts do not replace manual Arduino dependency review.

The required PR, compilation, up-to-date, force-push and deletion protections are
now configured; no additional GitHub settings change is required for those items.
Repository publication remains a separate manual decision.

To inspect the current configuration without changing it:

```sh
gh api repos/JoeyBobbles/lifepo4-battery-monitor/rulesets/25589712
gh api repos/JoeyBobbles/lifepo4-battery-monitor/rules/branches/main
gh api repos/JoeyBobbles/lifepo4-battery-monitor --jq '.security_and_analysis'
gh api -i repos/JoeyBobbles/lifepo4-battery-monitor/vulnerability-alerts
```

GitHub references: [repository rulesets](https://docs.github.com/en/rest/repos/rules),
[repository security settings](https://docs.github.com/en/rest/repos/repos).
