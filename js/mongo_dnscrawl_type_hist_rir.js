function map() {
    try {
        if ('rir' in this) {
            if ('ver' in this) {
                if ('type' in this) {
                    if (this.type == 'BIND') {
                        if (this.type_ver == '4.x') {
                            emit('BIND 4.x', 1);
                        } else if (this.type_ver == '8.x') {
                            emit('BIND 8.x', 1);
                        } else if (this.type_ver == '9.x') {
                            emit('BIND 9.x', 1);
                        }
                    } else if (this.type == 'dnsmasq') {
                        emit('dnsmasq', 1);
                    } else if (this.type == 'Nominum Vantio') {
                        emit('Nominum Vantio', 1);
                    } else if (this.type == 'Nominum ANS') {
                        emit('Nominum ANS', 1);
                    } else if (this.type == 'NSD') {
                        emit('NSD', 1);
                    } else if (this.type == 'unbound') {
                        emit('unbound', 1);
                    } else if (this.type == 'Windows') {
                        emit('windows', 1);
                    } else if (this.type == 'PowerDNS') {
                        emit('PowerDNS', 1);
                    }
                } else {
                    emit('other', 1);
                }
            } else {
                emit('no ver', 1);
            }
        }
    } catch (e) {
        emit('other', 1);
    }

    emit('total', 1);
}

function reduce(key, values) {
    var count = 0;

    values.forEach(function(value) {
        count += value;
    });

    return count;
}

var res;

res = db.servers.mapReduce(map, reduce,
                           {out: {replace: "type_hist_apnic"},
                            query: {rir: "APNIC"}});
shellPrint(res);

res = db.servers.mapReduce(map, reduce,
                           {out: {replace: "type_hist_ripe"},
                            query: {rir: "RIPE"}});
shellPrint(res);

res = db.servers.mapReduce(map, reduce,
                           {out: {replace: "type_hist_arin"},
                            query: {rir: "ARIN"}});
shellPrint(res);

res = db.servers.mapReduce(map, reduce,
                           {out: {replace: "type_hist_lacnic"},
                            query: {rir: "LACNIC"}});
shellPrint(res);

res = db.servers.mapReduce(map, reduce,
                           {out: {replace: "type_hist_afrinic"},
                            query: {rir: "AFRINIC"}});
shellPrint(res);

res = db.servers.mapReduce(map, reduce,
                           {out: {replace: "type_hist_other"},
                            query: {rir: "other"}});
shellPrint(res);