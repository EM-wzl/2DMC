param([string]$f = 'snap2.txt')
$lines = Get-Content $f -Encoding UTF8
$bgStart = -1; $txStart = -1
for($i=0; $i -lt $lines.Count; $i++){
  if($lines[$i] -match '^=== BACKGROUND'){ $bgStart = $i + 1 }
  if($lines[$i] -match '^=== TEXT'){ $txStart = $i + 1; break }
}
if($bgStart -lt 0 -or $txStart -lt 0){ Write-Output "format error"; exit }
Write-Output ("file={0} bgRows={1}" -f $f, ($txStart - $bgStart - 1))
for($r=0; $r -lt ($txStart - $bgStart - 1); $r++){
  $bg = $lines[$bgStart + $r]
  $tx = $lines[$txStart + $r]
  if($null -eq $tx){ $tx = '' }
  $runs = @(); $start = -1
  $ncells = [Math]::Min(70, [int]($bg.Length / 2))
  for($c=0; $c -lt $ncells; $c++){
    $b = $bg[$c*2]
    $t0 = if($c*2 -lt $tx.Length){$tx[$c*2]}else{' '}
    $t1 = if($c*2+1 -lt $tx.Length){$tx[$c*2+1]}else{' '}
    $isVoid = ($b -eq '0') -and ($t0 -eq ' ') -and ($t1 -eq ' ')
    if($isVoid){ if($start -lt 0){ $start = $c } }
    else { if($start -ge 0){ $runs += ('{0}-{1}' -f $start,($c-1)); $start = -1 } }
  }
  if($start -ge 0){ $runs += ('{0}-{1}' -f $start,($ncells-1)) }
  if($runs.Count -gt 0){ Write-Output ('row {0,2} void: {1}' -f $r, ($runs -join ' ')) }
}
