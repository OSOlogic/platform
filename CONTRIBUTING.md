# Contributing to OSOlogic

Thank you for helping build the future of open industrial automation!

---

## Before you start

1. **Read the [CLA](./CLA.md).** By submitting a pull request you agree
   to the Contributor License Agreement. Corporate contributors must
   contact us at **licensing@osologic.com** before contributing.

2. **Check existing issues and discussions** to avoid duplicating work.

3. **Open an issue first** for significant changes so we can align on
   approach before you invest time coding.

---

## Development workflow

```
git clone https://github.com/OSOlogic/platform
cd platform
git checkout -b feature/my-feature
# ... make changes ...
git commit -m "feat(module): short description"
git push origin feature/my-feature
# Open a pull request on GitHub
```

---

## License headers

Every source file **must** carry an SPDX license identifier in its
header. Use the identifier that matches the directory license
(see [LICENSING.md](./LICENSING.md)):

**AGPL-3.0 (core, iec61131, gateways, io, ui, api, cli):**

```c
/*
 * OSOlogic — <module name>
 * <filename> — <short description>
 *
 * Copyright (C) <year> <Your Name> <<email>>
 *               Roig Borrell S.L. / Ibercomp S.L.
 *
 * Part of the OSOlogic project — https://osologic.com
 * SPDX-License-Identifier: AGPL-3.0-or-later
 */
```

**Apache-2.0 (sdk, standard):**

```c
/*
 * OSOlogic SDK — <module name>
 * <filename> — <short description>
 *
 * Copyright (C) <year> <Your Name> <<email>>
 *               Roig Borrell S.L. / Ibercomp S.L.
 *
 * Part of the OSOlogic project — https://osologic.com
 * SPDX-License-Identifier: Apache-2.0
 */
```

Adjust comment syntax for your language (Python uses `"""`, shell uses `#`).

---

## Coding standards

- **C / C++**: follow the existing style (K&R braces, 4-space indent).
- **Python**: PEP-8, type annotations, docstrings for public functions.
- **JavaScript / TypeScript**: ESLint + Prettier config in each sub-project.
- **All languages**: no tabs (except Makefile), UTF-8 encoding, LF line endings.

---

## Commit messages

Follow [Conventional Commits](https://www.conventionalcommits.org/):

```
<type>(<scope>): <short summary>

[optional body]
```

Types: `feat`, `fix`, `docs`, `refactor`, `test`, `ci`, `chore`.
Scope: the directory or module affected (e.g. `core`, `iec61131`, `sdk`).

---

## Pull request checklist

- [ ] CLA accepted (PR description confirms it)
- [ ] SPDX header present in every new source file
- [ ] Code compiles / tests pass
- [ ] No secrets, keys, or personal data committed
- [ ] Commit messages follow Conventional Commits

---

## Reporting security issues

Do **not** open a public issue for security vulnerabilities.
Send details to **security@osologic.com** (PGP key available on request).

---

## Questions?

Open a GitHub Discussion or e-mail **hello@osologic.com**.
