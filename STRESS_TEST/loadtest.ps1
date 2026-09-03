# Capacity search for the game server under a fixed pass criterion.
#
# Each point: reset the DB seed, start a fresh server, have the stress tool
# connect N clients and hold them for HoldSeconds, sample the server's CPU and
# memory meanwhile, then read the tool's [final] line (client-measured
# round-trip time p50 / p95 / p99). A point passes when p95 <= 25 ms and
# p99 <= 120 ms (see README for where those limits come from).
#
# Search: start at -Start and move by -Step, up on a pass and down on a fail,
# until a passing N and a failing N are one step apart; then bisect between
# them down to -Resolution. Every measured point is kept in the summary and
# the answer is the largest passing N.
#
# Prerequisites: Release x64 builds of Server and STRESS_TEST, SQL Server with
# the GS_Term_Project DSN (see DB\schema.sql), sqlcmd on PATH or in the SQL
# Server client SDK folder.
#
# Usage:
#   powershell -ExecutionPolicy Bypass -File STRESS_TEST\loadtest.ps1
#   powershell -ExecutionPolicy Bypass -File STRESS_TEST\loadtest.ps1 -Start 4000 -HoldSeconds 60
#   powershell -ExecutionPolicy Bypass -File STRESS_TEST\loadtest.ps1 -ServerExe C:\old\Server\x64\Release\Server.exe
#
# Results go to STRESS_TEST\results\ (ignored by git): a markdown summary plus
# the raw server samples and tool logs of every point.

param(
    [int]$Start = 6000,
    [int]$Step = 1000,
    [int]$Resolution = 100,
    [int]$MaxClients = 10000,   # server MAX_USER, also the number of seeded accounts
    [int]$HoldSeconds = 60,
    [string]$Server = "127.0.0.1",
    [string]$SqlInstance = "localhost\SQLEXPRESS",
    [string]$OutDir = (Join-Path $PSScriptRoot "results"),
    # Point this at another build (for example a checkout of an older commit) to compare servers.
    # The server runs with its own Server\ folder as the working directory because it reads mymap.txt from there.
    [string]$ServerExe = ""
)

$P95Limit = 25
$P99Limit = 120

$Root = Split-Path $PSScriptRoot -Parent
if (-not $ServerExe) { $ServerExe = Join-Path $Root "Server\x64\Release\Server.exe" }
$ServerDir = Split-Path (Split-Path (Split-Path $ServerExe -Parent) -Parent) -Parent
$StressExe = Join-Path $PSScriptRoot "STRESS_TEST\x64\Release\STRESS_TEST.exe"
$StressDir = Join-Path $PSScriptRoot "STRESS_TEST"
$ResetSql  = Join-Path $Root "DB\reset_seed.sql"
$Cores     = [Environment]::ProcessorCount

$cmd = Get-Command sqlcmd -ErrorAction SilentlyContinue
if ($cmd) {
    $SqlCmd = $cmd.Source
} else {
    $SqlCmd = Get-ChildItem "C:\Program Files\Microsoft SQL Server\Client SDK\ODBC\*\Tools\Binn\SQLCMD.EXE" -ErrorAction SilentlyContinue |
        Select-Object -First 1 -ExpandProperty FullName
}
foreach ($p in @($ServerExe, $StressExe, $ResetSql, $SqlCmd)) {
    if (-not $p -or -not (Test-Path $p)) { throw "missing: $p" }
}

New-Item -ItemType Directory -Force $OutDir | Out-Null
$stamp   = Get-Date -Format "yyyyMMdd_HHmmss"
$summary = Join-Path $OutDir "summary_$stamp.md"
"server: $ServerExe  hold: ${HoldSeconds}s  pass: p95 <= $P95Limit ms and p99 <= $P99Limit ms  date: $stamp" | Out-File $summary -Encoding ascii
"" | Out-File $summary -Append -Encoding ascii
"| N | p50 ms | p95 ms | p99 ms | max ms | samples | server CPU % (hold avg, $Cores logical cores) | server memory peak MB | 30 s after disconnect MB | pass |" | Out-File $summary -Append -Encoding ascii
"|---|---|---|---|---|---|---|---|---|---|" | Out-File $summary -Append -Encoding ascii

# A value the tool could not express as a number (">1000") counts as failing.
function ToMs([string]$text) {
    $n = 0
    if ([int]::TryParse($text, [ref]$n)) { return $n }
    return 9999
}

