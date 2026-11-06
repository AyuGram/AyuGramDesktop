# Contributing to Drakeygram

Thanks for helping improve **Drakeygram Desktop** — an independent Telegram Desktop client.

**Table of Contents**

* [What contributions are accepted](#what-contributions-are-accepted)
* [Build instructions](#build-instructions)
* [Keep your fork up to date](#keep-your-fork-up-to-date)
* [How to get your pull request accepted](#how-to-get-your-pull-request-accepted)

## What contributions are accepted

We welcome:

* Bug fixes
* Performance improvements
* Documentation fixes
* Privacy / independence improvements (do **not** re-add third-party tracking)

Push to your fork and open a pull request against this repository.

## Build instructions

Follow the standard [Telegram Desktop build docs](https://github.com/telegramdesktop/tdesktop/tree/dev/docs) for your platform. Binary name is **Drakeygram**.

## Keep your fork up to date

```bash
git remote add upstream https://github.com/sqdq2/Drakeygram.git
git fetch upstream
git rebase upstream/main
git push --force-with-lease
```

## How to get your pull request accepted

* Keep PRs focused on a single issue
* Prefer one logical commit per PR
* Don’t mix refactors with whitespace-only cleanups
* Test a Debug build before submitting
* Write a clear commit message (what + why)

### Independence rule

Do **not** reintroduce:

* Connections to ayugram.one, exteragram, or similar
* Remote config / language CDNs outside Telegram
* Branding that undoes the Drakeygram identity

## License

Contributions are under the same GPL-3.0 family terms as the project / Telegram Desktop.
