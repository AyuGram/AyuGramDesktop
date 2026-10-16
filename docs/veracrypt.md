# Data folder and VeraCrypt (Windows)

AyuGram can keep its profile on a mounted encrypted volume.

1. Mount your VeraCrypt volume (for example to `V:`).
2. In Preferences → General → Storage, set **Data folder** to a directory on that volume.
3. Enable **Require data folder (strict)** so the app will not create a second profile under `%APPDATA%` if the volume is missing.
4. Restart the app.

CLI `-workdir` still overrides the saved folder for a single launch.

## Mount from the app

Preferences → Storage → **Mount VeraCrypt volume** looks for `VeraCrypt.exe`
and launches a quiet mount (`/v /l /q`). The password is entered in
VeraCrypt's own prompt (never passed on the command line; `/s` is not used
because it would suppress that prompt).
The mount drive letter is taken from the configured **Data folder**
(for example `E:` when the folder is `E:\AyuGramConfig`), not a hardcoded `V:`.
