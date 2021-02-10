# KCOV-remote example

There is an easy example for demonstrating the usage of remote kcov. 

In this kernel module, I intentionally add a workqueue to leave the job for background thread. You can how to interact with this module using the `interact` binary.

The following commands can help you do this

```
mount -t debugfs none /sys/kernel/debug # we mount debug filesystem

insmod remote_lkm.ko # we install this misc device driver

./interact <str> # input a string
```

You will get a series of address after that

```
0xffffffffc000010e
0xffffffff81dfccd1
0xffffffff81dfcd1c
0xffffffff81dfcd31
0xffffffff81dfcd50
0xffffffff81dfcd64
0xffffffffc0000121
```

And you can use `addr2line` to get the relevant code.
