param([string]$f = 'snap2.txt')
$lines = Get-Content $f -Encoding UTF8
$txStart = -1
for($i=0; $i -lt $lines.Count; $i++){
  if($lines[$i] -match '^=== TEXT'){ $txStart = $i + 1; break }
}
if($txStart -lt 0){ Write-Output 'no TEXT section'; exit }
for($r=0; $r -lt 50; $r++){
  if($txStart + $r -ge $lines.Count){ break }
  $l = $lines[$txStart + $r]
  if($null -eq $l){ continue }
  $s = ''
  for($c=0; $c -lt [Math]::Min(140, $l.Length); $c++){
    $ch = $l[$c]
    if($ch -ne ' '){ $s += $ch } else { $s += ' ' }
  }
  if($s.Trim().Length -gt 0){ Write-Output ('r{0,2}: {1}' -f $r, $s.TrimEnd()) }
}
