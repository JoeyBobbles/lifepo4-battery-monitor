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

- Inspected all 13 initially tracked files and all three commits reachable from
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

## GitHub setup before publication

Read-only authenticated GitHub API checks on 2026-10-21 confirmed the repository
is private and its default branch is `main`. No remote settings were changed.

| Protection | Observed state |
| --- | --- |
| Main branch protection | Disabled: protection endpoint returned “Branch not protected”; branch rules endpoint returned no rules |
| Force pushes blocked | No branch protection/rule enforces this |
| Branch deletion restricted | No branch protection/rule enforces this; GitHub separately prevents deleting the default branch |
| Pull requests required | No |
| Secret scanning | Disabled: alerts endpoint explicitly reported this |
| Dependabot alerts | Enabled: vulnerability-alerts endpoint returned HTTP 204 |

Keep `JoeyBobbles/lifepo4-battery-monitor` private while configuring protections.
Review the commands below before running them; they have not been executed.
Availability depends on the account plan and visibility.

After the first successful workflow run, protect `main` with pull requests,
required compilation, no force pushes and no deletion:

```sh
gh api --method PUT repos/JoeyBobbles/lifepo4-battery-monitor/branches/main/protection --input - <<'JSON'
{
  "required_status_checks": {
    "strict": true,
    "contexts": ["Compile Nano firmware"]
  },
  "enforce_admins": true,
  "required_pull_request_reviews": {
    "required_approving_review_count": 0
  },
  "restrictions": null,
  "allow_force_pushes": false,
  "allow_deletions": false
}
JSON
```

This requires a pull request but permits a sole maintainer to merge without an
independent approving review. Choose a different approval policy if desired.
Alternatively configure an active ruleset in Settings → Rules → Rulesets.

Dependabot alerts are already enabled. The first command below can re-enable
them if needed; enable secret scanning/push protection where available:

```sh
gh api --method PUT repos/JoeyBobbles/lifepo4-battery-monitor/vulnerability-alerts
gh api --method PATCH repos/JoeyBobbles/lifepo4-battery-monitor --input - <<'JSON'
{
  "security_and_analysis": {
    "secret_scanning": {"status": "enabled"},
    "secret_scanning_push_protection": {"status": "enabled"}
  }
}
JSON
```

Inspect the effective settings after applying the configuration:

```sh
gh api repos/JoeyBobbles/lifepo4-battery-monitor/branches/main/protection
gh api repos/JoeyBobbles/lifepo4-battery-monitor/rules/branches/main
gh api repos/JoeyBobbles/lifepo4-battery-monitor --jq '.security_and_analysis'
gh api -i repos/JoeyBobbles/lifepo4-battery-monitor/vulnerability-alerts
```

Confirm the effective settings before publishing. Dependabot alerts do not replace
manual Arduino library review. Check private-repository plan eligibility if GitHub
rejects a configuration request.

GitHub references: [branch protection](https://docs.github.com/en/rest/branches/branch-protection),
[repository security settings](https://docs.github.com/en/rest/repos/repos).
