#!/bin/sh
ifconfig lo 127.0.0.1 up
mkdir -p /www
echo '<h1>hello from riscv</h1>' > /www/index.html
httpd -f -p 8080 -h /www
