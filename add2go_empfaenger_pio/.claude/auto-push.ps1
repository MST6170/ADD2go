# add2go-empfaenger-pio Auto-Push-Hook
# Wird vom Claude-Code Stop-Hook aufgerufen.
# Logik:
#   1. Wenn im PIO-Unterordner nichts geaendert: still beenden (kein leerer Commit)
#   2. Build-Gate: pio run ueber alle 4 Envs muss durchgehen
#   3. Bei Erfolg: git add -A add2go_empfaenger_pio/, git commit -m "auto: <ts>", git push
#   4. Bei Build-Fehler: nicht pushen, in last-build.log loggen, exit 1
#
# WICHTIG zur PS-5.1-Stolperfalle: native git/pio schreiben harmlose Status-
# meldungen auf stderr. Wir DUERFEN stderr nicht via `*>&1` einsammeln, sonst
# werden alle stderr-Zeilen zu NativeCommandError-Records und das Skript bricht
# trotz exit 0 ab. Stattdessen pruefen wir $LASTEXITCODE direkt nach jedem
# nativen Aufruf.
#
# WICHTIG zur Stage-Strategie: `git -C $repo add -A add2go_empfaenger_pio/`
# staged additions/modifications UND deletions im Unterordner, beruehrt aber
# andere Pfade im Repo (Sender-Sketch, KiCad, STL, etc.) nicht.

$ErrorActionPreference = "Continue"
$repo = "D:\Add2go"
$workdir = "D:\Add2go\add2go_empfaenger_pio"
$pioBin = "C:\Users\MST\.platformio\penv\Scripts"
$ghBin  = "C:\Users\MST\bin\gh\bin"
$env:Path = "$pioBin;$ghBin;$env:Path"

# Schritt 1: Hat sich im PIO-Unterordner was geaendert? .gitignore-respektiert.
$status = & git -C $repo status --porcelain -- "add2go_empfaenger_pio"
if (-not $status) { exit 0 }

# Schritt 2: Build-Gate ueber alle Envs (Output direkt ins Log, nicht via Pipe)
$logFile = "$workdir\.claude\last-build.log"
"=== auto-push build $(Get-Date -Format 'yyyy-MM-dd HH:mm:ss') ===" | Set-Content -Path $logFile -Encoding utf8
Set-Location $workdir
& pio run -e main -e wifi_test -e display_test -e flow_tab_test >> $logFile 2>&1
if ($LASTEXITCODE -ne 0) {
    Write-Host "[auto-push] BUILD FAILED -- siehe $logFile -- nichts gepusht"
    exit 1
}

# Schritt 3: Commit + Push (kein stderr-Redirect, einfach Exitcode pruefen)
$ts = Get-Date -Format "yyyy-MM-dd HH:mm:ss"
& git -C $repo add -A add2go_empfaenger_pio/
if ($LASTEXITCODE -ne 0) { Write-Host "[auto-push] git add failed"; exit 1 }

& git -C $repo commit -m "auto: $ts" | Out-Null
if ($LASTEXITCODE -ne 0) {
    Write-Host "[auto-push] commit fehlgeschlagen oder nichts zum committen"
    exit 0
}

& git -C $repo push 2>&1 | Set-Content -Path "$workdir\.claude\last-push.log" -Encoding utf8
if ($LASTEXITCODE -ne 0) {
    Write-Host "[auto-push] PUSH FAILED -- siehe .claude\last-push.log"
    exit 1
}

Write-Host "[auto-push] OK: $ts"
exit 0
