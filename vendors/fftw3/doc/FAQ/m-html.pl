## HTML output
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

%saniarray= ('<','lt', '>','gt', '&','amp', '"','quot');

sub html_init {
    $html_prefix = './'.$prefix;
    $html_prefix =~ s:^\.//:/:;
    system('rm','-r',"$html_prefix.html");
    system('mkdir',"$html_prefix.html");
    open(HTML,">$html_prefix.html/index.html");
    print HTML "<!DOCTYPE HTML PUBLIC \"-//W3C//DTD HTML 3.2//EN\">\n";
    print HTML "<html>\n";
    $html_needpara= -1;
    $html_end='';
    chop($html_date=`date '+%d %B %Y'`);
    chop($html_year=`date '+%Y'`);
}

sub html_startup {
    print HTML <<END;
<head><title>
$user_title
</title>
<link rev="made" href="mailto:$user_authormail">
<link rel="Contents" href="index.html">
<link rel="Start" href="index.html">
<META name="description"
      content="Frequently asked questions and answers (FAQ) for FFTW.">
<link rel="Bookmark" title="FFTW FAQ" href="index.html">
<LINK rel="Bookmark" title="FFTW Home Page"
      href="http://www.fftw.org">
<LINK rel="Bookmark" title="FFTW Manual"
      href="http://www.fftw.org/doc/">
</head><body text="#000000" bgcolor="#FFFFFF"><h1>
$user_title
</h1>
END
    &html_readrefs($_[0]);
    if (length($user_copyrightref)) {
        local ($refn) = $qrefn{$user_copyrightref};
        if (!length($refn)) {
            warn "unknown question (copyright) `$user_copyrightref'";
        }
        $refn =~ m/(\d+)\.(\d+)/;
        local ($s,$n) = ($1,$2);
        $html_copyrighthref= ($s == $html_sectionn)?'':"section$s.html";
        $html_copyrighthref.= "#$qn2ref{$s,$n}";
    }
}

sub html_close {
    print HTML $html_end,"<address>\n$user_author\n";
    print HTML "- $html_date\n</address><br>\n";
    print HTML "Extracted from $user_title,\n";
    print HTML "<A href=\"$html_copyrighthref\">" if length($html_copyrighthref);
    print HTML "Copyright &copy; $html_year $user_copyholder.";
    print HTML "</A>" if length($html_copyrighthref);
    print HTML "\n</body></html>\n";
    close(HTML);
}

