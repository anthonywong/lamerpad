#!/usr/bin/perl -w

# For converting unistrok data files (as in JavaDict) for kanjipad
#
# e.g. cat unistrok.* | uconv_jdata.pl
#
# $Id: uconv_jdata.pl,v 1.1.1.1 1999/11/16 08:04:51 ypwong Exp $

# Copyright 1999 Anthony Wong
# License: GPL version 2 (or later)

@chars = ();
$line = 0;

while (<>) {
    $line++;

    next if !/^[0-9a-fA-F]{4} /;
    my ($first, $remain) = split(/\|/, $_, 2);
    my ($unicode, $halcode) = split(/\s/, $first);
#print hex(substr($unicode,2,2))."\n";
#print hex 'substr($unicode,2,2)';
    $newStroke = sprintf("\\%03o\\%03o", hex(substr($unicode,0,2)), hex(substr($unicode,2,2)));

    $pos = index ($remain, "|");
    if ( $pos == -1 ) {
        $stroke = $remain; 
        $filter = "";
    } else {
        ($stroke, $filter) = split(/\|/, $remain, 2);
        $filter =~ s/(^\s+|\s+$)//g;
    }
    $stroke =~ s/(^\s+|\s+$)//g;

    $strokecount = (($temp_for_counting = $stroke) =~ s/\s//g) + 1;

    @strokeCodeList = split(/\s/, $stroke);
    foreach $stroker (@strokeCodeList)
    {   
        if ($stroker =~ /^(.)(.*)$/)
        {   
            ($firstChar, $otherChars) = ($1, $2);

            $firstChar =~ tr/123456789xycb/ABCDEFGHIJKLM/;
            $otherChars =~ tr/123456789xycb/abcdefghijklm/;
            $stroker = "$firstChar$otherChars";
        }
        $newStroke .= $stroker;
    }

    $newLine = "\"$newStroke";
    $newLine .= "|$filter" if (length($filter) != 0);
    $newLine .= "\"";

#print $unicode."\n";
#print $stroke. " ". $strokecount."\n";
#print $filter."\n";
#print $newLine."\n";

    if (!defined $chars[$strokecount]) {
        $chars[$strokecount] = "";
    }
    # write the stroke data, and then terminated by a null char
    $chars[$strokecount] .= eval($newLine)."\0";
}

# while (<>) {
#     $line++;
# 
#     next if !/^\s*\"/;
#     $data = eval $_;
#     if (!defined $data) {
# 	die "Could not parse line $line: $!";
#     }
#     $strokecount = ord(substr($data,0,1)) - ord('A') + 1;
# 
#     if (!defined $chars[$strokecount]) {
# 	$chars[$strokecount] = "";
#     }
# 
#     $chars[$strokecount] .= substr($data,1,-1);
# }

for (0..$#chars) {
    if (defined $chars[$_]) {
	print pack("NN",$_,length($chars[$_])+2);
	print $chars[$_];
	print "\0\0";
    }
}
print pack("NN",0,0);
