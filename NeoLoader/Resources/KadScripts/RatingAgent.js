/*
*	Name: Rating Agent
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

	publishRating: function(lookup, args, fx)
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
		var path = "CMNT:" + pubID;
		delete args["PID"];
		
		lookup.store(path, args, function(lookup, expire){	
			fx.storeStats.push(parseInt(expire));
		});
		
		return true; // intercept this request
	}
};

var lookupAPI = new Lookup.API('lookupAPI');

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Index API

var Index = {
	API: function(name) {
		this.name = name;
	}
};

Index.API.prototype = {
	name: '',

  findRatings: function (request, targetID) {
		
		/*if("RID" in request)
		{
			var releaseID = request["RID"];
			releaseID.convert("binary");
			var path = "CMNT:" + releaseID;
			entrys = kademlia.index.list(targetID, path);
		}
		else*/
		var entrys = kademlia.index.list(targetID, "CMNT:*");

		var ratingList = new Variant();
		for (var i = 0; i < entrys.length; i++)
			ratingList.append(kademlia.index.load(entrys[i]));
		
		var result = new Variant();
		result["RL"] = ratingList;
		return result;
 }
}
	
var localAPI = new Index.API('localAPI');
var remoteAPI = new Index.API('remoteAPI');