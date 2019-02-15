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
$ make test
ls_test:        PASS
resource_test:  PASS
affinity_test:  PASS
thread_test:    PASS
fork_test:      PASS
withdraw_test:  PASS
inode_test:     PASS
daemon_test:    PASS
```

## Cleanup

```sh
$ make clean
```

Happy Hackin!
