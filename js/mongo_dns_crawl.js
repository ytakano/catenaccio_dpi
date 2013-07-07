var rirmap = {
    '1': 'APNIC',
    '2': 'RIPE NCC',
    '5': 'RIPE NCC',
    '14': 'APNIC',
    '23': 'ARIN',
    '24': 'ARIN',
    '27': 'APNIC',
    '31': 'RIPE NCC',
    '36': 'APNIC',
    '39': 'APNIC',
    '41': 'AFRINIC',
    '42': 'APNIC',
    '46': 'RIPE NCC',
    '49': 'APNIC',
    '50': 'ARIN',
    '58': 'APNIC',
    '59': 'APNIC',
    '60': 'APNIC',
    '61': 'APNIC',
    '62': 'RIPE NCC',
    '63': 'ARIN',
    '64': 'ARIN',
    '65': 'ARIN',
    '66': 'ARIN',
    '67': 'ARIN',
    '68': 'ARIN',
    '69': 'ARIN',
    '70': 'ARIN',
    '71': 'ARIN',
    '72': 'ARIN',
    '73': 'ARIN',
    '74': 'ARIN',
    '75': 'ARIN',
    '76': 'ARIN',
    '77': 'RIPE NCC',
    '78': 'RIPE NCC',
    '79': 'RIPE NCC',
    '80': 'RIPE NCC',
    '81': 'RIPE NCC',
    '82': 'RIPE NCC',
    '83': 'RIPE NCC',
    '84': 'RIPE NCC',
    '85': 'RIPE NCC',
    '86': 'RIPE NCC',
    '87': 'RIPE NCC',
    '88': 'RIPE NCC',
    '89': 'RIPE NCC',
    '90': 'RIPE NCC',
    '91': 'RIPE NCC',
    '92': 'RIPE NCC',
    '93': 'RIPE NCC',
    '94': 'RIPE NCC',
    '95': 'RIPE NCC',
    '96': 'ARIN',
    '97': 'ARIN',
    '98': 'ARIN',
    '99': 'ARIN',
    '100': 'ARIN',
    '101': 'APNIC',
    '102': 'AFRINIC',
    '103': 'APNIC',
    '104': 'ARIN',
    '105': 'AFRINIC',
    '106': 'APNIC',
    '107': 'ARIN',
    '108': 'ARIN',
    '109': 'RIPE NCC',
    '110': 'APNIC',
    '111': 'APNIC',
    '112': 'APNIC',
    '113': 'APNIC',
    '114': 'APNIC',
    '115': 'APNIC',
    '116': 'APNIC',
    '117': 'APNIC',
    '118': 'APNIC',
    '119': 'APNIC',
    '120': 'APNIC',
    '121': 'APNIC',
    '122': 'APNIC',
    '123': 'APNIC',
    '124': 'APNIC',
    '125': 'APNIC',
    '126': 'APNIC',
    '173': 'ARIN',
    '174': 'ARIN',
    '175': 'APNIC',
    '176': 'RIPE NCC',
    '177': 'LACNIC',
    '178': 'RIPE NCC',
    '179': 'LACNIC',
    '180': 'APNIC',
    '181': 'LACNIC',
    '182': 'APNIC',
    '183': 'APNIC',
    '184': 'ARIN',
    '185': 'RIPE NCC',
    '186': 'LACNIC',
    '187': 'LACNIC',
    '189': 'LACNIC',
    '190': 'LACNIC',
    '193': 'RIPE NCC',
    '194': 'RIPE NCC',
    '195': 'RIPE NCC',
    '197': 'AFRINIC',
    '199': 'ARIN',
    '200': 'LACNIC',
    '201': 'LACNIC',
    '202': 'APNIC',
    '203': 'APNIC',
    '204': 'ARIN',
    '205': 'ARIN',
    '206': 'ARIN',
    '207': 'ARIN',
    '208': 'ARIN',
    '209': 'ARIN',
    '210': 'APNIC',
    '211': 'APNIC',
    '212': 'RIPE NCC',
    '213': 'RIPE NCC',
    '216': 'ARIN',
    '217': 'RIPE NCC',
    '218': 'APNIC',
    '219': 'APNIC',
    '220': 'APNIC',
    '221': 'APNIC',
    '222': 'APNIC',
    '223': 'APNIC',
};

db.servers.ensureIndex({rir: 1});
db.servers.ensureIndex({type: 1});

