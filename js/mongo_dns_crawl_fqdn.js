function reduce(key, values) {
    var n = 0;

    values.forEach(function(value) {
        n += value;
    });

    return n;
}

function map_one() {
    try {
        if ('fqdn' in this) {
            var sp = this.fqdn.split('.');

            emit(sp[sp.length - 1], 1);
        }
    } catch (e) {
    }
}

function map_two() {
    try {
        if ('fqdn' in this) {
            var sp = this.fqdn.split('.');

            if (sp.length > 1) {
                emit(sp[sp.length - 2] + '.' + sp[sp.length - 1], 1);
            }
        }
    } catch (e) {
    }
}

function map_three() {
    try {
        if ('fqdn' in this) {
            var sp = this.fqdn.split('.');

            if (sp.length > 2) {
                emit(sp[sp.length - 3] + '.' +
                     sp[sp.length - 2] + '.' +
                     sp[sp.length - 1], 1);
            }
        }
    } catch (e) {
    }
}

function map_three_jp() {
    try {
        if ('fqdn' in this) {
            var sp = this.fqdn.split('.');

            if (sp.length > 2 && sp[sp.length - 1] == 'jp') {
                emit(sp[sp.length - 3] + '.' +
                     sp[sp.length - 2] + '.' +
                     sp[sp.length - 1], 1);
            }
        }
    } catch (e) {
    }
}

res = db.servers.mapReduce(map_one, reduce,
                           {out: {replace: 'fqdn1_dist_all'}});
shellPrint(res);

res = db.servers.mapReduce(map_two, reduce,
                           {out: {replace: 'fqdn2_dist_all'}});
shellPrint(res);

res = db.servers.mapReduce(map_three, reduce,
                           {out: {replace: 'fqdn3_dist_all'}});
shellPrint(res);


db.servers.ensureIndex({is_ra: 1});

res = db.servers.mapReduce(map_one, reduce,
                           {out: {replace: 'fqdn1_dist_open_resolver'},
                            query: {is_ra: true}});
shellPrint(res);

res = db.servers.mapReduce(map_two, reduce,
                           {out: {replace: 'fqdn2_dist_open_resolver'},
                            query: {is_ra: true}});
shellPrint(res);

res = db.servers.mapReduce(map_three, reduce,
                           {out: {replace: 'fqdn3_dist_open_resolver'},
                            query: {is_ra: true}});
shellPrint(res);

res = db.servers.mapReduce(map_three_jp, reduce,
                           {out: {replace: 'fqdn3_jp_dist_all'}});
shellPrint(res);

res = db.servers.mapReduce(map_three_jp, reduce,
                           {out: {replace: 'fqdn3_jp_dist_open_resolver'},
                            query: {is_ra: true}});
shellPrint(res);
