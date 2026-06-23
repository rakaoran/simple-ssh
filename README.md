# Simple SSH

## Description

This is my first project in my journey of learning systems programming, writing a barebone C program that works on different types of machines (such as Big and Little Endian bytes order) without extra dependencies. It's a simple ssh client and server, the server listens on a port, then the client connects to it and opens bash to execute commands. It is fully interactive and you can open even text editors at the host, such as `neovim`.

It is fully responsive, and reacts to changes in window size.

Both client and server are single threaded, and manage multiple I/O using `epoll`.

## Support

- OS: linux
- Arch: x86 and ARM (Tested on x86-64 Linux natively and AArch64 Linux via qemu-aarch64 user-mode emulation.)
- Endianness: Big and Little Endian

## Usage

### Server

Compile with `make bin/sshd`
Run with `bin/sshd <port>`

### Client

Compile with `make bin/ssh`
Run with `bin/ssh <remote_server> <port>`

> In both client and server the port is optional, defaulting to 10987 if unspecified.

## To be implemented

1. End to end encryption
2. Having IO buffers per connection to handle clients with terrible connection or just network inconsistencies in general.
