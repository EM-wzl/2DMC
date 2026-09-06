param([string]$f = 't.bin')
$fs = [System.IO.File]::OpenRead("$PWD\$f")
$br = New-Object System.IO.BinaryReader($fs)
$hdr = $br.ReadBytes(32)
$magic = [System.Text.Encoding]::ASCII.GetString($hdr[0..6])
$mo = [System.BitConverter]::ToInt64($hdr, 24)
Write-Output ("file={0} magic={1} metaOffset={2}" -f $f, $magic, $mo)
$fs.Position = $mo
$tagNames = @{1='DOUBLES';2='INTS';3='BEIBAO';4='WEAR';5='SHITI';6='SHITI2';7='BOX';8='BOIMES';9='BOIMES2';10='GAMEMODE';11='EXTRA'}
while($true){
  $tag = $br.ReadUInt16()
  $len = $br.ReadUInt32()
  if($tag -eq 0){ Write-Output "TAG_END reached"; break }
  $nm = if($tagNames.ContainsKey($tag)){$tagNames[$tag]}else{"tag$tag"}
  if($tag -eq 1){
    $p = $br.ReadBytes([int]$len)
    $x=[System.BitConverter]::ToDouble($p,0); $y=[System.BitConverter]::ToDouble($p,8)
    $life=[System.BitConverter]::ToDouble($p,16)
    Write-Output ("DOUBLES: x={0} y={1} life={2}" -f $x,$y,$life)
  } elseif($tag -eq 11){
    $p = $br.ReadBytes([int]$len)
    $n = [int]($len / 4)
    $ei = New-Object int[] 48
    for($i=0; $i -lt [Math]::Min($n,48); $i++){ $ei[$i] = [System.BitConverter]::ToInt32($p, $i*4) }
    Write-Output ("EXTRA: energy={0}/{1} coin={2} unlocked={3} curLevel={4} curRoom={5} waves={6}/{7} talentCnt={8} inDungeon={9} roomActive={10}" -f $ei[0],$ei[1],$ei[2],$ei[3],$ei[4],$ei[5],$ei[7],$ei[6],$ei[8],$ei[9],$ei[10])
    Write-Output ("EXTRA2: talents={0} portal=({1},{2}) lifeMax={3} armorMax={4} hujiazhi={5}" -f (($ei[11..34] | Where-Object {$_ -gt 0}) -join ','), $ei[35], $ei[36], $ei[37], $ei[38], $ei[39])
  } else {
    Write-Output ("{0}: len={1}" -f $nm, $len)
    $null = $br.ReadBytes([int]$len)
  }
}
$fs.Close()
