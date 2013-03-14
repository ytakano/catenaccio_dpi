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
        var host = c.next()['_id'].split('.').reverse();
        if (host.length <= 2)
            continue;

        hosts.push(host);
    }

    for (var i = 0; i < hosts.length; i++) {
        for (var j = 0; j < hosts.length; j++) {
            if (hosts[i] == hosts[j])
                continue;

            var n = 0;
            var len = hosts[i].length < hosts[j].length ? hosts[i].length : hosts[j].length;

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

                var h = hosts[i].reverse().join('.');

                shellPrint([h, tr_host, corr]);

                if (h in trunc) {
                    if (tr_host.length < trunc[h].length)
                        trunc[h] = tr_host;
                } else {
                    trunc[h] = tr_host;
                }
            }
        }
    }

    shellPrint(trunc);

    db.trunc_hosts.remove();
    for (var key in trunc) {
        db.trunc_hosts.save({_id: key, value: trunc[key]});
    }
}

var res = db.requests.mapReduce(map, reduce, {out: {replace: "hosts"}});

shellPrint(res);

trunc_host();
