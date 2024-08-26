# Servers and Clients
There are 5 c files which implents servers and clients on TCP and UDP protocols.
## Message fromat
Messages in txt file looks like this
```
phone_num1 phone_num2 hh:mm:ss message
```
for example:
```
+79991234567 +79997654321 02:25:58 message1
```
BUT NOTICE that servers and clients communication includes number of certain message like service information which isn't printed out

## Clients
### tcpclient and udpclient
run command (127.0.0.1 ip for localhost)
```
./client ip:port file_with_messages.txt
```
Sends to server messages from file
### tcpclient2
run command 
```
./tcpclient2 ip:port get file_to_write_messages.txt
```
Gets messages from server
## Servers
### TCP server
run command
```
./tcpserver port
```
Save messages from clients on port or send in tcpclient2 case
### UDP server
run command
```
./tcpserver port1 port2
```
Listen for clients in port range [port1; port2]