function Measure-Point([int]$N) {
    Write-Host "=== N=$N hold=${HoldSeconds}s ==="

    & $SqlCmd -S $SqlInstance -E -C -b -i $ResetSql | Out-Null
    if ($LASTEXITCODE -ne 0) { throw "seed reset failed" }

    $srvLog = Join-Path $OutDir "server_N${N}_$stamp.log"
    $srv = Start-Process -FilePath $ServerExe -WorkingDirectory $ServerDir `
        -RedirectStandardOutput $srvLog -RedirectStandardError ($srvLog + ".err") -PassThru
    $deadline = (Get-Date).AddSeconds(60)
    while (-not (Get-NetTCPConnection -LocalPort 4000 -State Listen -ErrorAction SilentlyContinue)) {
        if ((Get-Date) -gt $deadline -or $srv.HasExited) { throw "server did not start listening on port 4000" }
        Start-Sleep -Seconds 1
    }

    $stdin = Join-Path $OutDir "stdin_N$N.txt"
    Set-Content -Path $stdin -Value "$Server $N $HoldSeconds" -Encoding ascii
    $stressLog = Join-Path $OutDir "stress_N${N}_$stamp.log"
    $st = Start-Process -FilePath $StressExe -WorkingDirectory $StressDir -RedirectStandardInput $stdin `
        -RedirectStandardOutput $stressLog -RedirectStandardError ($stressLog + ".err") -PassThru

    # Sample the server every 5 s until the stress tool exits; it exits by itself when the hold ends.
    $samples = @()
    $csv = Join-Path $OutDir "server_samples_N${N}_$stamp.csv"
    "time,ws_MB,cpu_pct" | Out-File $csv -Encoding ascii
    $t0 = Get-Date
    $cpu0 = (Get-Process -Id $srv.Id).TotalProcessorTime.TotalSeconds
    while (-not $st.HasExited) {
        Start-Sleep -Seconds 5
        $p = Get-Process -Id $srv.Id -ErrorAction SilentlyContinue
        if (-not $p) { Write-Warning "server exited during N=$N"; break }
        $t1 = Get-Date
        $cpu1 = $p.TotalProcessorTime.TotalSeconds
        $pct = [math]::Round(100 * ($cpu1 - $cpu0) / (($t1 - $t0).TotalSeconds * $Cores), 1)
        $t0 = $t1
        $cpu0 = $cpu1
        $ws = [math]::Round($p.WorkingSet64 / 1MB, 1)
        $samples += [pscustomobject]@{ time = $t1; ws = $ws; cpu = $pct }
        "$($t1.ToString('HH:mm:ss')),$ws,$pct" | Out-File $csv -Append -Encoding ascii
    }

    $finalLine = Get-Content $stressLog -ErrorAction SilentlyContinue | Select-String '^\[final\]' | Select-Object -Last 1
    if ($finalLine) { $final = $finalLine.Line } else { Write-Warning "no [final] line for N=$N"; $final = "" }
    $f = @{}
    foreach ($kv in ($final -split ' ')) {
        if ($kv -match '^(\w+)=(.+)$') { $f[$Matches[1]] = $Matches[2] }
    }

    Start-Sleep -Seconds 30
    $after = Get-Process -Id $srv.Id -ErrorAction SilentlyContinue
    if ($after) { $afterWs = [math]::Round($after.WorkingSet64 / 1MB, 1) } else { $afterWs = "died" }
    Get-Process -Id $srv.Id -ErrorAction SilentlyContinue | Stop-Process -Force

    # CPU is averaged over the hold phase only: the tool exits right when the hold ends,
    # so the last HoldSeconds worth of samples is the hold.
    $holdCount = [math]::Max(1, [int]($HoldSeconds / 5))
    $holdSamples = $samples | Select-Object -Last $holdCount
    $cpuAvg = [math]::Round(($holdSamples | Measure-Object -Property cpu -Average).Average, 1)
    $wsPeak = ($samples | Measure-Object -Property ws -Maximum).Maximum

    $p95 = ToMs $f['p95']
    $p99 = ToMs $f['p99']
    $pass = ($final -ne "") -and ($p95 -le $P95Limit) -and ($p99 -le $P99Limit)
    $passText = if ($pass) { "yes" } else { "no" }

    $row = "| $N | $($f['p50']) | $($f['p95']) | $($f['p99']) | $($f['max']) | $($f['samples']) | $cpuAvg | $wsPeak | $afterWs | $passText |"
    Write-Host $final
    Write-Host $row
    $row | Out-File $summary -Append -Encoding ascii
    return $pass
}

# Coarse search: step up while passing, step down while failing, stop once a pass and a fail are one step apart.
$maxPass = $null
$minFail = $null
$N = $Start
while ($true) {
    $pass = Measure-Point $N
    if ($pass) {
        $maxPass = $N
        if ($N -ge $MaxClients) { break }
        if ($minFail -ne $null -and ($minFail - $N) -le $Step) { break }
        $N = [math]::Min($N + $Step, $MaxClients)
    } else {
        $minFail = $N
        if ($maxPass -ne $null -and ($N - $maxPass) -le $Step) { break }
        if ($N - $Step -lt $Step) { break }
        $N = $N - $Step
    }
}

# Refine between the last pass and the first fail down to the requested resolution.
while ($maxPass -ne $null -and $minFail -ne $null -and ($minFail - $maxPass) -gt $Resolution) {
    $mid = [int][math]::Floor((($maxPass + $minFail) / 2) / $Resolution) * $Resolution
    if ($mid -le $maxPass -or $mid -ge $minFail) { break }
    if (Measure-Point $mid) { $maxPass = $mid } else { $minFail = $mid }
}

"" | Out-File $summary -Append -Encoding ascii
if ($maxPass -eq $null) {
    $conclusion = "no measured N passed (lowest tried: $minFail)"
} elseif ($minFail -eq $null) {
    $conclusion = "largest passing N: $maxPass (search cap reached, nothing failed)"
} else {
    $conclusion = "largest passing N: $maxPass (first failing N: $minFail)"
}
$conclusion | Out-File $summary -Append -Encoding ascii
Write-Host "summary: $summary"
Get-Content $summary
