#!/usr/bin/env python
# -*- coding: utf-8 -*-

import matplotlib.pyplot as plt

x = []
y = []

for line in open('dht_log.txt').readlines():
    sp = line.split(' ')
    x.append(int(sp[0]) - 1371328122)
    y.append(int(sp[1]))

plt.xlabel('elapsed time [s]')
plt.ylabel('#address')

plt.axis([0, 30000, 0, 8000000])

plt.plot(x, y)
plt.show()
