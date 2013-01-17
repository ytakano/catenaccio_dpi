# catenaccio_dpi

Catenaccio DPI is a program for deep packet inspection.

### Dependencies

* [Boost C++ Library](http://www.boost.org/ "Boost")
* [libevent 2.0 or later](http://libevent.org/ "libevent")
* [libpcap](http://www.tcpdump.org/ "tcpdump/libpcap")
* [OpenSSL libcrypto](http://www.openssl.org/ "OpenSSL")

### How to Compile

    $ cmake CMakeLists.txt
    $ make

### How to run

If you want to use pcap for capturing packets, use -p option.
You can specify a network interface by -i option.

Example:

    $ ./cattenacio_dpi -p -i eth0

If you want to use divert socket (FreeBSD/MacOS X only), use -d option.
You can specify a port number of divert socket by -4 option.

Example:

    $ ./cattenacio_dpi -d -4 100