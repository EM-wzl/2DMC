$f = 'snap1.txt'
if($args.Count -ge 1){ $f = $args[0] }
$lines = Get-Content $f
for($r=0; $r -le 47; $r++){
  $s = $lines[2+$r]
  $runs = @()
  $start = -1
  for($c=0; $c -lt $s.Length; $c++){
    if($s[$c] -ne '0'){
      if($start -lt 0){ $start = $c }
    } else {
      if($start -ge 0){ $runs += ('{0}-{1}' -f $start,($c-1)); $start = -1 }
    }
  }
  if($start -ge 0){ $runs += ('{0}-{1}' -f $start,($s.Length-1)) }
  Write-Output ('row {0,2}: {1}' -f $r, ($runs -join ' '))
}
