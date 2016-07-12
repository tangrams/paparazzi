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

// Config
var BIN = 'build/bin/paparazzi';
var HTTP_PORT = 8080;
var CACHE_FOLDER = 'cache/';

// Logger
var logger = new (winston.Logger)({
    transports: [
        new (winston.transports.Console)({
            timestamp: function() {
                return Date.now();
            },
            formatter: function(options) {
                // Return string will be passed to logger.
                return options.timestamp() +' '+ options.level.toUpperCase() +' '+ (undefined !== options.message ? options.message : '') + 
                (options.meta && Object.keys(options.meta).length ? '\n\t'+ JSON.stringify(options.meta) : '' );
            }
        }),
        new (winston.transports.File)({ 
            filename: 'paparazzi.log',
            timestamp: function() {
                return Date.now();
            },
            formatter: function(options) {
                // Return string will be passed to logger.
                return options.timestamp() +' '+ options.level.toUpperCase() +' '+ (undefined !== options.message ? options.message : '') + 
                (options.meta && Object.keys(options.meta).length ? '\n\t'+ JSON.stringify(options.meta) : '' );
            }
        })
    ]
});

function parseQuery (qstr) {
    var query = {};
    var a = qstr.split('&');
    for (var i in a) {
        var b = a[i].split('=');
        query[decodeURIComponent(b[0])] = decodeURIComponent(b[1]);
    }
    return query;
}

function responseImg(unique_name, res) {
    var filePath = CACHE_FOLDER + unique_name + '.png';
    fs.exists(filePath, function (exists) {
        if (!exists) {
            responseErr(res, {msg:'Tangram never made the image '+filePath});
            return;
        }

        // set the content type
        res.writeHead(200, {
            'Content-Disposition': 'attachment;filename=' + filePath,
            'Content-Type': 'image/png'
        });

        // stream the file
        fs.createReadStream(filePath).pipe(res);
    });
}

function responseErr(res, msg) {
    logger.error(msg);
    res.writeHead(404, {'Content-Type': 'text/plain' });
    res.end('404 Not Found');
}

fs.readFile('/etc/os-release', 'utf8', function (err,data) {
    if (err) {
        logger.error('SERVER ERROR: ' + err);
    }

    if (data.startsWith('NAME="Amazon Linux AMI"')) {
        HTTP_PORT = 80;
        BIN = 'export DISPLAY=:0 && ' + BIN;
        console.log('Amazon Linux GPU HeadLess (need for now "sudo")');
    }

    var server = http.createServer( function (req, res) {
        var request = url.parse(req.url);

        if (request.query) {
            var query = parseQuery(request.query);
            var key = 'NONE';
            if (query['api_key']) {
                key = query['api_key'];
            }
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
                var scene = url.parse(query['scene']);
                if (scene.protocol.startsWith('http') && scene.href && path.extname(scene.href) == '.yaml') {
                    command += ' -s ' + query['scene'];    
                } else {
                    responseErr(res, {src:'NODE', msg:query['scene']+' does not look like a URL path to YAML file', qrt: query, key: key});
                    return;
                }
            }

            var unique_name = md5(command);
            var image_path = CACHE_FOLDER + unique_name + '.png';
            var src = 'NONE';

            fs.exists(image_path, function (exists) {
                if (exists) {
                    // If exist send back the cached one
                    src = 'CACHE_FOLDER';
                    responseImg(unique_name, res);
                } else {
                    // If file doesn't exist create it!
                    src = 'TANGRAM-ES';
                    command += ' -o ' + image_path;
                    
                    exec(command, function(error, stdout, stderr) {
                        if (error != undefined) {
                            responseErr(res, {src:src, msg:error, qrt:query, key:key});
                            return;
                        }
                        if (stdout) {
                            logger.debug({src:src, msg:stdout, qrt:query, key: key});
                        }
                        if (stderr) {
                            logger.error({src:src, msg:stderr, qry:query, key: key});
                        }
                        responseImg(unique_name, res);
                    });
                }
                logger.info({src:src, img:image_path, qry:query, key: key }); 
            });     
        } else {
            responseErr(res);
        }
    }).listen(HTTP_PORT);
    logger.info('SERVER running on ' + server.address().address + ':' +  HTTP_PORT);
});

