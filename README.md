# Linux System Programming

[![CircleCI]](https://circleci.com/gh/keinohguchi/workflows/lsp)

[Robert Love]'s wonderful [Linux System Programming] in Action!

[Robert Love]: https://rlove.org/
[CircleCI]: https://circleci.com/gh/keinohguchi/lsp.svg?style=svg
[Linux System Programming]: http://shop.oreilly.com/product/0636920026891.do

## Build

```sh
$ make
```

## Test

```sh
ocean$ make test
select_test:    PASS
resource_test:  PASS
affinity_test:  PASS
ls_test:        PASS
access_test:    PASS
fork_test:      PASS
httpd_test:     PASS
server_test:    PASS
time_test:      PASS
thread_test:    PASS
id_test:        PASS
sh_test:        PASS
withdraw_test:  PASS
netlink_test:   PASS
poll_test:      PASS
inode_test:     PASS
daemon_test:    PASS
find_test:      PASS
epoll_test:     PASS
```

## Cleanup

```sh
$ make clean
```

Happy Hackin!
