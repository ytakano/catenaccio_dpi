#!/bin/sh

js_http_files="mongo_http_hosts.js mongo_http_ip.js mongo_http_graph_trunc_host.js mongo_http_tld.js mongo_http_graph_trunc_host_ip.js"
js_dns_files="mongo_dns.js"

js_path=`dirname $0`

if [ $# -ge 1 ]; then
    host_http=$1/HTTP
    host_dns=$1/DNS
else
    host_http=HTTP
    host_dns=DNS
fi

for i in ${js_http_files}
do
    echo "${host_http} ${js_path}/${i}"
    mongo ${host_http} ${js_path}/${i}
done

for i in ${js_dns_files}
do
    echo "${host_dns} ${js_path}/${i}"
    mongo ${host_dns} ${js_path}/${i}
done
