# AES TCP Tunnel
A program to send encrypted TCP between tunnel endpoints.
Usage:
```
./aestun <encryption mode> <ip> <port> <ip to connect> <port to connect> [<encryption key>]
```
The 'ip' and 'port' are for clients to connect. The program instance connects to the 'ip to connect' and 'port to connect' which can be the other end of the tunnel for example.
If no key is provided to the aestun program, it will use a default key. The default key should be used for testing only.

The purpose of this program was to test the concept.
Therefore, the program may change a lot when developed further.

## Example run
The program can be used with netcat.
Netcat listener on port 54321:
```
nc -l 54321
```
AES tunnel endpoint on port 54322 decrypting TCP:
```
./aestun -d 127.0.0.1 54322 127.0.0.1 54321
```
AES tunnel endpoint on port 54323 encrypting TCP:
```
./aestun -e 127.0.0.1 54323 127.0.0.1 54322
```
Netcat sending TCP to the tunnel:
```
nc 127.0.0.1 54323
```
