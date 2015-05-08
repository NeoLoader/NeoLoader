/*
*	Name: Source Tracker
*   Version: 0.0.2.3
*/

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Lookup API

var Lookup = {
	API: function(name) {
		this.name = name;
	}
};

Lookup.API.prototype = {
	name: '',

	init: function(lookup)
	{
		lookup.fx = new Array();
	},
	finish: function(lookup)
	{
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

	announceSource: function(lookup, args, fx)
	{
		lookup.fx.push(fx);
		fx.storeStats = new Array();
			
		var ttl = args["TTL"];
		if(!(typeof ttl === "undefined")) {
			delete args["TTL"];
			lookup.storeTTL = ttl;
		}
		
		var pubID = args["PID"];
		pubID.convert("binary");
		var path = "SRC:" + pubID;
		delete args["PID"];
		
		lookup.store(path, args, function(lookup, expire){	
			debug.log("Tracker Storred: " + path);
			fx.storeStats.push(parseInt(expire));
		});
		
		return true; // intercept this request
	},
	
	collectSources: function(lookup, args, fx)
	{		
		lookup.fx.push(fx);
		
		lookup.load("SRC:*", function(lookup, data, path){
			// K-ToDo-Now: check if hash is correct
			debug.log("Tracker got: " + path);
			if(path.substr(0,3) == "SRC")
				fx.addResponse(data, true); // MORE
		});
		
		return true; // intercept this request
	}
};

var lookupAPI = new Lookup.API('lookupAPI');
