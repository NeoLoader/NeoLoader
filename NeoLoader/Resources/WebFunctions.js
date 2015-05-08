////////////////////////////////////////////////////////////////////////////
// Extern usable functions

function enterValueByID(field, value)
{
    var element = document.getElementById(field);
    element.value = value;
    return true;
}

function enterValueByClass(field, value)
{
    var allTags = document.getElementsByTagName("*");
    for (var i = 0; i < allTags.length; i++)
    {
        if (allTags[i].className == field)
			allTags[i].value = value;
    }
    return true;
}

function enterValueByName(field, value)
{
    var elements = document.getElementsByName(field);
    for (var i = 0; i < elements.length; i++)
        elements[i].value = value;
    return true;
}

function submitForm(element_name, new_window)
{
    var element = getElement(element_name);
    if (element == null)
        return "element not found";
    
	if (element.tagName == "FORM") {
		//alert("form");
		if (new_window)
        	element.target = "_blank";
		return element.submit();
	}
	else if (element.tagName == "INPUT" && (element.type == "submit" || element.type == "button")) {
		//alert("input " + element.type);
		if (new_window)
			element.form.target = "_blank";
		return element.click();
	}
	//alert("unknown element tag: " + element.tagName);
	return "unknown element tag: " + element.tagName;
}

function getContent(element_name)
{
	var element = getElement(element_name);
    if (element == null)
        return "null";
	if (element.tagName == "INPUT")
		return element.value;
	else
		return element.innerHTML;
}

function addLoadEvent(func)
{
	var oldonload = window.onload;
	if (typeof window.onload != 'function')
		window.onload = func;
	else {
		window.onload = function() {
			if (oldonload)
				oldonload();
			func();
		}
	}
}

function loadFinished()
{
    //alert("test");
	window.htmlSession.JavaScriptLoadFinished();
}

function testObject()
{
    if (window.htmlSession) {
        window.htmlSession.JavaScriptLoadFinished()
        return "success";
    }
    return "failed";
}


////////////////////////////////////////////////////////////////////////////
// Helper Functions

function getFirstElementByClass(name)
{
    var allTags = document.getElementsByTagName("*");
    for (var i = 0; i < allTags.length; i++)
    {
        if (allTags[i].className == name)
			return allTags[i];
    }
    return null;
}

function getElement(element_name)
{
	var mode = element_name.substring(0, 1);
    var element_right = element_name.substring(1);
	if (mode == '#')
        return document.getElementById(element_right);
    else if (mode == '.')
        return getFirstElementByClass(element_right);
    else
        return document.getElementsByName(element_name)[0];
}