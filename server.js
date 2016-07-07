// NATIVE
var http = require('http'),   // http Server
    https = require('https'), // https Server
      fs = require('fs'),     // FileSystem.
    path = require('path'),   // used for traversing your OS.
     url = require('url'),    // Url
    exec = require('child_process').exec;

// MODULES
var md5 = require('md5');
var winston = require('winston');

var BIN = 'build/bin/paparazzi';
var HTTP_PORT = 8080;
var CACHE_FOLDER = 'cache/';

function parseQuery (qstr) {
    var query = {};
    var a = qstr.split('&');
    for (var i in a) {
        var b = a[i].split('=');
        query[decodeURIComponent(b[0])] = decodeURIComponent(b[1]);
    }
    return query;
}

fs.readFile('/etc/os-release', 'utf8', function (err,data) {
    if (err) {
        return console.log(err);
    }

    if (data.startsWith('NAME="Amazon Linux AMI"')) {
        HTTP_PORT = 80;
        BIN = 'export DISPLAY=:0 && ' + BIN;
        console.log('Amazon Linux GPU HeadLess (need for now "sudo")');
    }

    var server = http.createServer( function (req, res) {
        var request = url.parse(req.url);  
        //console.log('Request:\n', request);

        if (request.query) {
            var query = parseQuery(request.query);
            //console.log('Query:\n', query);

            var command = BIN;
           
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
            if (query['width'] && typeof parseFloat(query['width']) === 'number') {
                command += ' -w ' + query['width'];
            }
            if (query['height'] && typeof parseFloat(query['height']) === 'number') {
                command += ' -h ' + query['height'];
            }
            if (query['scene']) {
                command += ' -s ' + query['scene'];
            }

            var unique_name = md5(command);
            var image_path = CACHE_FOLDER + unique_name + '.png';
            
            if (fs.existsSync(image_path)) {
                // If file is cached retrive it directly
                console.log('Responding to cached image', image_path);
                var img = fs.readFileSync(image_path);
                res.writeHead(200, {'Content-Type': 'image/png' });
                res.end(img, 'binary');
            } else {
                command += ' -o ' + image_path;
                // If file doesn't exist create it!
                exec(command, function(error, stdout, stderr) {
                    //console.log('Error:\n', error);
                    //console.log('ST out:\n', stdout);
                    //console.log('ST err:\n', stderr);
                    
                    console.log('Command:\n', command);
                    var img = fs.readFileSync(image_path);
                    res.writeHead(200, {'Content-Type': 'image/png' });
                    res.end(img, 'binary');
                });
            }            
        }
    }).listen(HTTP_PORT);
    console.log('Server running on', server.address().address,  HTTP_PORT)
});

