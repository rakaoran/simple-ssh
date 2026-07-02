# Remote Shell

## Description

Remote Shell is a small client/server project written in C that I used to learn systems programming and apply what I learned about socket programming.

The server listens on a TCP port, and on connection, spawns an interactive shell through a PTY. The client connects to it and provides an interactive terminal session. Programs such as `bash`, `vim`, and `neovim` work through it and are fully responsive.

It uses a custom protocol built on top of TCP with internal I/O buffers, supports terminal window resize events, and uses `epoll` to manage multiple file descriptors in a single-threaded event loop.

## Warning

This is a plaintext remote shell, no encryption whatsoever. Do not expose it to the internet or run it on an untrusted network if you really wanna test it lol.

## What I learned

- Socket programming in a bit more depth
- Signals handling
- Pseudo-terminals
- Terminal raw mode
- How important it is to have a custom protocol, even if simple, that separates packets, and how to implement it.
- `epoll`, non-blocking I/O
- Serialization issues with Endianness

## Support

- OS: Linux
- Architectures: x86-64 and AArch64
- Endianness: little-endian and big-endian

Tested on x86-64 Linux natively and AArch64 Linux through `qemu-aarch64` user-mode emulation.

## Usage

Build:

```sh
make
```

### Server

Run:

```sh
bin/sshd <port>
```

### Client

Run:

```sh
bin/ssh <remote_server> <port>
```

The port argument is optional for both client and server. If omitted, it defaults to `10987`.

## Project status

Finished as a learning project.

Possible future improvements:

- Encryption
- Authentication
- Better error reporting
- More protocol tests
