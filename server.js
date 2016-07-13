// NATIVE node modules
var http = require('http'),   // http Server
    https = require('https'), // https Server
      fs = require('fs'),     // FileSystem.
    path = require('path'),   // used for traversing your OS.
     url = require('url'),    // Url
    exec = require('child_process').exec;

// EXTRA node modules
var md5 = require('md5');
var winston = require('winston'); // I had listen that is a good log system... let's see

// Config GLOBAL variables
var BIN = 'build/bin/paparazzi'; // Where is the paparazi binnary
var HTTP_PORT = 8080;            // What port it should listen
var CACHE_FOLDER = 'cache/';     // Where are the catche response files (.png .log)

// LOG adding a time stamp to every entry to a file and the console.log simultaniusly
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

// Return an obj with the querry string
function parseQuery (qstr) {
    var query = {};
    var a = qstr.split('&');
    for (var i in a) {
        var b = a[i].split('=');
        query[decodeURIComponent(b[0])] = decodeURIComponent(b[1]);
    }
    return query;
}

// Check again if it exist or not and send back what ever file (PNG or LOG) that the user ask
function responseFile(unique_name, ext, res) {
    var filePath = CACHE_FOLDER + unique_name + ext;

    fs.exists(filePath, function (exists) {
        if (!exists) {
            responseErr(res, {msg:'Tangram-Paparazi binnary never generate ' + filePath});
            return;
        }

        var header = {};
        if (ext === 'png') {
            header = {
                'Content-Disposition': 'attachment;filename=' + unique_name + ext,
                'Content-Type': 'image/png'
            }
        } else if (ext === 'log') {
            header = {
                'Content-Type': 'text/plain'
            }
        }

        // set the content type
        res.writeHead(200, header);

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
        res.setHeader("access-control-allow-origin", "*");
        var request = url.parse(req.url);

        if (request.query) {
            // Parse the query string
            var query = parseQuery(request.query);
            
            // TODO:
            //  - For future API_KEY checking or tracking
            var key = 'NONE';
            if (query['api_key']) {
                key = query['api_key'];
            }
            
            // Listen for arguments to pass to Tangram-ES and construct a command string for it
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

            //  Is it asking for an image or the log file (STDOUT+STDERR)??
            var ext = '.png';
            if (query['ext']) {
                if (query['ext'] === 'log' || query['ext'] === 'png'){
                    ext = '.' + query['ext'];
                }
            } 

            // Based on the command that construct make a md5 unique name
            var unique_name = md5(command);

            // Infere the image and log file names for it
            var file_image = CACHE_FOLDER + unique_name + '.png';
            var file_log = CACHE_FOLDER + unique_name + '.log';
            
            var src = 'NONE'; // is going to reply from the catche or through a new tangram image?
            fs.exists(CACHE_FOLDER + unique_name + ext, function (exists) {
                if (exists) {
                    // If exist send back the cached one
                    src = 'CACHE_FOLDER';
                    responseFile(unique_name, ext, res);
                } else {
                    // If file doesn't exist create it!
                    src = 'TANGRAM-ES';
                    command += ' -o ' + file_image + ' >' + file_log + ' 2>&1';
                    
                    exec(command, function(error, stdout, stderr) {
                        if (error != undefined) {
                            responseErr(res, {src:src, msg:error, qrt:query, key:key});
                            return;
                        }
                        // It should need to enter to this two since they are redirected through the shell pipes
                        if (stdout) { logger.debug({src:src, msg:stdout, qrt:query, key: key}); }
                        if (stderr) { logger.error({src:src, msg:stderr, qry:query, key: key}); }

                        // Make a respond according to the extention
                        responseFile(unique_name, ext, res);
                    });
                }
                // Log the CALL 
                logger.info({src:src, img:file_image, qry:query, key: key }); 
            });     
        } else {
            // The user is asking for something that makes no sense... respond with a 404
            responseErr(res);
        }
    }).listen(HTTP_PORT);
    logger.info('SERVER running on port' +  HTTP_PORT);
});

