var http = require('http'),   // http server
    https = require('https'),
      fs = require('fs'),     // filesystem.
    path = require('path'),   // used for traversing your OS.
     url = require('url'),
    exec = require('child_process').execSync;

var HTTP_PORT = 8080;
var child;

function parseQuery (qstr) {
    var query = {};
    var a = qstr.split('&');
    for (var i in a) {
        var b = a[i].split('=');
        query[decodeURIComponent(b[0])] = decodeURIComponent(b[1]);
    }
    return query;
}

function download(url, dest, cb) {
    var file = fs.createWriteStream(dest);
    var request = https.get(url, function(response) {
        response.pipe(file);
        file.on('finish', function() {
            file.close(cb);  // close() is async, call cb after close completes.
        });
    }).on('error', function(err) { // Handle errors
        fs.unlink(dest); // Delete the file async. (But we don't check the result)
        if (cb) cb(err.message);
    });
};

var server = http.createServer( function (req, res) {
    var request = url.parse(req.url);
        
    console.log('Request', request);

    if (request.query) {
        var query = parseQuery(request.query);
        console.log('Query', query);

        var command = 'build/bin/./tangramPaparazzi';
       
        if (query['lat'] && typeof parseFloat(query['lat']) === 'number') {
            command += ' -lat ' + query['lat'];
        }
        if (query['lon'] && typeof parseFloat(query['lon']) === 'number') {
            command += ' -lon ' + query['lon'];
        }
        if (query['zoom'] && typeof parseFloat(query['zoom']) === 'number') {
            command += ' -z ' + query['zoom'];
        }
        if (query['tilt'] && typeof parseFloat(query['tilt']) === 'number') {
            command += ' -t ' + query['tilt'];
        }
        if (query['rot'] && typeof parseFloat(query['rot']) === 'number') {
            command += ' -r ' + query['rot'];
        }
        if (query['width'] && typeof query['width'] === 'number') {
            command += ' -w ' + query['width'];
        }
        if (query['height'] && typeof query['heihgt'] === 'number') {
            command += ' -h ' + query['height'];
        }
        command += ' -o out.png';
         if (query['scene']) {
            download(query['scene'],'infile.yaml', () => {
                console.log('File '+query['scene'] +' downloaded');
                command += ' -s infile.yaml';
                exec(command);
                console.log('Done');
                var img = fs.readFileSync('out.png');
                res.writeHead(200, {'Content-Type': 'image/png' });
                res.end(img, 'binary');
            });
        } else {
            exec(command);
            console.log('Done');
            var img = fs.readFileSync('out.png');
            res.writeHead(200, {'Content-Type': 'image/png' });
            res.end(img, 'binary');
        }
    }
}).listen(HTTP_PORT);
