#!/usr/bin/perl

#
#    Copyright 2005-2006 Intel Corporation
# 
#    Licensed under the Apache License, Version 2.0 (the "License");
#    you may not use this file except in compliance with the License.
#    You may obtain a copy of the License at
# 
#        http://www.apache.org/licenses/LICENSE-2.0
# 
#    Unless required by applicable law or agreed to in writing, software
#    distributed under the License is distributed on an "AS IS" BASIS,
#    WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
#    See the License for the specific language governing permissions and
#    limitations under the License.
#


use Getopt::Long;

my $logscalex = 0;
my $logscaley = 0;


my $expname='';
my $base='';


my $nodes=4;
my $loss=0;
my $bandwidth=100;
my $size=10;
my $num=100;


my $stop_at=-1;
my $is_mike = 1;
my $theoryfile = "theory1.txt";

GetOptions( 
          "exp=s" => \$expname,
          "base=s" => \$base,
          "nodes=i" => \$nodes,
          "num=i" => \$num,
          "size=i" => \$size,
          "stop=i" => \$stop_at,
          );

if($base eq '')
{
    print "base not given\n";
    exit;
}
if($expname eq '')
{
    print "expname not given\n";
    exit;
}
$expbase="$base/$expname";


if ( !(-d $expbase))
{
    print "$expbase does not exist\n";
    exit;
}

%protos=("5 tcp0" => "SFTP",
         #" 6 tcp1" => "TCP (HOP)",
         "4 dtn0" => "DTN (E2E)",
         "2 dtn1" => "DTN (HOP)",
         "1 dtn3" => "DTN (40K)",
         #"7 mail0" => "MAIL (E2E)",
         "3 mail1" => "MAIL"
         );
%linestyle=("tcp0" => "6",
         #"tcp1" => "7",
         "dtn0" => "5",
         "dtn1" => "3",
         "dtn3" => "2",
         #"mail0" => "8",
         "mail1" => "4"
         );

$plotbase="$expbase/gnuplot";
$logbase="$plotbase/log";
$psbase="$plotbase/ps";

mkdir($plotbase);
mkdir($logbase);
mkdir($psbase);

if($expname =~ /rabin/g)
{
    print "Getting info about experiment ..\n";
    $expname =~ /([\w]+)-N([1-9])-L([0-9\.]+)-M([0-9]+)-S([0-9]+)-B([0-9]+).*/g;
    $nodes=$2;
    $loss=$3;
    $num=$4;
    $size=$5;
    $bandwidth=$6;
}
print "Name: Nodes:$nodes Loss:$loss Num:$num Size:$size Bandwidth:$bandwidth\n";

$command="maketable.sh $nodes $expname $base";
print "First generating the time table ... : $command\n";
system($command);


### the code below needs a  $key ...$costo

$vacfile="$plotbase/plot.gnp";

print "\n\nNow starting gnuplot generation stuff ...\n";

open (VACFILE, ">$vacfile") || die ("cannot open:$vacfile\n");


