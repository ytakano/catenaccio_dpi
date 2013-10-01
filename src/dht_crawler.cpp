#include "cdpi_bencode.hpp"

#include <unistd.h>

#include <mongo/client/dbclient.h>

#include <iostream>
#include <sstream>
#include <string>

#include <arpa/inet.h>

#include <sys/types.h>
#include <sys/socket.h>

#include <time.h>

using namespace std;

string mongo_server("localhost:27017");
mongo::DBClientConnection mongo_conn;
mongo::Date_t             start_date;
const char *my_id = "01234567890123456789";
int sockfd;
int my_port = 21000;
time_t rm_time = time(NULL);
const time_t rm_period = 3600;

void
get_epoch_millis(mongo::Date_t &date)
{
    struct timeval tv;

    gettimeofday(&tv, NULL);

    date.millis = ((unsigned long long)(tv.tv_sec) * 1000 +
                   (unsigned long long)(tv.tv_usec) / 1000);
}

char
hex2char(char c)
{
    if ('0' <= c && c <= '9') {
        return c - '0';
    } else if ('a' <= c && c <= 'f') {
        return c - 'a' + 10;
    }

    return -1;
}

void
id2bin(string id, char buf[])
{
    int i;

    for (i = 0; i < 20; i += 2) {
        char c1, c2;
        c1 = hex2char(id[i]);
        c2 = hex2char(id[i + 1]);

        buf[i / 2] = (c1 << 4) | c2;
    }
}

void
gen_bencode(cdpi_bencode &ben, const char id[], const char target[])
{
    cdpi_bencode::ptr_ben_dict p_dic1(new cdpi_bencode::bencode_dict);
    cdpi_bencode::ptr_ben_dict p_dic2(new cdpi_bencode::bencode_dict);

    p_dic1->add_str("id", id, 20);
    p_dic1->add_str("target", target, 20);

    p_dic2->add_str("t", "ab", 2);
    p_dic2->add_str("y", "q", 1);
    p_dic2->add_str("q", "find_node", 9);
    p_dic2->add_data("a", p_dic1);

    ben.set_data(p_dic2);
}

void
init()
{
    string errmsg;
    sockaddr_in saddr;

    if (! mongo_conn.connect(mongo_server, errmsg)) {
        cerr << errmsg << endl;
        exit(-1);
    }

    sockfd = socket(AF_INET, SOCK_DGRAM, 0);

    if (sockfd < 0) {
        perror("socket");
        exit(-1);
    }
 
    memset(&saddr, 0, sizeof(saddr));
    saddr.sin_family = AF_INET;
    saddr.sin_port   = htons(my_port);

    bind(sockfd, (sockaddr*)&saddr, sizeof(saddr));

    mongo_conn.update("DHT.nodes", mongo::fromjson("{}"),
                      BSON("q" << BSON("$unset" << "")), false, true);
    mongo_conn.ensureIndex("DHT.nodes",
                           mongo::fromjson("{q: 1}"));

    get_epoch_millis(start_date);
}

void
query()
{
    sockaddr_in saddr;

    init();

    memset(&saddr, 0, sizeof(saddr));
    saddr.sin_family = AF_INET;

    for (;;) {
        auto_ptr<mongo::DBClientCursor> mongo_cursor;
        mongo_cursor = mongo_conn.query("DHT.nodes",
                                        BSON("q" << BSON("$exists" << false)));

        int i = 0;

        while (mongo_cursor->more()) {
            ostringstream ben_ost;
            cdpi_bencode ben;
            mongo::BSONObj p = mongo_cursor->next();
            string id   = p.getStringField("_id");
            string ip   = p.getStringField("ip");
            int    port = p.getIntField("port");
            char   id_bin[20];

            if (id.size() != 40) {
                continue;
            }

            id2bin(id, id_bin);

            gen_bencode(ben, my_id, id_bin);
            ben.encode(ben_ost);

            saddr.sin_port   = htons(port);
            inet_pton(AF_INET, ip.c_str(), &saddr.sin_addr);

            string  ben_str = ben_ost.str();
            sendto(sockfd, ben_str.c_str(), ben_str.size(), 0,
                   (sockaddr*)&saddr, sizeof(saddr));

            mongo_conn.update("DHT.nodes", BSON("_id" << id),
                              BSON("$set" << BSON("q" << true)));

            i++;
        }

        cout << i << endl;

        sockaddr_in saddr;
        socklen_t   slen = sizeof(saddr);
        ssize_t     readlen;
        char        buf[2048];

        for (;;) {
            readlen = recvfrom(sockfd, buf, sizeof(buf), MSG_DONTWAIT,
                               (sockaddr*)&saddr, &slen);

            if (readlen < 0)
                break;
        }

        sleep(5);
        time_t now = time(NULL);

        if (now - rm_time > rm_period) {
            mongo::Date_t an_hour_ago;
            get_epoch_millis(an_hour_ago);

            an_hour_ago.millis -= rm_period * 1000;

            mongo_conn.remove("DHT.nodes",
                              BSON("date" << BSON("$lt" << an_hour_ago)));

            rm_time = now;
        }
    }
}

void
print_usage(char *cmd)
{
    cout << "usage: " << cmd << " -m localhost:27017"
         << "\n\t -m: an address of MongoDB. localhost:27017 is a default value"
         << "\n\t -h: show this help"
         << endl;
}

int
main(int argc, char *argv[])
{
    int opt;
    const char *optstr = "m:h";

    while ((opt = getopt(argc, argv, optstr)) != -1) {
        switch (opt) {
        case 'm':
            mongo_server = optarg;
            break;
        case 'h':
        default:
            print_usage(argv[0]);
            return 0;
        }
    }

    query();

    return 0;
}
