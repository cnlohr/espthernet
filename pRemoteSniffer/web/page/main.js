//Copyright (C) 2015-2016 <>< Charles Lohr, see LICENSE file for more info.

window.addEventListener("load", WicapSettings, false);

function WicapSettings()
{
	if( IsTabOpen('WicapSettings') )
	{
		QueueOperation( "CH",  GotChannel );
		//$("#LastPacketBody").html("X" );
		QueueOperation( "CS",  CycleCS );
	}
}

function CycleCS( req, data )
{
	if( !IsTabOpen('WicapSettings') ) return;
	
	var lns = data.split("\n"); 
	var vs = lns[0].split(":");

	var hts = "<TABLE WIDTH=10%>" +
		 "<TR><TD nowrap WIDTH=10%>Connectedsocket</TD><TD>" + vs[1] + "</TD></TR>" +
		 "<TR><TD nowrap>Packet Count:</TD><TD>" + vs[2] + "</TD></TR>" +
		 "<TR><TD nowrap>Uptime:</TD><TD>" + vs[3] + "." + ("0000000" + vs[4]).slice(-6) + "</TD></TR>" +
		 "</TABLE>";
	hts += "<TABLE WIDTH=10% BORDER=1><TR><TH WIDTH=10>MAC</TH><TH>Since seen</TH><TH>RSSI</TH></TR>";
	for( var i = 1; i < lns.length; i++ )
	{
		var l = lns[i].split("\t");
		if( l.length < 3 ) continue;
		var percent = 100+Number(l[2]);
		hts += "<TR><TD WIDTH=10>" + l[0] + "</TD><TD WIDTH=10>" + (Number(l[1])/1000000).toFixed(2) + "</TD><TD><DIV style='background:linear-gradient(to right, #efe3af " + percent + "%,#ffffff " + percent + "%);'>" + l[2] + "</DIV></TD></TR>";
	}
	hts += "</TABLE>";

	$("#netstats").html( hts );

	QueueOperation( "CS",  CycleCS );
}

function UpdateValids()
{

	QueueOperation( "CH",  GotChannel );
	//If comms go on or off..
}

function GotChannel( req, data )
{
	console.log( data );
	$("#channel").val( data.split(":")[1] )
}

function SetChannel( req, data )
{
	$("#channel").val( data.split(":")[1] )
}

function getChannel()
{
	QueueOperation( "CH",  GotChannel );
}

function setChannel()
{
	QueueOperation( "CI:" + $("#channel").val(),  SetChannel );
}


function ToggleWicapSettings()
{
	WicapSettings();
}


