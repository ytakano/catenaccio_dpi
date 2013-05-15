#!/usr/bin/env python
# -*- coding: utf-8 -*-

import pymongo
import numpy
from matplotlib import pyplot as plt

mongo = 'localhost:27017'
con   = pymongo.Connection(mongo)

db = con.DNSCrawl

max_n = 50
width = 1.0
data  = {}
x     = []
n     = 0
xmin  = 100
xmax  = 100000

plt.figure(figsize=(12, 12))

plt.axes([0.12, 0.1, 0.35, 0.8])

plt.xlim(xmin=xmin, xmax=xmax)
plt.xscale('log')
plt.xlabel('#address')

for i in db.type_hist_bind9.find():
    data[i['_id']] = i['value']

for k, v in sorted(data.items(), key = lambda x: x[1], reverse = True):
    x.insert(0, k)

    if n >= max_n:
        n2 = n - max_n
    else:
        n2 = n

    plt.barh(max_n * width - (n2 + 1)* width, v, width,
             color = 'c', edgecolor = 'k')
    n += 1

    print k, v

    if n == max_n:
        plt.yticks(numpy.arange(len(x)) + width / 2, x)
        #break

        plt.axes([0.6, 0.1, 0.35, 0.8])
        plt.xlim(xmin=xmin, xmax=xmax)
        plt.xscale('log')
        plt.xlabel('#address')

        x = []

    if n >= max_n * 2:
        plt.yticks(numpy.arange(len(x)) + width / 2, x)
        break

plt.show()
