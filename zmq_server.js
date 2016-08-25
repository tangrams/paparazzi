// NATIVE node modules
var http = require('http'),   // http Server
    https = require('https'), // https Server
      fs = require('fs'),     // FileSystem.
    path = require('path'),   // used for traversing your OS.
     url = require('url'),    // Url
  crypto = require('crypto'), // SSL 
    exec = require('child_process').exec;

// ZeroMQ
var zmq = require('zmq');
var responder = zmq.socket('rep');

// EXTRA node modules
var md5 = require('md5');

// Config GLOBAL variables
var HTTP_PORT = parseInt(process.argv[2] || '8080');    // What port it should listen
var ZMQ_PORT = 5555;
var CACHE_FOLDER = 'cache/';     // Where are the catche response files (.png .log)
var SERVER_KEY = 'key.pem';
var SERVER_CRS = 'cert.pem';

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
        }
        else if (ext === 'log') {
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
    console.log(msg);
    res.writeHead(404, {'Content-Type': 'text/plain'});
    res.end(msg);
}

var app = function (req, res) {
    res.setHeader("access-control-allow-origin", "*");
    var request = url.parse(req.url);
    console.log("Request: ", request);

    var path = request.pathname;
    if (request.query && (path === 'image' || path === 'log')) {
        // Parse the query string
        var query = parseQuery(request.query);

        console.log("Querry: ", query);
        
        // Construct a command to send
        var command = "";
        if (query['scene']) {
            var scene = url.parse(query['scene']);
            if (scene.protocol.startsWith('http') && scene.href && path.extname(scene.href) == '.yaml') {
                command += 'scene' + query['scene'] + ';';    
            }
            else {
                responseErr(res, {src:'NODE', msg:query['scene']+' does not look like a URL path to YAML file', qrt: query, key: key});
                return;
            }
        }
        if ((query['width'] && typeof parseFloat(query['width']) === 'number') && 
            (query['height'] && typeof parseFloat(query['height']) === 'number')) {
            command += 'size ' + query['width'] + ' ' + query['height'] + ';';
        }
        if ((query['lat'] && typeof parseFloat(query['lat']) === 'number') && 
            (query['lon'] && typeof parseFloat(query['lon']) === 'number')) {
            command += 'position ' + query['lon'] + ' ' + query['lat'] + ';';
        }
        if (query['zoom'] && typeof parseFloat(query['zoom']) === 'number') {
            command += 'zoom ' + query['zoom'] + ';';
        }
        if (query['tilt'] && typeof parseFloat(query['tilt']) === 'number') {
            command += 'tilt ' + query['tilt'] + ';';
        }
        if (query['rot'] && typeof parseFloat(query['rot']) === 'number') {
            command += 'rotate ' + query['rot'] + ';';
        }

        // Based on the command that construct make a md5 unique name
        var unique_name = md5(command);
        
        // //  Is it asking for an image or the log file (STDOUT+STDERR)??
        if (path === 'log') {
            ext = '.log';
            command += 'log ';
        } else if (path === 'image') {
            ext = '.png';
            command += 'print ';
        }

        // Infere the image and log file names for it
        var file_name = CACHE_FOLDER + unique_name + ext;
        command += file_name;


        
        // var src = 'NONE'; // is going to reply from the catche or through a new tangram image?
        // fs.exists(CACHE_FOLDER + unique_name + ext, function (exists) {
        //     if (exists) {
        //         // If exist send back the cached one
        //         src = 'CACHE_FOLDER';
        //         responseFile(unique_name, ext, res);
        //     } else {
        //         // If file doesn't exist create it!
        //         src = 'TANGRAM-ES';
        //         command += ' -o ' + file_image + ' >' + file_log + ' 2>&1';
                
        //         exec(command, function(error, stdout, stderr) {
        //             if (error != undefined) {
        //                 responseErr(res, {src:src, msg:error, qrt:query, key:key});
        //                 return;
        //             }

        //             // Make a respond according to the extention
        //             responseFile(unique_name, ext, res);
        //         });
        //     }
        //     // Log the CALL 
        //     console.log({src:src, img:file_image, qry:query, key: key }); 
        // });     
    } else {
        // The user is asking for something that makes no sense... respond with a 404
        responseErr(res);
    }
}

// RUN HTTP/HTTPS server
http.createServer(app).listen(HTTP_PORT);
console.log('SERVER running on port ' +  HTTP_PORT);

// Check if there are SSL credentials
if (fs.existsSync(SERVER_KEY) && fs.existsSync(SERVER_CRS)) {
    var https = require('https');
    console.log('LOADING SSL self sign certificates');

    var options = {
      key: fs.readFileSync(SERVER_KEY),
      cert: fs.readFileSync(SERVER_CRS)
    };
    if (HTTP_PORT == 80) {
        HTTP_PORT = 443;
    } else {
        HTTP_PORT = 4433;
    }
    https.createServer(options,app).listen(HTTP_PORT);
    console.log('SERVER running on port ' + HTTP_PORT);
}

