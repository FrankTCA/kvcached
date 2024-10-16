# To-do List
These are the things that need to be done before depolyment.

## Before Deployment
- [x] Kvcached program
- [x] Protocol
- [x] Client program and headers
- [ ] Kvcachectl program starts kvcached programs
- [ ] Kvcachectl program stops kvcached programs
- [x] Protocol for test ping
- [ ] Kvcachectl client functionality
- [ ] Kvcached deletes data on timer
- [ ] client.c utilizes client.h (keep client.c for shell programming?)
- [ ] PHP Interface Library

## Bugs
- [ ] kvcached seems to stop when pinged - needs protocol
- [x] bus error following creation of new server (is it when sending port and PID to tracking daemon?)
- [x] kvcached server breaks when nonexistant key is attempted to be retrieved - needs to return error msg
- [ ] kvcachectl seems to have trouble doing more than one or two kvcached servers at a time, "bind(3) error: address already in use"
- [x] Delete command does not work, results in crash.
- [ ] kvcachectl hangs when second server is started. (recv(3) error: connection reset by peer)
