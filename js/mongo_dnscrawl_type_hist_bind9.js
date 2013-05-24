function map() {
    try {
        if ('ver' in this && 'type' in this &&
            this.type == 'BIND' && this.type_ver == '9.x') {
            var sp       = this.ver.split(/-| |\+/);
            var type_ver = sp[0];

            if (sp.length >= 2 && sp[1].match(/^P[1-9][0-9]*$/)) {
                type_ver += '-' + sp[1];
            } else if (sp.length >= 2 && sp[1].match(/^ESV$/)) {
                type_ver += '-' + sp[1];
                if (sp.length >= 3 && sp[2].match(/^R[1-9][0-9]*$/)) {
                    type_ver += '-' + sp[2];
                } else if (sp.length >= 4 && sp[3].match(/^P[1-9][0-9]*$/)) {
                    type_ver += '-' + sp[3];
                }
            } else if (sp.length >= 3 && sp[2].match(/^P[1-9]+$/)) {
                type_ver += '-' + sp[2];
            }

            if (this.ver.match(/.*RedHat/)) {
                type_ver += '(RedHat)';
            }

            emit(type_ver, 1);
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