db.servers.find().forEach(function(doc) {
    var ver;

    try {
        var prefix = doc['_id'].split('.')[0];
        if (prefix in rirmap) {
            db.servers.update(
                {_id: doc['_id']},
                {$set: {rir: rirmap[prefix]}}
            );
        } else {
            db.servers.update(
                {_id: doc['_id']},
                {$set: {rir: 'other'}}
            );
        }

        if ('ver' in doc) {
            if (doc['ver'].match(/^dnsmasq-/)) {
                // DNS masquerade
                ver = doc['ver'].split('-')[1];

                db.servers.update(
                    {_id: doc['_id']},
                    {$set: {type: 'dnsmasq', type_ver: ver}}
                );
            } else if (doc['ver'].match(/^Nominum Vantio /)) {
                // Nominum Vantio
                ver = doc['ver'].split(' ')[2];

                db.servers.update(
                    {_id: doc['_id']},
                    {$set: {type: 'Nominum Vantio', type_ver: ver}}
                );
            } else if (doc['ver'].match(/^Nominum ANS /)) {
                // Nominum ANS
                ver = doc['ver'].split(' ')[2];

                db.servers.update(
                    {_id: doc['_id']},
                    {$set: {type: 'Nominum ANS', type_ver: ver}}
                );
            } else if (doc['ver'].match(/^unbound /)) {
                // unbound
                ver = doc['ver'].split(' ')[1];

                db.servers.update(
                    {_id: doc['_id']},
                    {$set: {type: 'unbound', type_ver: ver}}
                );
            } else if (doc['ver'].match(/^NSD /)) {
                // NSD
                ver = doc['ver'].split(' ')[1];

                db.servers.update(
                    {_id: doc['_id']},
                    {$set: {type: 'NSD', type_ver: ver}}
                );
            } else if (doc['ver'].match(/.*Windows/)) {
                // Windows

                db.servers.update(
                    {_id: doc['_id']},
                    {$set: {type: 'Windows'}}
                );
            } else if (doc['ver'].match(/^PowerDNS /)) {
                // PowerDNS
                ver = doc['ver'].split(' ')[2];

                db.servers.update(
                    {_id: doc['_id']},
                    {$set: {type: 'PowerDNS', type_ver: ver}}
                );
            } else if (doc['ver'].match(/^4(\.[0-9])+/)) {
                // BIND 4.x
                ver = get_type_ver_bind48(doc['ver']);

                db.servers.update(
                    {_id: doc['_id']},
                    {$set: {type: 'BIND 4.x', type_ver: ver}}
                );
            } else if (doc['ver'].match(/^8(\.[0-9])+/)) {
                // BIND 8.x
                ver = get_type_ver_bind48(doc['ver']);

                db.servers.update(
                    {_id: doc['_id']},
                    {$set: {type: 'BIND 8.x', type_ver: ver}}
                );
            } else if (doc['ver'].match(/^9(\.[0-9])+/)) {
                // BIND 9.x
                ver = get_type_ver_bind9(doc['ver']);

                db.servers.update(
                    {_id: doc['_id']},
                    {$set: {type: 'BIND 9.x', type_ver: ver}}
                );
            }
        }
    } catch (e) {
        db.servers.update(
            {_id: doc['_id']},
            {$set: {ver: ''}}
        );
    }
});

function get_type_ver_bind48(ver) {
    var sp       = ver.split(/-| |\+/);
    var type_ver = sp[0];

    if (sp.length >= 2 && sp[1].match(/^P[1-9]+$/)) {
        type_ver += '-' + sp[1];
    }

    if (ver.match(/.*REL/)) {
        type_ver += '-REL';
    }

    return type_ver;
}

function get_type_ver_bind9(ver) {
    var sp       = ver.split(/-| |\+/);
    var type_ver = sp[0];

    if (sp.length >= 2 && sp[1].match(/^P[1-9][0-9]*$/)) {
        type_ver += '-' + sp[1];
    } else if (sp.length >= 2 && sp[1] == 'ESV') {
        type_ver += '-' + sp[1];
        if (sp.length >= 3 && sp[2].match(/^R[1-9][0-9]*$/)) {
            type_ver += '-' + sp[2];
        } else if (sp.length >= 4 && sp[3].match(/^P[1-9][0-9]*$/)) {
            type_ver += '-' + sp[3];
        }
    } else if (sp.length >= 3 && sp[2].match(/^P[1-9]+$/)) {
        type_ver += '-' + sp[2];
    }

    if (ver.match(/.*RedHat/)) {
        type_ver += '(RedHat)';
    }

    return type_ver;
}

function map_type_dist() {
    try {
        if ('type' in this) {
            emit(this.type, 1);
        } else {
            if ('ver' in this) {
                emit("can't detect", 1);
            } else {
                emit("no version info", 1);
            }
        }
    } catch (e) {
        emit("can't detect", 1);
    }

    emit('total', 1);
}

function reduce(key, values) {
    var n = 0;

    values.forEach(function(value) {
        n += value;
    });

    return n;
}

var res;

res = db.servers.mapReduce(map_type_dist, reduce,
                           {out: {replace: 'type_dist_all'}});
shellPrint(res);

res = db.servers.mapReduce(map_type_dist, reduce,
                           {out: {replace: 'type_dist_apnic'},
                            query: {rir: 'APNIC'}});
shellPrint(res);

