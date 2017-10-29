# nweb C
This is a poor man's C web server inspired by IBM's nweb.

Currently responds only to GET requests. Works with TCP connections and even browsers. Usage: `server [port] [directory]`

Build and run:
```bash
$ gcc nweb.c -o nweb
$ nweb 90 ./serveme
```
