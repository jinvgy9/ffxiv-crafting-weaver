# Search-regression runner: reads tests/cases.csv and checks STEPS for each case.
#   Why PowerShell: it can drive the exe and read its output (Node's child_process cannot inside
#   this sandbox), and ConvertFrom-Csv handles the quoted `ban` column correctly.
#   ASCII only on purpose (PS 5.1 decodes BOM-less UTF-8 as GBK).
#
#   Usage:
#     powershell -ExecutionPolicy Bypass -File tests\run-cases.ps1
#     powershell -ExecutionPolicy Bypass -File tests\run-cases.ps1 -Only main-rlv770
param(
    [string]$Exe = '',
    [string]$Cases = '',
    [string]$DataDir = '',
    [string]$Only = ''
)
$ErrorActionPreference = 'Continue'

# ★ $PSScriptRoot is not reliable here (it came back empty when this script is invoked with -File),
#   so fall back to the command path. Both are computed once at the top, never inside the loop.
$here = $PSScriptRoot
if (-not $here) { $here = Split-Path -Parent $MyInvocation.MyCommand.Path }
if (-not $here) { $here = (Get-Location).Path }
$proj = Split-Path -Parent $here
if (-not $Exe)     { $Exe     = Join-Path $proj 'craftweave.exe' }
if (-not $Cases)   { $Cases   = Join-Path $here 'cases.csv' }
if (-not $DataDir) { $DataDir = Join-Path $proj 'data' }

if (-not (Test-Path $Exe))   { Write-Error "executable not found: $Exe  (build first: build.ps1)"; exit 2 }
if (-not (Test-Path $Cases)) { Write-Error "cases not found: $Cases"; exit 2 }
Write-Host "[env] exe=$Exe"
Write-Host "[env] cases=$Cases"
Write-Host "[env] data=$DataDir"

# cases.csv has a leading '#' comment block; drop those lines, then parse the rest in memory.
# ★ MUST pass -Encoding UTF8: the comments are Chinese and PS 5.1 otherwise decodes the file as ANSI,
#   which eats line breaks (a 21-line file reads as 14 lines) and silently drops every data row.
$rows = @(Get-Content $Cases -Encoding UTF8 | Where-Object { $_ -notmatch '^\s*#' -and $_.Trim() -ne '' } | ConvertFrom-Csv)
if ($rows.Count -eq 0) { Write-Error "no cases parsed from $Cases"; exit 2 }
Write-Host "[env] cases parsed=$($rows.Count)"

$pass = 0; $fail = 0
foreach ($r in $rows) {
    if ($Only -and $r.name -ne $Only) { continue }

    $a = @(
        "--lv=$($r.lv)", "--rlv=$($r.rlv)", "--cm=$($r.cm)", "--ctrl=$($r.ctrl)", "--cp=$($r.cp)",
        "--prog=$($r.prog)", "--qual=$($r.qual)", "--dur=$($r.dur)", "--qabs=$($r.start_qual)",
        "--mode=$($r.mode)", "--depth=$($r.depth)", "--perlayer=$($r.perlayer)",
        "--datadir=$DataDir"
    )
    if ($r.ban) { $a += "--ban=$($r.ban)" }

    Write-Host "=== $($r.name)  (perlayer=$($r.perlayer) depth=$($r.depth)) ==="
    $sw = [System.Diagnostics.Stopwatch]::StartNew()
    # Capture via a file inside the project (sandbox-writable). Piping the exe's output straight
    # into Where-Object returned nothing in this environment, so don't rely on it.
    $outFile = Join-Path $here '_last.out'
    & $Exe @a > $outFile 2>$null
    $sw.Stop()
    $raw = @(Get-Content $outFile -Encoding UTF8 -ErrorAction SilentlyContinue)
    Remove-Item $outFile -Force -ErrorAction SilentlyContinue

    $hit = @($raw | Where-Object { $_ -match '^STEPS ' })
    $got = if ($hit.Count -gt 0) { ($hit[0] -replace '^STEPS\s+', '').Trim() } else { "(none; raw=$($raw.Count) lines)" }
    $secs = [math]::Round($sw.Elapsed.TotalSeconds, 1)
    if ($got -eq "$($r.expect_steps)") {
        Write-Host "  PASS  STEPS=$got  (${secs}s)"
        $pass++
    } else {
        Write-Host "  FAIL  STEPS=$got  expected=$($r.expect_steps)  (${secs}s)"
        $raw | Select-Object -Last 3 | ForEach-Object { Write-Host "        | $_" }
        $fail++
    }
}
Write-Host ""
Write-Host "result: pass=$pass fail=$fail"
if ($fail -gt 0) { exit 1 }
