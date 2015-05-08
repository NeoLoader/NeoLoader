Windows = Array();
function OpenWindow(Url, Name, Config) {
    if (Windows[Name] == undefined || Windows[Name].closed)
        Windows[Name] = window.open(Url, Name, Config);
    else
        Windows[Name].location = Url;
}

UpdateCounter = 0;
function RunUpdate() {
    UpdateCounter++;
    var arr = new Array();
    var arr = document.getElementsByName("Progress");
    for (var i = 0; i < arr.length; i++) {
        if (arr.item(i).complete) {
            var pos = arr.item(i).src.indexOf("&Update");
            if (pos == -1)
                pos = arr.item(i).src.length;
            arr.item(i).src = arr.item(i).src.substr(0, pos);
            arr.item(i).src += "&Update=" + UpdateCounter % 2;
            //arr.item(i).src += "&Update=" + UpdateCounter;
        }
    }
}