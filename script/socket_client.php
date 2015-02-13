<?php
error_reporting(E_ALL);
set_time_limit(0);
echo "<h2>tcp/ip  connection memwared</h2>\n";
$address = "127.0.0.1";
$port = 8021;

$socket = socket_create(AF_INET, SOCK_STREAM, SOL_TCP);

if ($socket === false){
	echo "socket_create() failed: reason: ".socket_strerror(socket_last_error())."\n";
}else {
	echo "OK. \n";
}

$result = socket_connect($socket, $address,$port);

if ($result === false){
	echo "socket_connect() failed: reason: ".socket_strerror(socket_last_error())."\n";
}else {
	echo "OK \n";
}

$in = "hello I'm memware client\n";
$in = array("hello","msgpack");
$in = msgpack_pack($in);
socket_write($socket, $in, strlen($in));

$data = '';
while(true){
	$out = @socket_read($socket, 1024, PHP_NORMAL_READ);
	if ($out === false){echo "stop;";break;}
	$data.=$out;
}
#echo $data;
$data = msgpack_unpack($data);
var_dump($data);
socket_close($socket);

?>