sub html_startmajorheading {
    local ($ref, $this,$next,$back) = @_;
    local ($nextt,$backt);
    $this =~ s/^Section /section/;  $html_sectionn= $ref;
    $next =~ s/^Section /section/ && ($nextt= $sn2title{$'});
    $back =~ s/^Section /section/ ? ($backt= $sn2title{$'}) : ($back='');
    if ($html_sectionn) {
        &html_close;
        open(HTML,">$html_prefix.html/$this.html");
	print HTML "<!DOCTYPE HTML PUBLIC \"-//W3C//DTD HTML 3.2//EN\">\n";
        print HTML "<html>\n";
        $html_end= "<hr>\n";
        $html_end.= "Next: <a href=\"$next.html\" rel=precedes>$nextt</a>.<br>\n"
            if $next;
        $html_end.= "Back: <a href=\"$back.html\" rev=precedes>$backt</a>.<br>\n"
            if $back;
        $html_end.= "<a href=\"index.html\" rev=subdocument>";
        $html_end.= "Return to contents</a>.<p>\n";
        print HTML <<END;
<head><title>
$user_brieftitle - Section $html_sectionn
</title>
<link rev="made" href="mailto:$user_authormail">
<link rel="Contents" href="index.html">
<link rel="Start" href="index.html">
END
        print HTML "<link rel=\"Next\" href=\"$next.html\">" if $next;
	print HTML "<link rel=\"Previous\" href=\"$back.html\">" if $back;
        print HTML <<END;
<link rel="Bookmark" title="FFTW FAQ" href="index.html">
</head><body text="#000000" bgcolor="#FFFFFF"><h1>
$user_brieftitle - Section $html_sectionn <br>
END
        $html_needpara= -1;
    }
    else {
	print HTML "\n<h1>\n";
	$html_needpara=-1;
    }
}

sub html_endmajorheading {
    print HTML "\n</h1>\n\n";
    $html_needpara=-1;
}

sub html_startminorheading {
    local ($ref, $this) = @_;
    $html_needpara=0;
    $this =~ m/^Question (\d+)\.(\d+)/;
    local ($s,$n) = ($1,$2);
    print HTML "\n<h2><A name=\"$qn2ref{$s,$n}\">\n";
}

sub html_endminorheading {
    print HTML "\n</A></h2>\n\n";
    $html_needpara=-1;
}

sub html_newsgroup { &arg('newsgroup'); }
sub html_endnewsgroup { &endarg('newsgroup'); }
sub html_do_newsgroup {
    print HTML "<A href=\"news:$_[0]\"><code>$_[0]</code></A>";
}

sub html_email { &arg('email'); }
sub html_endemail { &endarg('email'); }
sub html_do_email {
    print HTML "<A href=\"mailto:$_[0]\"><code>$_[0]</code></A>";
}

sub html_courier    { print HTML "<code>" ; }
sub html_endcourier { print HTML "</code>"; }
sub html_italic     { print HTML "<i>"   ; }
sub html_enditalic  { print HTML "</i>"  ; }

sub html_docref { &arg('docref'); }
sub html_enddocref { &endarg('docref'); }
sub html_do_docref {
    if (!defined($html_refval{$_[0]})) {
        warn "undefined HTML reference $_[0]";
        $html_refval{$n}='UNDEFINED';
    }
    print HTML "<A href=\"$html_refval{$_[0]}\">";
    &recurse($_[0]);
    print HTML "</A>";
}

sub html_readrefs {
    local ($p);
    open(HTMLREFS,"<$_[0]") || (warn("failed to open HTML refs $_[0]: $!"),return);
    while(<HTMLREFS>) {
        next if m/^\\\s/;
        s/\s*\n$//;
        if (s/^\\prefix\s*//) {
            $p= $'; next;
        } elsif (s/^\s*(\S.*\S)\s*\\\s*//) {
            $_=$1; $v=$';
            s/\\\\/\\/g;
            $html_refval{$_}= $p.$v;
        } else {
            warn("ununderstood line in HTML refs >$_<");
        }
    }
    close(HTMLREFS);
}
    
sub html_ftpsilent { &arg('ftpsilent'); }
sub html_endftpsilent { &endarg('ftpsilent'); }
sub html_do_ftpsilent {
    if ($_[0] =~ m/:/) {
        $html_ftpsite= $`;
        $html_ftpdir= $'.'/';
    } else {
        $html_ftpsite= $_[0];
        $html_ftpdir= '';
    }
}

sub html_ftpon { &arg('ftpon'); }
sub html_endftpon { &endarg('ftpon'); }
sub html_do_ftpon {
#print STDERR "ftpon($_[0])\n";
    $html_ftpsite= $_[0]; $html_ftpdir= '';
    print HTML "<code>";
    &recurse($_[0]);
    print HTML "</code>";
}

sub html_ftpin { &arg('ftpin'); }
sub html_endftpin { &endarg('ftpin'); }
sub html_do_ftpin {
#print STDERR "ftpin($_[0])\n";
    print HTML "<A href=\"ftp://$html_ftpsite$html_ftpdir$_[0]\"><code>";
    &recurse($_[0]);
    print HTML "</code></A>";
}

sub html_text {
    print HTML "\n<p>\n" if $html_needpara > 0;
    $html_needpara=0;
    $html_stuff= &html_sanitise($_[0]);
    while ($html_stuff =~ s/^(.{40,70}) //) {
        print HTML "$1\n";
    }
    print HTML $html_stuff;
}

sub html_tab {
    $htmltabignore++ || warn "html tab ignored";
}

sub html_newline       { print HTML "<br>\n"    ;                       }
sub html_startverbatim { print HTML "<pre>\n"   ;                       }
sub html_verbatim      { print HTML &html_sanitise($_[0]),"\n";         }
sub html_endverbatim   { print HTML "</pre>\n"  ;  $html_needpara= -1;  }

sub html_endpara {
    $html_needpara || $html_needpara++;
}

sub html_finish {
    &html_close;
}

sub html_startindex {
    print HTML "<ul>\n";
}

sub html_endindex {
    print HTML "</ul><hr>\n";
}

sub html_startindexitem {
    local ($ref,$qval) = @_;
    $qval =~ m/Q(\d+)\.(\d+)/;
    local ($s,$n) = ($1,$2);
    print HTML "<li><a href=\"";
    print HTML ($s == $html_sectionn)?'':"section$s.html";
    print HTML "#$qn2ref{$s,$n}\" rel=subdocument>Q$s.$n. ";
    $html_indexunhead='';
}

sub html_startindexmainitem {
    local ($ref,$s) = @_;
    $s =~ m/\d+/; $s= $&;
    print HTML "<br><br>" if ($s > 1);
    print HTML "<li><b><font size=\"+2\"><a href=\"section$s.html\" rel=subdocument>Section $s.  ";
    $html_indexunhead='</font></b>';
}

sub html_endindexitem {
    print HTML "</a>$html_indexunhead\n";
}

sub html_startlist {
    print HTML "\n";
    $html_itemend="<ul>";
}

sub html_endlist {
    print HTML "$html_itemend\n</ul>\n";
    $html_needpara=-1
}

sub html_item {
    print HTML "$html_itemend\n<li>";
    $html_itemend="";
    $html_needpara=-1;
}

sub html_startpackedlist {
    print HTML "\n";
    $html_itemend="<dir>";
}

sub html_endpackedlist {
    print HTML "$html_itemend\n</dir>\n";
    $html_needpara=-1;
}

sub html_packeditem {
    print HTML "$html_itemend\n<li>";
    $html_itemend="";
    $html_needpara=-1;
}

sub html_startindent   { print HTML "<blockquote>\n"; }
sub html_endindent     { print HTML "</blockquote>\n"; }

sub html_pageref {
    local ($ref,$sq) = @_;
    $sq =~ m/(\d+)\.(\d+)/;
    local ($s,$n) = ($1,$2);
    print HTML "<A href=\"";
    print HTML ($s == $html_sectionn)?'':"section$s.html";
    print HTML "#$qn2ref{$s,$n}\">Q$sq \`";
}

sub html_endpageref {
    print HTML "'</A>";
}

sub html_sanitise {
    local ($in) = @_;
    local ($out);
    while ($in =~ m/[<>&"]/) {
        $out.= $`. '&'. $saniarray{$&}. ';';
        $in=$';
    }
    $out.= $in;
    $out;
}

1;
