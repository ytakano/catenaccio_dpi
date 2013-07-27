function reduce(key, values) {
    var n = 0;

    values.forEach(function(value) {
        n += value;
    });

    return n;
}

function map() {
    try {
        if ('fqdn' in this) {
            var sp = this.fqdn.split('.');

            emit(sp[sp.length - 1], 1);

            if (sp.length > 1) {
                emit(sp[sp.length - 2] + '.' + sp[sp.length - 1], 1);
            }

            if (sp.length > 2) {
                emit(sp[sp.length - 3] + '.' +
                     sp[sp.length - 2] + '.' +
                     sp[sp.length - 1], 1);
            }
        }
    } catch (e) {
    }
}

res = db.servers.mapReduce(map, reduce,
                           {out: {replace: 'fqdn_dist_all'}});
shellPrint(res);

db.servers.ensureIndex({is_ra: 1});

res = db.servers.mapReduce(map, reduce,
                           {out: {replace: 'fqdn_dist_open_resolver'},
                            query: {is_ra: true}});
shellPrint(res);
