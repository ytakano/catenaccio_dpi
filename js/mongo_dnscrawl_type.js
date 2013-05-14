db.servers.find().forEach(function(doc) {
    var ver;

    try {
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
        } else if (doc['ver'].match(/^unbound/)) {
            // unbound
            ver = doc['ver'].split(' ')[1];

            db.servers.update(
                {_id: doc['_id']},
                {$set: {type: 'unbound', type_ver: ver}}
            );
        } else if (doc['ver'].match(/.*Windows/)) {
            // Windows

            db.servers.update(
                {_id: doc['_id']},
                {$set: {type: 'Windows'}}
            );
        } else if (doc['ver'].match(/^PowerDNS/)) {
            // PowerDNS
            ver = doc['ver'].split(' ')[2];

            db.servers.update(
                {_id: doc['_id']},
                {$set: {type: 'PowerDNS', type_ver: ver}}
            );
        } else if (doc['ver'].match(/^4\.[0-9]\.[0-9]/)) {
            // BIND 4.x

            db.servers.update(
                {_id: doc['_id']},
                {$set: {type: 'BIND', type_ver: '4.x'}}
            );
        } else if (doc['ver'].match(/^8\.[0-9]\.[0-9]/)) {
            // BIND 8.x

            db.servers.update(
                {_id: doc['_id']},
                {$set: {type: 'BIND', type_ver: '8.x'}}
            );
        } else if (doc['ver'].match(/^9\.[0-9]\.[0-9]/)) {
            // BIND 9.x

            db.servers.update(
                {_id: doc['_id']},
                {$set: {type: 'BIND', type_ver: '9.x'}}
            );
        }

    } catch (e) {
    }
});
