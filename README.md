# Cattenacio DPI

Catenaccio DPI is a program for deep packet inspection.

### Dependencies

Required:

* [Boost C++ Library](http://www.boost.org/ "Boost")
* [libpcap](http://www.tcpdump.org/ "tcpdump/libpcap")
* [OpenSSL libcrypto](http://www.openssl.org/ "OpenSSL")

Optional for MongoDB:

* [MongoDB](http://www.mongodb.org/ "MongoDB")
* [PyMongo](https://github.com/mongodb/mongo-python-driver "pyMongo")
* [libunbound](http://unbound.net/ "Unbound")
* [libevent 2.0 or later](http://libevent.org/ "libevent")

Optional for Divert Socket:

* [libevent 2.0 or later](http://libevent.org/ "libevent")

### How to Compile

    $ cmake -DCMAKE_BUILD_TYPE=Release CMakeLists.txt
    $ make

If you want to compile as debug mode, set a option of CMAKE_BUILD_YPE=Debug when running cmake. Debug mode passes a option of -g and a definition of DEBUG=1 to the compiler.

    $ cmake -DCMAKE_BUILD_TYPE=Debug CMakeLists.txt
    $ make

If you want to enable divert socket for packet capture, set a option of USE_DIVERT=1.

    $ cmake -DUSE_DIVERT=1 -DCMAKE_BUILD_TYPE=Release CMakeLists.txt
    $ make

If you want to build with mongoDB, set a option of USE_MONGO=1.

    $ cmake -DUSE_MONGO=1 -DCMAKE_BUILD_TYPE=Release CMakeLists.txt
    $ make

If you want to build static library, set a option of COMPILE_STATIC_LIB=1.

    $ cmake -DCOMPILE_STATIC_LIB=1 CMakeLists.txt
    $ make

You can use a verbose mode when compiling.

    $ make VERBOSE=1

### How to run Cattenacio DPI, which outputs to standard output

You can specify a network interface by -i option.

Example:

    $ ./src/cattenacio_dpi -i eth0

If you want to use divert socket (FreeBSD/MacOS X only) instead of pcap, use -d option.
You can specify a port number of divert socket by -4 option.

Example:

    $ ./src/cattenacio_dpi -d -4 100

### How to run with MongoDB

Before running, please start up MongoDB server. After satrting up MongoDB, run cdpi_mongo like this.

    $ ./src/cdpi_mongo -i eth0

You can specify IP address and port number of MongoDB by -m option. If you don't pass -m option, it connects to localhost:27017 by default.

    $ ./src/cdpi_mongo -i eth0 -m localhost:27017

### Example Usage of MongoDB (MindYourPrivacy)

Then, run js/mongo.sh for statistics.

    $ ./js/mongo.sh

You can specify MongoDB's address like this.

    $ ./js/mongo.sh localhost:27017

Then, run ./src/cdpi_mongo_soa to get SOA records of DNS

    $ ./src/cdpi_mongo_soa

Next, run py/http_stats.py for visualization like this. py/http_stats.py requires MongoDB's driver for python. -o option specifies output directory.

    $ ./py/http_stats.py -o output_direcotry

Finally, you can see HTTP statistics by opening http_stats.html with your web browser.

    $ ls output_directory/http_stats.html
    output_directory/http_stats.html

js/mongo.sh and py/http_stats.py should be run periodically for statistics. cron can help you to periodically update information.
