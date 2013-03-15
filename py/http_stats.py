#!/usr/bin/env python
# -*- coding: utf-8 -*-

import pymongo
import json
import getopt, sys
import os

try:
    import matplotlib.pyplot as plt
    is_plt = True
except ImportError:
    is_plt = False

try:
    import pydot
    is_dot = True
except ImportError:
    is_dot = False


html = ''
odir = './'
mongo = 'localhost:27017'
ref_graph = {}
out_graph = {}

def get_graph(db):
    global ref_graph
    global out_graph

    for i in db.graph_trunc_host.find():
        dst  = i['_id']
        srcs = i['value'].keys()

        ref_graph[dst] = srcs

        for src in srcs:
            if src in out_graph:
                out_graph[src].append(dst)
            else:
                out_graph[src] = [dst]

def top_n_uri(graph, num):
    total = 0
    ret   = []

    return [(k, v) for k, v in sorted(graph.items(), key = lambda x:len(x[1]),
                                      reverse = True)[0:num]]

def print_top_50(db):
    global html
    global ref_graph
    global out_graph

    best_ref = top_n_uri(ref_graph, 50)
    best_out = top_n_uri(out_graph, 50)

    max_ref = len(best_ref[0][1])
    max_out = len(best_out[0][1])

    best_ref.sort()
    best_out.sort()

    html += """
<script language="JavaScript">
<!--

function set_hosts(id, hosts) {
   var div = document.getElementById(id);

    while (div.firstChild) {
        div.removeChild(div.firstChild);
    }

    for (var i = 0; i < hosts.length; i++) {
        var text = document.createTextNode(hosts[i] + "\u00A0\u00A0 ");
        var span = document.createElement('span');

        span.appendChild(text)
        div.appendChild(span);
    }
}

function show_stats(uri, hosts, trds) {
    var div  = document.getElementById("host");
    var text = document.createTextNode(uri);

    while (div.firstChild) {
        div.removeChild(div.firstChild);
    }

    div.appendChild(text);

    set_hosts("referred", hosts);
    set_hosts("truncated", trds);
}
-->
</script>
"""

    html += '<div class="top50">'
    html += '<h2>Top 50 most referred domains</h2>'
    html += '<div class="content">'

    html += '<div>'

    for dst, src in best_ref:
        w = 150 - int(150.0 * len(src) / max_ref)

        trd_hosts = []

        for trd in db.trunc_hosts.find({"value": dst}):
            trd_hosts.append(trd['_id'])

        html += '<span style="font-size: %d%%;">' % (70.0 + 200.0 * len(src) / max_ref)
        html += '<a style="text-decoration: none;  color: rgb(%d, %d, %d);" href="javascript:void(0);" onclick=\'show_stats("%s", %s, %s)\'>' % (w, w, w, dst, json.dumps(src), json.dumps(trd_hosts))
        html += '%s(%d)</a>&nbsp;' % (dst, len(src))
        html += '</span>\n'

    html += '</div></div><hr>'

    html += '<h2>Top 50 mont originated domains</h2>'
    html += '<div class="content">'

    html += '<div>'

    for src, dst in best_out:
        w = 150 - int(150.0 * len(dst) / max_out)

        trd_hosts = []

        for trd in db.trunc_hosts.find({"value": dst}):
            trd_hosts.append(trd['_id'])

        html += '<span style="font-size: %d%%;">' % (70.0 + 200.0 * len(dst) / max_out)
        html += '<a style="text-decoration: none;  color: rgb(%d, %d, %d);" href="javascript:void(0);" onclick=\'show_stats("%s", %s, %s)\'>' % (w, w, w, src, json.dumps(dst), json.dumps(trd_hosts))
        html += '%s(%d)</a>&nbsp;' % (src, len(dst))
        html += '</span>\n'

    html += '</div></div></div>'

    html += """
<div class="stats">
  URI:
  <div id="host" class="in_stats"></div>
  referre(d):
  <div id="referred" class="in_stats"></div>
  truncated URIs:
  <div id="truncated" class="in_stats"></div>
</div>
<div style="clear: both;"></div>
"""

