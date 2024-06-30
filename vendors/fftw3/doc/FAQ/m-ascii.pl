## ASCII output
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

sub ascii_init {
    open(ASCII,">$prefix.ascii");
}

sub ascii_startmajorheading {
    print ASCII '='x79,"\n\n";
    $ascii_status= 'h';
    &ascii_text($_[0] ? "Section $_[0].  " : '');
}

sub ascii_startminorheading {
    print ASCII '-'x79,"\n\n";
    $ascii_status= 'h';
}

sub ascii_italic { &ascii_text('*'); }
sub ascii_enditalic { $ascii_para .= '*'; }

sub ascii_email { &ascii_text('<'); } sub ascii_endemail { &ascii_text('>'); }

sub ascii_ftpon { } sub ascii_endftpon { }
sub ascii_ftpin { } sub ascii_endftpin { }
sub ascii_docref { } sub ascii_enddocref { }
sub ascii_courier { } sub ascii_endcourier { }
sub ascii_newsgroup { }  sub ascii_endnewsgroup { }
sub ascii_ftpsilent { $ascii_ignore++; }
sub ascii_endftpsilent { $ascii_ignore--; }

sub ascii_text {
    return if $ascii_ignore;
    if ($ascii_status eq '') {
        $ascii_status= 'p';
    }
    $ascii_para .= $_[0];
}

sub ascii_tab {
    local ($n) = $_[0]-length($ascii_para);
    $ascii_para .= ' 'x$n if $n>0;
}

sub ascii_newline {
    return unless $ascii_status eq 'p';
    &ascii_writepara;
}

sub ascii_writepara {
    local ($thisline, $thisword, $rest);
    for (;;) {
        last unless $ascii_para =~ m/\S/;
        $thisline= $ascii_indentstring;
        for (;;) {
            last unless $ascii_para =~ m/^(\s*\S+)/;
            unless (length($1) + length($thisline) < 75 ||
                    length($thisline) == length($ascii_indentstring)) {
                last;
            }
            $thisline .= $1;
            $ascii_para= $';
        }
        $ascii_para =~ s/^\s*//;
        print ASCII $thisline,"\n";
        $ascii_indentstring= $ascii_nextindent;
        last unless length($ascii_para);
    }
    $ascii_status= '';  $ascii_para= '';
}    

sub ascii_endpara {
    return unless $ascii_status eq 'p';
    &ascii_writepara;
    print ASCII "\n";
}

sub ascii_endheading {
    $ascii_para =~ s/\s*$//;
    print ASCII "$ascii_para\n\n";
    $ascii_status= '';
    $ascii_para= '';
}

sub ascii_endmajorheading { &ascii_endheading(@_); }
sub ascii_endminorheading { &ascii_endheading(@_); }

sub ascii_startverbatim {
    $ascii_vstatus= $ascii_status;
    &ascii_writepara;
}

sub ascii_verbatim {
    print ASCII $_[0],"\n";
}

sub ascii_endverbatim {
    $ascii_status= $ascii_vstatus;
}

sub ascii_finish {
    close(ASCII);
}

sub ascii_startindex { $ascii_status= ''; }
sub ascii_endindex { $ascii_status= 'p'; }

sub ascii_endindexitem {
    printf ASCII " %-11s %-.66s\n",$ascii_left,$ascii_para;
    $ascii_status= 'p';
    $ascii_para= '';
}

sub ascii_startindexitem {
    $ascii_left= $_[1];
}

sub ascii_startindexmainitem {
    $ascii_left= $_[1];
    print ASCII "\n" if $ascii_status eq 'p';
}

sub ascii_startindent {
    $ascii_istatus= $ascii_status;
    &ascii_writepara;
    $ascii_indentstring= "   $ascii_indentstring";
    $ascii_nextindent= "   $ascii_nextindent";
}

sub ascii_endindent {
    $ascii_indentstring =~ s/^   //;
    $ascii_nextindent =~ s/^   //;
    $ascii_status= $ascii_istatus;
}

sub ascii_startpackedlist { $ascii_plc=0; }
sub ascii_endpackedlist { &ascii_newline if !$ascii_plc; }
sub ascii_packeditem {
    &ascii_newline if !$ascii_plc;
    &ascii_tab($ascii_plc*40+5);
    $ascii_plc= !$ascii_plc;
}

sub ascii_startlist {
    &ascii_endpara;
    $ascii_indentstring= "  $ascii_indentstring";
    $ascii_nextindent= "  $ascii_nextindent";
}

sub ascii_endlist {
    &ascii_endpara;
    $ascii_indentstring =~ s/^  //;
    $ascii_nextindent =~ s/^  //;
}

sub ascii_item {
    &ascii_newline;
    $ascii_indentstring =~ s/  $/* /;
}

sub ascii_pageref {
    &ascii_text("Q$_[1] \`");
}

sub ascii_endpageref {
    &ascii_text("'");
}

1;
