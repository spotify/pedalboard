## Info output
# Copyright (C) 1993-1995 Ian Jackson.

# This file is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2, or (at your option)
# any later version.

# It is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.

# You should have received a copy of the GNU General Public License
# along with GNU Emacs; see the file COPYING.  If not, write to
# the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
# Boston, MA 02111-1307, USA.

# (Note: I do not consider works produced using these BFNN processing
# tools to be derivative works of the tools, so they are NOT covered
# by the GPL.  However, I would appreciate it if you credited me if
# appropriate in any documents you format using BFNN.)

sub info_init {
    open(INFO,">$prefix.info");
    print INFO <<END;
Info file: $prefix.info,    -*-Text-*-
produced by bfnnconv.pl from the Bizarre Format With No Name.

END
}

sub info_heading {
    # refstring  Node  Next  Previous Up
    print INFO "\nFile: $prefix.info, Node: $_[1]";
    print INFO ", Next: $_[2]" if length($_[2]);
    print INFO ", Previous: $_[3]" if length($_[3]);
    print INFO ", Up: $_[4]" if length($_[4]);
    print INFO "\n\n";
    $info_status= '';
}

sub info_startmajorheading {
    return if $_[0] eq '0';
    &info_heading('s_'.$_[0],@_[1..$#_],'Top');
}

sub info_startminorheading {
    &info_heading(@_);
}

sub info_italic { &info_text('*'); }
sub info_enditalic { $info_para .= '*'; }

sub info_email { &info_text('<'); } sub info_endemail { &info_text('>'); }

sub info_ftpon { } sub info_endftpon { }
sub info_ftpin { } sub info_endftpin { }
sub info_docref { } sub info_enddocref { }
sub info_courier { } sub info_endcourier { }
sub info_newsgroup { }  sub info_endnewsgroup { }
sub info_ftpsilent { $info_ignore++; }
sub info_endftpsilent { $info_ignore--; }

sub info_text {
    return if $info_ignore;
    if ($info_status eq '') {
        $info_status= 'p';
    }
    $info_para .= $_[0];
}

sub info_tab {
    local ($n) = $_[0]-length($info_para);
    $info_para .= ' 'x$n if $n>0;
}

sub info_newline {
    return unless $info_status eq 'p';
    print INFO &info_writepara;
}

sub info_writepara {
    local ($thisline, $thisword, $rest, $output);
    for (;;) {
        last unless $info_para =~ m/\S/;
        $thisline= $info_indentstring;
        for (;;) {
            last unless $info_para =~ m/^(\s*\S+)/;
            unless (length($1) + length($thisline) < 75 ||
                    length($thisline) == length($info_indentstring)) {
                last;
            }
            $thisline .= $1;
            $info_para= $';
        }
        $info_para =~ s/^\s*//;
        $output.= $thisline."\n";
        $info_indentstring= $info_nextindent;
        last unless length($info_para);
    }
    $info_status= '';  $info_para= '';
    return $output;
}    

sub info_endpara {
    return unless $info_status eq 'p';
    print INFO &info_writepara;
    print INFO "\n";
}

sub info_endheading {
    $info_para =~ s/\s*$//;
    print INFO "$info_para\n\n";
    $info_status= '';
    $info_para= '';
}

sub info_endmajorheading { &info_endheading(@_); }
sub info_endminorheading { &info_endheading(@_); }

sub info_startverbatim {
    print INFO &info_writepara;
}

sub info_verbatim {
    print INFO $_[0],"\n";
}

sub info_endverbatim {
    $info_status= $info_vstatus;
}

sub info_finish {
    close(INFO);
}

sub info_startindex {
    &info_endpara;
    $info_moredetail= '';
    $info_status= '';
}

sub info_endindex {
    print INFO "$info_moredetail\n" if length($info_moredetail);
}

sub info_endindexitem {
    $info_indentstring= sprintf("* %-17s ",$info_label.'::');
    $info_nextindent= ' 'x20;
    local ($txt);
    $txt= &info_writepara;
    if ($info_main) {
        print INFO $label.$txt;
        $txt =~ s/^.{20}//;
        $info_moredetail.= $txt;
    } else {
        $info_moredetail.= $label.$txt;
    }
    $info_indentstring= $info_nextindent= '';
    $info_status='p';
}

sub info_startindexitem {
    print INFO "* Menu:\n" if $info_status eq '';
    $info_status= '';
    $info_label= $_[2];
    $info_main= 0;
}

sub info_startindexmainitem {
    print INFO "* Menu:\n" if $info_status eq '';
    $info_label= $_[2];
    $info_main= 1;
    $info_moredetail .= "\n$_[2], ";
    $info_status= '';
}

sub info_startindent {
    $info_istatus= $info_status;
    print INFO &info_writepara;
    $info_indentstring= "   $info_indentstring";
    $info_nextindent= "   $info_nextindent";
}

sub info_endindent {
    $info_indentstring =~ s/^   //;
    $info_nextindent =~ s/^   //;
    $info_status= $info_istatus;
}

sub info_startpackedlist { $info_plc=0; }
sub info_endpackedlist { &info_newline if !$info_plc; }
sub info_packeditem {
    &info_newline if !$info_plc;
    &info_tab($info_plc*40+5);
    $info_plc= !$info_plc;
}

sub info_startlist {
    $info_istatus= $info_status;
    print INFO &info_writepara;
    $info_indentstring= "  $info_indentstring";
    $info_nextindent= "  $info_nextindent";
}

sub info_endlist {
    $info_indentstring =~ s/^  //;
    $info_nextindent =~ s/^  //;
    $info_status= $info_lstatus;
}

sub info_item {
    &info_newline;
    $info_indentstring =~ s/  $/* /;
}

sub info_pageref {
    &info_text("*Note Question $_[1]:: \`");
}

sub info_endpageref {
    &info_text("'");
}

1;