def print_degree():
    global html
    global odir
    global is_plt
    global is_dot
    global ref_graph
    global out_graph

    dot = 'digraph http_referre{\ngraph [rankdir = LR];\n'
    in_dist  = {}
    out_dist = {}

    nodes = set()
    total_links = 0

    for dst, srcs in ref_graph.items():
        n = len(srcs)

        total_links += n
        nodes.add(dst)

        if n in in_dist:
            in_dist[n] += 1
        else:
            in_dist[n] = 1

        for src in srcs:
            dot += '"%s" -> "%s";\n' % (src, dst)
            nodes.add(src)

    for src, dsts in out_graph.items():
        n = len(dsts)

        if n in out_dist:
            out_dist[n] += 1
        else:
            out_dist[n] = 1

    dot += '}'
    open(os.path.join(odir, 'http_graph.dot'), 'w').write(dot)

    in_dist_x = sorted(in_dist.keys())
    in_dist_y = [in_dist[x] for x in in_dist_x]

    out_dist_x = sorted(out_dist.keys())
    out_dist_y = [out_dist[x] for x in out_dist_x]

    if is_plt:
        plt.loglog(in_dist_x, in_dist_y, 'ro-')
        plt.loglog(out_dist_x, out_dist_y, 'bv:')
        plt.xlabel('degree')
        plt.ylabel('count')
        plt.legend(('incoming', 'outgoing'), loc='upper right')
        plt.savefig(os.path.join(odir, 'http_dist.png'))

    html += """
<h2>Referred graph and degree distribution</h2>
<div class="content">
"""
    if is_dot:
        try:
            graph = pydot.graph_from_dot_file(os.path.join(odir,
                                                           'http_graph.dot'))
            graph.write(os.path.join(odir, 'http_graph.png'), prog = 'fdp',
                        format = 'png')
            html += '<p><a href="http_graph.png">download HTTP graph</a></p>'
        except:
            pass

    html += """
<p><a href="http_graph.dot">download GraphViz dot file</a></p>
<table>
  <tr>
    <td>Total nodes: </td>
    <td>%d</td>
  </tr>
  <tr>
    <td>Total links: </td>
    <td>%d</td>
  </tr>
</table>
""" % (len(nodes), total_links)

    if is_plt:
        html += '<div style="text-align: center;"><img src="http_dist.png"></div>'

    html += '</div>'


def print_html():
    global html

    html += """
<html>
  <head>
    <style type="text/css">
      body { margin: 20px; }
      a:hover {color: red}

      div.content { margin: 20px; margin-top: 0px; }
      div.in_stats { margin: 10px; margin-top: 2px; font-size: 80%;
                     color: gray; }
      div.stats { padding: 20px; }
      div.top50 { float: left; width: 65%; padding-right: 10px; }
    </style>
    <title>HTTP Statistics</title>
  </head>
  <h1>HTTP Statistics</h1>
  <body>
"""

    con = pymongo.Connection(mongo)
    db  = con.HTTP

    get_graph(db)

    print_top_50(db)

    html += '<hr>'

    print_degree()

    html += """
  </body>
</html>
"""

def usage():
    print sys.argv[0], '-m mongo_server -o ouput_dir'

def main():
    global html
    global odir

    try:
        opts, args = getopt.getopt(sys.argv[1:], "hm:o:", ["help", "mongo=", "output="])
    except getopt.GetoptError:
        usage()
        sys.exit(2)

    for o, a in opts:
        if o in ("-h", "--help"):
            usage()
            sys.exit()
        if o in ("-o", "--output"):
            odir = a
        if o in ("-m", "--mongo"):
            mongo = a

    print_html()

    open(os.path.join(odir, 'http_stats.html'), 'w').write(html)

main()
