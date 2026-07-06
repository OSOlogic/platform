# Security Policy

## Supported Versions

OSOlogic is in active pre-release development (alpha → beta). Security fixes are
applied to the `main` branch and the latest tagged release.

| Version        | Supported          |
|----------------|--------------------|
| `main`         | :white_check_mark: |
| latest release | :white_check_mark: |
| older releases | :x:                |

## Reporting a Vulnerability

**Please do not open public issues for security vulnerabilities.**

Industrial control systems have real-world safety implications. If you discover a
vulnerability — especially anything affecting the runtime scan cycle, the I/O layer,
the gateways, or authentication — report it privately:

- **Email:** osologic.team@gmail.com
- Alternatively, use GitHub's [private vulnerability reporting](https://docs.github.com/code-security/security-advisories/guidance-on-reporting-and-writing-information-about-vulnerabilities/privately-reporting-a-security-vulnerability)
  on this repository (Security → Report a vulnerability).

Please include:

- A description of the vulnerability and its impact.
- Steps to reproduce (proof-of-concept if possible).
- Affected component(s) and version/commit.
- Any suggested mitigation.

## Disclosure Process

1. We acknowledge your report within **5 business days**.
2. We investigate and confirm the issue, and agree on a remediation timeline.
3. We prepare a fix and, where relevant, a coordinated disclosure and CVE.
4. We credit reporters who wish to be named once the fix is released.

Thank you for helping keep industrial automation safe and open.
