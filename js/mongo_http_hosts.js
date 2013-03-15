var ex_domains = {'ac.jp': true,
                  'ad.jp': true,
                  'co.jp': true,
                  'ed.jp': true,
                  'go.jp': true,
                  'gr.jp': true,
                  'lg.jp': true,
                  'ne.jp': true,
                  'or.jp': true}

function map() {
    var uri = this.uri.split('/')[2];
    emit(uri, 1);
}

function reduce(key, values) {
    var n = 0;

    values.forEach(function(value) {
        n += value;
    });

    return n;
}

function trunc_host() {
    var hosts = [];
    var trunc = {};

    for (var c = db.hosts.find(); c.hasNext(); ) {
        hosts.push(c.next()['_id'].split('.').reverse());
    }

    for (var i = 0; i < hosts.length; i++) {
        for (var j = i + 1; j < hosts.length; j++) {
            var n = 0;
            var len = ((hosts[i].length < hosts[j].length) ?
                       hosts[i].length : hosts[j].length);

            for (var k = 0; k < len; k++) {
                if (hosts[i][k] == hosts[j][k])
                    n++;
                else
                    break;
            }

            var corr = n / len;

            if (corr > 0.5) {
                var tr_host = [];

                for (var p = 0; p < n; p++) {
                    tr_host.push(hosts[i][p]);
                }

                tr_host = tr_host.reverse().join('.');

                if (tr_host in ex_domains)
                    continue;

                var host = hosts[i].reverse().join('.');
                hosts[i].reverse();

                if (host in trunc) {
                    if (tr_host.length < trunc[host].length)
                        trunc[host] = tr_host;
                } else {
                    trunc[host] = tr_host;
                }
            }
        }
    }

    db.trunc_hosts.remove();
    db.trunc_hosts.ensureIndex({value: 1});

    for (var key in trunc) {
        db.trunc_hosts.save({_id: key, value: trunc[key]});
    }
}

var res = db.requests.mapReduce(map, reduce, {out: {replace: "hosts"}});

shellPrint(res);

trunc_host();
