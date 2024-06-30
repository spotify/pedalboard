## POST output
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

sub post_init {
    open(POST,">$prefix.post");
}

sub post_startmajorheading {
    print POST '='x79,"\n\n";
    $post_status= 'h';
    &post_text($_[0] ? "Section $_[0].  " : '');
}

sub post_startminorheading {
    print POST '-'x77,"\n\n";
    $post_status= 'h';
}

sub post_italic { &post_text('*'); }
sub post_enditalic { $post_para .= '*'; }

sub post_email { &post_text('<'); } sub post_endemail { &post_text('>'); }

sub post_ftpon { } sub post_endftpon { }
sub post_ftpin { } sub post_endftpin { }
sub post_docref { } sub post_enddocref { }
sub post_courier { } sub post_endcourier { }
sub post_newsgroup { }  sub post_endnewsgroup { }
sub post_ftpsilent { $post_ignore++; }
sub post_endftpsilent { $post_ignore--; }

sub post_text {
    return if $post_ignore;
    if ($post_status eq '') {
        $post_status= 'p';
    }
    $post_para .= $_[0];
}

sub post_tab {
    local ($n) = $_[0]-length($post_para);
    $post_para .= ' 'x$n if $n>0;
}

sub post_newline {
    return unless $post_status eq 'p';
    &post_writepara;
}

sub post_writepara {
    local ($thisline, $thisword, $rest);
    for (;;) {
        last unless $post_para =~ m/\S/;
        $thisline= $post_indentstring;
        for (;;) {
            last unless $post_para =~ m/^(\s*\S+)/;
            unless (length($1) + length($thisline) < 75 ||
                    length($thisline) == length($post_indentstring)) {
                last;
            }
            $thisline .= $1;
            $post_para= $';
        }
        $post_para =~ s/^\s*//;
        print POST $thisline,"\n";
        $post_indentstring= $post_nextindent;
        last unless length($post_para);
    }
    $post_status= '';  $post_para= '';
}    

sub post_endpara {
    return unless $post_status eq 'p';
    &post_writepara;
    print POST "\n";
}

sub post_endheading {
    $post_para =~ s/\s*$//;
    print POST "$post_para\n\n";
    $post_status= '';
    $post_para= '';
}

sub post_endmajorheading { &post_endheading(@_); }
sub post_endminorheading { &post_endheading(@_); }

sub post_startverbatim {
    $post_vstatus= $post_status;
    &post_writepara;
}

sub post_verbatim {
    print POST $_[0],"\n";
}

sub post_endverbatim {
    $post_status= $post_vstatus;
}

sub post_finish {
    close(POST);
}

sub post_startindex { $post_status= ''; }
sub post_endindex { $post_status= 'p'; }

sub post_endindexitem {
    printf POST " %-11s %-.66s\n",$post_left,$post_para;
    $post_status= 'p';
    $post_para= '';
}

sub post_startindexitem {
    $post_left= $_[1];
}

sub post_startindexmainitem {
    $post_left= $_[1];
    print POST "\n" if $post_status eq 'p';
}

sub post_startindent {
    $post_istatus= $post_status;
    &post_writepara;
    $post_indentstring= "   $post_indentstring";
    $post_nextindent= "   $post_nextindent";
}

sub post_endindent {
    $post_indentstring =~ s/^   //;
    $post_nextindent =~ s/^   //;
    $post_status= $post_istatus;
}

sub post_startpackedlist { $post_plc=0; }
sub post_endpackedlist { &post_newline if !$post_plc; }
sub post_packeditem {
    &post_newline if !$post_plc;
    &post_tab($post_plc*40+5);
    $post_plc= !$post_plc;
}

sub post_startlist {
    &post_endpara;
    $post_indentstring= "  $post_indentstring";
    $post_nextindent= "  $post_nextindent";
}

sub post_endlist {
    &post_endpara;
    $post_indentstring =~ s/^  //;
    $post_nextindent =~ s/^  //;
}

sub post_item {
    &post_newline;
    $post_indentstring =~ s/  $/* /;
}

sub post_pageref {
    &post_text("Q$_[1] \`");
}

sub post_endpageref {
    &post_text("'");
}

1;
