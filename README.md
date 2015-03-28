# memwared
memwared is a simple middle ware, support mongodb.
memwared是一个轻量级的数据库连接池，目前仅仅支持mongodb。

# library
- libevent-1.4.13-stable
- mongo-c-driver-1.1.0
- msgpack-c-cpp-0.6.0

＃ description
memwared 是一个单进程的系统，也就表明你可以再一台机器上开启多个实例，各个实例指定不同的端口。
内部工作采用多线程方式，主程序执行流放在主线程中，主线程中主要负责初始化，循环分发，创建子线程，建立和分发连接等。
子线程用于处理到来的连接的读写，及相应的触发事件。
使用libevent网络库处理io事件，利用msgpack协议作为通讯协议，mongo-c-driver应为其中已经封装了client_pool。

# Example
#### php client
[more code](https://github.com/rryqszq4/yaf-lib/blob/master/application/controllers/Socket.php) 
```php
<?php
	...
	$socket = System_Socket::create(AF_INET, SOCK_STREAM, SOL_TCP);
    $socket->connect('127.0.0.1',8021);
    $in = array(
        'gamedb',
        'entity_hs_card_zhcn',
        'find',
        array(array("cost"=>20))
    );
    $in = msgpack_pack($in);
    $socket->write($in, strlen($in));

    $out = '';
    $res = '';
    while (true){
        $out = $socket->read(1024, PHP_NORMAL_READ);
        if ($out === false){
            break;
        }
        $res .= $out;

    }

    DebugTools::print_r($res);
    ...
?>
```