$pointsize="1.5";
#p1("");
#set pointsize 2.0 ;
&p1("
set linestyle 1 lt -1 lw 4  ;
set linestyle 2 lt 1 lw 7 pt 1 ps $pointsize  ;
set linestyle 3 lt 3 lw 3 pt 3 ps $pointsize  ;
set linestyle 4 lt 10 lw 7 pt 2 ps $pointsize  ;
set linestyle 5 lt 7 lw 3 pt 5 ps $pointsize  ;
set linestyle 6 lt 10 lw 6 pt 6 ps $pointsize  ;
set linestyle 7 lt 15 lw 3 pt 17 ps 2 ;
set linestyle 8 lt 3 lw 3 pt 15 ps 2 ; 
");


&p1("set ylabel ", &qs("# of messages received"));
&p1("set xlabel ", &qs("Time (s)"));


$gx_init = $logscalex;
$gy_init = $logscaley;


#&p1("set xrange [$gx_init:1.1]");
#&p1("set yrange [$gy_init:1.1]");
&p1("!touch empty ; \n"); 
&p1("set xrange [$gx_init:500]");
&p1("set yrange [$gx_init:500]");
&p1("set term x11 1 ; ","plot ",&qs("empty"), " title ",&qs(""));



#$plottitle="Nodes:$nodes Size:$size Loss=$loss" ;
#&p1("set title ",&qs($plottitle));

if($is_mike == 1)
{
    &p1("set xtics 240");
    &p1("set ytics border nomirror 0,2");
    &p1("set y2tics border nomirror 0,2");
    &p1("set grid xtics ");
    &p1("set key top left");
    $lsid = 1;
    &p1( "replot  ",&qs($theoryfile), "  using 1:(\$6/($size*8)) title ",&qs("MAX"), " with lines ls $lsid");
}



$xrange=100;
$yrange=100;
foreach $proto_ (sort keys %protos) {
    @parts = split(/[ \t]+/,$proto_);
    $proto = $parts[1];
    my $timesfile="$expbase/$proto/times.txt";
    print "Proto:$proto_:$proto\n";
    if ( !(-e $timesfile)) 
    {
        next;
    }
    my $timesfile1="$expbase/$proto/times1.txt";
    
    &add_stair_step($timesfile,$timesfile1);
    
    open(FIN,"$timesfile") || die("cannot open file:$timesfile\n");
    $lineno=0;
    $startime=0;$endtime=0;
    while($line=<FIN>)
    {
        @parts=split(/[ \t]+/,$line);
        if($lineno == 0) {
            $startime = $parts[1];
        }
        $endtime=$parts[2];
        $lineno=$lineno+1;
    }
    close(FIN);
    
    $diff=$endtime-$startime;
    if($xrange < $diff) {$xrange=$diff;}
    
    print "good: $timesfile start:$startime end:$endtime xrange:$xrange\n";
    
    my $title ="$protos{$proto_}";

    #&p1( "replot  ",&qs($timesfile), "  using 1:(\$3-$startime) title ",&qs($title), " with lines ls $lsid");
    $lsid=$linestyle{$proto};
    &p1( "replot  ",&qs($timesfile1), "  using (\$3-$startime):1 title ",&qs($title), " with linespoints ls $lsid");
} 
    


if($stop_at != -1 )
{
    $xrange = $stop_at;
}
$yrange=$num*1.2;
if($is_mike == 1)
{
    $yrange=$yrange*1.2;
    $yrange=14.5;
}

&p1("set xrange [$gx_init:$xrange]");
&p1("set yrange [$gy_init:$yrange]");



if($is_mike == 1)
{
    &p1("set xtics 240");
    &p1("set ytics border nomirror 0,2,14");
    &p1("set y2tics border nomirror 0,2,14");
    &p1("set grid xtics ");
    &p1("set key top left");
}

&p1("replot");

&p1("set term postscript noenhanced color dashed 20 ; "," set output ",&qs("$psbase/plot.ps"), " ; replot ; set term x11 ; ");



close(VACFILE);
print "Run : \ngnuplot $plotbase/plot.gnp\n";
print "psfile: $psbase/plot.ps\n";




system ("gnuplot $plotbase/plot.gnp");

exit;


sub qs() {    return "\"" . $_[0] ."\""  ;}  #print &qs("HELLO ") ;
sub p1() { 
    foreach $foo (@_) {
        print VACFILE $foo;
    }
	print VACFILE "\n";
}

sub find() {
    $_key=$_[0];
    $_file =$_[1];
    open(FILE,$_file) or  die " file not found $_file";
    while(<FILE>) {
	    $foo=$_;
        #	print $foo;
	    #if (($key,$gap,$val) = ($foo =~ /(.*)($_key)(\s*=\s*)([\d:_\.]*\b)(.*)/)){
        #    return $4;
        #}
    }
    close(FILE);
}





sub add_stair_step()
{
    my($file,$file1) = @_;

    open(FIN,"$file") || die("cannot open file:$file\n");
    open(FOUT,">$file1") || die("cannot open file:$file1\n");

    
    $line = <FIN>;
    print FOUT $line;
    
    @parts = split(/[ \t]+/,$line);
    
    $start = $parts[1];
    $cur_del = $parts[3];
    $end = $parts[4];
    $rcvlen = $parts[5];
    $sentlen =  $parts[6];
    $index = 0;
    print FOUT "0 0 0\n";
    while($line = <FIN>)
    {
        @parts = split(/[ \t]+/,$line);
        $cur_del = $parts[2];
        chomp($cur_del);
        #if($cur_del - $start > $stop_at) {last;}
        
        print FOUT "$index\t$start $cur_del\n";
        print FOUT $line;
        $index=$index+1;
    }
    
    if($rcvlen >= $sentlen)
    {
        $diff = $end-$start;
        $finalend = $end;
        if($stop_at != -1 and $stop_at > $diff) {
            $finalend = $start + $stop_at;
        }
        print FOUT "$index $start $finalend\n";
    }
    close(FIN);
    close(FOUT);
}

