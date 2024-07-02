## Lout output
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

sub lout_init {
    open(LOUT,">$prefix.lout");
    chop($dprint= `date '+%d %B %Y'`);
    $dprint =~ s/^0//;
}

sub lout_startup {
    local ($lbs) = &lout_sanitise($user_brieftitle);
    print LOUT <<END;
\@SysInclude{ fontdefs }
\@SysInclude{ langdefs }
\@SysInclude{ dl }
\@SysInclude{ docf }
\@Use { \@DocumentLayout
  \@OddTop { \@Null }
  \@EvenTop { \@Null }
  \@StartOddTop { \@Null }
  \@StartEvenTop { \@Null }
  \@OddFoot { { $lbs } \@Centre{ - \@PageNum - } \@Right{ $dprint } }
  \@EvenFoot { { $lbs } \@Centre{ - \@PageNum - } \@Right{ $dprint } }
  \@StartOddFoot { { $lbs } \@Centre{ - \@PageNum - } \@Right{ $dprint } }
  \@StartEvenFoot { { $lbs } \@Centre{ - \@PageNum - } \@Right{ $dprint } }
  \@ParaGap { 1.70vx }
  \@InitialBreak { 1.0fx ragged hyphen }
}
\@Use { \@OrdinaryLayout }
END
    $lout_textstatus= 'p';
}

sub lout_pageref {
    print LOUT "Q$_[1] (page {\@PageOf{$_[0]}}) ";
    &lout_text("\`");
}

sub lout_endpageref {
    &lout_text("'");
}

sub lout_finish {
    print LOUT "\@End \@Text\n";
    close(L);
}

sub lout_startmajorheading {
    $lout_styles .= 'h';
    print LOUT <<END
\@CNP
{
  newpath   0  ysize 0.3 ft sub  moveto
            xsize  0  rlineto
            0  0.2 ft  rlineto
            xsize neg  0  rlineto
  closepath fill
} \@Graphic { //1.6f \@HAdjust \@Heading{
END
    ;
    $endh= "}\n{\@PageMark s_$_[0]}\n/1.0fo\n";
    &lout_text($_[0] ? "Section $_[0].  " : '');
}

sub lout_startminorheading {
    $lout_styles .= 'h';
    print LOUT "//0.2f \@CNP {\@PageMark $_[0]} \@Heading{\n";
    $endh= '';
}

sub lout_endheading {
    $lout_styles =~ s/.$//; print LOUT "}\n$endh";
    $lout_status= 'p';
}

sub lout_endmajorheading { &lout_endheading(@_); }
sub lout_endminorheading { &lout_endheading(@_); }

sub lout_courier {
    $lout_styles .= 'f';
    print LOUT "{{0.7 1.0} \@Scale {Courier Bold} \@Font {";
}

sub lout_endcourier {
    $lout_styles =~ s/.$//; print LOUT "}}";
}

sub lout_italic { $lout_styles .= 'f'; print LOUT "{Slope \@Font {"; }
sub lout_enditalic { $lout_styles =~ s/.$//; print LOUT "}}"; }

sub lout_startindent { $lout_styles .= 'i'; print LOUT "\@IndentedDisplay {\n"; }

sub lout_endindent {
    &lout_endpara;
    $lout_styles =~ s/.$//; print LOUT "}\n\@LP\n";
}

sub lout_startpackedlist { $lout_plc=-1; }
sub lout_endpackedlist { &lout_newline if !$lout_plc; }
sub lout_packeditem {
    &lout_newline if !$lout_plc;
    &lout_tab(($lout_plc>0)*40+5);
    $lout_plc= !$lout_plc;
}

sub lout_startlist {
    &lout_endpara;
    print LOUT "\@RawIndentedList style {\@Bullet} indent {0.5i} gap {1.1vx}\n";
    $lout_styles .= 'l';
    $lout_status= '';
}

sub lout_endlist {
    &lout_endpara;
    print LOUT "\@EndList\n\n";
    $lout_styles =~ s/.$//;
}

sub lout_item {
    &lout_endpara;
    print LOUT "\@ListItem{";
    $lout_styles.= 'I';
}

sub lout_startindex {
    print LOUT "//0.0fe\n";
}

sub lout_endindex {
    $lout_status='p';
}

sub lout_startindexmainitem {
    $lout_marker= $_[0];
    $lout_status= '';
    print LOUT "//0.3vx Bold \@Font \@HAdjust { \@HContract { { $_[1] } |3cx {";
    $lout_iiendheight= '1.00';
    $lout_styles .= 'X';
}

sub lout_startindexitem {
    $lout_marker= $_[0];
    print LOUT "\@HAdjust { \@HContract { { $_[1] } |3cx {";
    $lout_iiendheight= '0.95';
    $lout_styles .= 'X';
}

sub lout_endindexitem {
    print LOUT "} } |0c \@PageOf { $lout_marker } } //${lout_iiendheight}vx\n";
    $lout_styles =~ s/.$//;
}

sub lout_email { &lout_courier; &lout_text('<'); }
sub lout_endemail { &lout_text('>'); &lout_endcourier; }

sub lout_ftpon { &lout_courier; }  sub lout_endftpon { &lout_endcourier; }
sub lout_ftpin { &lout_courier; }  sub lout_endftpin { &lout_endcourier; }
sub lout_docref { }  sub lout_enddocref { }
sub lout_ftpsilent { $lout_ignore++; }
sub lout_endftpsilent { $lout_ignore--; }

sub lout_newsgroup { &lout_courier; }
sub lout_endnewsgroup { &lout_endcourier; }

sub lout_text {
    return if $lout_ignore;
    $lout_status= 'p';
    $_= &lout_sanitise($_[0]);
    s/ $/\n/ unless $lout_styles =~ m/[fhX]/;
    print LOUT $_;
}

sub lout_tab {
    local ($size) = $_[0]*0.5;
    print LOUT " |${size}ft ";
}

sub lout_newline {
    print LOUT " //1.0vx\n";
}

sub lout_sanitise {
    local ($in) = @_;
    local ($out);
    $in= ' '.$in.' ';
    $out='';
    while ($in =~ m/(\s)(\S*[\@\/|\\\"\^\&\{\}\#]\S*)(\s)/) {
        $out .= $`.$1;
        $in = $3.$';
        $_= $2;
        s/[\\\"]/\\$&/g;
        $out .= '"'.$_.'"';
    }
    $out .= $in;
    $out =~ s/^ //;  $out =~ s/ $//;
    $out;
}

sub lout_endpara {
    return if $lout_status eq '';
    if ($lout_styles eq '') {
        print LOUT "\@LP\n\n";
    } elsif ($lout_styles =~ s/I$//) {
        print LOUT "}\n";
    }
    $lout_status= '';
}

sub lout_startverbatim {
    print LOUT "//0.4f\n\@RawIndentedDisplay lines \@Break".
               " { {0.7 1.0} \@Scale {Courier Bold} \@Font {\n";
}

sub lout_verbatim {
    $_= $_[0];
    s/^\s*//;
    print LOUT &lout_sanitise($_),"\n";
}

sub lout_endverbatim { print LOUT "}\n}\n//0.4f\n"; }

1;
