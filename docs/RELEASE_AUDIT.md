# Release preparation — 2026-10-21

This repository starts with a clean initial commit containing the verified project
files. No prior Git history, private backups, old workflow runs or activity records
were imported. Original project material retains the MIT licence, copyright 2026
Joe Green. Firmware, KiCad designs and the SVG are unchanged from the verified
release snapshot.

## Validation

- Nano ATmega328P compilation passed with Arduino CLI 1.5.1, AVR core 1.8.8,
  U8g2 2.36.19, Adafruit INA228 3.0.0 and BusIO 1.17.4. Flash: 24,146 / 30,720
  bytes; static RAM: 1,023 / 2,048 bytes.
- Workflow syntax passed actionlint. Local documentation links and ignore rules
  were checked. No generated build files, KiCad recovery files, local preferences,
  locks or automatic ZIP backups are included.
- The release files were scanned for credential patterns and the previously
  discovered machine-identifying strings and home-directory paths; no matches
  were found. Joe Green's licence attribution and the GitHub author identity are
  intentionally retained. Pattern scans cannot guarantee the absence of every
  possible secret format.
- KiCad project JSON and schematic/PCB structure parsed; all five used symbols
  are embedded and no separate custom library dependency was found.
- KiCad CLI is unavailable: opening with KiCad and a fresh SVG export remain
  manual checks. The PCB is an empty placeholder. Compilation does not validate
  the electrical design or measurements; physical upload and bench testing remain
  outstanding. See [README.md](../README.md) for commands and limitations.
- Third-party source, font and binary distribution obligations are described in
  [THIRD_PARTY_NOTICES.md](../THIRD_PARTY_NOTICES.md).

## GitHub setup before publication

Keep `JoeyBobbles/lifepo4-battery-monitor` private while configuring protections.
Repository policies are not copied from another repository. Review the commands
below before running them; they are configuration instructions, not a record of
settings already enabled. Availability depends on the account plan and visibility.

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

Enable Dependabot alerts and secret scanning/push protection where available:

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

Confirm the effective settings before publishing. Dependabot alerts do not replace
manual Arduino library review. Check private-repository plan eligibility if GitHub
rejects a configuration request.

GitHub references: [branch protection](https://docs.github.com/en/rest/branches/branch-protection),
[repository security settings](https://docs.github.com/en/rest/repos/repos).
