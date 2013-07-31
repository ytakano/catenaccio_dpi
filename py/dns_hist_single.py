#!/usr/bin/env python
# -*- coding: utf-8 -*-

import pymongo
import numpy
from matplotlib import pyplot as plt

mongo = 'localhost:27017'
con   = pymongo.Connection(mongo)

db = con.DNSCrawl

max_n = 25
width = 1.0
data  = {}
x     = []
n     = 0
xmin  = 1
xmax  = 100000

plt.figure(figsize=(6, 6))

plt.xlim(xmin=xmin, xmax=xmax)
plt.ylim(ymin=0, ymax=max_n * width)
plt.xscale('log')
plt.xlabel('#address')

for i in db.ver_dist_nsd.find():
    if i['_id'] == None:
        continue
    data[i['_id']] = i['value']

for k, v in sorted(data.items(), key = lambda x: x[1], reverse = True):
    x.insert(0, k)
    plt.barh(max_n * width - (n + 1)* width, v, width,
             color = 'c', edgecolor = 'k')
    n += 1

    print k, v

    if n == max_n:
        break

plt.yticks(numpy.arange(len(x)) + width / 2, x)

plt.show()
