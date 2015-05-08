/*
*	Name: Sharing Hub
*   Version: 0.0.1.2
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
	},
	finish: function(lookup)
	{
		//lookup.sendResponse("example", data)
	},
	stored: function(lookup, expiration)
	{
	},
	retrieved: function(lookup, payloads)
	{
		//return true; // filter the payload
		return false;
	},
	
	example: function(lookup, request)
	{
		lookup.addRequest("example", request, function(lookup, result){
			debug.log('recived result:' + result);
			//return true; // filter the response
			return false;
		});
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
	
	example: function(arguments, targetID){
		return "sent data: " + arguments;
	}
}
	
var localAPI = new Index.API('localAPI');
var remoteAPI = new Index.API('remoteAPI');