/*
*	Name: Keyword Index
*   Version: 0.0.2.5
*/

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Lookup API

var Lookup = {
  API: function (name) {
      this.name = name;
  }
};

Lookup.API.prototype = {
  name: '',

  init: function (lookup) {
		lookup.fx = new Array();
  },
  finish: function (lookup) {
		for(var i=0; i < lookup.fx.length; i++)
		{
			var fx = lookup.fx[i];
			
			var response = new Variant();
			if(!(typeof fx.storeStats === "undefined"))
			{
				var count = fx.storeStats.length;
				// K-ToDo: Calculate median instead of taking from middle
				fx.storeStats.sort();
				var expiration = fx.storeStats[parseInt(count/2)];
				
				response["StoreCount"] = count;
				response["Expiration"] = expiration;
				
				debug.log(fx.name + " Storred: " + count);
			}
			debug.log("Finishing " + fx.name);
			fx.addResponse(response); // no MORE
		}
	},

  publishKeyword: function (lookup, args, fx) {
		lookup.fx.push(fx);
		fx.storeStats = new Array();
		
		debug.log("Starting " + fx.name);
		
		var ttl = args["TTL"];
		if(!(typeof ttl === "undefined")){
			delete args["TTL"];
			lookup.storeTTL = ttl;
		}
		
		var files = args["FL"];
		for (var i = 0; i < files.length; i++) {
			var file = files[i];
			
			var pubID = file["PID"];
			pubID.convert("binary");
			var path = "KWRD:" + pubID;
			delete file["PID"];
			
			lookup.store(path, file, function(lookup, expire){	
				fx.storeStats.push(parseInt(expire));
			});
		}
		
		return true; // intercept this request
	},
	
	findFiles: function(lookup, args, fx)
	{
		fx.onResponse = function(lookup, result)
		{
			var fileList = result["FL"];
			
			debug.info("recived: " + fileList.length + " results");
		};
	}
};

var lookupAPI = new Lookup.API('lookupAPI');

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Index API

var Index = {
  API: function (name) {
      this.name = name;
  }
};

Index.API.prototype = {
  name: '',

  findFiles: function (request, targetID) {
    debug.log("find files: " + request["EXP"]);

    var kwrdSplit = new RegExp("[\t\r\n \\!\\\"\\&\\/\\(\\)\\=\\?\\{\\}\\[\\]\\+\\*\\~\\#\\\'\\,\\;\\.\\:\\-\\_\\<\\>\\|\\\\]");
    // "bla blup" <- tis exact sequence of words
    // bla/blup <- the word bla or blup
    // bl* <- wildcard 
    // -blup must not be contained
    // bla must be contained
    //var expSplit = new RegExp(" (?=[^\"]*(\"[^\"]*\"[^\"]*)*$)"); // split keeping "bla blup" intact
    //var expressions = request["EXP"].toString().split(expSplit);
    var expressions = request["EXP"].toString().toLowerCase().split(" ");

    var foundFiles = kademlia.index.list(targetID, "KWRD:*");
		debug.log("possible matches: " + foundFiles.length);

    var fileMap = new Object();
    for (var i = 0; i < foundFiles.length; i++) {
      var file = kademlia.index.load(foundFiles[i]); // file is a frozen variant
      
      var fileName = file["FN"];
      var hashMap = file["HM"];
      if((typeof fileName === "undefined") || (typeof hashMap === "undefined"))
      	continue; // this is not a valid file entry
      
      var maching = true;
      var keywords = fileName.toString().toLowerCase().split(kwrdSplit);
      for (var j = 0; j < expressions.length; j++) {
        // K-ToDo-Now: add complex expressions see top comment
        if (expressions[j].length == 0)
					continue; // ignore empty expressions
        if (keywords.indexOf(expressions[j]) == -1) {
          maching = false;
          break;
        }
			}
			if (!maching)
				continue;

			// Ok that the plan: if we have a file with more than one hash its a neo file so it must have a neo hash
			//	in this case we use the neo hash, else we use the only available hash
			// Note: Torrent and Archive hashes may be a list of hashes, but only for a file that has a neo Hash
			//	We can test if a variant is a list by .length > 0

      var hash;
      if("HF" in request)
      {
      	var hashf = request["HF"];
      	hash = hashMap[hashf];
      	if(!hash && hashf == "neo")
      		hash = hashMap["neox"];
      }
			else if ("neo" in hashMap)
      	hash = hashMap["neo"];
      else if ("neox" in hashMap)
        hash = hashMap["neox"];
      else if ("ed2k" in hashMap)
        hash = hashMap["ed2k"];
      else if ("btih" in hashMap)
        hash = hashMap["btih"];
      else if ("arch" in hashMap)
        hash = hashMap["arch"];
      if(!hash)
        continue; // files without a valid hash are useless	
			
			// TODO: properly merge files that dont have a neo hash to neo hashed files where psosible
			
			//debug.log("found matches: " + file["FN"]);
			if (!(hash in fileMap)) {
				file.unfreeze(); // make the variant writable
				file["AC"] = 1;
				fileMap[hash] = file;
			}
			else {
				fileMap[hash]["AC"]++; // increment availability count
  		}
		}
		debug.log("found: " + Object.keys(fileMap).length + " unique results");
		
		var fileList = new Variant();
		for (var file in fileMap) {
			fileList.append(fileMap[file]);
			
			if(fileList.length >= 300)
				break;
		}
		
		debug.info("sending: " + fileList.length + " results");
		
		var result = new Variant();
		result["FL"] = fileList;
		return result;
  }
}

var localAPI = new Index.API('localAPI');
var remoteAPI = new Index.API('remoteAPI');