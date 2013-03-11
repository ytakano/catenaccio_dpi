# catenaccio_dpi

Catenaccio DPI is a program for deep packet inspection.

### Dependencies

Required:

* [Boost C++ Library](http://www.boost.org/ "Boost")
* [libpcap](http://www.tcpdump.org/ "tcpdump/libpcap")
* [OpenSSL libcrypto](http://www.openssl.org/ "OpenSSL")

Optional:
* [libevent 2.0 or later (If you use divert socket)](http://libevent.org/ "libevent")
* [mongoDB's Client Driver for C++ (If you use mongoDB)](http://www.mongodb.org/ "mongoDB")

### How to Compile

    $ cmake -DCMAKE_BUILD_TYPE=Release CMakeLists.txt
    $ make

If you want to compile as debug mode, set a option of CMAKE_BUILD_YPE=Debug when running cmake. Debug mode passes a option of -g and a definition of DEBUG=1 to the compiler.

    $ cmake -DCMAKE_BUILD_TYPE=Debug CMakeLists.txt
    $ make

If you want to build with mongoDB, set a option of USE_MONGO.

    $ cmake -DUSE_MONGO=1 CMAKE_BUILD_TYPE=Release CMakeLists.txt
    $ make

You can use a verbose mode when compiling.

    $ make VERBOSE=1

### How to run

If you want to use pcap for capturing packets, use -p option.
You can specify a network interface by -i option.

Example:

    $ ./cattenacio_dpi -p -i eth0

If you want to use divert socket (FreeBSD/MacOS X only), use -d option.
You can specify a port number of divert socket by -4 option.

Example:

    $ ./cattenacio_dpi -d -4 100