res = db.servers.mapReduce(map_type_dist, reduce,
                           {out: {replace: 'type_dist_ripe'},
                            query: {rir: 'RIPE NCC'}});
shellPrint(res);

res = db.servers.mapReduce(map_type_dist, reduce,
                           {out: {replace: 'type_dist_arin'},
                            query: {rir: 'ARIN'}});
shellPrint(res);

res = db.servers.mapReduce(map_type_dist, reduce,
                           {out: {replace: 'type_dist_lacnic'},
                            query: {rir: 'LACNIC'}});
shellPrint(res);

res = db.servers.mapReduce(map_type_dist, reduce,
                           {out: {replace: 'type_dist_afrinic'},
                            query: {rir: 'AFRINIC'}});
shellPrint(res);

res = db.servers.mapReduce(map_type_dist, reduce,
                           {out: {replace: 'type_dist_other'},
                            query: {rir: 'other'}});
shellPrint(res);


function map_ver_dist() {
    try {
        if ('type_ver' in this) {
            emit(this.type_ver, 1);
        }
    } catch (e) {
    }
}

res = db.servers.mapReduce(map_ver_dist, reduce,
                           {out: {replace: 'ver_dist_bind9'},
                            query: {type: 'BIND 9.x'}});
shellPrint(res);

res = db.servers.mapReduce(map_ver_dist, reduce,
                           {out: {replace: 'ver_dist_bind8'},
                            query: {type: 'BIND 8.x'}});
shellPrint(res);

res = db.servers.mapReduce(map_ver_dist, reduce,
                           {out: {replace: 'ver_dist_bind4'},
                            query: {type: 'BIND 4.x'}});
shellPrint(res);

res = db.servers.mapReduce(map_ver_dist, reduce,
                           {out: {replace: 'ver_dist_nsd'},
                            query: {type: 'NSD'}});
shellPrint(res);

res = db.servers.mapReduce(map_ver_dist, reduce,
                           {out: {replace: 'ver_dist_unbound'},
                            query: {type: 'unbound'}});
shellPrint(res);

res = db.servers.mapReduce(map_ver_dist, reduce,
                           {out: {replace: 'ver_dist_nominum_ans'},
                            query: {type: 'Nominum ANS'}});
shellPrint(res);

res = db.servers.mapReduce(map_ver_dist, reduce,
                           {out: {replace: 'ver_dist_nominum_vantio'},
                            query: {type: 'Nominum Vantio'}});
shellPrint(res);

res = db.servers.mapReduce(map_ver_dist, reduce,
                           {out: {replace: 'ver_dist_dnsmasq'},
                            query: {type: 'dnsmasq'}});
shellPrint(res);

res = db.servers.mapReduce(map_ver_dist, reduce,
                           {out: {replace: 'ver_dist_power_dns'},
                            query: {type: 'PowerDNS'}});
shellPrint(res);

function map_open_resolver() {
    if (! this.is_ra)
        return;

    try {
        if ('type' in this) {
            emit(this.type, 1);
        } else {
            if ('ver' in this) {
                emit("can't detect", 1);
            } else {
                emit("no version info", 1);
            }
        }
    } catch (e) {
        emit("can't detect", 1);
    }

    emit('total', 1);
}

res = db.servers.mapReduce(map_open_resolver, reduce,
                           {out: {replace: 'open_resolver_all'}});
shellPrint(res);

res = db.servers.mapReduce(map_open_resolver, reduce,
                           {out: {replace: 'open_resolver_apnic'},
                            query: {rir: 'APNIC'}});
shellPrint(res);

res = db.servers.mapReduce(map_open_resolver, reduce,
                           {out: {replace: 'open_resolver_ripe'},
                            query: {rir: 'RIPE NCC'}});
shellPrint(res);

res = db.servers.mapReduce(map_open_resolver, reduce,
                           {out: {replace: 'open_resolver_arin'},
                            query: {rir: 'ARIN'}});
shellPrint(res);

res = db.servers.mapReduce(map_open_resolver, reduce,
                           {out: {replace: 'open_resolver_lacnic'},
                            query: {rir: 'LACNIC'}});
shellPrint(res);

res = db.servers.mapReduce(map_open_resolver, reduce,
                           {out: {replace: 'open_resolver_afrinic'},
                            query: {rir: 'AFRINIC'}});
shellPrint(res);

res = db.servers.mapReduce(map_open_resolver, reduce,
                           {out: {replace: 'open_resolver_other'},
                            query: {rir: 'other'}});
shellPrint(res);


function map_rcode_a() {
    emit(this.rcode_a, 1);
}

res = db.servers.mapReduce(map_rcode_a, reduce,
                           {out: {replace: 'rcode_a'}});
shellPrint(res);


function map_rcode_ver() {
    emit(this.rcode_ver, 1);
}

res = db.servers.mapReduce(map_rcode_ver, reduce,
                           {out: {replace: 'rcode_ver'}});
shellPrint(res);
