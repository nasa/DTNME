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


$make_cumul = 0;
$make_bars = 0;
$outputfile = "res.txt";
$run_no = 2;

GetOptions( 
          "cumul" => \$make_cumul,
          "bars" => \$make_bars,
          "o=s" => \$outputfile,
          "run=i" => \$run_no
          );

$base="./logs/run$run_no/";


if($run_no == 2)
{
    @loss_rates=("L0","L0.01","L0.02","L0.05");
    @loss_rates=("L0","L0.02");
    @sizes=("1","10","20","40","60","100","1000");
    @nums=("500","200","100","50","50","50","10");
    $is_rabin = 1;
}
elsif($run_no == 1)
{
    @loss_rates=("L0");
    @sizes=("1","10","100");
    @nums=("500","200","50");
    $is_rabin = 1;
}
elsif($run_no == 3)
{
    @loss_rates = ("L0");
    $is_sush = 1;
    %shifts =(
            "0 0" => "offset0_S40",
            "1 10" => "offset10_S40",
            "2 60" => "offset60_S40",
            "3 RND" => "offsetXX_S40"
            );
     $p_size = 40;
     $p_num  = 40; 
     
}


%protos=("tcp0" => "TCP (E2E)",
         "tcp1" => "TCP (HOP)",
         "dtn0" => "DTN (E2E)",
         "dtn1" => "DTN (HOP)",
         "mail0" => "MAIL (E2E)",
         "mail1" => "MAIL (HOP)"
         );


$bandwidth = "100";
$nodes=5;

if($make_cumul == 1)
{
    foreach $loss (@loss_rates)
    {
        if($is_rabin == 1)
        {
            $index=0;
            foreach $size (@sizes)
            {
                $num=$nums[$index];
                $key="rabin$size-N$nodes-$loss-M$num-S$size-B$bandwidth";
                $command="perl makeplot.pl -base $base/$loss/ -exp $key";
                print "Executing : $command ....\n";
                system($command);
            }
            $index=$index+1;
        }
        if($is_sush == 1)
        {
            foreach $shift (keys %shifts)
            {
            
                $key = $shifts{$shift};
                $command="perl makeplot.pl -base $base/$loss/ -exp $key -nodes 5 -num $p_num";
                print "Executing : $command ....\n";
                system($command);
            }        
        }
    }
}





if($make_bars == 1)
{
    open(FOUT,">$outputfile") || die ("cannot open file:$outputfile\n");
    

    $topline = "";
    $datalines = "";
    foreach $loss (@loss_rates)
    {
        if($is_rabin == 1)
        {
            $index=0;
            foreach $size (@sizes)
            {
                $num=$nums[$index];
                $key="rabin$size-N$nodes-$loss-M$num-S$size-B$bandwidth";
                
                $topline = "";
                $printline = "";
                $bwfactor = 1.0;
                $maxtime = $size*$num*8.0/$bandwidth;
                ($printline,$topline) = &make_protos_line($loss,$size,$num,$key,$bwfactor,$maxtime,\%protos);
                
                $printline = "$size KB $printline";
                $datalines="$datalines $printline\n";
                $index=$index+1;
            }
        }
            
        if($is_sush == 1)
        {
            foreach $shift (sort keys %shifts)
            {
                @parts=split(/[ \t]+/,$shift);
                $shiftsecs = $parts[1];
                $printline = "";
                $topline = "";
                
                $bwfactor = 4.0;
                $key = $shifts{$shift};
                $maxtime=1800; 
                ($printline,$topline) = &make_protos_line($loss,$p_size,$p_num,$key,$bwfactor,$maxtime,\%protos);
                $printline = "$shiftsecs $printline";

                $datalines="$datalines $printline\n";
             }
        }
    }
    print FOUT "$topline\n";
    print FOUT $datalines;  
    close(FOUT); 
}

exit;


sub make_protos_line()
{
    my ($loss,$size,$num,$key,$bwfactor,$maxtime,$protos) = @_;
    
    my ($topline,$printline);
    $topline="";
    $printline="";
    
    foreach $proto(keys %$protos)
    {
        $fname = "$base/$loss/$key/$proto/times.txt";
        $firstline = &get_first_line($fname); 
        print "Fname: $fname \n\tAnd line:$firstline maxtime:$maxtime\n";
        @parts=split(/[ \t]+/,$firstline);
        
        $total_rcvd=$parts[5];
        $total_time=$parts[4]-$parts[1];
        
        $total_sent=$parts[6];
        
        $used_bw = 0;
        if($total_rcvd > 0)  { 
            if($total_rcvd < $num) {
                $total_time = $maxtime;
            }
            $used_bw = $bwfactor*8.0*$size*$total_rcvd/$total_time;
        }
        
        $fraction= $used_bw/$bandwidth;
        print "\t[$fraction] --> rcv:$total_rcvd sent:$num time:$total_time used:$used_bw \n";
        
        $printline = "$printline,$fraction";
        $topline = "$topline,$protos->{$proto}";
        print "\n";
    }
    return ($printline,$topline);
}



sub get_first_line() {
    my($fname) = @_;
    open(F,"$fname") || return "# 0 0 0 0 0\n";
    $line = <F>;
    close(F);
    return $line;
}


