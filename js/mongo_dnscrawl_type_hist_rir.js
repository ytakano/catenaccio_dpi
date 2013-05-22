function map() {
    try {
        if ('rir' in this && this.rir == 'other') {
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
}

function reduce(key, values) {
    var count = 0;

    values.forEach(function(value) {
        count += value;
    });

    return count;
}

var res = db.servers.mapReduce(map, reduce, {out: {replace: "type_hist_other"}});

shellPrint(res);
