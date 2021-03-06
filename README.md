[![Build Status](https://travis-ci.org/codepr/sol.svg?branch=master)](https://travis-ci.org/codepr/sol)

Sol
===

Oversimplified MQTT broker written from scratch, which mimic mosquitto
features. Implemented to learning how the protocol works, it supports
almost all MQTT v3.1.1 commands on linux platform and relies on EPOLL interface
for multiplexing I/O. The basic development process is documented in this
[series of posts](https://codepr.github.io/posts/sol-mqtt-broker), referring to
the [tutorial branch](https://github.com/codepr/sol/tree/tutorial); the master
branch is the most up-to-date and tested.
**Not for production use**.

### Features

It's still a work in progress but it already handles the most of the basic
features expected from an MQTT broker. It does not leak memory as of now,
there's probably some corner cases, not deeply investigated yet.

- Configuration file on disk
- Support for QoS messages 0, 1 and 2
- Retained messages per topic
- Session present check and handling
- Periodic stats publishing
- Support multiple topics subscriptions through wildcard (#) and (+) for single
  level wildcard e.g. foo/+/bar/#
- Authentication through username and password
- SSL/TLS connections, configuration accepts minimum protocols to be used
- Logging on disk
- Daemon mode
- Multiplexing IO with abstraction over backend, currently supports
  select/poll/epoll choosing the better implementation.
- Multithread load-balancing on connections for high concurrent performance

### To be implemented

- Last will & Testament, already started
- Persistence on disk for inflight messages on disconnected clients
- Check on max memory used

### Maybe

- MQTT 5.0 support
- Porting for BSD/MAC osx
- Scaling (feeling brave?)

## Build

```sh
$ cmake .
$ make
```

## Quickstart play

The broker can be tested using `mosquitto_sub` and `mosquitto_pub` or with
`paho-mqtt` python driver.

To run the broker with DEBUG logging on:

```sh
$ ./sol -v
```

A simple configuration can be passed in with `-c` flag:

```sh
$ ./sol -c path/to/sol.conf
```

As of now the configuration is very small and it's possible to set some of the
most common parameters, default path is located to `/etc/sol/sol.conf`:

```sh
# Sol configuration file, uncomment and edit desired configuration

# Network configuration

# Comment ip_address and ip_port, set unix_socket and
# UNIX family socket will be used instead

ip_address 127.0.0.1
ip_port 8883

# unix_socket /tmp/sol.sock

# Logging configuration

# Could be either DEBUG, INFO/INFORMATION, WARNING, ERROR
log_level DEBUG

log_path /tmp/sol.log

# Max memory to be used, after which the system starts to reclaim memory by
# freeing older items stored
max_memory 2GB

# Max memory that will be allocated for each request
max_request_size 2MB

# TCP backlog, size of the complete connection queue
tcp_backlog 128

# Interval of time between one stats publish on $SOL topics and the subsequent
stats_publish_interval 10s

# TLS certs paths, cafile act as a flag as well to set TLS/SSL ON
# cafile /etc/sol/certs/ca.crt
# certfile /etc/sol/certs/cert.crt
# keyfile /etc/sol/certs/cert.key

# Authentication
# allow_anonymous false
# password_file /etc/sol/passwd

# TLS protocols, supported versions should be listed comma separated
# example:
# tls_protocols tlsv1_2,tlsv1_3
```

If `allow_anonymous` is false, a password file have to be specified. The
password file follows the standard format of username:password line by line.
To generate one just add all entries needed in a file and run passwd.py after
it:

```sh
$ cat sample_passwd_file
user1:pass1
user2:pass2
$ python passwd.py sample_passwd_file
$ cat sample_passwd_file
user1:$6$69qVAELLWuKXWQPQ$oO7lP/hNS4WPABTyK4nkJs4bcRLYFi365YX13cEc/QBJtQgqf2d5rOIUdqoUin.YVGXC3OXY9MSz7Z66ZDkCW/
user2:$6$vtHdafhGhxpXwgBa$Y3Etz8koC1YPSYhXpTnhz.2vJTZvCUGk3xUdjyLr9z9XgE8asNwfYDRLIKN4Apz48KKwKz0YntjHsPRiE6r3g/
```

## Concurrency

The broker provides an access through a simple IO multiplexing event-loop based
TCP server, the model is designed to be self-contained and thus easy to spread
on multiple threads. Another approach and probably more elegant would be to
share a single event loop to multiple threads, at the cost of higher complexity
and race-conditions to be handled.

```
                            THREADS 1..N
                            [EVENT-LOOP]

    ACCEPT_CALLBACK         READ_CALLBACK         WRITE_CALLBACK
  -------------------    ------------------    --------------------
          |                      |                       |
       ACCEPT                    |                       |
          | -------------------> |                       |
          |                READ AND DECODE               |
          |                      |                       |
          |                      |                       |
          |                   PROCESS                    |
          |                      |                       |
          |                      |                       |
          |                      | --------------------> |
          |                      |                     WRITE
       ACCEPT                    |                       |
          | -------------------> | <-------------------- |
          |                      |                       |

```

However, the current model gives some advantages against the single-thread
implementation, as each thread can handle a share of the total number of
connected client effectively increasing the throughput of the broker, the
main drawback is that in worst cases a scenario that can originate is where some
threads are serving heavy-load clients and some others are esentially idling
without partecipating or helping.

## Benchmarks

I've run some basic benchmarks comparing Sol and Mosquitto's performance, using
[mqtt-benchmark](https://github.com/krylovsk/mqtt-benchmark) tool to test
concurrent connections and different loads of traffic on my laptop, an
Intel(R) I5-8265U (8) @ 3.900 GHz CPUs and 8 GB RAM.

To be minded that currently Sol skips some checks like UTF-8 strings for topics
and the memory footprint is also much greater than Mosquitto's one; moreover
this benchmarks doesn't take into account real publish on multiple subscribers,
just the publish traffic load on multiple clients with different QoS levels.

![MQTT mosquitto vs sol comparison](MQTTcomparison.png)

All tests are run pointing to localhost:1883, apparently, out of QoS 0 tests
which are the least interesting as there's no real "handshakes" or overhead on
the communication between the peers, QoS 1 and even more QoS 2 is where Sol
seems to shine, highlighting a 95% increased throughtput over Mosquitto on the
QoS 2 1000 clients benchmark.
Of course these benchmarks are of little relevance and Mosquitto is compiled
without any optimizations, just an out of the box version binary shipped by
the OS repository, version 1.6.8, but in terms of sheer concurrency Sol does
pretty good.

## Contributing

Pull requests are welcome, just create an issue and fork it.
