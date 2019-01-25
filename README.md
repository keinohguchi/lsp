# LSP; Linux System Programming

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
inode_test:     N/A
block_test:     N/A
wait_test:      N/A
system_test:    N/A
daemon_test:    PASS
affinity_test:  PASS
resource_test:  PASS
thread_test:    N/A
withdraw_test:  PASS
xattr_test:     N/A
mstat_test:     N/A
clocks_test:    N/A
```

## Cleanup

```sh
$ make clean
```

Happy Hackin!
