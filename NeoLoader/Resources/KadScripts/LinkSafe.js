/*
*	Name: Link Safe
*   Version: 0.0.2.3
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
			}
			fx.addResponse(response); // no MORE
		}
  },

  secureLinks: function (lookup, args, fx) {
  	if(lookup.isProxy())
  		return false; // do not intercept the request - and dont do anything
  		
  	lookup.fx.push(fx);
		fx.storeStats = new Array();
	
		var storeKey = lookup.getAccessKey(true);
  	if (storeKey != null) { // sign the link packet with the store key
			args["PSK"] = storeKey.pubKey.value;
			args.sign(storeKey);
    }

    var rq = lookup.addRequest("secureLinks", args, function (lookup, ret, req) {
    	if ("ERR" in ret)
				debug.error("secureLinks ERR: " + ret["ERR"]);
      else
      	fx.storeStats.push(parseInt(ret["EXP"]));
    });
    
    return true; // intercept the request - and let the own one run
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

  secureLinks: function (request, targetID) {
		var error = null;
		var path = "LNKS:" + request["PID"];
		
		var oldEntry = script.index.load(targetID, path);
		if(oldEntry)
		{
			var pubKey = null;
			if ("PSK" in oldEntry) 
				pubKey = new PubKey(oldEntry["PSK"]);
			else if("PSK" in request)
				pubKey = new PubKey(request["PSK"]);
			
			if(pubKey && !request.verify(pubKey))
			 	error = "BadSign";
		}

    debug.info("secureLinks for " + request["HV"]);

    var result = new Variant();
    if (error) {
			result["ERR"] = error;
    } 
    else {
      var date = new Date();
      var time = parseInt(date.getTime() / 1000); // in seconds
      var expire = time + (30 * 24 * 3600); // 30 days

      // update/store link list
      script.index.store(targetID, path, request, expire);

      result["EXP"] = expire;
    }
    return result;
  },

  findLinks: function (request, targetID) {
  	
    var list = script.index.list(targetID, "LNKS:*"); // list all link lists

    // Remove Duplicatd
    var linkMap = new Object();
    for (var i = 0; i < list.length; i++) {
      var foundEntry = script.index.load(list[i]);
      var links = foundEntry["LL"]; // Link List
      for (var j = 0; j < links.length; j++) {
        var link = links[j];
        //if (!(link in linkMap))
        linkMap[link] = 1;
        //else
        //    linkMap[link]++;
      }
     }

    var linkList = new Variant();
    for (var link in linkMap)
      linkList.append(link);

    debug.info("findLinks for " + request["HV"] + " found " + linkList.length);

    var result = new Variant();
    result["HF"] = request["HF"];
    result["HV"] = request["HV"];
    result["LL"] = linkList;
    return result;
  }
}

var localAPI = new Index.API('localAPI');
var remoteAPI = new Index.API('remoteAPI');