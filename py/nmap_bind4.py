#!/usr/bin/env python
# -*- coding: utf-8 -*-

import pymongo
import subprocess
import re

bind4_re = re.compile('^4\.[0-9]\.[0-9]')

mongo = 'localhost:27017'
con   = pymongo.Connection(mongo)
db    = con.DNSCrawl

addr = []

for i in db.servers_bind4.find():
    if not 'ver' in i:
        continue

    if bind4_re.match(i['ver']) != None:
	addr.append(i['_id'])

for i in addr:
    print i

    cmd = 'nmap -O --osscan-limit -PN -n %s > nmap_log/%s.log' % (i, i)

    print cmd

    subprocess.call(cmd, shell=True)
