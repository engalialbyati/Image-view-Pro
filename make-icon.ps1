Add-Type -AssemblyName System.Drawing

function New-Logo([int]$size) {
    $bmp = New-Object System.Drawing.Bitmap $size, $size
    $g = [System.Drawing.Graphics]::FromImage($bmp)
    $g.SmoothingMode = [System.Drawing.Drawing2D.SmoothingMode]::AntiAlias
    $g.PixelOffsetMode = [System.Drawing.Drawing2D.PixelOffsetMode]::HighQuality
    $g.InterpolationMode = [System.Drawing.Drawing2D.InterpolationMode]::HighQualityBicubic
    $g.Clear([System.Drawing.Color]::Transparent)

    $rect = New-Object System.Drawing.Rectangle 0, 0, $size, $size
    $r = [int]($size * 0.22)
    $gp = New-Object System.Drawing.Drawing2D.GraphicsPath
    $gp.AddArc($rect.X, $rect.Y, $r * 2, $r * 2, 180, 90)
    $gp.AddArc($rect.Right - $r * 2, $rect.Y, $r * 2, $r * 2, 270, 90)
    $gp.AddArc($rect.Right - $r * 2, $rect.Bottom - $r * 2, $r * 2, $r * 2, 0, 90)
    $gp.AddArc($rect.X, $rect.Bottom - $r * 2, $r * 2, $r * 2, 90, 90)
    $gp.CloseFigure()
    $grad = New-Object System.Drawing.Drawing2D.LinearGradientBrush($rect, [System.Drawing.Color]::FromArgb(255, 90, 146, 255), [System.Drawing.Color]::FromArgb(255, 168, 96, 255), 45.0)
    $g.FillPath($grad, $gp)

    $m = [int]($size * 0.27)
    $fw = $size - 2 * $m
    $fr = New-Object System.Drawing.Rectangle $m, $m, $fw, $fw
    $frR = [int]($size * 0.09)
    $fp = New-Object System.Drawing.Drawing2D.GraphicsPath
    $fp.AddArc($fr.X, $fr.Y, $frR * 2, $frR * 2, 180, 90)
    $fp.AddArc($fr.Right - $frR * 2, $fr.Y, $frR * 2, $frR * 2, 270, 90)
    $fp.AddArc($fr.Right - $frR * 2, $fr.Bottom - $frR * 2, $frR * 2, $frR * 2, 0, 90)
    $fp.AddArc($fr.X, $fr.Bottom - $frR * 2, $frR * 2, $frR * 2, 90, 90)
    $fp.CloseFigure()
    $g.FillPath((New-Object System.Drawing.SolidBrush ([System.Drawing.Color]::White)), $fp)

    $accent = New-Object System.Drawing.SolidBrush ([System.Drawing.Color]::FromArgb(255, 90, 146, 255))
    $sunR = [int]($size * 0.075)
    $g.FillEllipse($accent, [int]($size * 0.58), [int]($size * 0.36), $sunR * 2, $sunR * 2)

    $tri = New-Object System.Drawing.Drawing2D.GraphicsPath
    $tri.AddLine([int]($size * 0.34), [int]($size * 0.66), [int]($size * 0.50), [int]($size * 0.44))
    $tri.AddLine([int]($size * 0.50), [int]($size * 0.44), [int]($size * 0.63), [int]($size * 0.66))
    $tri.AddLine([int]($size * 0.63), [int]($size * 0.66), [int]($size * 0.34), [int]($size * 0.66))
    $tri.CloseFigure()
    $g.FillPath($accent, $tri)
    $tri2 = New-Object System.Drawing.Drawing2D.GraphicsPath
    $tri2.AddLine([int]($size * 0.55), [int]($size * 0.66), [int]($size * 0.66), [int]($size * 0.50))
    $tri2.AddLine([int]($size * 0.66), [int]($size * 0.50), [int]($size * 0.72), [int]($size * 0.66))
    $tri2.AddLine([int]($size * 0.72), [int]($size * 0.66), [int]($size * 0.55), [int]($size * 0.66))
    $tri2.CloseFigure()
    $g.FillPath($accent, $tri2)

    $g.Dispose()
    return $bmp
}

$root = Split-Path -Parent $MyInvocation.MyCommand.Path
if (-not $root) { $root = (Get-Location).Path }
$sizes = 256, 128, 64, 48, 32, 16
$pngs = @()
foreach ($s in $sizes) {
    $b = New-Logo $s
    $ms = New-Object System.IO.MemoryStream
    $b.Save($ms, [System.Drawing.Imaging.ImageFormat]::Png)
    $pngs += , $ms.ToArray()
    $b.Dispose()
}

$out = New-Object System.IO.MemoryStream
$bw = New-Object System.IO.BinaryWriter $out
$bw.Write([uint16]0)
$bw.Write([uint16]1)
$bw.Write([uint16]$sizes.Count)
$offset = 6 + 16 * $sizes.Count
for ($i = 0; $i -lt $sizes.Count; $i++) {
    $s = $sizes[$i]; $bytes = $pngs[$i]
    $dim = if ($s -eq 256) { 0 } else { $s }
    $bw.Write([byte]$dim); $bw.Write([byte]$dim)
    $bw.Write([byte]0); $bw.Write([byte]0)
    $bw.Write([uint16]1); $bw.Write([uint16]32)
    $bw.Write([uint32]$bytes.Length); $bw.Write([uint32]$offset)
    $offset += $bytes.Length
}
foreach ($bytes in $pngs) { $bw.Write($bytes) }
[System.IO.File]::WriteAllBytes((Join-Path $root 'app.ico'), $out.ToArray())
Write-Output ("app.ico written ({0} KB, {1} sizes)" -f [math]::Round((Get-Item (Join-Path $root 'app.ico')).Length / 1KB, 1), $sizes.Count)
