<?php
    header('Content-Type: image/jpeg');
    $width = "";
    if (isset($_GET['width'])) {
        $width = " -w ".$_GET['width'];
    }
    $height = "";
    if (isset($_GET['height'])) {
        $height = " -h ".$_GET['height'];
    }
    $lat = "";
    if (isset($_GET['lat'])) {
        $lat = " -lat ".$_GET['lat'];
    }
    $lon = "";
    if (isset($_GET['lon'])) {
        $lon = " -lon ".$_GET['lon'];
    }
    $zoom = "";
    if (isset($_GET['zoom'])) {
        $zoom = " -z ".$_GET['zoom'];
    }
    $tilt = "";
    if (isset($_GET['tilt'])) {
        $tilt = " -t ".$_GET['tilt'];
    }
    $rot = "";
    if (isset($_GET['rot'])) {
        $rot = " -r ".$_GET['rot'];
    }
    
    $command = "./build/bin/./tangramPaparazzi -o out.jpg".$width.$height.$lat.$lon.$zoom.$tilt.$rot;
    $last_line = system($command);
    readfile('out.jpg');
?>
<!doctype>
