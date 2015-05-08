// This part of the scripting MUST be at the bottom of the page.
// The codes below will work with multiple combo boxes all in one page.
// Just set them up like you see in the html above and you will be good to go.

// Generic test for Mozilla based browsers (all versions)
if (window.XMLHttpRequest)
{
	var iframeHeight = '0px';
	var iframeBottom = '0px';
	var selectBottom = '-13px';
}

// Generic test for IE based browsers (all versions)
else if (window.ActiveXObject)
{
	var iframeHeight = '14px';
	var iframeBottom = '-16px';
	var selectBottom = '-17px';
}
else
{
		console.log('Probably not gonna be pretty!');
}

// First set the select box's bottom margin
var selectList = document.getElementsByTagName('select');
for (var i=0; i<selectList.length; i++)
{
	if (selectList[i].getAttribute('attr') == 'select')
	{
		selectList[i].style.marginBottom = selectBottom;
	}
}

// Then set the iframe's bottom margin
var iframeList = document.getElementsByTagName('iframe');
for (var i=0; i<iframeList.length; i++)
{
	if (iframeList[i].getAttribute('attr') == 'iframe')
	{
		iframeList[i].style.marginBottom = iframeBottom;
		iframeList[i].style.height = iframeHeight;
	}
}

function writeValue(field)
{
	document.getElementById(field).focus();
	document.getElementById(field).value = document.getElementById(field + '_select').options[document.getElementById(field + '_select').selectedIndex].value;

	// Get IE to behave and put the cursor at the end of the text
	var t = document.getElementById(field), len = t.value.length;
	if (t.setSelectionRange)
	{
		t.setSelectionRange(len,len);
		t.focus();
	}
	else if (t.createTextRange)
	{
		var rn = t.createTextRange();
		rn.moveStart('character',len);
		rn.select();
	}
}