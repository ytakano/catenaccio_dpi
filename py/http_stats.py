#!/usr/bin/env python
# -*- coding: utf-8 -*-

import pymongo
import json
import getopt, sys
import os
import subprocess

try:
    import matplotlib.pyplot as plt
    is_plt = True
except ImportError:
    is_plt = False

class http_stats:
    def __init__(self, server, outdir):
        self._con    = pymongo.Connection(server)
        self._outdir = outdir

        self._soa_rname = {}
        self._in_graph  = {}
        self._out_graph = {}
        self._max_ref   = 0.0
        self._top_n     = set()
        self._total_ref = 0

        self._dot_full   = 'http_full_graph.dot'
        self._dot_pruned = 'http_pruned_graph.dot'
        self._sif_full   = 'http_full_graph.sif'
        self._sif_pruned = 'http_pruned_graph.sif'

        self._html = """
<?xml version="1.0" encoding="utf-8"?>
<!DOCTYPE html PUBLIC "-//W3C//DTD XHTML 1.0 Strict//EN" "http://www.w3.org/TR/xhtml1/DTD/xhtml1-strict.dtd">
<html xmlns="http://www.w3.org/1999/xhtml" xml:lang="ja" lang="ja">
  <head>
    <meta http-equiv="Content-Type" content="text/html; charset=utf-8" />
    <style type="text/css">
      body { margin: 20px; }
      a:hover {color: red}

      div.content { margin: 20px; margin-top: 0px; }

      h3.soa { margin-bottom: 5px;
               margin-top: 0px;
               padding-left: 3px;
               border-left: 7px solid #000000; }
      div.refs { border-left: 1px solid #000000;
                 border-bottom: 1px solid #000000;
                 padding: 5px;
                 margin-bottom: 10px; }
      span.dst { margin-right : 10px; }
      div.small { font-size: small; margin-bottom: 5px; }
      div.bold { font-weight: bold;
                 padding-left: 3px;
                 border-left: 4px solid #000000;
                 border-bottom: 1px solid #000000; }

      span.src { margin-right: 8px; }
      div.srcs { line-height: 1.5em; }
    </style>

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

function show_stats(uri, hosts, trds, id_host, id_refered, id_truncated) {
    var div  = document.getElementById(id_host);
    var text = document.createTextNode(uri);

    while (div.firstChild) {
        div.removeChild(div.firstChild);
    }

    div.appendChild(text);

    set_hosts(id_refered, hosts);
    set_hosts(id_truncated, trds);
}

function show_stats_soa(uri, hosts, trds, soa, id_host, id_refered, id_truncated, id_soa) {
    var div  = document.getElementById(id_soa);
    var text = document.createTextNode(soa);

    while (div.firstChild) {
        div.removeChild(div.firstChild);
    }

    div.appendChild(text);

    show_stats(uri, hosts, trds, id_host, id_refered, id_truncated);
}
-->
</script>

    <title>Cattenacio DPI: HTTP Statistics</title>
  </head>
  <body>
    <h1>HTTP Statistics</h1>

    <h2>Most Refered URLs</h2>
    <div class="content">
      <table>
        <tr valign="top">
          <td width=65%%>%(top_n)s</td>
          <td width=30%%>
            <div class="bold">host</div>
            <div id="host_dst" class="small"></div>
            <div class="bold">refered by</div>
            <div id="refered_dst" class="small"></div>
            <div class="bold">truncated URLs</div>
            <div id="truncated_dst" class="small"></div>
          </td>
        </tr>
      </table>
    </div>

    <hr>

    <h2>Sites Refering Most Refered URLs</h2>
    <div class="content">

      <table>
        <tr valign="top">
          <td width=65%%>%(refs)s</td>
          <td width=30%%>
            <div class="bold">host</div>
            <div id="host_src" class="small"></div>
            <div class="bold">SOA RNAME</div>
            <div id="soa_src" class="small"></div>
            <div class="bold">refering</div>
            <div id="refered_src" class="small"></div>
            <div class="bold">truncated URLs</div>
            <div id="truncated_src" class="small"></div>
          </td>
        </tr>
      </table>
    </div>

    <hr>

    <h2>HTTP Refered Graph</h2>
    <div class="content">
      <p>
        <a href="http_full_graph.dot">download Graphviz dot file</a>
      </p>
      <p>
        <a href="http_full_graph.sif">download SIF file</a>
      </p>
    </div>
  </body>
</html>
"""

    def _get_full_sif(self):
        sif = ''

        for dst, srcs in self._in_graph.items():
            for src in srcs:
                sif += '%s referer %s\n' % (src, dst)

        return sif

    def _get_full_dot(self):
        dot = 'digraph http_referre{\ngraph [rankdir = LR];\n'

        for dst, srcs in self._in_graph.items():
            for src in srcs:
                dot += '"%s" -> "%s";\n' % (src, dst)

        dot += '}'

        return dot

    def _get_graph(self):
        db = self._con.HTTP

        for i in db.graph_trunc_host.find():
            dst  = i['_id']
            srcs = i['value']

            self._in_graph[dst] = srcs
            self._total_ref += len(srcs)

            for src in srcs:
                if src in self._out_graph:
                    self._out_graph[src].append(dst)
                else:
                    self._out_graph[src] = [dst]

    def _get_soa_rname(self):
        db = self._con.HTTP

        for dst in self._in_graph.keys():
            soa = db.soa.find_one({'_id': dst})

            if soa == None:
                continue

            rname = soa['rname']
            elm   = {'dst': dst, 'srcs': self._in_graph[dst]}

            if rname in self._soa_rname:
                self._soa_rname[rname].append(elm)
            else:
                self._soa_rname[rname] = [elm]

    def _get_top_n_refered(self, n):
        soa = sorted(self._soa_rname.items(),
                     key = lambda x: sum([len(refs['srcs']) for refs in x[1]]),
                     reverse = True)

        return soa[0:n]

    def _get_top_n_leaking(self, n):
        html = '<div class="srcs">'
        uri  = {}

        for k, v in self._out_graph.items():
            val = 0
            for dst in v:
                try:
                    score = (float(len(self._in_graph[dst])) / self._total_ref) ** 2
                    val += score
                except:
                    continue

            uri[k] = val

        sites = sorted(uri.items(), key = lambda x: x[1], reverse = True)[0:n]

        if len(sites) == 0:
            return ''

        i = 0
        for k, v in sites:
            score = round(v * 100, 2)

            db = self._con.HTTP
            trd_hosts = []

            for trd in db.trunc_hosts.find({"value": k}):
                trd_hosts.append(trd['_id'])

            soa   = db.soa.find_one({'_id': k})
            rname = ''

            if soa != None:
                rname = soa['rname']

            params = {'src':    k,
                      'weight': 125 - i * 1,
                      'score':  score,
                      'uris':   json.dumps(self._out_graph[k]),
                      'truncated': json.dumps(trd_hosts),
                      'soa': rname}

            i += 1

            html += '<span class="src"><a style="text-decoration: none; color: rgb(0, 0, 0);" href="javascript:void(0);" onclick=\'show_stats_soa("%(src)s", %(uris)s, %(truncated)s, "%(soa)s", "host_src", "refered_src", "truncated_src", "soa_src")\'>%(src)s(%(score)2.2f)</a></span> ' % params

        html += '</div>'

        return html

    def _get_top_n_html(self, n):
        html = ''

        top_n = self._get_top_n_refered(n)

        max_ref = 0
        for (soa, refs) in top_n:
            for ref in refs:
                if max_ref < len(ref['srcs']):
                    max_ref = len(ref['srcs'])

        for (soa, refs) in top_n:
            html += '<div class="refs"><h3 class="soa">' + soa + '</h3>'

            for ref in refs:
                len_src = len(ref['srcs'])

                db = self._con.HTTP
                trd_hosts = []

                for trd in db.trunc_hosts.find({"value": ref['dst']}):
                    trd_hosts.append(trd['_id'])

                params = {'dst': ref['dst'],
                          'len': len(ref['srcs']),
                          'uris': json.dumps(ref['srcs'].keys()),
                          'truncated': json.dumps(trd_hosts),
                          'color': 150 - int(150.0 * len_src / max_ref),
                          'weight': 80.0 + 150.0 * len_src / max_ref}

                html += '<span class="dst" style="font-size: %(weight)d%%;"><a style="text-decoration: none; color: rgb(%(color)d, %(color)d, %(color)d);" href="javascript:void(0);" onclick=\'show_stats("%(dst)s", %(uris)s, %(truncated)s, "host_dst", "refered_dst", "truncated_dst")\'>%(dst)s(%(len)d)</a></span> ' % params

            html += '</div>'

        return html

    def print_html(self):
        self._get_graph()
        self._get_soa_rname()

        dot = self._get_full_dot()
        sif = self._get_full_sif()
        html = self._html % {'top_n': self._get_top_n_html(10),
                            'refs' : self._get_top_n_leaking(50) }

        open(os.path.join(self._outdir, self._dot_full), 'w').write(dot)
        open(os.path.join(self._outdir, self._sif_full), 'w').write(sif)
        open(os.path.join(self._outdir, 'http_stats.html'),'w').write(html)

def usage():
    print sys.argv[0], '-m mongo_server -o ouput_dir'

if __name__ == '__main__':
    server = 'localhost:27017'
    outdir = os.getcwd()

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
            outdir = a
        if o in ("-m", "--mongo"):
            server = a

    stats = http_stats(server, outdir)
    stats.print_html()
