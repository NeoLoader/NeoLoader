/*
*	Name: File Repository
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

	publishAlias: function(lookup, args, fx)
	{
		this.publish("ALIAS", lookup, args, fx);
		return true; // intercept this request
	},
	publishNesting: function(lookup, args, fx)
	{
		this.publish("NEST", lookup, args, fx);
		return true; // intercept this request
	},
	publishIndex: function(lookup, args, fx)
	{
		this.publish("INDEX", lookup, args, fx);
		return true; // intercept this request
	},
	publishHash: function(lookup, args, fx)
	{
		this.publish("HASH", lookup, args, fx);
		return true; // intercept this request
	},

	publish: function(ID, lookup, args, fx)
	{
		lookup.fx.push(fx);
		fx.storeStats = new Array();
			
		var ttl = args["TTL"];
		if(!(typeof ttl === "undefined")){
			delete args["TTL"];
			lookup.storeTTL = ttl;
		}
		
		var pubID = args["PID"];
		pubID.convert("binary");
		var path = ID + ":" + pubID;
		delete args["PID"];
		
		lookup.store(path, args, function(lookup, expire){	
			fx.storeStats.push(parseInt(expire));
		});
	},
	
	findAliases: function(lookup, args, fx)
	{		
		this.find("ALIAS", lookup, args, fx);
		return true; // intercept this request
	},
	findNestings: function(lookup, args, fx)
	{		
		this.find("NEST", lookup, args, fx);
		return true; // intercept this request
	},
	findIndex: function(lookup, args, fx)
	{		
		lookup.loadCount = 1;
		this.find("INDEX", lookup, args, fx);
		return true; // intercept this request
	},
	findHash: function(lookup, args, fx)
	{		
		lookup.loadCount = 1;
		this.find("HASH", lookup, args, fx);
		return true; // intercept this request
	},
	
	find: function(ID, lookup, args, fx){
		lookup.fx.push(fx);
		
		lookup.load(ID + ":*", function(lookup, data, path){
			if(path.substr(0,ID.length) == ID)
				fx.addResponse(data, true); // MORE
		});
	}
	
};

var lookupAPI = new Lookup.API('lookupAPI');
