#!/usr/bin/perl 
# we register the script
# if someone knows how to unload it clean....do tell
IRC::register("xas", "1.6", "", "Xchat Amule Statistics");
# welcome message
IRC::print "\n\0033  Follow the \0034 white\0033 rabbit\0038...\003\n";
IRC::print "\n\0035 Use command \0038/xas\0035 to print out aMule statistics\003";
# we have no life. we are robots...and we hang around in here:
IRC::print "\0035 (#amule @ irc.freenode.net)\003";
# command that we use
IRC::add_command_handler("xas","xas");

#16.6.2004 - niet      : added support for memory usage and binary name
#05.5.2004 - Jacobo221 : fixed typos, sig 2 support, new outputs, crash detect 
#29.4.2004 - niet      : renamed astats to xas (X-Chat Amule statistics)
#22.4.2004 - citroklar : added smp support
#     2004 - bootstrap : some hints on file opening
#     2004 - niet      : used some of cheetah.pl script and so astats was born

# Five status currently: online, connecting, offline, closed, crashed
sub xas
{
	#amule program name
	chomp(my $amulename = `ps --no-header -u $ENV{USER} -o ucmd --sort start_time|grep amule|head -n 1`);
	#amule binary date (because we still don't have date in CVS version). GRRRRRR
	#ls -lAF `ps --no-header -u j -o cmd --sort start_time|grep amule|head -n 1|awk '{print $2}'`
	# system uptime
	chomp(my $uptime = `uptime|cut -d " " -f 4- | tr -s " "`);
	# number of cpu's calculated from /proc/cpuinfo
	chomp(my $number_cpus = `cat /proc/cpuinfo | grep 'processor' -c`);
	# type of cpu
        chomp(my $cpu = `cat /proc/cpuinfo | grep 'model name' -m 1 | cut -f 2 -d ":" | cut -c 2-`);
	# cpu speed
	chomp(my $mhz = `cat /proc/cpuinfo | grep "cpu MHz" -m 1 | cut -f 2 -d ":" | cut -c 2-`);
	# what is the aMule's load on cpu
	chomp(my $amulecpu = `ps --no-header -C $amulename -o %cpu --sort start_time|head -n 1`);
	# how much memory is aMule using
	chomp(my $amulemem = (sprintf("%.02f", `ps --no-header -C $amulename  -o rss --sort start_time|head -n 1` / 1024 )));

	# bootstrap
	# there is no spoon...err.... signature
	open(amulesig,"$ENV{'HOME'}/.aMule/amulesig.dat") or die "aMule online signature not found. Did you enable it ?";
	chomp(@amulesigdata = <amulesig>);
	close amulesig;

	# are we high or what ? :-Q
	if ($amulesigdata[4] eq "H")
		{$amuleid="high"}
	 else
	 	{$amuleid="low"};

	# are we online / offline / connecting
	if ($amulesigdata[0]==0) {
		$amulestatus="offline";
		$amulextatus="" }
	elsif ($amulesigdata[0]==2) {	# Since aMule v2-rc4
		$amulestatus="connecting"; 
                $amulextatus="" }
	else {
		$amulestatus="online";
		$amulextatus="with $amuleid ID on server $amulesigdata[1] [ $amulesigdata[2]:$amulesigdata[3] ]" };

	# total download traffic in Gb
	my $tdl = (sprintf("%.02f", $amulesigdata[10] / 1073741824));

	# total upload traffic in Gb
	my $tul = (sprintf("%.02f", $amulesigdata[11] / 1073741824));

	# session download traffic in Gb
	my $sdl = (sprintf("%.02f", $amulesigdata[13] / 1048576));

	# session upload traffic in Gb
	my $sul = (sprintf("%.02f", $amulesigdata[14] / 1048576));

	# and display it

	# if current user isn't running aMule
	if ( ! `ps --no-header -u $ENV{USER} | grep amule`) {
		IRC::command "/say $amulesigdata[9] is not running";
		# Crash detection is implemented since v2-rc4, so XAS should be backwards compatible
		if ( grep(/^1./,$amulesigdata[12]) || $amulesigdata[12]=="2.0.0rc1" || $amulesigdata[12]=="2.0.0rc2" || $amulesigdata[12]=="2.0.0rc3" ) {
			IRC::command "/say aMule $amulesigdata[12] was closed after $amulesigdata[15]!" }
		elsif ( ! grep(/^00 /,$amulesigdata[15])) {
			IRC::command "/say aMule $amulesigdata[12] crashed after $amulesigdata[15]!" }
		else {
			IRC::command "/say aMule $amulesigdata[12] was closed" };
		IRC::command "/say Total download traffic: $tdl Gb";
		IRC::command "/say Total upload traffic:   $tul Gb" }
	# if aMule is running
	else {
		IRC::command "/say $amulesigdata[9] is $amulestatus $amulextatus";
		IRC::command "/say aMule $amulesigdata[12] is using $amulecpu% CPU, $amulemem MB of memory and it has been running for $amulesigdata[15]";

		# we only display "number of cpus" when we have more then one
		if ($number_cpus > 1) {
			IRC::command "/say on $number_cpus x $cpu @ $mhz up $uptime" } 
		else {
			IRC::command "/say on $cpu @ $mhz MHz up $uptime" };

		IRC::command "/say Sharing $amulesigdata[8] files with $amulesigdata[7] clients in queue";
		IRC::command "/say Total download traffic: $tdl Gb, total upload traffic: $tul Gb";
		IRC::command "/say Session download traffic: $sdl Mb, session upload traffic: $sul Mb";
		IRC::command "/say Current DL speed: $amulesigdata[5] KB/s, current UL speed:  $amulesigdata[6] KB/s" };
	return 1;
	# that's it
}

