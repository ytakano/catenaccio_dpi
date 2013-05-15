function map() {
    try {
        if ('ver' in this && 'type' in this &&
            this.type == 'BIND' && this.type_ver == '9.x') {
            var sp  = this.ver.split(/-| /);
            var ver = sp[0];

            if (sp.length >= 2 && sp[1].match(/^P[1-9]$/)) {
                ver += '-' + sp[1];
            }

            if (this.ver.match(/.*RedHat/)) {
                ver += ' RedHat';
            }

            emit(ver, 1);
        }
    } catch (e) {
    }
}

function reduce(key, values) {
    var count = 0;

    values.forEach(function(value) {
        count += value;
    });

    return count;
}

var res = db.servers.mapReduce(map, reduce, {out: {replace: "type_hist_bind9"}});

shellPrint(res);
